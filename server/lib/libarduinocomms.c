#define _GNU_SOURCE

#include <stdio.h>
#include <arduinocomms.h>
#include <string.h>
#include <unistd.h>

int arduino_init(arduino_t *interface, const char *filename) {
    interface->filename = filename;
    interface->ptr = fopen(interface->filename, "r+");
    if(interface->ptr){
    	interface->opened = 1;
    	sleep(2);
    }
    return interface->opened;
}

int arduino_exit(arduino_t *interface){
	fclose(interface->ptr);
	interface->opened = 0;
	return 0;
}

int arduino_sendchar(arduino_t *interface, const char c) {
    fprintf(interface->ptr,"%c",c);
    fflush(interface->ptr);
}
int arduino_sendstring(arduino_t *interface, const char *string) {
    fprintf(interface->ptr,"%s",string);
    fflush(interface->ptr);
}
int arduino_sendline(arduino_t *interface, const char *line) {
    fprintf(interface->ptr,"%s\n",line);
    fflush(interface->ptr);
}

char arduino_readchar(arduino_t *interface) {
    char c = fgetc((FILE *)interface->ptr);
    return c;
}
int arduino_readstring(arduino_t *interface, char *result, unsigned int size) {
    int c;
    unsigned int count;
    fgets(result, size, interface->ptr);
    count = strlen(result);
    return count;
}
int arduino_readline(arduino_t *interface, char *result) {
    int c;
    unsigned int count;
    if(!interface->ptr){
    	return FILE_ERROR;
    }
    count = 0;
    while((c = fgetc(interface->ptr)) != '\n') {
        if(c > 0 && c != EOF) result[count++] = (char)c;
    }
    if(count) result[count] = 0;
    return count;
}
