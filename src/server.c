#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "server.h"
#include "buffer.h"
#include "http.h"

#define CONN_BACKLOG 20
#define SERVER_CTAB_IDX 0
#define CTAB_LEN 10
#define POLL_TIMEOUT 0
#define BUFLEN (1 << 16)

struct Connection
{
    struct pollfd pdf;
    struct sockaddr_in saddr;
};

struct Server
{
    struct pollfd server_pfd;
    struct pollfd ctab[CTAB_LEN];
    Buffer **inbufs, **outbufs;
    int ctlen;
};

RetState accept_connection(Server *s)
{
    int server_sock = s->ctab[SERVER_CTAB_IDX].fd, client_sock;
    struct pollfd client = {0, POLLIN | POLLOUT, 0};

    if( 
        (s->ctlen >= CTAB_LEN) ||
        ((client.fd = accept(server_sock, NULL, NULL)) < 0)
    )
        return ERR_S;
    
    s->ctab[s->ctlen] = client;
    s->inbufs[s->ctlen] = buffer_new(0);

    if(s->inbufs[s->ctlen] == NULL)
    {
        close(client.fd);
        return ERR_S;
    }
    s->ctlen += 1;
    s->ctab[SERVER_CTAB_IDX].revents = 0;
    printf("Connection accepted at %d\n", client.fd);
    return OK_S;
}

void close_connection(Server *s, int ctidx)
{
    close(s->ctab[ctidx].fd);
    s->ctab[ctidx].fd = -1;
    s->ctab[ctidx].events = 0;
    s->ctab[ctidx].revents = 0;
    
    buffer_reset(s->inbufs[ctidx]);
    
    // swap closed connection with last one to keep ctab continuous
    if((s->ctlen > 2) && (ctidx != s->ctlen -1))
        s->ctab[ctidx] = s->ctab[s->ctlen-1];
    s->ctlen -= 1;
}

void read_from_ready_clients(Server *s)
{
    uchar buff[BUFLEN];
    int rb = 0;
    short int revents = 0;

    for(int ctidx = 1; ctidx < s->ctlen; ctidx++)
    {
        revents = s->ctab[ctidx].revents;
        if(revents & (POLLERR | POLLHUP | POLLNVAL))
            close_connection(s, ctidx);
        else if(revents & POLLIN)
        {
            buffer_reset(s->inbufs[ctidx]);
            if((rb = recv(s->ctab[ctidx].fd, buff, BUFLEN, MSG_DONTWAIT)) > 0)
                buffer_append(s->inbufs[ctidx], buff, rb);
            else
                close_connection(s, ctidx);
            s->ctab[ctidx].revents = 0;
        }
    }
}

void write_to_ready_clients(Server *s)
{
    uchar buff[BUFLEN];
    int rb;
    for(int ctidx = 1; ctidx < s->ctlen; ctidx++)
    {
        if(
            (s->ctab[ctidx].revents & POLLOUT) &&
            (buffer_datasize(s->outbufs[ctidx]) > 0)
        )
        {
            buffer_seek(s->outbufs[ctidx], 0, Start_P);
            while((rb = buffer_read(s->outbufs[ctidx], buff, BUFLEN)) > 0)
                send(s->ctab[ctidx].fd, buff, rb, MSG_DONTWAIT);
            buffer_destroy(s->outbufs[ctidx]);
            s->outbufs[ctidx] = NULL;
        }
    }
}

void process_request(Server *s)
{
    uchar buf[2048] = {0};
    Request *req;
    Response *resp;

    for(int ctidx = 1; ctidx < s->ctlen; ctidx++)
    {
        if(buffer_datasize(s->inbufs[ctidx]) > 0)
        {
            buffer_seek(s->inbufs[ctidx], 0, Start_P);
            req = parse_request(s->inbufs[ctidx]);
            resp = handle_request(req);
            s->outbufs[ctidx] = serialize_response(resp);
            buffer_reset(s->inbufs[ctidx]);
        }
    }
}

Server *server_init(char *addr, int port)
{
    Server *s;
    struct sockaddr_in saddr;
    int sock;

    // TCP Socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket cration failed\n");
        return NULL;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    if (inet_aton(addr, &(saddr.sin_addr)) == 0)
    {
        printf("Invalid address\n");
        close(sock);
        return NULL;
    }

    if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        printf("Bind failed\n");
        close(sock);
        return NULL;
    }

    if (listen(sock, CONN_BACKLOG) < 0)
    {
        printf("Listen failed\n");
        close(sock);
        return NULL;
    }
    printf("Server started at %s:%d\n", addr, port);

    // Initilize server info and  input, output buffers for connections
    if (
        ((s = (Server *)calloc(1, sizeof(Server))) == NULL) ||
        ((s->inbufs = (Buffer **)calloc(CTAB_LEN, sizeof(Buffer *))) == NULL) ||
        ((s->outbufs = (Buffer **)calloc(CTAB_LEN, sizeof(Buffer *))) == NULL)
    )
    {   
        if(s)
        {
            free(s->inbufs);
            free(s->outbufs);
        }
        free(s);
        close(sock);
        return NULL;
    }

    // server is ready to accept connections
    s->ctab[SERVER_CTAB_IDX].fd = sock;
    s->ctab[SERVER_CTAB_IDX].events = POLLIN;
    s->ctab[SERVER_CTAB_IDX].revents = 0;
    s->ctlen = 1;

    return s;
}

int server_run(Server *s)
{
    if(!s)
        return -1;

    while (1)
    {
        if(poll(s->ctab, s->ctlen, POLL_TIMEOUT) > 0)
        {
            if(s->ctab[SERVER_CTAB_IDX].revents & POLLIN)
                accept_connection(s);
            
            read_from_ready_clients(s);
            process_request(s);
            write_to_ready_clients(s);
               
        }
    }
    return 0;
}

int server_stop(Server *s)
{
    if(!s)
        return -1;

    for(int i = 0; i < s->ctlen; i++)
    {
        if(s->ctab[i].fd > 0)
            close(s->ctab[i].fd);
        if(s->inbufs[i])
            free(s->inbufs[i]);
        if(s->outbufs[i])
            free(s->outbufs[i]);
    }    
    free(s->outbufs);
    free(s->inbufs);
    free(s);
    printf("Server stopped\n");
}
