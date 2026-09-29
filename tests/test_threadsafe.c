#define TEST_THREADSAFE_C

// Scripted pipe and ASCII conversion for the output reader tests
#define PeekNamedPipe Test_PeekNamedPipe
#define ReadFile Test_ReadFile
#define MultiByteToWideChar Test_MultiByteToWideChar

#include "mock_windows.h"

static const char* g_pipeData = NULL;
static size_t g_pipeSize = 0;
static size_t g_pipePos = 0;

static BOOL Test_PeekNamedPipe(HANDLE h, LPVOID buf, DWORD bufsz, DWORD* read, DWORD* avail, DWORD* message) {
    (void)h; (void)buf; (void)bufsz; (void)read; (void)message;
    if (avail) *avail = (DWORD)(g_pipeSize - g_pipePos);
    return TRUE;
}

static BOOL Test_ReadFile(HANDLE h, LPVOID buf, DWORD bufsz, DWORD* read, LPVOID overlap) {
    (void)h; (void)overlap;
    size_t count = g_pipeSize - g_pipePos;
    if (count > bufsz) count = bufsz;
    memcpy(buf, g_pipeData + g_pipePos, count);
    g_pipePos += count;
    *read = (DWORD)count;
    return TRUE;
}

static int Test_MultiByteToWideChar(uint32_t cp, DWORD flags, const char* src, int srclen, wchar_t* dst, int dstlen) {
    (void)cp; (void)flags;
    if (!dst) return srclen;
    if (dstlen < srclen) return 0;
    for (int i = 0; i < srclen; i++) dst[i] = (wchar_t)(unsigned char)src[i];
    return srclen;
}

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

int test_initialization() {
    printf("Starting thread safety initialization tests...\n");

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

int test_set_executable() {
    printf("Starting SetSubprocessExecutable tests...\n");

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

    printf("All SetSubprocessExecutable tests passed successfully!\n");
    return 0;
}

int test_debug_output() {
    printf("Starting ThreadSafeDebugOutput tests...\n");

    // Initialize thread safety
    InitializeThreadSafety();

    // Test ThreadSafeDebugOutput with NULL message
    printf("Testing ThreadSafeDebugOutput(NULL)... ");
    ThreadSafeDebugOutput(NULL);
    printf("Passed.\n");

    // Test ThreadSafeDebugOutputF with NULL format
    printf("Testing ThreadSafeDebugOutputF(NULL)... ");
    ThreadSafeDebugOutputF(NULL);
    printf("Passed.\n");

    // Test with actual message to ensure it still works
    printf("Testing ThreadSafeDebugOutput(L\"Test message\")... ");
    ThreadSafeDebugOutput(L"Test message");
    printf("Passed.\n");

    printf("Testing ThreadSafeDebugOutputF(L\"Test format %%d\", 42)... ");
    ThreadSafeDebugOutputF(L"Test format %d", 42);
    printf("Passed.\n");

    // Cleanup
    CleanupThreadSafety();

    printf("All ThreadSafeDebugOutput tests passed successfully!\n");
    return 0;
}

int test_clear_and_dir_null() {
    printf("Starting ClearThreadSafeSubprocessOutput and NULL directory tests...\n");

    // Initialize
    ThreadSafeSubprocessContext context;
    InitializeThreadSafeSubprocessContext(&context);

    // Test ClearThreadSafeSubprocessOutput with NULL context (PR 28)
    printf("Testing ClearThreadSafeSubprocessOutput(NULL)... ");
    ClearThreadSafeSubprocessOutput(NULL);
    printf("Passed.\n");

    // Test ClearThreadSafeSubprocessOutput with uninitialized context (PR 28)
    printf("Testing ClearThreadSafeSubprocessOutput(uninitialized)... ");
    ThreadSafeSubprocessContext uninitContext;
    memset(&uninitContext, 0, sizeof(ThreadSafeSubprocessContext));
    uninitContext.initialized = FALSE;
    ClearThreadSafeSubprocessOutput(&uninitContext);
    printf("Passed.\n");

    // Test SetSubprocessWorkingDirectory with NULL directory (PR 29)
    printf("Testing SetSubprocessWorkingDirectory(NULL)... ");
    if (SetSubprocessWorkingDirectory(&context, NULL)) {
        if (context.workingDirectory == NULL) {
            printf("Passed.\n");
        } else {
            printf("FAILED (Expected NULL workingDirectory)\n");
            return 1;
        }
    } else {
        printf("FAILED (Expected TRUE return)\n");
        return 1;
    }

    // Cleanup
    CleanupThreadSafeSubprocessContext(&context);

    printf("All ClearThreadSafeSubprocessOutput and NULL directory tests passed successfully!\n");
    return 0;
}

int test_exit_code() {
    printf("Starting GetThreadSafeSubprocessExitCode tests...\n");

    // Test with NULL context
    printf("Testing GetThreadSafeSubprocessExitCode(NULL)... ");
    if (GetThreadSafeSubprocessExitCode(NULL) == (DWORD)-1) {
        printf("Passed.\n");
    } else {
        printf("FAILED (Expected -1)\n");
        return 1;
    }

    // Test with uninitialized context
    printf("Testing GetThreadSafeSubprocessExitCode(uninitialized)... ");
    ThreadSafeSubprocessContext uninitContext;
    memset(&uninitContext, 0, sizeof(ThreadSafeSubprocessContext));
    uninitContext.initialized = FALSE;
    if (GetThreadSafeSubprocessExitCode(&uninitContext) == (DWORD)-1) {
        printf("Passed.\n");
    } else {
        printf("FAILED (Expected -1)\n");
        return 1;
    }

    // Test with initialized context
    printf("Testing GetThreadSafeSubprocessExitCode(initialized)... ");
    ThreadSafeSubprocessContext context;
    InitializeThreadSafeSubprocessContext(&context);
    context.exitCode = 42;
    if (GetThreadSafeSubprocessExitCode(&context) == 42) {
        printf("Passed.\n");
    } else {
        printf("FAILED (Expected 42)\n");
        return 1;
    }

    // Cleanup
    CleanupThreadSafeSubprocessContext(&context);

    printf("All GetThreadSafeSubprocessExitCode tests passed successfully!\n");
    return 0;
}

int test_output_reader() {
    printf("Starting SubprocessOutputReaderThread tests...\n");

    // A 5000-character line (longer than one 4 KB read), a short line that
    // crosses the 8 KB read boundary, and a final line with no terminator.
    // The mocked process has already exited, so all of it must be drained.
    size_t longLen = 5000;
    size_t padLen = 8192 - (longLen + 2) - 3;
    size_t dataSize = longLen + 2 + padLen + 1 + 12 + 11;
    char* data = (char*)malloc(dataSize + 1);
    size_t pos = 0;
    memset(data + pos, 'a', longLen); pos += longLen;
    memcpy(data + pos, "\r\n", 2); pos += 2;
    memset(data + pos, 'b', padLen); pos += padLen;
    data[pos++] = '\n';
    memcpy(data + pos, "cross-line\r\n", 12); pos += 12;
    memcpy(data + pos, "ERROR: tail", 11); pos += 11;
    data[pos] = '\0';

    size_t expectedLen = longLen + 2 + padLen + 2 + 12 + 13;
    wchar_t* expected = (wchar_t*)malloc((expectedLen + 1) * sizeof(wchar_t));
    size_t e = 0;
    for (size_t i = 0; i < longLen; i++) expected[e++] = L'a';
    expected[e++] = L'\r'; expected[e++] = L'\n';
    for (size_t i = 0; i < padLen; i++) expected[e++] = L'b';
    expected[e++] = L'\r'; expected[e++] = L'\n';
    wcscpy(expected + e, L"cross-line\r\nERROR: tail\r\n");

    g_pipeData = data;
    g_pipeSize = pos;
    g_pipePos = 0;

    ThreadSafeSubprocessContext context;
    InitializeThreadSafeSubprocessContext(&context);
    context.hProcess = (HANDLE)1;
    context.hOutputRead = (HANDLE)1;
    context.processRunning = TRUE;

    printf("Testing long lines, 4 KB boundaries and drain after exit... ");
    SubprocessOutputReaderThread(&context);
    if (g_pipePos != g_pipeSize) {
        printf("FAILED (pipe not drained: %zu of %zu bytes read)\n", g_pipePos, g_pipeSize);
        return 1;
    }
    if (!context.outputComplete) {
        printf("FAILED (outputComplete not set)\n");
        return 1;
    }
    if (context.outputLength != expectedLen || wcscmp(context.outputBuffer, expected) != 0) {
        printf("FAILED (output mismatch: got %zu characters, expected %zu)\n", context.outputLength, expectedLen);
        return 1;
    }
    printf("Passed.\n");

    CleanupThreadSafeSubprocessContext(&context);
    free(expected);
    free(data);

    printf("All SubprocessOutputReaderThread tests passed successfully!\n");
    return 0;
}

int main() {
    if (test_initialization() != 0) return 1;
    if (test_set_executable() != 0) return 1;
    if (test_debug_output() != 0) return 1;
    if (test_clear_and_dir_null() != 0) return 1;
    if (test_exit_code() != 0) return 1;
    if (test_output_reader() != 0) return 1;
    return 0;
}
