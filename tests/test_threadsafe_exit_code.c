#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mock_windows.h"
#include "YouTubeCacher.h"

// Mock variables and functions used in threadsafe.c
ErrorHandler g_ErrorHandler;
ApplicationState* GetApplicationState(void) { return NULL; }
DWORD GetCurrentThreadId(void) { return 1; }
void DebugOutput(const wchar_t* msg) { (void)msg; }
void AppendToYtDlpSessionLog(const wchar_t* message) { (void)message; }
wchar_t* CreateUserFriendlyYtDlpError(DWORD exitCode, const wchar_t* output, const wchar_t* url) {
    (void)exitCode; (void)output; (void)url; return NULL;
}
BOOL GetYtDlpArgsForOperation(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath,
                              const YtDlpConfig* config, wchar_t* buffer, size_t bufferSize) {
    (void)operation; (void)url; (void)outputPath; (void)config; (void)buffer; (void)bufferSize;
    return TRUE;
}
void* SafeMalloc(size_t size, const char* file, int line) { (void)file; (void)line; return malloc(size); }
void SafeFree(void* ptr, const char* file, int line) { (void)file; (void)line; void** p = (void**)ptr; if (p && *p) { free(*p); *p = NULL; } }
void* SafeRealloc(void* ptr, size_t size, const char* file, int line) { (void)file; (void)line; return realloc(ptr, size); }
wchar_t* SafeWcsdup(const wchar_t* str, const char* file, int line) {
    (void)file; (void)line;
    if (!str) return NULL;
    size_t len = wcslen(str) + 1;
    wchar_t* dup = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (dup) wcscpy(dup, str);
    return dup;
}

// Include the source file to test internal functions directly
#include "../threadsafe.c"

// Test function
void test_GetThreadSafeSubprocessExitCode() {
    ThreadSafeSubprocessContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    // Test 1: Null context
    DWORD exitCode = GetThreadSafeSubprocessExitCode(NULL);
    if (exitCode != (DWORD)-1) {
        printf("FAILED: test_GetThreadSafeSubprocessExitCode(NULL) returned %u, expected %u\n", exitCode, (DWORD)-1);
    } else {
        printf("PASSED: test_GetThreadSafeSubprocessExitCode(NULL)\n");
    }

    // Test 2: Uninitialized context
    ctx.initialized = FALSE;
    exitCode = GetThreadSafeSubprocessExitCode(&ctx);
    if (exitCode != (DWORD)-1) {
        printf("FAILED: test_GetThreadSafeSubprocessExitCode(uninitialized) returned %u, expected %u\n", exitCode, (DWORD)-1);
    } else {
        printf("PASSED: test_GetThreadSafeSubprocessExitCode(uninitialized)\n");
    }

    // Test 3: Valid initialized context with specific exit code
    ctx.initialized = TRUE;
    ctx.exitCode = 42;
    exitCode = GetThreadSafeSubprocessExitCode(&ctx);
    if (exitCode != 42) {
        printf("FAILED: test_GetThreadSafeSubprocessExitCode(initialized) returned %u, expected 42\n", exitCode);
    } else {
        printf("PASSED: test_GetThreadSafeSubprocessExitCode(initialized)\n");
    }
}

int main() {
    test_GetThreadSafeSubprocessExitCode();
    return 0;
}

// Provide SafeWcsDup (capital D) since the code uses it
wchar_t* SafeWcsDup(const wchar_t* str, const char* file, int line) {
    return SafeWcsdup(str, file, line);
}
