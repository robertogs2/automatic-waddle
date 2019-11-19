#include "arduinocomms.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int anillo() {
    int file, data_s;
    struct termios toptions;

    file = open("/dev/arduino0", O_RDWR | O_NOCTTY);
    printf("File opened as %i\n", file);
    sleep(2);
    /* get current serial port settings */
    tcgetattr(file, &toptions);
    /* set 9600 baud both ways */
    cfsetispeed(&toptions, B9600);
    cfsetospeed(&toptions, B9600);
    /* 8 bits, no parity, no stop bits */
    toptions.c_cflag &= ~PARENB;
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag &= ~CSIZE;
    toptions.c_cflag |= CS8;
    /* Canonical mode */
    toptions.c_lflag &= ~ICANON;
    /* commit the serial port settings */
    tcsetattr(file, TCSANOW, &toptions);

    printf("Running....\n");
    char c[1];
    while(1) {
        read(file, c, 1);
        //sleep(1);
        printf("%c\n", c[0]);
        break;

        //break;
    }
    close(file);
    return 0;
}

int main(int argc, char **argv) {

    arduino_t *arduino = (arduino_t *) malloc(sizeof(arduino_t));
    arduino_init(arduino, "/dev/arduino3");

    char buffer[256];
    char* s1 = "9x\n";
    char* s2 = "8y\n";
    while(1) {
        //int n = arduino_readuntil(arduino, buffer, 'B');
        arduino_sendstring(arduino, s1);
        //printf("%s\n", buffer);
        sleep(2);
        arduino_sendstring(arduino, s2);
        sleep(2);
    }
    return 0;
}