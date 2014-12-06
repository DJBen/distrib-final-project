CC=gcc
LD=gcc
CFLAGS=-g -Wall
CPPFLAGS=-I. -I include
SP_LIBRARY_DIR=include

all: chat_server client test

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

vector_test: vector_test.o vector.o
	$(LD) -o $@ vector_test.o vector.o

chat_server:  $(SP_LIBRARY_DIR)/libspread-core.a chat_server.o vector.o
	$(LD) -o $@ chat_server.o vector.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

client:  $(SP_LIBRARY_DIR)/libspread-core.a client.o vector.o
	$(LD) -o $@ client.o vector.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

clean:
	rm -f *.o chat_server client test

