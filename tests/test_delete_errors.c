#include "mock_windows.h"

// Extracted from ui.c by the Makefile
#include "delete_errors_logic.c"

static const wchar_t* HEADER = L"Multiple Delete Operation Results:\n"
                               L"=====================================\n\n";

static void test_append_delete_errors(void) {
    printf("Running AppendDeleteErrorDetails tests...\n");

    wchar_t* combined = NULL;
    size_t len = 0;

    // First append adds the header and the title line
    assert(AppendDeleteErrorDetails(&combined, &len, L"Short", L"abcdefghijk", L"details1"));
    assert(len == wcslen(combined));
    assert(wcsncmp(combined, HEADER, wcslen(HEADER)) == 0);
    assert(wcscmp(combined + wcslen(HEADER), L"Video: Short\ndetails1\n") == 0);

    // A 200-character title and long details (the overflow case) fit exactly
    wchar_t title[201];
    for (int i = 0; i < 200; i++) title[i] = L'T';
    title[200] = L'\0';
    wchar_t details[1001];
    for (int i = 0; i < 1000; i++) details[i] = L'd';
    details[1000] = L'\0';

    size_t before = len;
    assert(AppendDeleteErrorDetails(&combined, &len, title, L"abcdefghijk", details));
    assert(len == wcslen(combined));
    assert(len == before + wcslen(L"Video: ") + 200 + 1 + 1000 + 1);
    assert(wcsstr(combined, HEADER) == combined);
    assert(wcsstr(combined + wcslen(HEADER), HEADER) == NULL); // Header only once

    // Without a title the video ID is used
    before = len;
    assert(AppendDeleteErrorDetails(&combined, &len, NULL, L"abcdefghijk", L"x"));
    assert(wcscmp(combined + before, L"Video ID: abcdefghijk\nx\n") == 0);

    // Degenerate input leaves the text alone
    before = len;
    assert(!AppendDeleteErrorDetails(&combined, &len, NULL, NULL, NULL));
    assert(len == before);
    assert(!AppendDeleteErrorDetails(NULL, &len, NULL, L"id", L"x"));

    free(combined);
    printf("All AppendDeleteErrorDetails tests passed!\n");
}

int main(void) {
    test_append_delete_errors();
    return 0;
}
