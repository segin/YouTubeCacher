# Makefile for tests

CC = gcc
CFLAGS = -Wall -Wextra -g -I.. -I./include -fno-strict-aliasing

TEST_SOURCES = $(wildcard test_*.c)
TEST_EXECUTABLES = $(TEST_SOURCES:.c=)

all: $(TEST_EXECUTABLES)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

run: all
	@for test in $(TEST_EXECUTABLES); do \
		echo "Running $$test..."; \
		./$$test; \
	done

clean:
	rm -f $(TEST_EXECUTABLES)

.PHONY: all run clean
