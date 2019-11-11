#ifndef LIB_SERVER_H
#define LIB_SERVER_H

#include <stdint.h>

#define SOCKET_ERROR 	1
#define BIND_ERROR 		2
#define LISTEN_ERROR 	3
#define ACCEPT_ERROR 	4
#define MAX_CLIENTS 	20
#define BUFFER_SIZE 	2048
typedef struct {
    int socket_descriptor;
    int port;
    struct sockaddr_in direction;
} server_t;

typedef struct {
    int socket_descriptor;
    struct sockaddr client_sockaddr;
    socklen_t length;
    char buffer[BUFFER_SIZE];
    char *ip;
} client_t;

struct sockaddr_in get_direction(server_t *server);
uint8_t bind_socket(server_t *server);
uint8_t listen_server(server_t *server);
uint8_t init_server(server_t *server, int port);
uint8_t close_server(server_t *server);

char *get_ip(client_t *client);
uint8_t accept_client(server_t *server, client_t *client);
uint8_t close_client(client_t *client);
uint8_t process_client(client_t *client);

void trim_query_line(client_t *client);
void delimit_query(client_t *client);
uint8_t process_query(client_t* client);
#endif