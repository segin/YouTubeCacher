# CC always has make's built-in default (cc), so ?= would never apply.
ifeq ($(origin CC),default)
CC = gcc
endif
CFLAGS ?= -Wall -Wextra
# The tests build against the mock Windows headers here whatever CFLAGS
# the environment or command line supplies.
override CFLAGS += -I. -I./include -DTEST_BUILD
RM ?= rm -f

all: test_cache_duration test_parser_classify test_uri test_uri_mem test_base64 test_threadsafe test_settings test_memory test_ytdlp_cache test_parser_postprocess test_subproc test_cache_validation test_delete_errors test_memory_debug test_cmdline test_multidl_lines test_settings_registry

test_memory: test_memory.c ../memory.c
	$(CC) $(CFLAGS) test_memory.c -o $@

test_memory_debug: test_memory.c ../memory.c
	$(CC) $(CFLAGS) -DMEMORY_DEBUG test_memory.c -o $@

test_cache_duration: test_cache_duration.c cache_duration.c
	$(CC) $(CFLAGS) test_cache_duration.c cache_duration.c -o $@

test_parser_postprocess: test_parser_postprocess.c postprocess_logic.c
	$(CC) $(CFLAGS) test_parser_postprocess.c -o $@

test_settings: test_settings.c settings_logic.c
	$(CC) $(CFLAGS) test_settings.c settings_logic.c -o $@

test_settings_registry: test_settings_registry.c settings_registry_logic.c dpi_position_logic.c
	$(CC) $(CFLAGS) test_settings_registry.c -o $@

dpi_position_logic.c: ../dpi.c
	sed -n '/^static BOOL QueryWindowPositionValue/,/^}/p' ../dpi.c > $@

settings_registry_logic.c: ../settings.c
	sed -n '/^BOOL LoadSettingFromRegistry/,/^}/p' ../settings.c > $@
	sed -n '/^BOOL LoadBoolSettingFromRegistry/,/^}/p' ../settings.c >> $@

settings_logic.c: ../settings.c
	echo '#include "mock_windows.h"' > $@
	echo '#include <wchar.h>' >> $@
	echo '#include <wctype.h>' >> $@
	echo '#include <stdio.h>' >> $@
	sed -n '/void FormatDuration/,/^}/p' ../settings.c >> $@

cache_duration.c: ../cache.c
	echo '#include <wchar.h>' > $@
	echo '#include <stdio.h>' >> $@
	sed -n '/int ParseDurationToSeconds/,/^}/p' ../cache.c >> $@

test_cache_validation: test_cache_validation.c cache_validation_logic.c
	$(CC) $(CFLAGS) test_cache_validation.c -o $@

cache_validation_logic.c: ../cache.c ../cache.h
	grep '#define MAX_CACHE_SUBTITLES' ../cache.h > $@
	sed -n '/static BOOL ParseSubtitleCount/,/^}/p' ../cache.c >> $@
	sed -n '/static BOOL IsPathWithinFolder/,/^}/p' ../cache.c >> $@

test_delete_errors: test_delete_errors.c delete_errors_logic.c
	$(CC) $(CFLAGS) test_delete_errors.c -o $@

delete_errors_logic.c: ../ui.c
	sed -n '/static BOOL AppendDeleteErrorDetails/,/^}/p' ../ui.c > $@

test_parser_classify: test_parser_classify.c parser_types.h classify_logic.c
	$(CC) $(CFLAGS) test_parser_classify.c -o $@

parser_types.h: ../parser.h
	sed -n '/typedef enum {/,/OutputLineType;/p' ../parser.h > $@

classify_logic.c: ../parser.c
	sed -n '/OutputLineType ClassifyOutputLine/,/^}/p' ../parser.c > $@

postprocess_logic.c: ../parser.c
	echo '#ifndef POSTPROCESS_LOGIC_C' > $@
	echo '#define POSTPROCESS_LOGIC_C' >> $@
	echo '#define windows_h' >> $@
	echo '#define _INC_WINDOWS' >> $@
	/usr/bin/sed -n '/BOOL InitializeEnhancedProgressInfo/,/^}/p' ../parser.c >> $@
	/usr/bin/sed -n '/void FreeEnhancedProgressInfo/,/^}/p' ../parser.c >> $@
	/usr/bin/sed -n '/BOOL AddTrackedFile/,/^}/p' ../parser.c >> $@
	/usr/bin/sed -n '/BOOL AddPreDownloadMessage/,/^}/p' ../parser.c >> $@
	/usr/bin/sed -n '/void UpdateDownloadState/,/^}/p' ../parser.c >> $@
	/usr/bin/sed -n '/BOOL IsVideoFileExtension/,/^}/p' ../parser.c >> $@
	/usr/bin/sed -n '/wchar_t\* ExtractFileExtension/,/^}/p' ../parser.c >> $@
	/usr/bin/sed -n '/BOOL ParseFileDestinationLine/,/^}/p' ../parser.c >> $@
	/usr/bin/sed -n '/BOOL ParsePostProcessingLine/,/^}/p' ../parser.c >> $@
	echo '#endif // POSTPROCESS_LOGIC_C' >> $@

test_uri: test_uri.c uri_functions.c
	$(CC) $(CFLAGS) test_uri.c -o $@

uri_functions.c: ../uri.c
	sed -n '/BOOL IsYouTubeURL/,/^}/p' ../uri.c > $@
	sed -n '/BOOL IsYouTubePlaylistURL/,/^}/p' ../uri.c >> $@
	sed -n '/BOOL ContainsMultipleURLs/,/^}/p' ../uri.c >> $@
	sed -n '/wchar_t\* NormalizeURL/,/^}/p' ../uri.c >> uri_functions.c

test_uri_mem: test_uri_mem.c uri_functions.c
	$(CC) $(CFLAGS) test_uri_mem.c -o $@

test_base64: test_base64.c
	$(CC) $(CFLAGS) test_base64.c -o $@

test_threadsafe: test_threadsafe.c mock_windows.h ../threadsafe.c
	$(CC) $(CFLAGS) test_threadsafe.c -o $@

test_subproc: test_subproc.c mock_windows.h ../subproc.c
	$(CC) $(CFLAGS) test_subproc.c -o $@

test_error: test_error.c error_logic.c
	$(CC) $(CFLAGS) test_error.c -o $@

error_logic.c: ../error.c
	sed -n '/BOOL IsRecoverableError/,/^}/p' ../error.c > $@

test_cmdline: test_cmdline.c cmdline_logic.c
	$(CC) $(CFLAGS) test_cmdline.c -o $@

cmdline_logic.c: ../main.c
	sed -n '/static LPWSTR GetCommandLineArgsW/,/^}/p' ../main.c > $@

test_ytdlp_cache: test_ytdlp_cache.c ytdlp_cache_logic.c
	$(CC) $(CFLAGS) test_ytdlp_cache.c -o $@

ytdlp_cache_logic.c: ../ytdlp.c
	/usr/bin/sed -n '/BOOL IsCachedMetadataValid/,/^}/p' ../ytdlp.c > $@

test_multidl_lines: test_multidl_lines.c multidl_lines_logic.c
	$(CC) $(CFLAGS) test_multidl_lines.c -o $@

multidl_lines_logic.c: ../dialogs.c
	sed -n '/^BOOL MultiDl_RemoveExactLine/,/^}/p' ../dialogs.c > $@

run: all
	./test_cache_duration
	./test_parser_classify
	./test_uri
	./test_uri_mem
	./test_base64
	./test_threadsafe
	./test_settings
	./test_memory
	./test_ytdlp_cache
	./test_parser_postprocess
	./test_subproc
	./test_cache_validation
	./test_delete_errors
	./test_memory_debug
	./test_cmdline
	./test_multidl_lines
	./test_settings_registry

clean:
	$(RM) *.o test_error error_logic.c test_cache_duration cache_duration.c test_parser_classify parser_types.h classify_logic.c postprocess_logic.c test_parser_postprocess test_uri test_uri_mem uri_functions.c test_base64 test_threadsafe test_settings settings_logic.c test_memory test_ytdlp_cache ytdlp_cache_logic.c test_subproc test_cache_validation cache_validation_logic.c test_delete_errors delete_errors_logic.c test_memory_debug test_cmdline cmdline_logic.c test_multidl_lines multidl_lines_logic.c test_settings_registry settings_registry_logic.c dpi_position_logic.c

.PHONY: all run clean
