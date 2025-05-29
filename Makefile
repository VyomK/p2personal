# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -Ilibs
LDFLAGS = -pthread

# Object files
OBJS_SERVER = \
    source/server.o \
    source/markdown.o \
    source/document.o \
    source/memory.o \
    source/array_list.o

OBJS_CLIENT = \
    source/client.o \
    source/markdown.o \
    source/document.o \
    source/memory.o \
    source/array_list.o


TEST_SRC = tests/main.c tests/test_insert.c tests/test_newline.c tests/test_block_format.c tests/test_OL.c tests/test_inline.c tests/test_delete.c
TEST_BIN = test_runner

# Targets
all: server client

server: $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o server $(OBJS_SERVER) $(LDFLAGS)

client: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o client $(OBJS_CLIENT) $(LDFLAGS)

# Object file rules
source/markdown.o: source/markdown.c libs/markdown.h
	$(CC) $(CFLAGS) -c $< -o $@

source/document.o: source/document.c libs/document.h
	$(CC) $(CFLAGS) -c $< -o $@

source/memory.o: source/memory.c libs/memory.h
	$(CC) $(CFLAGS) -c $< -o $@

source/array_list.o: source/array_list.c libs/array_list.h
	$(CC) $(CFLAGS) -c $< -o $@

source/client.o: source/client.c
	$(CC) $(CFLAGS) -c $< -o $@

source/server.o: source/server.c
	$(CC) $(CFLAGS) -c $< -o $@



# Test 
test: $(TEST_BIN)
		valgrind --leak-check=full --track-origins=yes --error-exitcode=1 ./$(TEST_BIN)


$(TEST_BIN): $(TEST_SRC) $(OBJS_CLIENT)  
	$(CC) $(CFLAGS)  -o $@ $^
# Clean
clean:
	rm -f server client source/*.o FIFO_* test_* *.swp *.swo *.log Thumbs.db

.PHONY: all clean test


