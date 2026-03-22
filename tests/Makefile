CC = gcc
CFLAGS = -Wall -Wextra -g -DUNICODE -D_UNICODE -I..
LDFLAGS =

SRCS = ../cache.c ../memory.c ../error.c ../log.c ../uri.c ../parser.c bench.c
OBJS = $(SRCS:.c=.o)

bench: $(OBJS)
	$(CC) $(OBJS) -o bench $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: bench
	./bench

clean:
	rm -f bench $(OBJS)
