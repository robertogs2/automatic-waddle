#ifndef ARDUINOCOMMS 
#define ARDUINOCOMMS 

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE

typedef struct{
	char buffer[BUFFER_SIZE];
	char* filename;
} arduino_comm_t;

int init_arduino_comm(arduino_comm_t* interface, const char* filename);

int sendchar(arduino_comm_t* interface, const char c);
int sendstring(arduino_comm_t* interface, const char* string);
int sendline(arduino_comm_t* interface, const char* line);

char readchar(arduino_comm_t* interface);
unsigned int readstring(arduino_comm_t* interface, char* result, unsigned int nbytes);
unsigned int readline(arduino_comm_t* interface, char* result, unsigned int nbytes);

#endif