CC = gcc
CFLAGS = -Wall -Wextra -I.

all: test_cache_duration test_parser_classify

test_cache_duration: test_cache_duration.o cache_duration.o
	$(CC) $(CFLAGS) $^ -o $@

test_cache_duration.o: test_cache_duration.c
	$(CC) $(CFLAGS) -c $< -o $@

cache_duration.o: cache_duration.c
	$(CC) $(CFLAGS) -c $< -o $@

cache_duration.c: ../cache.c
	echo '#include <wchar.h>' > $@
	echo '#include <stdio.h>' >> $@
	sed -n '/int ParseDurationToSeconds/,/^}/p' ../cache.c >> $@

test_parser_classify: test_parser_classify.c parser_types.h classify_logic.c
	$(CC) $(CFLAGS) -I. test_parser_classify.c -o $@

parser_types.h: ../parser.h
	sed -n '/typedef enum {/,/OutputLineType;/p' ../parser.h > $@

classify_logic.c: ../parser.c
	sed -n '/OutputLineType ClassifyOutputLine/,/^}/p' ../parser.c > $@

run: all
	./test_cache_duration
	./test_parser_classify

clean:
	rm -f *.o test_cache_duration cache_duration.c test_parser_classify parser_types.h classify_logic.c
