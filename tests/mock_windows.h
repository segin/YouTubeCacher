#ifndef MOCK_WINDOWS_H
#define MOCK_WINDOWS_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Basic Windows Types
typedef int BOOL;
#define TRUE 1
#define FALSE 0
typedef uint32_t DWORD;
typedef void* LPVOID;
typedef void* HANDLE;
typedef void* HWND;
typedef uint16_t WORD;
typedef wchar_t WCHAR;
typedef void* HKEY;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* HICON;
typedef void* HFONT;
typedef void* HBRUSH;
typedef void* HMENU;
typedef void* HDC;
typedef void* HBITMAP;
typedef void* HGDIOBJ;

typedef struct _SYSTEMTIME {
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _CRITICAL_SECTION {
    int locked;
} CRITICAL_SECTION;

#define EnterCriticalSection(x) do { (x)->locked = 1; } while(0)
#define LeaveCriticalSection(x) do { (x)->locked = 0; } while(0)
#define InitializeCriticalSection(x) do { (x)->locked = 0; } while(0)
#define DeleteCriticalSection(x) do { (x)->locked = 0; } while(0)

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

static inline HANDLE CreateEventW(void* attr, BOOL manualReset, BOOL initialState, const wchar_t* name) {
    return (HANDLE)1;
}
static inline BOOL CloseHandle(HANDLE h) { return TRUE; }

// ThreadSafeSubprocessContext specific mock needs
#define WINAPI

// Replace other headers used by included files
#define YOUTUBECACHER_H
#define THREADING_H
#define MEMORY_H
#define ERROR_H

#define SAFE_MALLOC(s) malloc(s)
#define SAFE_FREE(p) do { if(p) { free(p); (p) = NULL; } } while(0)
#define SAFE_REALLOC(p, s) realloc(p, s)

static inline wchar_t* SafeWcsDup(const wchar_t* str) {
    if (!str) return NULL;
    size_t len = wcslen(str) + 1;
    wchar_t* dup = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (dup) wmemcpy(dup, str, len);
    return dup;
}
#define SAFE_WCSDUP(s) SafeWcsDup(s)

typedef void (*ProgressCallback)(int progress, const wchar_t* message, void* userData);

// Structs needed for threadsafe.c
typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION;

typedef struct {
    DWORD cb;
    DWORD dwFlags;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOW;

// We need ThreadSafeSubprocessContext definition
typedef struct {
    // Process handles and synchronization
    HANDLE hProcess;
    HANDLE hThread;
    DWORD processId;
    DWORD threadId;

    // Pipe handles for I/O redirection
    HANDLE hOutputRead;
    HANDLE hOutputWrite;

    // Process execution state
    CRITICAL_SECTION processStateLock;
    BOOL processRunning;
    BOOL processCompleted;
    DWORD exitCode;

    // Output collection
    CRITICAL_SECTION outputLock;
    wchar_t* outputBuffer;
    size_t outputBufferSize;
    size_t outputLength;
    BOOL outputComplete;

    // Configuration
    CRITICAL_SECTION configLock;
    wchar_t* executablePath;
    wchar_t* arguments;
    wchar_t* workingDirectory;
    DWORD timeoutMs;

    // Callbacks
    ProgressCallback progressCallback;
    void* callbackUserData;
    HWND parentWindow;

    // Cancellation support
    BOOL cancellationRequested;
    HANDLE cancellationEvent;

    // Initialization state
    BOOL initialized;
} ThreadSafeSubprocessContext;

#define GetCurrentThreadId() 1
#define GetCurrentProcess() ((HANDLE)-1)
#define DuplicateHandle(sP, sH, tP, tH, da, ih, o) (*(tH) = (sH), TRUE)

void DebugOutput(const wchar_t* msg) {}
void AppendToYtDlpSessionLog(const wchar_t* msg) {}

#define CreatePipe(r, w, a, s) (*(r) = (HANDLE)2, *(w) = (HANDLE)3, TRUE)
#define SetHandleInformation(h, m, f) TRUE

typedef struct {
    DWORD nLength;
    void* lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES;

#define STARTF_USESTDHANDLES 0x00000100
#define CREATE_NO_WINDOW 0x08000000

#define CreateProcessW(ap, cl, pa, ta, ih, cf, env, cd, si, pi) ((pi)->hProcess = (HANDLE)4, (pi)->hThread = (HANDLE)5, (pi)->dwProcessId = 1234, (pi)->dwThreadId = 5678, TRUE)

#define GetLastError() 0
#define GetLocalTime(st) do { (st)->wYear = 2024; } while(0)
#define GetTickCount() 0
#define STILL_ACTIVE 259
#define GetExitCodeProcess(h, ec) (*(ec) = STILL_ACTIVE, TRUE)
#define ResetEvent(h) TRUE
#define SetEvent(h) TRUE
#define GenerateConsoleCtrlEvent(ce, p) TRUE
#define CTRL_C_EVENT 0
#define WaitForSingleObject(h, t) 0
#define WAIT_OBJECT_0 0
#define TerminateProcess(h, ec) TRUE
#define PeekNamedPipe(h, b, bs, br, tba, bml) (*(tba) = 0, TRUE)
#define ERROR_BROKEN_PIPE 109
#define ReadFile(h, b, ntbr, br, o) (*(br) = 0, TRUE)
#define MultiByteToWideChar(cp, f, mb, mbc, wc, wcc) 0
#define CP_UTF8 65001
#define CreateThread(sa, ss, sa_fn, p, cf, tid) ((HANDLE)6)
#define Sleep(ms)

typedef struct {
    int errorCount;
} ErrorHandler;
ErrorHandler g_ErrorHandler = {0};

typedef struct ApplicationState ApplicationState;
typedef struct MemoryManager MemoryManager;
ApplicationState* GetApplicationState(void) { return NULL; }

// Dummy config to make things compile if needed
typedef struct {
    wchar_t* ytDlpPath;
} YtDlpConfig;

typedef struct {
    wchar_t* url;
    wchar_t* outputPath;
    int operation;
} YtDlpRequest;

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
    ProgressCallback progressCallback;
    void* callbackUserData;
    HWND parentWindow;
    BOOL completed;
    DWORD completionTime;
    YtDlpResult* result;
    ThreadContext threadContext;
} SubprocessContext;

#define GetYtDlpArgsForOperation(op, u, p, c, a, l) (wcscpy(a, L"args"), TRUE)
#define CreateUserFriendlyYtDlpError(ec, o, u) NULL

// Add any required declarations to build threadsafe.c
BOOL InitializeThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context);
void CleanupThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context);
BOOL SetSubprocessExecutable(ThreadSafeSubprocessContext* context, const wchar_t* path);

#endif // MOCK_WINDOWS_H

static inline void OutputDebugStringW(const wchar_t* msg) {}
BOOL CancelThreadSafeSubprocess(ThreadSafeSubprocessContext* context);
BOOL WaitForThreadSafeSubprocessCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs);
BOOL ForceKillThreadSafeSubprocess(ThreadSafeSubprocessContext* context);
