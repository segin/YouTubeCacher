#include "mock_windows.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// Extracted from dialogs.c via multidl_lines_logic.c
#include "multidl_lines_logic.c"

static int g_tests_run = 0;
static int g_tests_failed = 0;

static void check_remove(const char* name, const wchar_t* input, const wchar_t* url,
                         BOOL expectedResult, const wchar_t* expectedText) {
    wchar_t buffer[512];
    BOOL result;

    wcsncpy(buffer, input, 511);
    buffer[511] = L'\0';
    result = MultiDl_RemoveExactLine(buffer, url);

    g_tests_run++;
    if (result != expectedResult || wcscmp(buffer, expectedText) != 0) {
        g_tests_failed++;
        printf("FAILED: %s\n  Expected: %d \"%ls\"\n  Actual:   %d \"%ls\"\n",
               name, expectedResult, expectedText, result, buffer);
    }
}

int main(void) {
    printf("Running MultiDl_RemoveExactLine tests...\n");

    check_remove("Longer URL above is kept",
                 L"https://youtu.be/?v=abcd\r\nhttps://youtu.be/?v=abc\r\n",
                 L"https://youtu.be/?v=abc", TRUE,
                 L"https://youtu.be/?v=abcd\r\n");
    check_remove("First line removed",
                 L"a\r\nb\r\nc", L"a", TRUE, L"b\r\nc");
    check_remove("Middle line removed",
                 L"a\r\nb\r\nc", L"b", TRUE, L"a\r\nc");
    check_remove("Last line without terminator removed",
                 L"a\r\nb\r\nc", L"c", TRUE, L"a\r\nb");
    check_remove("Only line removed",
                 L"a", L"a", TRUE, L"");
    check_remove("LF-only line endings",
                 L"ab\na\nb", L"a", TRUE, L"ab\nb");
    check_remove("Surrounding blanks ignored",
                 L"x\r\n  a\t\r\ny", L"a", TRUE, L"x\r\ny");
    check_remove("Substring only is not removed",
                 L"abc\r\nxabc", L"ab", FALSE, L"abc\r\nxabc");
    check_remove("Only first duplicate removed",
                 L"a\r\na\r\n", L"a", TRUE, L"a\r\n");
    check_remove("Blank lines around kept",
                 L"a\r\n\r\nb\r\n\r\nc", L"b", TRUE, L"a\r\n\r\n\r\nc");
    check_remove("Empty URL removes nothing",
                 L"a\r\n\r\nb", L"", FALSE, L"a\r\n\r\nb");

    printf("Tests run: %d, failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
