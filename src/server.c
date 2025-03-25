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

#define CONN_BACKLOG 20
#define INIT_CTAB_LEN 1
#define POLL_TIMEOUT 0

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
    if (inet_aton(addr, &(saddr.sin_addr.s_addr)) == 0)
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

int server_run(Server *s);

int server_stop(Server *);
