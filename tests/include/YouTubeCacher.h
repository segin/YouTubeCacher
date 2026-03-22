#ifndef YOUTUBE_CACHER_H_MOCK
#define YOUTUBE_CACHER_H_MOCK

#include "mock_windows.h"

typedef struct { int dummy; } ErrorHandler;
typedef struct { int dummy; } MemoryManager;
typedef struct { int dummy; } ApplicationState;
typedef enum { DUMMY_OP } YtDlpOperation;
typedef struct { int dummy; } YtDlpConfig;
typedef struct { int dummy; } YtDlpRequest;
typedef struct { int dummy; } YtDlpResult;
typedef void (*ProgressCallback)(int percentage, const wchar_t* status, void* userData);

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
    BOOL isRunning;
    CRITICAL_SECTION criticalSection;
} ThreadContext;

typedef struct {
    YtDlpConfig* config;
    YtDlpRequest* request;
    YtDlpResult* result;
    ProgressCallback progressCallback;
    void* callbackUserData;
    HWND parentWindow;
    BOOL completed;
    DWORD completionTime;
    ThreadContext threadContext;
} SubprocessContext;

extern ErrorHandler g_ErrorHandler;
ApplicationState* GetApplicationState(void);
void DebugOutput(const wchar_t* msg);
void AppendToYtDlpSessionLog(const wchar_t* message);
wchar_t* CreateUserFriendlyYtDlpError(DWORD exitCode, const wchar_t* output, const wchar_t* url);
BOOL GetYtDlpArgsForOperation(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath,
                              const YtDlpConfig* config, wchar_t* buffer, size_t bufferSize);

#define SAFE_MALLOC(s) SafeMalloc(s, __FILE__, __LINE__)
#define SAFE_FREE(p) SafeFree(&(p), __FILE__, __LINE__)
#define SAFE_REALLOC(p, s) SafeRealloc(p, s, __FILE__, __LINE__)
#define SAFE_WCSDUP(s) SafeWcsdup(s, __FILE__, __LINE__)
#define SafeWcsDup SafeWcsdup

void* SafeMalloc(size_t size, const char* file, int line);
void SafeFree(void* ptr, const char* file, int line);
void* SafeRealloc(void* ptr, size_t size, const char* file, int line);
wchar_t* SafeWcsdup(const wchar_t* str, const char* file, int line);

#endif
