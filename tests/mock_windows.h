#ifndef MOCK_WINDOWS_H
#define MOCK_WINDOWS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <wchar.h>
#include <wctype.h>
#include <stdarg.h>

#ifndef MOCK_WINDOWS_H_GUARD
#define MOCK_WINDOWS_H_GUARD

#define windows_h
#define _INC_WINDOWS

#define WINAPI
#define CALLBACK

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef uint64_t DWORDLONG;
typedef int32_t LONG;
typedef uint32_t UINT;
typedef void* HANDLE;
typedef void* HWND;
typedef wchar_t WCHAR;
typedef void* LPVOID;
typedef void* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef intptr_t INT_PTR;
typedef intptr_t LRESULT;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef void* HINSTANCE;
typedef void* HBRUSH;
typedef void* HMONITOR;
typedef void* HFONT;
typedef void* HICON;
typedef uint32_t COLORREF;

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct { long x; long y; } POINT;
typedef struct { long left; long top; long right; long bottom; } RECT;

typedef struct { DWORD LowPart; DWORD HighPart; } FILETIME;

typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  };
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } u;
  long long QuadPart;
} LARGE_INTEGER;

typedef struct tagLOGFONTW {
  LONG lfHeight;
  LONG lfWidth;
  LONG lfEscapement;
  LONG lfOrientation;
  LONG lfWeight;
  BYTE lfItalic;
  BYTE lfUnderline;
  BYTE lfStrikeOut;
  BYTE lfCharSet;
  BYTE lfOutPrecision;
  BYTE lfClipPrecision;
  BYTE lfQuality;
  BYTE lfPitchAndFamily;
  WCHAR lfFaceName[32];
} LOGFONTW;

#define MAX_PATH 260

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

#define GENERIC_READ 0x80000000
#define FILE_SHARE_READ 0x00000001
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x00000080

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

static inline BOOL CreateProcessW(LPCWSTR name, LPWSTR cmd, LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa, BOOL inherit, DWORD flags, LPVOID env, LPCWSTR dir, STARTUPINFOW* si, PROCESS_INFORMATION* pi) {
    (void)name; (void)cmd; (void)psa; (void)tsa; (void)inherit; (void)flags; (void)env; (void)dir; (void)si; (void)pi;
    return TRUE;
}

static inline HANDLE CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD creation, DWORD flags, HANDLE template) {
    (void)name; (void)access; (void)share; (void)sa; (void)creation; (void)flags; (void)template;
    return (HANDLE)1;
}

static inline BOOL GetFileTime(HANDLE h, FILETIME* creation, FILETIME* access, FILETIME* write) {
    (void)h; (void)creation; (void)access; (void)write;
    return TRUE;
}

static inline BOOL GetFileSizeEx(HANDLE h, LARGE_INTEGER* size) {
    (void)h;
    if (size) size->QuadPart = 0;
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

static inline int WideCharToMultiByte(uint32_t cp, DWORD flags, const wchar_t* src, int srclen, char* dst, int dstlen, const char* def, BOOL* used) {
    (void)cp; (void)flags; (void)src; (void)srclen; (void)dst; (void)dstlen; (void)def; (void)used;
    return 0;
}

// Memory mocks to avoid redefinition issues and link failures
#ifndef SAFE_MALLOC
#define SAFE_MALLOC(sz) malloc(sz)
#endif
#ifndef SAFE_FREE
#define SAFE_FREE(ptr) free(ptr)
#endif
#ifndef SAFE_REALLOC
#define SAFE_REALLOC(ptr, sz) realloc(ptr, sz)
#endif

#ifndef TEST_THREADSAFE_C
#define SafeMalloc(sz, file, line) malloc(sz)
#define SafeFree(ptr, file, line) free(ptr)
#define SafeRealloc(ptr, sz, file, line) realloc(ptr, sz)
#define SafeWcsDup(str, file, line) _wcsdup_mock(str)
#endif

static inline wchar_t* _wcsdup_mock(const wchar_t* str) {
    if (!str) return NULL;
    size_t len = wcslen(str);
    wchar_t* dup = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (dup) wcscpy(dup, str);
    return dup;
}

#ifndef SAFE_WCSDUP
#define SAFE_WCSDUP(str) _wcsdup_mock(str)
#endif

static inline int _wtoi(const wchar_t* str) {
    if (!str) return 0;
    return (int)wcstol(str, NULL, 10);
}

#define wcsicmp _wcsicmp
static inline int _wcsicmp(const wchar_t* s1, const wchar_t* s2) {
    if (!s1 || !s2) return s1 == s2 ? 0 : (s1 ? 1 : -1);
    while (*s1 && *s2) {
        wchar_t c1 = towlower(*s1);
        wchar_t c2 = towlower(*s2);
        if (c1 != c2) return (int)c1 - (int)c2;
        s1++;
        s2++;
    }
    return (int)towlower(*s1) - (int)towlower(*s2);
}

static inline int _wcsnicmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
    if (n == 0) return 0;
    if (!s1 || !s2) return s1 == s2 ? 0 : (s1 ? 1 : -1);
    while (n > 0) {
        wchar_t c1 = towlower(*s1);
        wchar_t c2 = towlower(*s2);
        if (c1 != c2) {
            return (int)c1 - (int)c2;
        }
        if (c1 == L'\0') break;
        s1++;
        s2++;
        n--;
    }
    return 0;
}

// Other mocks
#ifndef TEST_THREADSAFE_C
#define ThreadSafeDebugOutputF ThreadSafeDebugOutputF_mock
static inline void ThreadSafeDebugOutputF_mock(const wchar_t* format, ...) { (void)format; }
#endif

#define IsSubtitleFileExtension IsSubtitleFileExtension_mock
static inline BOOL IsSubtitleFileExtension_mock(const wchar_t* extension) { (void)extension; return 0; }

#endif // MOCK_WINDOWS_H_GUARD
#endif // MOCK_WINDOWS_H
