#define _GNU_SOURCE

int init_arduino_comm(arduino_comm_t *interface, const char *filename) {
    interface->filename = filename;
}

char readchar(arduino_comm_t *interface) {
    FILE *ptr = fopen(interface->filename, "w");
    char c = fgetc((FILE *)ptr);
    return c;
}
unsigned int readstring(arduino_comm_t* interface, char* result, unsigned int nbytes){
	FILE *ptr = fopen(interface->filename, "w");
    char c = fgetc((FILE *)ptr);
    unsigned int count = (unsigned int) strtoul(buffer, NULL, 16);
    fclose(ptr);
    return count;
}
unsigned int readline(arduino_comm_t* interface, char* result, unsigned int nbytes);
