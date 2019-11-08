#include "arduinocomms.h"
#include <unistd.h>

int main(int argc, char **argv) {

    arduino_t *arduino = (arduino_t *) malloc(sizeof(arduino_t));
    arduino_init(arduino, "/dev/ttyACM0");

    char buffer[256];
    while(1){
		arduino_sendline(arduino, "hola");
		sleep(2);
		arduino_readline(arduino, buffer);
		printf("%s\n", buffer);
    }


    return 0;
}