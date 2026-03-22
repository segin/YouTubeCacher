CC = gcc
CFLAGS = -Wall -Wextra -g -I.
TARGET = test_threadsafe

all: $(TARGET) run

$(TARGET): test_threadsafe.c mock_windows.h
	$(CC) $(CFLAGS) -o $@ test_threadsafe.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
