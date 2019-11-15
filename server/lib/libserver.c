#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <libserver.h>

char html_web_error[] =
    "HTTP/1.1 200 Ok\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n"
    "<html><head><br><br><br><br><title>WebService</title>\r\n"
    "<style>body { background-color: #008080 }</style></head>\r\n"
    "<body><center><h1>Error 404 Not Found</h1><br>\r\n"
    "</center></body></html>\r\n";

char html_web_image[] =
    "HTTP/1.1 200 Ok\r\n"
    "Content-Type: image/png\r\n\r\n";

char html_web_text[] =
    "HTTP/1.1 200 Ok\r\n"
    "Content-Type: text/plain\r\n\r\n";

char html_web_file[] =
    "HTTP/1.1 200 Ok\r\n"
    "Content-Type: image/*\r\n\r\n";

struct sockaddr_in get_direction(server_t *server) {
    struct sockaddr_in direction;
    direction.sin_family = AF_INET;
    direction.sin_port = htons(server->port);
    direction.sin_addr.s_addr = INADDR_ANY;
    return direction;
}

uint8_t bind_socket(server_t *server) {
    if (bind(server->socket_descriptor, (struct sockaddr *)& server->direction, sizeof(server->direction)) == -1) return 1;
    return 0;
}

uint8_t listen_server(server_t *server) {
    if (listen(server->socket_descriptor, MAX_CLIENTS) == -1) return 1;
    return 0;
}

uint8_t init_server(server_t *server, int port) {
    // Creates the server socket descriptor
    server->socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_descriptor == -1) return SOCKET_ERROR;
    // Sets the port
    server->port = port;
    // Sets the sockaddr_in
    server->direction = get_direction(server);
    // Binds the server socket
    if(bind_socket(server)) {
        close(server->socket_descriptor);
        return BIND_ERROR;
    }
    // Sets the server socket to listen
    if(listen_server(server)) {
        close(server->socket_descriptor);
        return LISTEN_ERROR;
    }
    return 0;
}

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
uint8_t process_client(client_t *client) {
    trim_query_line(client);    // First line
    delimit_query(client);      // Removes GET till space
    process_query(client);      // Processes that query
}

void trim_query_line(client_t *client) {
    int i = 0;
    char *buffer = client->buffer;
    while(buffer[i] != '\0') {
        if(buffer[i] == '\n') buffer[i] = '\0';
        ++i;
    }
}
void delimit_query(client_t *client) {
    char *buffer = client->buffer;
    int i = 0;
    int s = 0;
    int in_query = 0;
    while(buffer[i] != '\0') {
        if(in_query == 0 && buffer[i] == '/') {
            s = i; //sets start
            in_query = 1;
        }
        if(in_query == 1 && buffer[i] == ' ') {
            buffer[i] = '\0';
            break;
        }
        ++i;
    }
    memcpy(client->buffer, buffer+s, sizeof(buffer+s));
}

uint8_t process_query(client_t* client){
    static int count;
    char* query = client->buffer;
    printf("Processing %s\n", query);
    if(strcmp(query, "/dummy")){
        printf("Sending dummy\n");
        printf("Socket %d\n", client->socket_descriptor);
        write(client->socket_descriptor, html_web_text, sizeof(html_web_text) - 1);
        write(client->socket_descriptor, "Im dummy", 8);
    }
    else if(strcmp(query, "/picture")==0){                       // Take photo
        //send_picture(client->socket_descriptor, count);
    }
    else{
        write(client->socket_descriptor, html_web_error, sizeof(html_web_error) - 1);
    }
}

// int send_file(char *file_path, int client_socket_descriptor) {
//     int file = open(file_path, O_RDONLY);
//     int result = sendfile(client_socket_descriptor, file, NULL, file_size(file_path));
//     close(file);
//     return result;
// }