#include <stdio.h>
#include <wchar.h>
#include <assert.h>
#include <string.h>

// Function prototype
int ParseDurationToSeconds(const wchar_t* duration);

void test_duration_parsing() {
    printf("Running ParseDurationToSeconds tests...\n");

    // HH:MM:SS
    assert(ParseDurationToSeconds(L"1:00:00") == 3600);
    assert(ParseDurationToSeconds(L"01:02:03") == 3723);
    assert(ParseDurationToSeconds(L"10:20:30") == 37230);

    // MM:SS
    assert(ParseDurationToSeconds(L"1:00") == 60);
    assert(ParseDurationToSeconds(L"02:03") == 123);
    assert(ParseDurationToSeconds(L"59:59") == 3599);

    // Edge cases
    assert(ParseDurationToSeconds(NULL) == 0);
    assert(ParseDurationToSeconds(L"") == 0);
    assert(ParseDurationToSeconds(L"invalid") == 0);
    assert(ParseDurationToSeconds(L"123") == 0); // Single number is not MM:SS or HH:MM:SS

    printf("All ParseDurationToSeconds tests passed!\n");
}

int main() {
    test_duration_parsing();
    return 0;
}
