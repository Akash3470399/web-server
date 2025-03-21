#include <unistd.h>
#include <arpa/inet.h>

#include "server.h"
#include "buffer.h"

typedef struct{
    int sock;
}Connection;


typedef struct
{
    Connection *selfconn;
    Connection *connection_table;
}Server;



int moniter_active_connections(Connection *conn_tab, int nconns);
int accept_new_connection(Connection *server_conn);
int read_message_from_connection(Connection *rdfrom, Buffer *buf);
int write_message_to_connection(Connection *writeto, Buffer *buf);


Server *server_init(Connection *);
int server_run(Server *);
int server_stop(Server *);

