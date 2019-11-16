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

server_t server;
// void close_socket(){
//     close_server(&server);
//     //killpg(getpid(), SIGKILL);
// }

void init_game(game_t* game){
    strcpy(game->username0, "Not assigned");
    strcpy(game->username1, "Not assigned");
    game->size = 0;
    game->type = 0;
    game->players = 0;
    game->symbol0 = -1;
    game->symbol1 = -1;
    game->turn = TURN_PLAYER0;
    game->next_turn = TURN_WAITING;
    memset(game->matrix, 0, 9);
}

int main(int argc, char *argv[]) {
    int port = 8080;
    if(argc > 1) port = atoi(argv[1]);
    
    // server_t server;
    if(init_server(&server, port)) exit(1);
    printf("Now listening on port %d\n", port);
    //signal(SIGINT, close_socket);
    game_t current_game;
    init_game(&current_game);
    server.game = &current_game;
    while (1) {
        client_t client;
        
        printf("Size: %d\n", server.game->size);
        printf("Type: %d\n", server.game->type);
        printf("Username0: %s\n", server.game->username0);
        printf("Symbol0: %d\n", server.game->symbol0);
        printf("Username1: %s\n", server.game->username1);
        printf("Symbol1: %d\n", server.game->symbol1);
        if(accept_client(&server, &client) == 0) {
            printf("New client received coming from ip %s\n", client.ip);
            process_client(&client, &server);
            close_client(&client);
        }
    }
    close_server(&server);
}