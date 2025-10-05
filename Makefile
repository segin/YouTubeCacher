# Makefile for native Windows C program

# Ensure we're using MinGW32 environment for native Windows binaries
export MSYSTEM := MINGW32
export PATH := /mingw32/bin:$(PATH)

# Compiler and flags - use MinGW32 for native Windows binaries without MSYS2 dependencies
CC = /mingw32/bin/gcc.exe
RC = /mingw32/bin/windres.exe
CFLAGS = -Wall -Wextra -Werror -std=c99 -DUNICODE -D_UNICODE -static-libgcc
LDFLAGS = -lgdi32 -luser32 -lkernel32 -lshell32 -lcomdlg32 -lole32 -lcomctl32 -luuid -static

# Target executable
TARGET = YouTubeCacher.exe

# Source files
SOURCES = main.c uri.c errordialog.c
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