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
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sched.h> /* For clone */
#include <signal.h> /* For SIGCHLD */
#include <sys/wait.h> /* For wait */
#include <time.h>
#include <stdarg.h>
#include <sys/syscall.h> // For call to gettid
#include <libserver.h>
#include <libarduinocomms.h>

server_t server;
client_t client;
int in_use;

void close_socket(){
    printf("Killing\n");
    close_server(&server);
    if(in_use){
        close_client(&client);
    }
    kill(getpid(), SIGKILL);
}

int main(int argc, char *argv[]) {
    int port = 8080;
    if(argc > 1) port = atoi(argv[1]);
    
    // server_t server;
    if(init_server(&server, port)) exit(1);
    printf("Now listening on port %d\n", port);
    signal(SIGINT, close_socket);
    game_t current_game;
    server.game = &current_game;
    
    init_game(&server);
    
    while (1) {
        
        // printf("Size: %d\n", server.game->size);
        // printf("Type: %d\n", server.game->type);
        // printf("Username0: %s\n", server.game->username0);
        // printf("Symbol0: %d\n", server.game->symbol0);
        // printf("Username1: %s\n", server.game->username1);
        // printf("Symbol1: %d\n", server.game->symbol1);
        if(accept_client(&server, &client) == 0) {
            in_use=1;
            //printf("New client received coming from ip %s\n", client.ip);
            process_client(&client, &server);
            in_use=0;
            close_client(&client);
        }
    }
    close_server(&server);
}