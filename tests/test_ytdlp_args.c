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

static void check_escaped(const char* name, wchar_t* (*escape)(const wchar_t*),
                          const wchar_t* input, const wchar_t* expected) {
    wchar_t* actual = escape(input);
    check(name, actual && wcscmp(actual, expected) == 0);
    if (actual && wcscmp(actual, expected) != 0) {
        printf("  Expected: %ls\n  Actual:   %ls\n", expected, actual);
    }
    free(actual);
}

static void test_escape_command_line_argument(void) {
    check_escaped("plain argument is quoted", EscapeCommandLineArgument, L"abc", L"\"abc\"");
    check_escaped("empty argument", EscapeCommandLineArgument, L"", L"\"\"");
    check_escaped("embedded quote", EscapeCommandLineArgument, L"a\"b", L"\"a\\\"b\"");
    check_escaped("backslashes before a quote are doubled", EscapeCommandLineArgument,
                  L"a\\\\\"b", L"\"a\\\\\\\\\\\"b\"");
    check_escaped("trailing backslash is doubled", EscapeCommandLineArgument,
                  L"C:\\dir\\", L"\"C:\\dir\\\\\"");
    check_escaped("inner backslashes are kept", EscapeCommandLineArgument,
                  L"C:\\a\\b", L"\"C:\\a\\b\"");
    check_escaped("worst case fits", EscapeCommandLineArgument, L"\\\"\\\"\"\\",
                  L"\"\\\\\\\"\\\\\\\"\\\"\\\\\"");
    check("NULL argument", EscapeCommandLineArgument(NULL) == NULL);
}

static void test_escape_output_template_text(void) {
    check_escaped("no percent", EscapeOutputTemplateText, L"C:\\Videos", L"C:\\Videos");
    check_escaped("lone percent", EscapeOutputTemplateText, L"C:\\100%", L"C:\\100%%");
    check_escaped("template field in folder", EscapeOutputTemplateText,
                  L"C:\\a%(title)s", L"C:\\a%%(title)s");
    check_escaped("double percent", EscapeOutputTemplateText, L"C:\\100%%", L"C:\\100%%%%");
    check_escaped("empty", EscapeOutputTemplateText, L"", L"");
    check("NULL text", EscapeOutputTemplateText(NULL) == NULL);
}

static void test_validate_arguments(void) {
    // Allowed
    check("NULL arguments", ValidateYtDlpArguments(NULL));
    check("empty arguments", ValidateYtDlpArguments(L""));
    check("ordinary options", ValidateYtDlpArguments(L"-f bestaudio --embed-metadata --limit-rate 1M"));
    check("--no-exec", ValidateYtDlpArguments(L"--no-exec"));
    check("--no-batch-file", ValidateYtDlpArguments(L"--no-batch-file"));
    check("--exec inside a value", ValidateYtDlpArguments(L"-o C:\\foo--exec\\%(id)s"));
    check("-a inside a -f value", ValidateYtDlpArguments(L"-fbestaudio"));
    check("-a as a value of -o group", ValidateYtDlpArguments(L"-oa.mp4"));
    check("group of flags without a", ValidateYtDlpArguments(L"-iwc"));
    check("end of options marker", ValidateYtDlpArguments(L"--"));

    // Blocked
    check("--exec", !ValidateYtDlpArguments(L"--exec calc"));
    check("--exec=", !ValidateYtDlpArguments(L"--exec=calc"));
    check("--exec among others", !ValidateYtDlpArguments(L"-f best   --exec calc"));
    check("--exec after a tab", !ValidateYtDlpArguments(L"-f best\t--exec calc"));
    check("--exec-before-download", !ValidateYtDlpArguments(L"--exec-before-download calc"));
    check("abbreviated --exe", !ValidateYtDlpArguments(L"--exe calc"));
    check("--batch-file", !ValidateYtDlpArguments(L"--batch-file urls.txt"));
    check("abbreviated --batch", !ValidateYtDlpArguments(L"--batch urls.txt"));
    check("--batch-file=", !ValidateYtDlpArguments(L"--batch-file=urls.txt"));
    check("-a", !ValidateYtDlpArguments(L"-a urls.txt"));
    check("-a with attached value", !ValidateYtDlpArguments(L"-aurls.txt"));
    check("-a in a group", !ValidateYtDlpArguments(L"-ia urls.txt"));
    check("quoted --exec", !ValidateYtDlpArguments(L"\"--exec\" calc"));
    check("partly quoted --exec", !ValidateYtDlpArguments(L"\"-\"-exec calc"));
    check("escaped quote before --exec", !ValidateYtDlpArguments(L"x\\\" --exec calc"));
}

int main(void) {
    printf("Running yt-dlp argument tests...\n");
    test_validate_arguments();
    test_validate_executable();
    test_escape_command_line_argument();
    test_escape_output_template_text();
    printf("\nTests run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed ? 1 : 0;
}
