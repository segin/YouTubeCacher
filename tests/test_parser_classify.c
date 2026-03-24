#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Mock Win32 types needed for parser_types.h
typedef int BOOL;
#define TRUE 1
#define FALSE 0
typedef void* HWND;
typedef uint32_t DWORD;
typedef struct { DWORD LowPart; DWORD HighPart; } FILETIME;

#include "parser_types.h"

// Mock memory macros used in ClassifyOutputLine
wchar_t* my_wcsdup(const wchar_t* s) {
    if (!s) return NULL;
    size_t len = wcslen(s) + 1;
    wchar_t* d = malloc(len * sizeof(wchar_t));
    if (d) memcpy(d, s, len * sizeof(wchar_t));
    return d;
}
#define SAFE_WCSDUP my_wcsdup
#define SAFE_FREE free

// Include the actual logic extracted from parser.c
#include "classify_logic.c"

// Test code
const wchar_t* GetLineTypeName(OutputLineType type) {
    switch (type) {
        case LINE_TYPE_UNKNOWN: return L"UNKNOWN";
        case LINE_TYPE_INFO_EXTRACTION: return L"INFO_EXTRACTION";
        case LINE_TYPE_FORMAT_SELECTION: return L"FORMAT_SELECTION";
        case LINE_TYPE_DOWNLOAD_PROGRESS: return L"DOWNLOAD_PROGRESS";
        case LINE_TYPE_POST_PROCESSING: return L"POST_PROCESSING";
        case LINE_TYPE_FILE_DESTINATION: return L"FILE_DESTINATION";
        case LINE_TYPE_ERROR: return L"ERROR";
        case LINE_TYPE_WARNING: return L"WARNING";
        case LINE_TYPE_DEBUG: return L"DEBUG";
        case LINE_TYPE_COMPLETION: return L"COMPLETION";
        default: return L"INVALID";
    }
}

int g_tests_run = 0;
int g_tests_passed = 0;

void run_test(const wchar_t* line, OutputLineType expected) {
    g_tests_run++;
    OutputLineType actual = ClassifyOutputLine(line);
    if (actual == expected) {
        g_tests_passed++;
        wprintf(L"PASS: [%ls] -> %ls\n", line ? line : L"NULL", GetLineTypeName(actual));
    } else {
        wprintf(L"FAIL: [%ls] -> Expected %ls, got %ls\n", line ? line : L"NULL", GetLineTypeName(expected), GetLineTypeName(actual));
    }
}

int main() {
    wprintf(L"Running ClassifyOutputLine tests against logic extracted from parser.c...\n\n");

    run_test(NULL, LINE_TYPE_UNKNOWN);
    run_test(L"[info] extracting video information", LINE_TYPE_INFO_EXTRACTION);
    run_test(L"[info] format selection", LINE_TYPE_FORMAT_SELECTION);
    run_test(L"[info] quality selection", LINE_TYPE_FORMAT_SELECTION);
    run_test(L"[download] 10%", LINE_TYPE_DOWNLOAD_PROGRESS);
    run_test(L"[download] downloading video", LINE_TYPE_DOWNLOAD_PROGRESS);
    run_test(L" 100|200|300|400", LINE_TYPE_DOWNLOAD_PROGRESS);
    run_test(L"[ffmpeg] merging formats", LINE_TYPE_POST_PROCESSING);
    run_test(L"post-process something", LINE_TYPE_POST_PROCESSING);
    run_test(L"converting video", LINE_TYPE_POST_PROCESSING);
    run_test(L"destination: file.mp4", LINE_TYPE_FILE_DESTINATION);
    run_test(L"saving to: file.mp4", LINE_TYPE_FILE_DESTINATION);
    run_test(L"[download] file.mp4 has already been downloaded", LINE_TYPE_FILE_DESTINATION);
    run_test(L"error: something went wrong", LINE_TYPE_ERROR);
    run_test(L"failed to download", LINE_TYPE_ERROR);
    run_test(L"exception occurred", LINE_TYPE_ERROR);
    run_test(L"warning message", LINE_TYPE_WARNING);
    run_test(L"warn: message", LINE_TYPE_WARNING);
    run_test(L"100%", LINE_TYPE_COMPLETION);
    run_test(L"download completed", LINE_TYPE_COMPLETION);
    run_test(L"finished downloading", LINE_TYPE_COMPLETION);
    run_test(L"[debug] info", LINE_TYPE_DEBUG);
    run_test(L"[INFO] EXTRACTING", LINE_TYPE_INFO_EXTRACTION);
    run_test(L"   100|200|300|400", LINE_TYPE_DOWNLOAD_PROGRESS);
    run_test(L"100|200", LINE_TYPE_UNKNOWN);
    run_test(L"abc|def|ghi|jkl", LINE_TYPE_UNKNOWN);
    run_test(L"", LINE_TYPE_UNKNOWN);
    run_test(L"Random text", LINE_TYPE_UNKNOWN);

    wprintf(L"\nTests run: %d, Passed: %d, Failed: %d\n", g_tests_run, g_tests_passed, g_tests_run - g_tests_passed);

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
