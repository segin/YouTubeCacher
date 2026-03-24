# Standardize paths for MSYS2/MinGW environment
export PATH := /mingw64/bin:/usr/bin:$(PATH)

CC = /mingw64/bin/gcc.exe
CFLAGS = -Wall -Wextra -I. -DTEST_BUILD
RM = /usr/bin/rm -f

all: test_cache_duration test_parser_classify test_uri test_base64 test_threadsafe

test_cache_duration: test_cache_duration.c cache_duration.c
	$(CC) $(CFLAGS) test_cache_duration.c cache_duration.c -o $@

cache_duration.c: ../cache.c
	echo '#include <wchar.h>' > $@
	echo '#include <stdio.h>' >> $@
	/usr/bin/sed -n '/int ParseDurationToSeconds/,/^}/p' ../cache.c >> $@

test_parser_classify: test_parser_classify.c parser_types.h classify_logic.c
	$(CC) $(CFLAGS) test_parser_classify.c -o $@

parser_types.h: ../parser.h
	/usr/bin/sed -n '/typedef enum {/,/OutputLineType;/p' ../parser.h > $@

classify_logic.c: ../parser.c
	/usr/bin/sed -n '/OutputLineType ClassifyOutputLine/,/^}/p' ../parser.c > $@

test_uri: test_uri.c uri_functions.c
	$(CC) $(CFLAGS) test_uri.c -o $@

uri_functions.c: ../uri.c
	/usr/bin/sed -n '/BOOL IsYouTubeURL/,/^}/p' ../uri.c > $@
	/usr/bin/sed -n '/BOOL IsYouTubePlaylistURL/,/^}/p' ../uri.c >> $@
	/usr/bin/sed -n '/BOOL ContainsMultipleURLs/,/^}/p' ../uri.c >> $@

test_base64: test_base64.c
	$(CC) $(CFLAGS) test_base64.c -o $@

test_threadsafe: test_threadsafe.c mock_windows.h ../threadsafe.c
	$(CC) $(CFLAGS) test_threadsafe.c -o $@

run: all
	./test_cache_duration
	./test_parser_classify
	./test_uri
	./test_base64
	./test_threadsafe

clean:
	$(RM) *.o test_cache_duration cache_duration.c test_parser_classify parser_types.h classify_logic.c test_uri uri_functions.c test_base64 test_threadsafe

.PHONY: all run clean
