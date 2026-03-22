#include "mock_windows.h"

// Define macros to prevent inclusion of real headers
#define THREADSAFE_H
#define YOUTUBECACHER_H

// Mock systems
void DebugOutput(const wchar_t* msg) { (void)msg; }
void AppendToYtDlpSessionLog(const wchar_t* msg) { (void)msg; }
void* GetApplicationState(void) { return NULL; }
void* GetYtDlpArgsForOperation(int op, const wchar_t* url, const wchar_t* path, void* config, wchar_t* args, int len) {
    (void)op; (void)url; (void)path; (void)config; (void)args; (void)len;
    return (void*)1;
}
void* CreateUserFriendlyYtDlpError(DWORD code, const wchar_t* output, const wchar_t* url) {
    (void)code; (void)output; (void)url;
    return NULL;
}

// Dummy structures needed by threadsafe.c
typedef struct { int dummy; } ErrorHandler;
typedef struct { int dummy; } MemoryManager;
typedef struct { int dummy; } ApplicationState;
typedef void (*ProgressCallback)(int, const wchar_t*, void*);
typedef struct {
    CRITICAL_SECTION criticalSection;
    BOOL isRunning;
} ThreadContext;
typedef struct {
    BOOL success;
    DWORD exitCode;
    wchar_t* output;
    wchar_t* errorMessage;
} YtDlpResult;
typedef struct {
    wchar_t* ytDlpPath;
} YtDlpConfig;
typedef struct {
    int operation;
    wchar_t* url;
    wchar_t* outputPath;
} YtDlpRequest;
typedef struct {
    YtDlpConfig* config;
    YtDlpRequest* request;
    ProgressCallback progressCallback;
    void* callbackUserData;
    HWND parentWindow;
    ThreadContext threadContext;
    YtDlpResult* result;
    BOOL completed;
    DWORD completionTime;
} SubprocessContext;

typedef struct {
    BOOL initialized;
    CRITICAL_SECTION processStateLock;
    CRITICAL_SECTION outputLock;
    CRITICAL_SECTION configLock;
    HANDLE cancellationEvent;
    BOOL cancellationRequested;
    BOOL processRunning;
    BOOL processCompleted;
    DWORD exitCode;
    wchar_t* outputBuffer;
    size_t outputLength;
    size_t outputBufferSize;
    BOOL outputComplete;
    wchar_t* executablePath;
    wchar_t* arguments;
    wchar_t* workingDirectory;
    DWORD timeoutMs;
    ProgressCallback progressCallback;
    void* callbackUserData;
    HWND parentWindow;
    HANDLE hProcess;
    HANDLE hThread;
    DWORD processId;
    DWORD threadId;
    HANDLE hOutputRead;
    HANDLE hOutputWrite;
} ThreadSafeSubprocessContext;

// Forward declarations of functions in threadsafe.c that are used before they are defined
BOOL CancelThreadSafeSubprocess(ThreadSafeSubprocessContext* context);
BOOL WaitForThreadSafeSubprocessCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs);
BOOL ForceKillThreadSafeSubprocess(ThreadSafeSubprocessContext* context);

ErrorHandler g_ErrorHandler;

// Include the actual source file
#include "../threadsafe.c"

int main() {
    printf("Starting tests for IsThreadSafetyInitialized using threadsafe.c...\n");

    // Test 1: Initial state
    printf("Test 1: Check initial state... ");
    if (IsThreadSafetyInitialized() != FALSE) {
        printf("FAILED (Expected FALSE, got TRUE)\n");
        return 1;
    }
    printf("Passed.\n");

    // Test 2: Initialize
    printf("Test 2: Check state after InitializeThreadSafety... ");
    InitializeThreadSafety();
    if (IsThreadSafetyInitialized() != TRUE) {
        printf("FAILED (Expected TRUE, got FALSE)\n");
        return 1;
    }
    printf("Passed.\n");

    // Test 3: Cleanup
    printf("Test 3: Check state after CleanupThreadSafety... ");
    CleanupThreadSafety();
    if (IsThreadSafetyInitialized() != FALSE) {
        printf("FAILED (Expected FALSE, got TRUE)\n");
        return 1;
    }
    printf("Passed.\n");

    // Test 4: Multiple calls
    printf("Test 4: Check state after re-initialization... ");
    InitializeThreadSafety();
    InitializeThreadSafety(); // Double call should be fine
    if (IsThreadSafetyInitialized() != TRUE) {
        printf("FAILED (Expected TRUE, got FALSE)\n");
        return 1;
    }
    printf("Passed.\n");

    printf("Test 5: Check state after re-cleanup... ");
    CleanupThreadSafety();
    CleanupThreadSafety(); // Double call should be fine
    if (IsThreadSafetyInitialized() != FALSE) {
        printf("FAILED (Expected FALSE, got TRUE)\n");
        return 1;
    }
    printf("Passed.\n");

    printf("\nAll thread safety initialization tests passed successfully!\n");
    return 0;
}
