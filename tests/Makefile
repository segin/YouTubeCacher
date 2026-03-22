CC = gcc
CFLAGS = -Wall -Wextra -g -DTEST_ENVIRONMENT

all: test_threadsafe

test_threadsafe: test_threadsafe.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./test_threadsafe

clean:
	rm -f test_threadsafe
