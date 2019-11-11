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

char *get_ip(client_t *client) {
    struct sockaddr_in *addr_in = (struct sockaddr_in *) &client->client_sockaddr;
    char *ip = inet_ntoa(addr_in->sin_addr);
    return ip;
}
uint8_t accept_client(server_t *server, client_t *client) {
    client->length = sizeof(client->client_sockaddr);
    client->socket_descriptor = accept(server->socket_descriptor,
                                       &client->client_sockaddr,
                                       &client->length);
    if(client->socket_descriptor == -1) {
        close(client->socket_descriptor);
        return ACCEPT_ERROR;
    }
    // Clears buffer
    memset(client->buffer, 0, BUFFER_SIZE);
    // Reads the incoming data
    read(client->socket_descriptor, client->buffer, BUFFER_SIZE);
    client->ip = get_ip(client);
    return 0;
}
uint8_t close_client(client_t *client) {
    close(client->socket_descriptor);
    return 0;
}

#endif