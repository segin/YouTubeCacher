# Makefile for native Windows C program

# Source files
SOURCES = main.c uri.c dialogs.c cache.c base64.c parser.c appstate.c settings.c threading.c ytdlp.c log.c ui.c
RC_SOURCE = YouTubeCacher.rc

# Object directories
OBJ32_DIR = obj32
OBJ64_DIR = obj64

# Object files with separate directories
OBJECTS32 = $(SOURCES:%.c=$(OBJ32_DIR)/%.o)
RC_OBJECT32 = $(RC_SOURCE:%.rc=$(OBJ32_DIR)/%.o)
OBJECTS64 = $(SOURCES:%.c=$(OBJ64_DIR)/%.o)
RC_OBJECT64 = $(RC_SOURCE:%.rc=$(OBJ64_DIR)/%.o)

# Target executables
TARGET32 = YouTubeCacher.exe
TARGET64 = YouTubeCacher-x64.exe

# Common compiler flags
COMMON_CFLAGS = -Wall -Wextra -Werror -std=c99 -DUNICODE -D_UNICODE -static-libgcc
COMMON_LDFLAGS = -mwindows -lgdi32 -luser32 -lkernel32 -lshell32 -lcomdlg32 -lole32 -lcomctl32 -luuid -lshlwapi -static

# MinGW32 settings
CC32 = /mingw32/bin/gcc.exe
RC32 = /mingw32/bin/windres.exe
CFLAGS32 = $(COMMON_CFLAGS)
LDFLAGS32 = $(COMMON_LDFLAGS)

# MinGW64 settings  
CC64 = /mingw64/bin/gcc.exe
RC64 = /mingw64/bin/windres.exe
CFLAGS64 = $(COMMON_CFLAGS)
LDFLAGS64 = $(COMMON_LDFLAGS)

# Release flags
RELEASE_CFLAGS = -Os -DNDEBUG -s
RELEASE_LDFLAGS = -s

# Default target (32-bit debug)
all: debug32

# Debug targets
debug32: export MSYSTEM := MINGW32
debug32: export PATH := /mingw32/bin:$(PATH)
debug32: CC = $(CC32)
debug32: RC = $(RC32)
debug32: CFLAGS = $(CFLAGS32)
debug32: LDFLAGS = $(LDFLAGS32)
debug32: $(OBJ32_DIR) $(TARGET32)

debug64: export MSYSTEM := MINGW64
debug64: export PATH := /mingw64/bin:$(PATH)
debug64: CC = $(CC64)
debug64: RC = $(RC64)
debug64: CFLAGS = $(CFLAGS64)
debug64: LDFLAGS = $(LDFLAGS64)
debug64: $(OBJ64_DIR) $(TARGET64)

debug: debug32 debug64

# Release targets
release32: export MSYSTEM := MINGW32
release32: export PATH := /mingw32/bin:$(PATH)
release32: CC = $(CC32)
release32: RC = $(RC32)
release32: CFLAGS = $(CFLAGS32) $(RELEASE_CFLAGS)
release32: LDFLAGS = $(LDFLAGS32) $(RELEASE_LDFLAGS)
release32: $(OBJ32_DIR) $(TARGET32)

release64: export MSYSTEM := MINGW64
release64: export PATH := /mingw64/bin:$(PATH)
release64: CC = $(CC64)
release64: RC = $(RC64)
release64: CFLAGS = $(CFLAGS64) $(RELEASE_CFLAGS)
release64: LDFLAGS = $(LDFLAGS64) $(RELEASE_LDFLAGS)
release64: $(OBJ64_DIR) $(TARGET64)

release: release32 release64

# Directory creation
$(OBJ32_DIR):
	mkdir -p $(OBJ32_DIR)

$(OBJ64_DIR):
	mkdir -p $(OBJ64_DIR)

# Build rules
$(TARGET32): $(OBJECTS32) $(RC_OBJECT32)
	$(CC) $(OBJECTS32) $(RC_OBJECT32) -o $@ $(LDFLAGS)

$(TARGET64): $(OBJECTS64) $(RC_OBJECT64)
	$(CC) $(OBJECTS64) $(RC_OBJECT64) -o $@ $(LDFLAGS)

# Compile source files to object files (32-bit)
$(OBJ32_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile resource files (32-bit)
$(OBJ32_DIR)/%.o: %.rc
	$(RC) $< -o $@

# Compile source files to object files (64-bit)
$(OBJ64_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile resource files (64-bit)
$(OBJ64_DIR)/%.o: %.rc
	$(RC) $< -o $@

# Cleaning targets
clean-objects:
	rm -rf $(OBJ32_DIR) $(OBJ64_DIR)

clean32:
	rm -rf $(OBJ32_DIR) $(TARGET32)

clean64:
	rm -rf $(OBJ64_DIR) $(TARGET64)

clean: clean32 clean64

# Run the program
run: debug32
	./$(TARGET32)

run32: debug32
	./$(TARGET32)

run64: debug64
	./$(TARGET64)

# Phony targets
.PHONY: all debug debug32 debug64 release release32 release64 clean clean32 clean64 clean-objects run run32 run64