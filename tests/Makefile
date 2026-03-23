CC = gcc
CFLAGS = -Wall -Wextra -I.

all: test_cache_duration

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

run: all
	./test_cache_duration

clean:
	rm -f *.o test_cache_duration cache_duration.c
