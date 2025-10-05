# Makefile for basic Windows C program

# Compiler and flags
CC = gcc
RC = windres
CFLAGS = -Wall -Wextra -Werror -std=c99
LDFLAGS = -lgdi32 -luser32 -lkernel32 -lshell32 -lcomdlg32 -lole32 -lcomctl32 -luuid

# Target executable
TARGET = YouTubeCacher.exe

# Source files
SOURCES = main.c uri.c
RC_SOURCE = YouTubeCacher.rc

# Object files
OBJECTS = $(SOURCES:.c=.o)
RC_OBJECT = $(RC_SOURCE:.rc=.o)

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJECTS) $(RC_OBJECT)
	$(CC) $(OBJECTS) $(RC_OBJECT) -o $(TARGET) $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile resource files
%.o: %.rc
	$(RC) $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(RC_OBJECT) $(TARGET) *.o

# Run the program
run: $(TARGET)
	./$(TARGET)

# Phony targets
.PHONY: all clean run