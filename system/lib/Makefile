ARDUINO_COMMS_LIB = libarduinocomms.a
ARDUINO_COMMS_SRC = arduinocomms.c
ARDUINO_COMMAND_OBJ = arduinocomms.o

INC_DIR = ../include/
LIB_DIR=../lib/
CC = gcc -c -I$(INC_DIR)
AR = ar rvs

all: $(ARDUINO_COMMS_LIB)

$(ARDUINO_COMMS_LIB) : $(ARDUINO_COMMAND_OBJ)
		$(AR) $(ARDUINO_COMMS_LIB) $(ARDUINO_COMMAND_OBJ)
$(ARDUINO_COMMAND_OBJ) : $(ARDUINO_COMMS_SRC)
		$(CC) $(ARDUINO_COMMS_SRC)

clean:
		rm *.o