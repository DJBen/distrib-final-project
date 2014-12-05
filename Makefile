CC=gcc
LD=gcc
CFLAGS=-g -Wall
CPPFLAGS=-I. -I include
SP_LIBRARY_DIR=include

all: chat_server client

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

chat_server:  $(SP_LIBRARY_DIR)/libspread-core.a chat_server.o arraylist.o
	$(LD) -o $@ chat_server.o arraylist.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

client:  $(SP_LIBRARY_DIR)/libspread-core.a client.o arraylist.o
	$(LD) -o $@ client.o arraylist.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

clean:
	rm -f *.o chat_server client

