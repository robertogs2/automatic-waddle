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

#define BUFFER_SIZE 2048

static const char* html_web_error =
        "HTTP/1.1 200 Ok\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
        "<!DOCTYPE html>\r\n"
        "<html><head><br><br><br><br><title>WebService</title>\r\n"
        "<style>body { background-color: #008080 }</style></head>\r\n"
        "<body><center><h1>Error 404 Not Found</h1><br>\r\n"
        "</center></body></html>\r\n";

static const char* html_web_image =
        "HTTP/1.1 200 Ok\r\n"
        "Content-Type: image/png\r\n\r\n";

static const char* html_web_text =
        "HTTP/1.1 200 Ok\r\n"
        "Content-Type: text/plain\r\n\r\n";

static const char* html_web_file =
        "HTTP/1.1 200 Ok\r\n"
        "Content-Type: image/*\r\n\r\n";

int file_size(const char* path){
  struct stat buffer;
  int status = stat(path, &buffer);
  if(status==0) return buffer.st_size;
  else return 0;
}

int send_file(char *file_path, int client_socket_descriptor) {
    int file = open(file_path, O_RDONLY);
    int result = sendfile(client_socket_descriptor, file, NULL, file_size(file_path));
    close(file);
    return result;
}

// struct sockaddr_in get_direction(int port){
//     struct sockaddr_in direction;
//     direction.sin_family = AF_INET;
//     direction.sin_port = htons(port);
//     direction.sin_addr.s_addr = INADDR_ANY;
//     return direction;
// }

// void bind_server_socket(int server_socket_descriptor, struct sockaddr_in direction){
//     if (bind(server_socket_descriptor, (struct sockaddr*)& direction, sizeof(direction)) == -1) {
//         close(server_socket_descriptor);
//         server_log("Bind Error");
//         exit(1);
//     }
// }

// void listen_server(int server_socket_descriptor){
//     if (listen(server_socket_descriptor, MAX_CLIENTS) == -1) {
//         close(server_socket_descriptor);
//         server_log("Listen Error");
//         exit(1);
//     }
// }

char* get_ip(struct sockaddr client){
    struct sockaddr_in* addr_in = (struct sockaddr_in*) &client;
    char *s = inet_ntoa(addr_in->sin_addr);
    return s;
}

void trim_line(char* buffer){
    int i = 0;
    while(buffer[i]!='\0'){
        if(buffer[i]=='\n') buffer[i]='\0';
        ++i;
    }
}

char* actual_query(char* buffer){
    int i = 0;
    int s = 0;
    int in_query=0;
    while(buffer[i]!='\0'){
        if(in_query==0&&buffer[i]=='/'){
            s=i; //sets start
            in_query=1;
        }
        if(in_query==1&&buffer[i]==' '){
            buffer[i]='\0';
            break;
        }
        ++i;
    }
    return buffer+s;
}

// void send_picture(int client_socket_descriptor, int count){
//     char file_path_name[1024];
//     char command[1024];
//     server_log("Taking picture");
//     write(client_socket_descriptor, html_web_image, sizeof(html_web_image) - 1);
//     sprintf(file_path_name, "%simage%d.png\0", image_path, count++);
//     server_log(file_path_name);
//     sprintf(command, "%s %s", fswebcam_command, file_path_name);
//     server_log(command);
//     system(command);
//     sleep(2);
//     server_log("Picture taken");
//     send_file(file_path_name, client_socket_descriptor);
//     server_log("Picture sent");
// }

// int process_query(int client_socket_descriptor, struct sockaddr client, char* query){
//     static int count;
//     int using_ip = strcmp(ip, get_ip(client))==0;              // Check if ip used
//     if(strncmp(query, "/login", 6)==0){
//         server_log("Login in");
//         write(client_socket_descriptor, html_web_text, sizeof(html_web_text) - 1);
//         strcpy(ip, get_ip(client));                             // Copies the ip   
//         server_log("Login done");
//     }
//     else if(using_ip){
//         if(strcmp(query, "/picture")==0){                       // Take photo
//             send_picture(client_socket_descriptor, count);
//         }
//         else if(strncmp(query, "/led", 4)==0){                  // Change led
//             change_led(query[4], query[5]);
//             write(client_socket_descriptor, html_web_text, sizeof(html_web_text) - 1);
//         }
//         else if(strncmp(query, "/status", 7)==0){
//             server_log("Taking status");
//             write(client_socket_descriptor, html_web_text, sizeof(html_web_text) - 1);
//             update_doors();
//             write(client_socket_descriptor, pin_values, sizeof(pin_values)-1);
//             server_log(pin_values);
//             server_log("Status taken");
//         }
//         else{
//             server_log("Unknown query");
//             write(client_socket_descriptor, html_web_error, sizeof(html_web_error) - 1);
//         }
//     }
//     else{
//         server_log("Not the current client, use login");
//         write(client_socket_descriptor, html_web_error, sizeof(html_web_error) - 1);
//     }
// }

int process(int client_socket_descriptor, struct sockaddr client, char* client_buffer){

    trim_line(client_buffer);
    char* query = actual_query(client_buffer);
    //server_log(query);
    //process_query(client_socket_descriptor, client, query);

    return 0;
}

int main(int argc, char *argv[]) {

    //int server_socket_descriptor, port;
    int client_socket_descriptor;
    //struct sockaddr_in direction;
    struct sockaddr client;
    socklen_t client_length = sizeof(client);
    char client_buffer[BUFFER_SIZE];
    char *extension, *residue;

    // // Creates the server socket descriptor
    // server_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    // if (server_socket_descriptor == -1) {
    //     server_log("Error creating socket");
    //     exit(1);
    // }

    // // Sets the port
    // if (argc >= 2) {
    //     port = strtol(argv[1], &residue, 10);           //define port given by the user
    // } 
    // else {
    //     port = 8888;                                    //default port
    // }
    // printf("Port set to %d\n", port);
    
    // // Sets the sockaddr
    // direction = get_direction(port);

    // // Binds the server socket
    // bind_server_socket(server_socket_descriptor, direction);

    // // Sets the server socket to listen
    // listen_server(server_socket_descriptor);
    // server_log("Server initiated");

    server_t server;
    if(init_server(&server, 8888)) exit(1);

    while (1) {
        client_socket_descriptor = accept(server.socket_descriptor, &client, &client_length);

        if (client_socket_descriptor == -1) {
            close(server.socket_descriptor);
            //server_log("Accept error");
            exit(1);
        }
        //server_log("New request");

        // Clean read buffer
        memset(client_buffer, 0, BUFFER_SIZE);
        read(client_socket_descriptor, client_buffer, BUFFER_SIZE);

        if(process(client_socket_descriptor, client, client_buffer)==-1){
            close(server.socket_descriptor);
            exit(1);
        }

        close(client_socket_descriptor);
    }

    //server_log("Server: Stop");

    close(server.socket_descriptor);
}