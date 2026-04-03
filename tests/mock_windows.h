#ifndef MOCK_WINDOWS_H
#define MOCK_WINDOWS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <wchar.h>
#include <stdarg.h>

#define WINAPI
#define CALLBACK

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef uint64_t DWORDLONG;
typedef void* HANDLE;
typedef void* HWND;
typedef wchar_t WCHAR;
typedef void* LPVOID;
typedef void* LPWSTR;
typedef const wchar_t* LPCWSTR;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct _CRITICAL_SECTION {
    void* DebugInfo;
    long LockCount;
    long RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    uintptr_t SpinCount;
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;

static inline void InitializeCriticalSection(PCRITICAL_SECTION lpCriticalSection) {
    memset(lpCriticalSection, 0, sizeof(CRITICAL_SECTION));
}

static inline void DeleteCriticalSection(PCRITICAL_SECTION lpCriticalSection) {
    (void)lpCriticalSection;
}

static inline void EnterCriticalSection(PCRITICAL_SECTION lpCriticalSection) {
    (void)lpCriticalSection;
}

static inline void LeaveCriticalSection(PCRITICAL_SECTION lpCriticalSection) {
    (void)lpCriticalSection;
}

static inline DWORD GetCurrentThreadId(void) {
    return 1;
}

static inline void OutputDebugStringW(LPCWSTR msg) {
    (void)msg;
}

static inline HANDLE CreateEventW(void* sa, BOOL manual, BOOL initial, LPCWSTR name) {
    (void)sa; (void)manual; (void)initial; (void)name;
    return (HANDLE)1;
}

static inline BOOL CloseHandle(HANDLE h) {
    (void)h;
    return TRUE;
}

static inline BOOL ResetEvent(HANDLE h) {
    (void)h;
    return TRUE;
}

static inline BOOL SetEvent(HANDLE h) {
    (void)h;
    return TRUE;
}

static inline HANDLE GetCurrentProcess(void) {
    return (HANDLE)1;
}

static inline DWORD GetLastError(void) {
    return 0;
}

#define CTRL_C_EVENT 0
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define CREATE_NO_WINDOW 0x08000000
#define STARTF_USESTDHANDLES 0x00000100
#define DUPLICATE_SAME_ACCESS 0x00000002
#define CP_UTF8 65001
#define STILL_ACTIVE 259
#define WAIT_OBJECT_0 0
#define ERROR_BROKEN_PIPE 109

typedef struct _SECURITY_ATTRIBUTES {
    DWORD  nLength;
    LPVOID lpSecurityDescriptor;
    BOOL   bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _STARTUPINFOW {
    DWORD   cb;
    LPWSTR  lpReserved;
    LPWSTR  lpDesktop;
    LPWSTR  lpTitle;
    DWORD   dwX;
    DWORD   dwY;
    DWORD   dwXSize;
    DWORD   dwYSize;
    DWORD   dwXCountChars;
    DWORD   dwYCountChars;
    DWORD   dwFillAttribute;
    DWORD   dwFlags;
    WORD    wShowWindow;
    WORD    cbReserved2;
    BYTE    *lpReserved2;
    HANDLE  hStdInput;
    HANDLE  hStdOutput;
    HANDLE  hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

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

typedef DWORD (WINAPI *PTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

static inline HANDLE CreateThread(LPSECURITY_ATTRIBUTES sa, size_t stack, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, DWORD* tid) {
    (void)sa; (void)stack; (void)start; (void)param; (void)flags; (void)tid;
    return (HANDLE)1;
}

static inline void GetLocalTime(LPSYSTEMTIME st) {
    memset(st, 0, sizeof(SYSTEMTIME));
}

static inline void Sleep(DWORD ms) {
    (void)ms;
}

static inline DWORD GetTickCount(void) {
    return 0;
}

static inline BOOL CreateProcessW(LPCWSTR name, LPWSTR cmd, LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa, BOOL inherit, DWORD flags, LPVOID env, LPCWSTR dir, LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi) {
    (void)name; (void)cmd; (void)psa; (void)tsa; (void)inherit; (void)flags; (void)env; (void)dir; (void)si; (void)pi;
    return TRUE;
}

static inline BOOL GetExitCodeProcess(HANDLE h, DWORD* code) {
    (void)h;
    if (code) *code = 0;
    return TRUE;
}

static inline BOOL TerminateProcess(HANDLE h, DWORD code) {
    (void)h; (void)code;
    return TRUE;
}

static inline DWORD WaitForSingleObject(HANDLE h, DWORD ms) {
    (void)h; (void)ms;
    return WAIT_OBJECT_0;
}

static inline BOOL CreatePipe(HANDLE* read, HANDLE* write, LPSECURITY_ATTRIBUTES sa, DWORD size) {
    (void)read; (void)write; (void)sa; (void)size;
    return TRUE;
}

#define HANDLE_FLAG_INHERIT 0x00000001
static inline BOOL SetHandleInformation(HANDLE h, DWORD mask, DWORD flags) {
    (void)h; (void)mask; (void)flags;
    return TRUE;
}

static inline BOOL GenerateConsoleCtrlEvent(DWORD event, DWORD pid) {
    (void)event; (void)pid;
    return TRUE;
}

static inline BOOL DuplicateHandle(HANDLE hsrc, HANDLE hsh, HANDLE hdst, HANDLE* hth, DWORD access, BOOL inherit, DWORD options) {
    (void)hsrc; (void)hsh; (void)hdst; (void)hth; (void)access; (void)inherit; (void)options;
    return TRUE;
}

static inline BOOL PeekNamedPipe(HANDLE h, LPVOID buf, DWORD bufsz, DWORD* read, DWORD* avail, DWORD* message) {
    (void)h; (void)buf; (void)bufsz; (void)read; (void)avail; (void)message;
    return TRUE;
}

static inline BOOL ReadFile(HANDLE h, LPVOID buf, DWORD bufsz, DWORD* read, LPVOID overlap) {
    (void)h; (void)buf; (void)bufsz; (void)read; (void)overlap;
    return TRUE;
}

static inline int MultiByteToWideChar(uint32_t cp, DWORD flags, const char* src, int srclen, wchar_t* dst, int dstlen) {
    (void)cp; (void)flags; (void)src; (void)srclen; (void)dst; (void)dstlen;
    return 0;
}

// Memory macros
#define SAFE_MALLOC(sz) malloc(sz)
#define SAFE_FREE(ptr) free(ptr)
#define SAFE_REALLOC(ptr, sz) realloc(ptr, sz)

static inline wchar_t* SAFE_WCSDUP(const wchar_t* str) {
    if (!str) return NULL;
    size_t len = wcslen(str);
    wchar_t* dup = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (dup) wcscpy(dup, str);
    return dup;
}

#endif // MOCK_WINDOWS_H
