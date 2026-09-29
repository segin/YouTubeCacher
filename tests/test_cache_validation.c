#include "mock_windows.h"
#include <limits.h>

// Extracted from cache.c by the Makefile
#include "cache_validation_logic.c"

static void test_subtitle_count(void) {
    printf("Running ParseSubtitleCount tests...\n");
    int count = -1;

    // Valid counts
    assert(ParseSubtitleCount(L"0", &count) && count == 0);
    assert(ParseSubtitleCount(L"3", &count) && count == 3);
    assert(ParseSubtitleCount(L"100", &count) && count == 100);

    // Out of range counts are rejected and leave the output untouched
    count = 7;
    assert(!ParseSubtitleCount(L"-5", &count) && count == 7);
    assert(!ParseSubtitleCount(L"-1", &count));
    assert(!ParseSubtitleCount(L"101", &count));
    assert(!ParseSubtitleCount(L"1000", &count));

    wchar_t big[32];
    swprintf(big, 32, L"%d", INT_MAX);
    assert(!ParseSubtitleCount(big, &count));
    swprintf(big, 32, L"%d", INT_MIN);
    assert(!ParseSubtitleCount(big, &count));
    assert(!ParseSubtitleCount(L"99999999999999999999999", &count));
    assert(count == 7);

    // Malformed tokens are rejected
    assert(!ParseSubtitleCount(L"", &count));
    assert(!ParseSubtitleCount(L"abc", &count));
    assert(!ParseSubtitleCount(L"2x", &count));
    assert(!ParseSubtitleCount(NULL, &count));
    assert(!ParseSubtitleCount(L"1", NULL));

    printf("All ParseSubtitleCount tests passed!\n");
}

int main(void) {
    test_subtitle_count();
    return 0;
}
