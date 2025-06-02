# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -Ilibs
LDFLAGS = -pthread
LD = ld

# Common object files used by client, server, and test
OBJS_COMMON = \
    source/markdown.o \
    source/document.o \
    source/memory.o \
    source/array_list.o \
    source/naive_ops.o

OBJS_SERVER = source/server.o $(OBJS_COMMON)
OBJS_CLIENT = source/client.o $(OBJS_COMMON)

# Test runner setup
TEST_SRC = tests/main.c tests/tests_refactored.c
TEST_BIN = test_runner
OBJS_TEST = $(OBJS_COMMON)
 

all: server client markdown.o


server: $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


client: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


markdown.o: source/markdown.o source/document.o source/memory.o source/array_list.o source/naive_ops.o
	$(LD) -r $^ -o $@


$(TEST_BIN): $(TEST_SRC) $(OBJS_TEST)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

source/%.o: source/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	valgrind --leak-check=full --track-origins=yes --error-exitcode=1 ./$(TEST_BIN)


clean:
	rm -f server client $(TEST_BIN) markdown.o source/*.o FIFO_* test_* *.swp *.swo *.log Thumbs.db

.PHONY: all clean test
