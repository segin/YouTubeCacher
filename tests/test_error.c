#include "mock_windows.h"

// The test will include error_logic.c which contains the function implementation.
// We include error.h to get the StandardErrorCode enum and IsRecoverableError prototype.
// Since we have a mock windows.h in this directory, it should be picked up first.
#include "../error.h"

// This will be extracted from error.c and included via error_logic.c
#include "error_logic.c"

int g_tests_run = 0;
int g_tests_failed = 0;

const wchar_t* GetErrorCodeName(StandardErrorCode code) {
    switch (code) {
        case YTC_ERROR_SUCCESS: return L"YTC_ERROR_SUCCESS";
        case YTC_ERROR_MEMORY_ALLOCATION: return L"YTC_ERROR_MEMORY_ALLOCATION";
        case YTC_ERROR_FILE_NOT_FOUND: return L"YTC_ERROR_FILE_NOT_FOUND";
        case YTC_ERROR_INVALID_PARAMETER: return L"YTC_ERROR_INVALID_PARAMETER";
        case YTC_ERROR_NETWORK_FAILURE: return L"YTC_ERROR_NETWORK_FAILURE";
        case YTC_ERROR_YTDLP_EXECUTION: return L"YTC_ERROR_YTDLP_EXECUTION";
        case YTC_ERROR_CACHE_OPERATION: return L"YTC_ERROR_CACHE_OPERATION";
        case YTC_ERROR_THREAD_CREATION: return L"YTC_ERROR_THREAD_CREATION";
        case YTC_ERROR_VALIDATION_FAILED: return L"YTC_ERROR_VALIDATION_FAILED";
        case YTC_ERROR_SUBPROCESS_FAILED: return L"YTC_ERROR_SUBPROCESS_FAILED";
        case YTC_ERROR_DIALOG_CREATION: return L"YTC_ERROR_DIALOG_CREATION";
        case YTC_ERROR_FILE_ACCESS: return L"YTC_ERROR_FILE_ACCESS";
        case YTC_ERROR_BUFFER_OVERFLOW: return L"YTC_ERROR_BUFFER_OVERFLOW";
        case YTC_ERROR_URL_INVALID: return L"YTC_ERROR_URL_INVALID";
        case YTC_ERROR_PERMISSION_DENIED: return L"YTC_ERROR_PERMISSION_DENIED";
        case YTC_ERROR_DISK_FULL: return L"YTC_ERROR_DISK_FULL";
        case YTC_ERROR_TIMEOUT: return L"YTC_ERROR_TIMEOUT";
        case YTC_ERROR_AUTHENTICATION: return L"YTC_ERROR_AUTHENTICATION";
        case YTC_ERROR_CONFIGURATION: return L"YTC_ERROR_CONFIGURATION";
        case YTC_ERROR_INITIALIZATION: return L"YTC_ERROR_INITIALIZATION";
        case YTC_ERROR_CLEANUP: return L"YTC_ERROR_CLEANUP";
        default: return L"UNKNOWN";
    }
}

void assert_recoverable(StandardErrorCode code, BOOL expected) {
    g_tests_run++;
    BOOL actual = IsRecoverableError(code);
    if (expected != actual) {
        g_tests_failed++;
        wprintf(L"FAILED: IsRecoverableError(%ls)\n  Expected: %s, Actual: %s\n",
               GetErrorCodeName(code), expected ? L"TRUE" : L"FALSE", actual ? L"TRUE" : L"FALSE");
    } else {
        wprintf(L"PASS: IsRecoverableError(%ls) == %s\n",
               GetErrorCodeName(code), expected ? L"TRUE" : L"FALSE");
    }
}

int main() {
    wprintf(L"Running IsRecoverableError tests...\n\n");

    // Recoverable errors
    assert_recoverable(YTC_ERROR_MEMORY_ALLOCATION, TRUE);
    assert_recoverable(YTC_ERROR_NETWORK_FAILURE, TRUE);
    assert_recoverable(YTC_ERROR_FILE_ACCESS, TRUE);
    assert_recoverable(YTC_ERROR_TIMEOUT, TRUE);
    assert_recoverable(YTC_ERROR_CACHE_OPERATION, TRUE);

    // Non-recoverable errors
    assert_recoverable(YTC_ERROR_SUCCESS, FALSE);
    assert_recoverable(YTC_ERROR_FILE_NOT_FOUND, FALSE);
    assert_recoverable(YTC_ERROR_INVALID_PARAMETER, FALSE);
    assert_recoverable(YTC_ERROR_YTDLP_EXECUTION, FALSE);
    assert_recoverable(YTC_ERROR_THREAD_CREATION, FALSE);
    assert_recoverable(YTC_ERROR_VALIDATION_FAILED, FALSE);
    assert_recoverable(YTC_ERROR_SUBPROCESS_FAILED, FALSE);
    assert_recoverable(YTC_ERROR_DIALOG_CREATION, FALSE);
    assert_recoverable(YTC_ERROR_BUFFER_OVERFLOW, FALSE);
    assert_recoverable(YTC_ERROR_URL_INVALID, FALSE);
    assert_recoverable(YTC_ERROR_PERMISSION_DENIED, FALSE);
    assert_recoverable(YTC_ERROR_DISK_FULL, FALSE);
    assert_recoverable(YTC_ERROR_AUTHENTICATION, FALSE);
    assert_recoverable(YTC_ERROR_CONFIGURATION, FALSE);
    assert_recoverable(YTC_ERROR_INITIALIZATION, FALSE);
    assert_recoverable(YTC_ERROR_CLEANUP, FALSE);

    // Edge cases
    assert_recoverable((StandardErrorCode)9999, FALSE);
    assert_recoverable((StandardErrorCode)0, FALSE);

    wprintf(L"\nTests run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}
