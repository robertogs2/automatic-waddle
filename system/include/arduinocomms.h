#ifndef ARDUINOCOMMS_H
#define ARDUINOCOMMS_H 

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 256

#define FILE_ERROR -1

typedef struct{
	char buffer[BUFFER_SIZE];
	const char* filename;
	FILE* ptr;
	char opened;
} arduino_t;

int arduino_init(arduino_t* interface, const char* filename);
int arduino_exit(arduino_t* interface);

int arduino_sendchar(arduino_t* interface, const char c);
int arduino_sendstring(arduino_t* interface, const char* string);
int arduino_sendline(arduino_t* interface, const char* line);

char arduino_readchar(arduino_t* interface);
int arduino_readstring(arduino_t* interface, char* result, unsigned int nbytes);
int arduino_readline(arduino_t* interface, char* result);

#endif