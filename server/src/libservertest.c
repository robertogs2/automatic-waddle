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

int file_size(const char *path) {
    struct stat buffer;
    int status = stat(path, &buffer);
    if(status == 0) return buffer.st_size;
    else return 0;
}

int send_file(char *file_path, int client_socket_descriptor) {
    int file = open(file_path, O_RDONLY);
    int result = sendfile(client_socket_descriptor, file, NULL, file_size(file_path));
    close(file);
    return result;
}

int main(int argc, char *argv[]) {
    int port = 8778;
    server_t server;
    if(init_server(&server, port)) exit(1);
    printf("Now listening on port %d\n", port);
    while (1) {
        client_t client;
        if(accept_client(&server, &client) == 0) {
            printf("New client received coming from ip %s\n", client.ip);
            process_client(&client);
            close_client(&client);
        }
    }
    close_server(&server);
}