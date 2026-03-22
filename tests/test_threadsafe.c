#include "mock_windows.h"

// Include the source directly
#include "../threadsafe.c"

int main() {
    printf("Starting threadsafe tests...\n");

    // 1. Test Setup
    ThreadSafeSubprocessContext context;
    if (!InitializeThreadSafeSubprocessContext(&context)) {
        printf("FAILED: Could not initialize context\n");
        return 1;
    }

    // 2. Test SetSubprocessExecutable
    const wchar_t* testPath = L"C:\\path\\to\\yt-dlp.exe";
    BOOL result = SetSubprocessExecutable(&context, testPath);

    if (!result) {
        printf("FAILED: SetSubprocessExecutable returned FALSE\n");
        return 1;
    }

    if (context.executablePath == NULL) {
        printf("FAILED: executablePath is NULL after setting\n");
        return 1;
    }

    if (wcscmp(context.executablePath, testPath) != 0) {
        printf("FAILED: executablePath mismatch\n");
        return 1;
    }

    // Test updating the path
    const wchar_t* testPath2 = L"C:\\other\\yt-dlp.exe";
    result = SetSubprocessExecutable(&context, testPath2);

    if (!result) {
        printf("FAILED: SetSubprocessExecutable returned FALSE on update\n");
        return 1;
    }

    if (wcscmp(context.executablePath, testPath2) != 0) {
        printf("FAILED: executablePath mismatch after update\n");
        return 1;
    }

    // Test missing context
    result = SetSubprocessExecutable(NULL, testPath2);
    if (result) {
        printf("FAILED: SetSubprocessExecutable returned TRUE for NULL context\n");
        return 1;
    }

    // Test uninitialized context
    ThreadSafeSubprocessContext uninitContext;
    memset(&uninitContext, 0, sizeof(ThreadSafeSubprocessContext));
    result = SetSubprocessExecutable(&uninitContext, testPath2);
    if (result) {
        printf("FAILED: SetSubprocessExecutable returned TRUE for uninitialized context\n");
        return 1;
    }

    // Test NULL path
    result = SetSubprocessExecutable(&context, NULL);
    if (result) {
        printf("FAILED: SetSubprocessExecutable returned TRUE for NULL path\n");
        return 1;
    }

    // Clean up
    CleanupThreadSafeSubprocessContext(&context);

    printf("SUCCESS: All tests passed\n");
    return 0;
}

// Add tests to main testing struct
void run_all_tests() {
    // Already defined in main
}
