#include <stdio.h>
#include <wchar.h>

// Minimal Windows types for the extracted function
typedef int BOOL;
typedef wchar_t* LPWSTR;
#define TRUE 1
#define FALSE 0

static LPWSTR g_commandLine = NULL;
static LPWSTR GetCommandLineW(void) { return g_commandLine; }

// GetCommandLineArgsW, extracted from main.c
#include "cmdline_logic.c"

static int g_tests_run = 0;
static int g_tests_failed = 0;

static void check(const wchar_t* commandLine, const wchar_t* expected) {
    wchar_t buffer[256];
    g_tests_run++;
    g_commandLine = NULL;
    if (commandLine) {
        wcscpy(buffer, commandLine);
        g_commandLine = buffer;
    }
    const wchar_t* actual = GetCommandLineArgsW();
    if (wcscmp(actual, expected) != 0) {
        g_tests_failed++;
        printf("FAILED: [%ls]\n  Expected: [%ls], Actual: [%ls]\n",
               commandLine ? commandLine : L"(null)", expected, actual);
    } else {
        printf("PASSED: [%ls]\n", commandLine ? commandLine : L"(null)");
    }
}

int main(void) {
    check(NULL, L"");
    check(L"YouTubeCacher.exe", L"");
    check(L"YouTubeCacher.exe   ", L"");
    check(L"YouTubeCacher.exe https://youtu.be/abc", L"https://youtu.be/abc");
    check(L"\"C:\\Program Files\\YTC\\YouTubeCacher.exe\" https://youtu.be/abc", L"https://youtu.be/abc");
    check(L"\"C:\\Program Files\\YTC\\YouTubeCacher.exe\"\thttps://youtu.be/abc", L"https://youtu.be/abc");
    check(L"C:\\Pro\"gram Fi\"les\\YouTubeCacher.exe arg", L"arg");
    check(L"YouTubeCacher.exe \"C:\\Vid\u00e9os\\\u65e5\u672c\"", L"\"C:\\Vid\u00e9os\\\u65e5\u672c\"");

    printf("\n%d tests, %d failed\n", g_tests_run, g_tests_failed);
    return g_tests_failed ? 1 : 0;
}
