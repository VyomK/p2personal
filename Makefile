CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -Ilibs -Ihelpers
LDFLAGS = -pthread

COMMON_OBJS = source/markdown.o helpers/ArrayList.o

all: client server

server: source/server.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o server source/server.c $(COMMON_OBJS) $(LDFLAGS)

client: source/client.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o client source/client.c $(COMMON_OBJS) $(LDFLAGS)

# Object file rules
source/markdown.o: source/markdown.c libs/markdown.h
	$(CC) $(CFLAGS) -c $< -o $@

helpers/ArrayList.o: helpers/ArrayList.c helpers/ArrayList.h
	$(CC) $(CFLAGS) -c $< -o $@

helpers/ArrayList.o: helpers/ArrayList.c helpers/ArrayList.h
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f server client *.o */*.o FIFO_* test_* *.swp *.swo *.log .DS_Store Thumbs.db

.PHONY: all clean
