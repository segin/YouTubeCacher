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

static void test_path_containment(void) {
    printf("Running IsPathWithinFolder tests...\n");
    const wchar_t* folder = L"C:\\Users\\me\\Downloads";

    // Files inside the folder, at any depth, in any case
    assert(IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\a [abcdefghijk].mp4", folder));
    assert(IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\sub\\b.srt", folder));
    assert(IsPathWithinFolder(L"c:\\users\\ME\\downloads\\a.mp4", folder));
    assert(IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\a.mp4", L"C:\\Users\\me\\Downloads\\"));
    assert(IsPathWithinFolder(L"C:\\a.mp4", L"C:\\"));
    assert(IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\..a.mp4", folder));

    // Outside the folder
    assert(!IsPathWithinFolder(L"C:\\Windows\\win.ini", folder));
    assert(!IsPathWithinFolder(L"D:\\Users\\me\\Downloads\\a.mp4", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads2\\a.mp4", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Download", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\sub\\", folder));
    assert(!IsPathWithinFolder(L"\\\\server\\share\\a.mp4", folder));

    // Traversal and relative components are refused even if not canonicalized
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\..\\..\\x", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\sub\\..\\a.mp4", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads/../x", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\.\\a.mp4", folder));
    assert(!IsPathWithinFolder(L"C:\\Users\\me\\Downloads\\..", folder));

    // Degenerate input
    assert(!IsPathWithinFolder(NULL, folder));
    assert(!IsPathWithinFolder(L"C:\\a.mp4", NULL));
    assert(!IsPathWithinFolder(L"C:\\a.mp4", L""));
    assert(!IsPathWithinFolder(L"\\a.mp4", L"\\"));

    printf("All IsPathWithinFolder tests passed!\n");
}

int main(void) {
    test_subtitle_count();
    test_path_containment();
    return 0;
}
