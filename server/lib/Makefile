SERVER_LIB = libserver.a
SERVER_SRC = libserver.c
SERVER_OBJ = libserver.o

INC_DIR = ../include/
LIB_DIR=../lib/
CC = gcc -c -I$(INC_DIR)
AR = ar rvs

all: $(SERVER_LIB)

$(SERVER_LIB) : $(SERVER_OBJ)
		$(AR) $(SERVER_LIB) $(SERVER_OBJ)
$(SERVER_OBJ) : $(SERVER_SRC)
		$(CC) $(SERVER_SRC)

clean:
		rm *.o