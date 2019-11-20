#include "arduinocomms.h"
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv) {

    char* arduino_file = "/dev/arduino0";
    if(argc > 1) arduino_file = argv[1];
    arduino_t *arduino = (arduino_t *) malloc(sizeof(arduino_t));
    arduino_init(arduino, arduino_file);

    //char buffer[256];
    // char* s1 = "s0\n";
    // arduino_sendstring(arduino, s1);
    // char* yy = "03\n";
    // sleep(2);
    // arduino_sendstring(arduino, yy);
    //char* s2 = "d\n";
    char buffer[256];
    while(1) {
        //int n = arduino_readuntil(arduino, buffer, 'B');
        printf("Insert command:\n");
        //scanf("%s\n", buffer);
        fgets(buffer, 256, stdin);
        arduino_sendstring(arduino, buffer);
        //printf("%s\n", buffer);
        sleep(2);
        //arduino_sendstring(arduino, s2);
        //sleep(2);
    }
    return 0;
}