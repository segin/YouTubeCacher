// Tests for yt-dlp path and command-line checks in ytdlp.c

#include "mock_windows.h"
#include <stdio.h>
#include <string.h>

#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10

// Every path "exists" as a file unless it contains "missing" or "dir"
static DWORD GetFileAttributesW(const wchar_t* path) {
    if (wcsstr(path, L"missing")) return INVALID_FILE_ATTRIBUTES;
    if (wcsstr(path, L"dir")) return FILE_ATTRIBUTE_DIRECTORY;
    return FILE_ATTRIBUTE_NORMAL;
}

#include "ytdlp_args_logic.c"

static int g_tests_run = 0;
static int g_tests_failed = 0;

static void check(const char* name, int condition) {
    g_tests_run++;
    if (condition) {
        printf("PASSED: %s\n", name);
    } else {
        g_tests_failed++;
        printf("FAILED: %s\n", name);
    }
}

static void test_validate_executable(void) {
    check(".exe accepted", ValidateYtDlpExecutable(L"C:\\tools\\yt-dlp.exe"));
    check(".EXE accepted", ValidateYtDlpExecutable(L"C:\\tools\\YT-DLP.EXE"));
    check(".bat rejected", !ValidateYtDlpExecutable(L"C:\\tools\\yt-dlp.bat"));
    check(".cmd rejected", !ValidateYtDlpExecutable(L"C:\\tools\\yt-dlp.cmd"));
    check(".py rejected", !ValidateYtDlpExecutable(L"C:\\tools\\yt-dlp.py"));
    check(".ps1 rejected", !ValidateYtDlpExecutable(L"C:\\tools\\yt-dlp.ps1"));
    check("no extension rejected", !ValidateYtDlpExecutable(L"C:\\tools\\yt-dlp"));
    check(".exe folder with .bat file rejected", !ValidateYtDlpExecutable(L"C:\\a.exe\\yt-dlp.bat"));
    check("trailing dot rejected", !ValidateYtDlpExecutable(L"C:\\tools\\yt-dlp.bat."));
    check("missing file rejected", !ValidateYtDlpExecutable(L"C:\\missing\\yt-dlp.exe"));
    check("directory rejected", !ValidateYtDlpExecutable(L"C:\\dir.exe"));
    check("empty rejected", !ValidateYtDlpExecutable(L""));
    check("NULL rejected", !ValidateYtDlpExecutable(NULL));
}

int main(void) {
    printf("Running yt-dlp argument tests...\n");
    test_validate_executable();
    printf("\nTests run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed ? 1 : 0;
}
