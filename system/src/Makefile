CC = gcc -o
EXE = main
ARDUINOCOMMS_LIB = libarduinocomms.a

LIB_DIR = ../lib/
INCLUDE_DIR = ../include/
BIN_DIR = ../bin/

LIBS = $(LIB_DIR)$(ARDUINOCOMMS_LIB)

done: $(EXE)

$(EXE) : $(EXE).c
		$(CC) $(BIN_DIR)$(EXE) $(EXE).c $(LIBS) -I$(INCLUDE_DIR) -lm

