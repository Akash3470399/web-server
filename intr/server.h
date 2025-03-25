#ifndef SERVER_H
#define SERVER_H

typedef struct Connection Connection;
typedef struct Server Server;


Connection *create_connection(char *addr, int port);

Server *server_init(char *addr, int port);
int server_run(Server *);
int server_stop(Server *);

#endif