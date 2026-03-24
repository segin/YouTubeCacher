CC = /mingw64/bin/gcc.exe
CFLAGS = -Wall -Wextra -I.
RM = /usr/bin/rm -f

all: test_cache_duration test_parser_classify test_uri test_base64

test_cache_duration: test_cache_duration.o cache_duration.o
	$(CC) $(CFLAGS) $^ -o $@

test_cache_duration.o: export PATH := /mingw64/bin:/usr/bin:$(PATH)
test_cache_duration.o: test_cache_duration.c
	$(CC) $(CFLAGS) -c $< -o $@

cache_duration.o: export PATH := /mingw64/bin:/usr/bin:$(PATH)
cache_duration.o: cache_duration.c
	$(CC) $(CFLAGS) -c $< -o $@

cache_duration.c: ../cache.c
	echo '#include <wchar.h>' > $@
	echo '#include <stdio.h>' >> $@
	/usr/bin/sed -n '/int ParseDurationToSeconds/,/^}/p' ../cache.c >> $@

test_parser_classify: export PATH := /mingw64/bin:/usr/bin:$(PATH)
test_parser_classify: test_parser_classify.c parser_types.h classify_logic.c
	$(CC) $(CFLAGS) -I. test_parser_classify.c -o $@

parser_types.h: ../parser.h
	/usr/bin/sed -n '/typedef enum {/,/OutputLineType;/p' ../parser.h > $@

classify_logic.c: ../parser.c
	/usr/bin/sed -n '/OutputLineType ClassifyOutputLine/,/^}/p' ../parser.c > $@

test_uri: export PATH := /mingw64/bin:/usr/bin:$(PATH)
test_uri: test_uri.c uri_functions.c
	$(CC) $(CFLAGS) -I. test_uri.c -o $@

uri_functions.c: ../uri.c
	/usr/bin/sed -n '/BOOL IsYouTubeURL/,/^}/p' ../uri.c > $@
	/usr/bin/sed -n '/BOOL IsYouTubePlaylistURL/,/^}/p' ../uri.c >> $@
	/usr/bin/sed -n '/BOOL ContainsMultipleURLs/,/^}/p' ../uri.c >> $@

test_base64: export PATH := /mingw64/bin:/usr/bin:$(PATH)
test_base64: test_base64.c
	$(CC) $(CFLAGS) -I. test_base64.c -o $@

run: all
	./test_cache_duration
	./test_parser_classify
	./test_uri
	./test_base64

clean:
	$(RM) -f *.o test_cache_duration cache_duration.c test_parser_classify parser_types.h classify_logic.c test_uri uri_functions.c test_base64
