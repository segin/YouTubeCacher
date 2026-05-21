#include "mock_windows.h"

// Define macros to prevent inclusion of real headers
#define THREADING_H
#define YOUTUBECACHER_H

// Enums needed for the structs
typedef enum {
    YTDLP_OP_GET_INFO,
    YTDLP_OP_GET_TITLE,
    YTDLP_OP_GET_DURATION,
    YTDLP_OP_GET_TITLE_DURATION,
    YTDLP_OP_DOWNLOAD,
    YTDLP_OP_VALIDATE,
    YTDLP_OP_GET_PLAYLIST_INFO,
    YTDLP_OP_DOWNLOAD_PLAYLIST
} YtDlpOperation;

typedef enum {
    TEMP_DIR_SYSTEM,
    TEMP_DIR_DOWNLOAD,
    TEMP_DIR_CUSTOM,
    TEMP_DIR_APPDATA
} TempDirStrategy;

// Progress callback function type
typedef void (*ProgressCallback)(int percentage, const wchar_t* status, void* userData);

// Struct definitions exactly as in production headers
typedef struct {
    wchar_t ytDlpPath[MAX_PATH]; // Simplified for test
    wchar_t defaultTempDir[MAX_PATH];
    wchar_t defaultArgs[1024];
    DWORD timeoutSeconds;
    BOOL enableVerboseLogging;
    BOOL autoRetryOnFailure;
    TempDirStrategy tempDirStrategy;
} YtDlpConfig;

typedef struct {
    YtDlpOperation operation;
    wchar_t* url;
    wchar_t* outputPath;
    wchar_t* tempDir;
    BOOL useCustomArgs;
    wchar_t* customArgs;
} YtDlpRequest;

typedef struct {
    BOOL success;
    DWORD exitCode;
    wchar_t* output;
    wchar_t* errorMessage;
    wchar_t* diagnostics;
} YtDlpResult;

typedef struct {
    HANDLE hThread;
    DWORD threadId;
    BOOL isRunning;
    BOOL cancelRequested;
    CRITICAL_SECTION criticalSection;
    DWORD timeoutMs;
    SYSTEMTIME startTime;
    wchar_t threadName[64];
} ThreadContext;

typedef struct {
    CRITICAL_SECTION processStateLock;
    CRITICAL_SECTION outputLock;
    CRITICAL_SECTION configLock;
    HANDLE hProcess;
    HANDLE hThread;
    DWORD processId;
    DWORD threadId;
    BOOL processRunning;
    BOOL processCompleted;
    DWORD exitCode;
    HANDLE hOutputRead;
    HANDLE hOutputWrite;
    wchar_t* outputBuffer;
    size_t outputBufferSize;
    size_t outputLength;
    BOOL outputComplete;
    wchar_t* executablePath;
    wchar_t* arguments;
    wchar_t* workingDirectory;
    DWORD timeoutMs;
    ProgressCallback progressCallback;
    void* callbackUserData;
    HWND parentWindow;
    BOOL cancellationRequested;
    HANDLE cancellationEvent;
    BOOL initialized;
} ThreadSafeSubprocessContext;

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
    HANDLE hProcess;
    HANDLE hOutputRead;
    HANDLE hOutputWrite;
    wchar_t* accumulatedOutput;
    size_t outputBufferSize;
    ThreadSafeSubprocessContext* threadSafeContext;
} SubprocessContext;

// Mock application functions
void ThreadSafeDebugOutput(const wchar_t* message) { (void)message; }
void ThreadSafeDebugOutputF(const wchar_t* format, ...) { (void)format; }
void StartNewYtDlpInvocation(void) {}
void AppendToYtDlpSessionLog(const wchar_t* msg) { (void)msg; }

BOOL InitializeThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context) {
    (void)context; return TRUE;
}
void CleanupThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context) {
    (void)context;
}
BOOL SetSubprocessExecutable(ThreadSafeSubprocessContext* context, const wchar_t* path) {
    (void)context; (void)path; return TRUE;
}
BOOL GetYtDlpArgsForOperation(YtDlpOperation op, const wchar_t* url, const wchar_t* path, const YtDlpConfig* config, wchar_t* args, int len) {
    (void)op; (void)url; (void)path; (void)config; (void)args; (void)len; return TRUE;
}
BOOL SetSubprocessArguments(ThreadSafeSubprocessContext* context, const wchar_t* args) {
    (void)context; (void)args; return TRUE;
}
BOOL SetSubprocessWorkingDirectory(ThreadSafeSubprocessContext* context, const wchar_t* dir) {
    (void)context; (void)dir; return TRUE;
}
BOOL SetSubprocessTimeout(ThreadSafeSubprocessContext* context, DWORD timeoutMs) {
    (void)context; (void)timeoutMs; return TRUE;
}
BOOL ExecuteThreadSafeSubprocessWithOutput(ThreadSafeSubprocessContext* context) {
    (void)context; return TRUE;
}
BOOL WaitForThreadSafeSubprocessWithOutputCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs) {
    (void)context; (void)timeoutMs; return TRUE;
}

// Global flag to track if CancelThreadSafeSubprocess was called
static BOOL g_CancelThreadSafeSubprocessCalled = FALSE;
static ThreadSafeSubprocessContext* g_ExpectedContext = NULL;
static BOOL g_CancelResult = TRUE;

BOOL CancelThreadSafeSubprocess(ThreadSafeSubprocessContext* context) {
    g_CancelThreadSafeSubprocessCalled = TRUE;
    if (context == g_ExpectedContext) {
        return g_CancelResult;
    }
    return FALSE;
}

BOOL WaitForThreadSafeSubprocessCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs) {
    (void)context; (void)timeoutMs; return TRUE;
}
BOOL ForceKillThreadSafeSubprocess(ThreadSafeSubprocessContext* context) {
    (void)context; return TRUE;
}
BOOL GetFinalThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context, wchar_t** output, size_t* length, DWORD* exitCode) {
    (void)context; (void)output; (void)length; (void)exitCode; return TRUE;
}
wchar_t* CreateUserFriendlyYtDlpError(DWORD code, const wchar_t* output, const wchar_t* url) {
    (void)code; (void)output; (void)url; return NULL;
}
BOOL SetSubprocessProgressCallback(ThreadSafeSubprocessContext* context, ProgressCallback callback, void* userData) {
    (void)context; (void)callback; (void)userData; return TRUE;
}
BOOL SetSubprocessParentWindow(ThreadSafeSubprocessContext* context, HWND parentWindow) {
    (void)context; (void)parentWindow; return TRUE;
}
BOOL IsThreadSafeSubprocessRunning(ThreadSafeSubprocessContext* context) {
    (void)context; return TRUE;
}

// Include the implementation to test
#include "../subproc.c"

// Test cases for CancelLegacySubprocessExecution
void test_CancelLegacySubprocessExecution_NullContext() {
    printf("Test: CancelLegacySubprocessExecution(NULL)... ");
    BOOL result = CancelLegacySubprocessExecution(NULL);
    assert(result == FALSE);
    printf("Passed.\n");
}

void test_CancelLegacySubprocessExecution_NullThreadSafeContext() {
    printf("Test: CancelLegacySubprocessExecution with NULL threadSafeContext... ");
    SubprocessContext legacyContext = {0};
    legacyContext.threadSafeContext = NULL;

    BOOL result = CancelLegacySubprocessExecution(&legacyContext);
    assert(result == FALSE);
    printf("Passed.\n");
}

void test_CancelLegacySubprocessExecution_ValidContext() {
    printf("Test: CancelLegacySubprocessExecution with valid context... ");
    SubprocessContext legacyContext = {0};
    ThreadSafeSubprocessContext tsContext = {0};
    legacyContext.threadSafeContext = &tsContext;

    g_CancelThreadSafeSubprocessCalled = FALSE;
    g_ExpectedContext = &tsContext;
    g_CancelResult = TRUE;

    BOOL result = CancelLegacySubprocessExecution(&legacyContext);

    assert(g_CancelThreadSafeSubprocessCalled == TRUE);
    assert(result == TRUE);

    // Test with FALSE return from delegate
    g_CancelThreadSafeSubprocessCalled = FALSE;
    g_CancelResult = FALSE;
    result = CancelLegacySubprocessExecution(&legacyContext);
    assert(g_CancelThreadSafeSubprocessCalled == TRUE);
    assert(result == FALSE);

    printf("Passed.\n");
}

int main() {
    test_CancelLegacySubprocessExecution_NullContext();
    test_CancelLegacySubprocessExecution_NullThreadSafeContext();
    test_CancelLegacySubprocessExecution_ValidContext();

    printf("\nAll CancelLegacySubprocessExecution tests passed successfully!\n");
    return 0;
}
