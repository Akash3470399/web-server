#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "server.h"
#include "buffer.h"
#include "http.h"

#define CONN_BACKLOG 20
#define SERVER_CTAB_IDX 0
#define INIT_CTAB_LEN 1
#define POLL_TIMEOUT 0
#define BUFLEN 1024

struct Connection
{
    struct pollfd pdf;
    struct sockaddr_in saddr;
};

struct Server
{
    struct pollfd server_pfd;
    struct pollfd ctab[INIT_CTAB_LEN];
    Buffer **inbufs, **outbufs;
    int ctlen;
};

RetState accept_connection(Server *s)
{
    int server_sock = s->ctab[SERVER_CTAB_IDX].fd, client_sock;
    struct pollfd client = {0, POLLIN | POLLOUT, 0};

    if( 
        (s->ctlen == INIT_CTAB_LEN) ||
        ((client.fd = accept(server_sock, NULL, NULL)) < 0)
    )
        return ERR_S;
    
    s->ctab[s->ctlen] = client;
    s->inbufs[s->ctlen] = buffer_new(0);

    if((s->inbufs[s->ctlen] == NULL) || (s->outbufs[s->ctlen] == NULL))
    {
        close(client.fd);
        return ERR_S;
    }
    s->ctlen += 1;
    return OK_S;
}

void read_from_ready_clients(Server *s)
{
    uchar buff[BUFLEN];
    int rb = 0;

    for(int i = 1; i < s->ctlen; i++)
    {
        if((s->ctab[i].revents & POLLIN))
        {
            buffer_reset(s->inbufs[i]);
            while((rb = recv(s->ctab[i].fd, buff, BUFLEN, MSG_DONTWAIT)) > 0)
                buffer_append(s->inbufs[i], buff, rb);
        }
    }
}

void write_to_ready_clients(Server *s)
{
    uchar buff[BUFLEN];
    int rb;
    for(int i = 1; i < s->ctlen; i++)
    {
        if(
            (s->ctab[i].revents & POLLOUT) && (s->outbufs[i]) &&
            (buffer_datasize(s->outbufs[i]) > 0)
        )
        {
            buffer_seek(s->outbufs[i], 0, Start_P);
            while((rb = buffer_read(s->outbufs[i], buff, BUFLEN)) > 0)
                send(s->ctab[i].fd, buff, rb, MSG_DONTWAIT);
            buffer_destroy(s->outbufs[i]);
            s->outbufs[i] = NULL;
        }
    }
}

int process_request(Server *s)
{
    Request *req;
    Response *resp;

    for(int i = 1; i < s->ctlen; i++)
    {
        if(buffer_datasize(s->inbufs[i]) > 0)
        {
            req = parse_request(s->inbufs[i]);
            resp = handle_request(req);
            s->outbufs[i] = serialize_response(resp);
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

    if (
        ((s = (Server *)malloc(sizeof(Server))) == NULL) &&
        ((s->inbufs = (Buffer **)calloc(INIT_CTAB_LEN, sizeof(Buffer *))) == NULL) &&
        ((s->outbufs = (Buffer **)calloc(INIT_CTAB_LEN, sizeof(Buffer *))) == NULL)
    )
    {
        close(sock);
        return NULL;
    }

    s->ctab[s->ctlen].fd = sock;
    s->ctab[s->ctlen].events = POLLIN;
    s->ctab[s->ctlen++].revents = 0;
    return s;
}

int server_run(Server *s)
{
    if(!s)
        return -1;

    int ready;
    while (1)
    {
        ready = poll(s->ctab, s->ctlen, POLL_TIMEOUT);
        if(ready > 0)
        {
            if(s->ctab[SERVER_CTAB_IDX].revents == POLLIN)
                accept_connection(s);
            else
            {
                read_from_ready_clients(s);
                process_request(s);
                write_to_ready_clients(s);
            }    
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
