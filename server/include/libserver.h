#ifndef LIB_SERVER_H
#define LIB_SERVER_H

#include <stdint.h>

#define SOCKET_ERROR 	1
#define BIND_ERROR 		2
#define LISTEN_ERROR 	3
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
    socklen_t client_length;
    struct sockaddr_in direction;
    char client_buffer[BUFFER_SIZE];
} client_t;

struct sockaddr_in get_direction(server_t *server);
uint8_t bind_socket(server_t *server);
uint8_t listen_server(server_t *server);
uint8_t init_server(server_t *server, int port);

#endif