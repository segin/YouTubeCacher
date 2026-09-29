# Makefile for native Windows C program

# Source files
SOURCES = main.c uri.c cache.c base64.c parser.c appstate.c settings.c threading.c ytdlp.c log.c ui.c dialogs.c memory.c error.c threadsafe.c subproc.c accessibility.c keyboard.c components.c dpi.c
RC_SOURCE = YouTubeCacher.rc

# Every build variant (toolchain) and configuration gets its own object
# directory, obj/<variant>-<config>, and its own executable, so switching
# between them never reuses objects built with another toolchain or flags.
#
#   variant  toolchain            release executable
#   32       MINGW32 gcc          YouTubeCacher.exe
#   64       MINGW64 gcc          YouTubeCacher-x64.exe
#   ucrt64   UCRT64 gcc           YouTubeCacher-x64-ucrt.exe
#   arm64    CLANGARM64 clang     YouTubeCacher-arm64.exe
#
# Debug builds add -debug to the name, e.g. YouTubeCacher-x64-debug.exe.
OBJ_ROOT = obj
EXE_32 = YouTubeCacher
EXE_64 = YouTubeCacher-x64
EXE_ucrt64 = YouTubeCacher-x64-ucrt
EXE_arm64 = YouTubeCacher-arm64

# Common compiler flags
COMMON_CFLAGS ?= -Wall -Wextra -Werror -std=c11 -DUNICODE -D_UNICODE -Wno-unused-command-line-argument

# Debug flags for memory tracking
DEBUG_CFLAGS ?= -g -DMEMORY_DEBUG -DLEAK_DETECTION
COMMON_LDFLAGS ?= -mwindows -static-libgcc -lgdi32 -luser32 -lkernel32 -lshell32 -lcomdlg32 -lole32 -lcomctl32 -luuid -lshlwapi -ldbghelp -lbcrypt -static

# MinGW32 settings
CC32 ?= /mingw32/bin/gcc.exe
RC32 ?= /mingw32/bin/windres.exe
CFLAGS32 ?= $(COMMON_CFLAGS)
LDFLAGS32 ?= $(COMMON_LDFLAGS)

# MinGW64 settings
CC64 ?= /mingw64/bin/gcc.exe
RC64 ?= /mingw64/bin/windres.exe
CFLAGS64 ?= $(COMMON_CFLAGS)
LDFLAGS64 ?= $(COMMON_LDFLAGS)

# UCRT64 settings
CCUCRT64 ?= /ucrt64/bin/gcc.exe
RCUCRT64 ?= /ucrt64/bin/windres.exe
CFLAGSUCRT64 ?= $(COMMON_CFLAGS)
LDFLAGSUCRT64 ?= $(COMMON_LDFLAGS)

# ARM64 settings
CCARM64 ?= /clangarm64/bin/clang.exe
RCARM64 ?= /clangarm64/bin/llvm-windres.exe
CFLAGSARM64 ?= $(COMMON_CFLAGS)
LDFLAGSARM64 ?= $(COMMON_LDFLAGS)
RM ?= rm -f
MKDIR ?= mkdir -p

# Release flags
RELEASE_CFLAGS ?= -Os -DNDEBUG -DMEMORY_RELEASE -flto
RELEASE_LDFLAGS ?= -flto -s

# Per-variant toolchain variable suffix, MSYS2 environment and bin directory
TOOLVAR_32 = 32
TOOLVAR_64 = 64
TOOLVAR_ucrt64 = UCRT64
TOOLVAR_arm64 = ARM64
MSYSTEM_32 = MINGW32
MSYSTEM_64 = MINGW64
MSYSTEM_ucrt64 = UCRT64
MSYSTEM_arm64 = CLANGARM64
BINDIR_32 = /mingw32/bin
BINDIR_64 = /mingw64/bin
BINDIR_ucrt64 = /ucrt64/bin
BINDIR_arm64 = /clangarm64/bin

VARIANTS = 32 64 ucrt64 arm64
BUILD_TARGETS = $(foreach v,$(VARIANTS),debug$(v) release$(v))

# Default target: builds native arch for current MSYSTEM or 64-bit
ifeq ($(MSYSTEM),UCRT64)
DEFAULT_TARGET ?= releaseucrt64
else ifeq ($(MSYSTEM),MINGW32)
DEFAULT_TARGET ?= release32
else ifeq ($(MSYSTEM),CLANGARM64)
DEFAULT_TARGET ?= releasearm64
else
DEFAULT_TARGET ?= release64
endif

all: $(DEFAULT_TARGET)

debug: debug32 debug64 debugarm64
release: release32 release64 releasearm64

# debug<variant> and release<variant> re-run make for that one variant and
# configuration; the rules below the ifdef then build it in isolation.
$(BUILD_TARGETS):
	@$(MAKE) --no-print-directory -f $(firstword $(MAKEFILE_LIST)) build \
		CONFIG=$(if $(filter debug%,$@),debug,release) \
		VARIANT=$(patsubst release%,%,$(patsubst debug%,%,$@))

ifdef VARIANT
ifeq ($(filter $(VARIANT),$(VARIANTS)),)
$(error Unknown VARIANT '$(VARIANT)'; expected one of: $(VARIANTS))
endif

TOOLVAR := $(TOOLVAR_$(VARIANT))
CC := $(CC$(TOOLVAR))
RC := $(RC$(TOOLVAR))
ifeq ($(CONFIG),debug)
CFLAGS := $(CFLAGS$(TOOLVAR)) $(DEBUG_CFLAGS)
LDFLAGS := $(LDFLAGS$(TOOLVAR))
TARGET := $(EXE_$(VARIANT))-debug.exe
else
CFLAGS := $(CFLAGS$(TOOLVAR)) $(RELEASE_CFLAGS)
LDFLAGS := $(LDFLAGS$(TOOLVAR)) $(RELEASE_LDFLAGS)
TARGET := $(EXE_$(VARIANT)).exe
endif
export MSYSTEM := $(MSYSTEM_$(VARIANT))
export PATH := $(BINDIR_$(VARIANT)):$(PATH)

OBJ_DIR := $(OBJ_ROOT)/$(VARIANT)-$(CONFIG)
OBJECTS := $(SOURCES:%.c=$(OBJ_DIR)/%.o)
RC_OBJECT := $(RC_SOURCE:%.rc=$(OBJ_DIR)/%.o)

build: $(TARGET)

$(TARGET): $(OBJECTS) $(RC_OBJECT)
	$(CC) $(OBJECTS) $(RC_OBJECT) -o $@ $(LDFLAGS)

$(OBJ_DIR):
	$(MKDIR) $@

# -MMD -MP writes each object's header dependencies next to it, so editing
# a header rebuilds exactly the objects that include it.
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: %.rc | $(OBJ_DIR)
	$(RC) $< -o $@

$(RC_OBJECT): resource.h YouTubeCacher.manifest

-include $(OBJECTS:.o=.d)
endif

# Cleaning targets. obj32, obj64 and objarm64 are where older versions of
# this Makefile put objects.
clean-objects:
	$(RM) -r $(OBJ_ROOT) obj32 obj64 objarm64

clean32:
	$(RM) -r $(OBJ_ROOT)/32-debug $(OBJ_ROOT)/32-release obj32 $(EXE_32).exe $(EXE_32)-debug.exe

clean64:
	$(RM) -r $(OBJ_ROOT)/64-debug $(OBJ_ROOT)/64-release obj64 $(EXE_64).exe $(EXE_64)-debug.exe

cleanucrt64:
	$(RM) -r $(OBJ_ROOT)/ucrt64-debug $(OBJ_ROOT)/ucrt64-release $(EXE_ucrt64).exe $(EXE_ucrt64)-debug.exe

cleanarm64:
	$(RM) -r $(OBJ_ROOT)/arm64-debug $(OBJ_ROOT)/arm64-release objarm64 $(EXE_arm64).exe $(EXE_arm64)-debug.exe

clean: clean32 clean64 cleanucrt64 cleanarm64
	$(MAKE) -C tests clean

# Run the program
run: run32

run32: debug32
	./$(EXE_32)-debug.exe

run64: debug64
	./$(EXE_64)-debug.exe

runucrt64: debugucrt64
	./$(EXE_ucrt64)-debug.exe

runarm64: debugarm64
	./$(EXE_arm64)-debug.exe

# Phony targets
.PHONY: all build debug release $(BUILD_TARGETS) clean clean32 clean64 cleanucrt64 cleanarm64 clean-objects run run32 run64 runucrt64 runarm64 test

test:
	$(MAKE) -C tests run
