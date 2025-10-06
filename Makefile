# Makefile for native Windows C program

# Ensure we're using MinGW32 environment for native Windows binaries
export MSYSTEM := MINGW32
export PATH := /mingw32/bin:$(PATH)

# Compiler and flags - use MinGW32 for native Windows binaries without MSYS2 dependencies
CC = /mingw32/bin/gcc.exe
RC = /mingw32/bin/windres.exe
CFLAGS = -Wall -Wextra -Werror -std=c99 -DUNICODE -D_UNICODE -static-libgcc
LDFLAGS = -mwindows -lgdi32 -luser32 -lkernel32 -lshell32 -lcomdlg32 -lole32 -lcomctl32 -luuid -lshlwapi -static

# Target executable
TARGET = YouTubeCacher.exe

# Source files
SOURCES = main.c uri.c dialogs.c cache.c base64.c
RC_SOURCE = YouTubeCacher.rc

# Object files
OBJECTS = $(SOURCES:.c=.o)
RC_OBJECT = $(RC_SOURCE:.rc=.o)

# Default target
all: $(TARGET)

# Optimized release build
release: CFLAGS += -Os -DNDEBUG -s
release: LDFLAGS += -s
release: $(TARGET)

# MinGW64 build targets
mingw64: export MSYSTEM := MINGW64
mingw64: export PATH := /mingw64/bin:$(PATH)
mingw64: CC = /mingw64/bin/gcc.exe
mingw64: RC = /mingw64/bin/windres.exe
mingw64: TARGET = YouTubeCacher-x64.exe
mingw64: clean-mingw64 $(TARGET)

# MinGW64 release build
mingw64-release: export MSYSTEM := MINGW64
mingw64-release: export PATH := /mingw64/bin:$(PATH)
mingw64-release: CC = /mingw64/bin/gcc.exe
mingw64-release: RC = /mingw64/bin/windres.exe
mingw64-release: TARGET = YouTubeCacher-x64.exe
mingw64-release: CFLAGS += -Os -DNDEBUG -s
mingw64-release: LDFLAGS += -s
mingw64-release: clean-mingw64 $(TARGET)

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

# Clean MinGW64 build artifacts
clean-mingw64:
	rm -f $(OBJECTS) $(RC_OBJECT) YouTubeCacher-x64.exe *.o

# Run the program
run: $(TARGET)
	./$(TARGET)

# Phony targets
.PHONY: all clean run release mingw64 mingw64-release clean-mingw64