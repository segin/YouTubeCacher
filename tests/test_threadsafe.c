#include "mock_windows.h"
#include <wchar.h>

// Override windows.h to point to our mock
#define _WINDOWS_
#define _INC_WINDOWS

// Provide empty implementations to avoid missing dependencies
#define YouTubeCacher_h
#define YOUTUBECACHER_H

// Mock necessary functions
void DebugOutput(const wchar_t* msg) {
    wprintf(L"DEBUG: %ls\n", msg);
}

// Forward declarations to mock structs and functions for YouTubeCacher.h
typedef struct { int dummy; } ErrorHandler;
ErrorHandler g_ErrorHandler;
typedef struct { int dummy; } MemoryManager;
typedef struct { int dummy; } ApplicationState;
typedef void (*ProgressCallback)(int, const wchar_t*, void*);

ApplicationState* GetApplicationState() { return NULL; }
void AppendToYtDlpSessionLog(const wchar_t* msg) { (void)msg; }

#define SAFE_MALLOC malloc
#define SAFE_FREE free
#define SAFE_WCSDUP wcsdup
#define SAFE_REALLOC realloc

// Hack to skip original windows.h
#define THREADSAFE_H_INCLUDED
#define THREADSAFE_H

// Mock threadsafe.h definitions here to avoid including it and triggering windows.h
BOOL InitializeThreadSafety(void);
void CleanupThreadSafety(void);
void ThreadSafeDebugOutput(const wchar_t* message);
void ThreadSafeDebugOutputF(const wchar_t* format, ...);

// Define just the struct we need from threading.h for threadsafe.c
typedef struct {
    CRITICAL_SECTION processStateLock;
    CRITICAL_SECTION outputLock;
    CRITICAL_SECTION configLock;
    HANDLE cancellationEvent;

    wchar_t* executablePath;
    wchar_t* arguments;
    wchar_t* workingDirectory;

    wchar_t* outputBuffer;
    size_t outputBufferSize;
    size_t outputLength;
    BOOL outputComplete;

    DWORD timeoutMs;

    HANDLE hProcess;
    HANDLE hThread;
    DWORD processId;
    DWORD threadId;
    BOOL processRunning;
    BOOL processCompleted;
    DWORD exitCode;

    HANDLE hOutputRead;
    HANDLE hOutputWrite;

    BOOL initialized;
    BOOL cancellationRequested;

    ProgressCallback progressCallback;
    void* callbackUserData;
    void* parentWindow;
} ThreadSafeSubprocessContext;

// Forward declare functions from threadsafe.c to resolve implicit declaration warnings
BOOL CancelThreadSafeSubprocess(ThreadSafeSubprocessContext* context);
BOOL WaitForThreadSafeSubprocessCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs);
BOOL ForceKillThreadSafeSubprocess(ThreadSafeSubprocessContext* context);

// Mock external functions used in threadsafe.c
void* CreateEventW(void* attr, BOOL manualReset, BOOL initialState, const wchar_t* name) { (void)attr; (void)manualReset; (void)initialState; (void)name; return (void*)1; }
BOOL CloseHandle(HANDLE h) { (void)h; return TRUE; }
BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, HANDLE* lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) {
    (void)hSourceProcessHandle; (void)hSourceHandle; (void)hTargetProcessHandle; (void)lpTargetHandle; (void)dwDesiredAccess; (void)bInheritHandle; (void)dwOptions;
    return TRUE;
}
HANDLE GetCurrentProcess() { return (HANDLE)2; }
#define DUPLICATE_SAME_ACCESS 2
#define INVALID_HANDLE_VALUE ((void*)-1)
void OutputDebugStringW(const wchar_t* str) { (void)str; }
#define CREATE_NO_WINDOW 0x08000000
#define STARTF_USESTDHANDLES 0x00000100
BOOL CreatePipe(HANDLE* hReadPipe, HANDLE* hWritePipe, SECURITY_ATTRIBUTES* lpPipeAttributes, DWORD nSize) {
    (void)hReadPipe; (void)hWritePipe; (void)lpPipeAttributes; (void)nSize;
    return TRUE;
}
BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags) {
    (void)hObject; (void)dwMask; (void)dwFlags;
    return TRUE;
}
#define HANDLE_FLAG_INHERIT 0x00000001
BOOL CreateProcessW(const wchar_t* lpApplicationName, wchar_t* lpCommandLine, SECURITY_ATTRIBUTES* lpProcessAttributes, SECURITY_ATTRIBUTES* lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, void* lpEnvironment, const wchar_t* lpCurrentDirectory, STARTUPINFOW* lpStartupInfo, PROCESS_INFORMATION* lpProcessInformation) {
    (void)lpApplicationName; (void)lpCommandLine; (void)lpProcessAttributes; (void)lpThreadAttributes; (void)bInheritHandles; (void)dwCreationFlags; (void)lpEnvironment; (void)lpCurrentDirectory; (void)lpStartupInfo; (void)lpProcessInformation;
    return TRUE;
}
DWORD GetLastError() { return 0; }
BOOL GetExitCodeProcess(HANDLE hProcess, DWORD* lpExitCode) { (void)hProcess; (void)lpExitCode; return TRUE; }
#define STILL_ACTIVE 259
BOOL ResetEvent(HANDLE hEvent) { (void)hEvent; return TRUE; }
BOOL SetEvent(HANDLE hEvent) { (void)hEvent; return TRUE; }
BOOL GenerateConsoleCtrlEvent(DWORD dwCtrlEvent, DWORD dwProcessGroupId) { (void)dwCtrlEvent; (void)dwProcessGroupId; return TRUE; }
#define CTRL_C_EVENT 0
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) { (void)hHandle; (void)dwMilliseconds; return 0; }
#define WAIT_OBJECT_0 0
BOOL TerminateProcess(HANDLE hProcess, DWORD uExitCode) { (void)hProcess; (void)uExitCode; return TRUE; }
HANDLE CreateThread(SECURITY_ATTRIBUTES* lpThreadAttributes, size_t dwStackSize, void* lpStartAddress, void* lpParameter, DWORD dwCreationFlags, DWORD* lpThreadId) {
    (void)lpThreadAttributes; (void)dwStackSize; (void)lpStartAddress; (void)lpParameter; (void)dwCreationFlags; (void)lpThreadId;
    return (HANDLE)3;
}
BOOL PeekNamedPipe(HANDLE hNamedPipe, void* lpBuffer, DWORD nBufferSize, DWORD* lpBytesRead, DWORD* lpTotalBytesAvail, DWORD* lpBytesLeftThisMessage) {
    (void)hNamedPipe; (void)lpBuffer; (void)nBufferSize; (void)lpBytesRead; (void)lpTotalBytesAvail; (void)lpBytesLeftThisMessage;
    return TRUE;
}
#define ERROR_BROKEN_PIPE 109
void Sleep(DWORD dwMilliseconds) { (void)dwMilliseconds; }
BOOL ReadFile(HANDLE hFile, void* lpBuffer, DWORD nNumberOfBytesToRead, DWORD* lpNumberOfBytesRead, void* lpOverlapped) {
    (void)hFile; (void)lpBuffer; (void)nNumberOfBytesToRead; (void)lpNumberOfBytesRead; (void)lpOverlapped;
    return TRUE;
}
int MultiByteToWideChar(unsigned int CodePage, DWORD dwFlags, const char* lpMultiByteStr, int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar) {
    (void)CodePage; (void)dwFlags; (void)lpMultiByteStr; (void)cbMultiByte; (void)lpWideCharStr; (void)cchWideChar;
    return 0;
}
#define CP_UTF8 65001
DWORD GetTickCount() { return 0; }

// Dummy structures for legacy compatibility functions
typedef struct {
    int operation;
    const wchar_t* url;
    const wchar_t* outputPath;
} YtDlpRequest;

typedef struct {
    const wchar_t* ytDlpPath;
} YtDlpConfig;

typedef struct {
    BOOL success;
    DWORD exitCode;
    wchar_t* output;
    wchar_t* errorMessage;
} YtDlpResult;

typedef struct {
    CRITICAL_SECTION criticalSection;
    BOOL isRunning;
} ThreadContext;

typedef struct {
    YtDlpConfig* config;
    YtDlpRequest* request;
    YtDlpResult* result;
    ProgressCallback progressCallback;
    void* callbackUserData;
    void* parentWindow;
    ThreadContext threadContext;
    BOOL completed;
    DWORD completionTime;
} SubprocessContext;

BOOL GetYtDlpArgsForOperation(int op, const wchar_t* url, const wchar_t* out, YtDlpConfig* config, wchar_t* args, size_t max) {
    (void)op; (void)url; (void)out; (void)config; (void)args; (void)max;
    return TRUE;
}
wchar_t* CreateUserFriendlyYtDlpError(DWORD exitCode, const wchar_t* output, const wchar_t* url) {
    (void)exitCode; (void)output; (void)url;
    return NULL;
}

// Include the source file, which will use our mocked functions instead of including headers since we defined the guards
#include "../threadsafe.c"

int main() {
    printf("Starting tests...\n");

    // Initialize thread safety
    InitializeThreadSafety();

    // Test ThreadSafeDebugOutput with NULL message
    printf("Testing ThreadSafeDebugOutput(NULL)...\n");
    ThreadSafeDebugOutput(NULL);
    printf("ThreadSafeDebugOutput(NULL) passed (no crash).\n");

    // Test ThreadSafeDebugOutputF with NULL format
    printf("Testing ThreadSafeDebugOutputF(NULL)...\n");
    ThreadSafeDebugOutputF(NULL);
    printf("ThreadSafeDebugOutputF(NULL) passed (no crash).\n");

    // Test with actual message to ensure it still works
    printf("Testing ThreadSafeDebugOutput(L\"Test message\")...\n");
    ThreadSafeDebugOutput(L"Test message");

    printf("Testing ThreadSafeDebugOutputF(L\"Test format %%d\", 42)...\n");
    ThreadSafeDebugOutputF(L"Test format %d", 42);

    // Cleanup
    CleanupThreadSafety();

    printf("All tests passed!\n");
    return 0;
}
