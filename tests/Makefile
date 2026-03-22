CC = gcc
CFLAGS = -Wall -Wextra -I. -DTEST_BUILD
TARGET = test_threadsafe

all: $(TARGET)

$(TARGET): test_threadsafe.c mock_windows.h ../threadsafe.c
	$(CC) $(CFLAGS) test_threadsafe.c -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
