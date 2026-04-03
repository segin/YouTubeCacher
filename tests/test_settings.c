#include "mock_windows.h"
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Function prototype
void FormatDuration(wchar_t* duration, size_t bufferSize);

void test_format_duration() {
    printf("Running FormatDuration tests...\n");

    wchar_t duration[32];

    // Single digit case
    wcscpy(duration, L"5");
    FormatDuration(duration, 32);
    assert(wcscmp(duration, L"00:05") == 0);

    // Single number case (seconds)
    wcscpy(duration, L"125");
    FormatDuration(duration, 32);
    assert(wcscmp(duration, L"02:05") == 0);

    // Already in format MM:SS
    wcscpy(duration, L"01:23");
    FormatDuration(duration, 32);
    assert(wcscmp(duration, L"01:23") == 0);

    // Already in format H:MM:SS
    wcscpy(duration, L"1:02:03");
    FormatDuration(duration, 32);
    assert(wcscmp(duration, L"01:02:03") == 0);

    // Spaces
    wcscpy(duration, L" 1 : 23 ");
    FormatDuration(duration, 32);
    assert(wcscmp(duration, L"01:23") == 0);

    printf("All FormatDuration tests passed!\n");
}

int main() {
    test_format_duration();
    return 0;
}
