#ifndef MOCK_WINDOWS_H
#define MOCK_WINDOWS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <wchar.h>
#include <stdarg.h>

// Basic Windows Types
typedef int BOOL;
#define TRUE 1
#define FALSE 0

typedef uint32_t DWORD;
typedef void* HANDLE;
typedef void* HWND;
typedef uint16_t WORD;
typedef uint16_t WCHAR;
typedef void* LPVOID;
typedef void* LPSECURITY_ATTRIBUTES;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef unsigned int UINT;
typedef const wchar_t* LPCWSTR;
typedef void* HINSTANCE;
typedef void* HFONT;
typedef void* HICON;
typedef void* HBRUSH;
typedef void* HMONITOR;
typedef void* WNDPROC;
typedef void* LPTHREAD_START_ROUTINE;
typedef uint32_t COLORREF;

// Windows Constants
#define DUPLICATE_SAME_ACCESS 0x00000002
#define HANDLE_FLAG_INHERIT 0x00000001
#define STARTF_USESTDHANDLES 0x00000100
#define CREATE_NO_WINDOW 0x08000000
#define STILL_ACTIVE 259
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define CTRL_C_EVENT 0
#define WAIT_OBJECT_0 0
#define ERROR_BROKEN_PIPE 109
#define CP_UTF8 65001
#define MAX_PATH 260

// Calling Conventions
#define WINAPI
#define CALLBACK

// Windows Structs
typedef struct _SYSTEMTIME {
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME;

typedef struct _CRITICAL_SECTION {
    void* DebugInfo;
    long LockCount;
    long RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    size_t SpinCount;
} CRITICAL_SECTION;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES;

typedef struct _STARTUPINFOW {
    DWORD cb;
    char *lpReserved;
    char *lpDesktop;
    char *lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    char *lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOW;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

typedef struct _RECT {
    long left;
    long top;
    long right;
    long bottom;
} RECT;

typedef struct _POINT {
    long x;
    long y;
} POINT;

typedef struct _LOGFONTW {
    long lfHeight;
    long lfWidth;
    long lfEscapement;
    long lfOrientation;
    long lfWeight;
    unsigned char lfItalic;
    unsigned char lfUnderline;
    unsigned char lfStrikeOut;
    unsigned char lfCharSet;
    unsigned char lfOutPrecision;
    unsigned char lfClipPrecision;
    unsigned char lfQuality;
    unsigned char lfPitchAndFamily;
    wchar_t lfFaceName[32];
} LOGFONTW;

typedef INT_PTR (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);

// Synchronization macros
#define EnterCriticalSection(x) do { (void)(x); } while(0)
#define LeaveCriticalSection(x) do { (void)(x); } while(0)
#define InitializeCriticalSection(x) do { (void)(x); } while(0)
#define DeleteCriticalSection(x) do { (void)(x); } while(0)

// Function mocks
static inline HANDLE CreateEventW(void* lpEventAttributes, BOOL bManualReset, BOOL bInitialState, const wchar_t* lpName) {
    (void)lpEventAttributes; (void)bManualReset; (void)bInitialState; (void)lpName; return (HANDLE)1;
}
static inline BOOL CloseHandle(HANDLE hObject) { (void)hObject; return TRUE; }
static inline BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, HANDLE* lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) {
    (void)hSourceProcessHandle; (void)hSourceHandle; (void)hTargetProcessHandle; (void)lpTargetHandle; (void)dwDesiredAccess; (void)bInheritHandle; (void)dwOptions; return TRUE;
}
static inline HANDLE GetCurrentProcess(void) { return (HANDLE)1; }
// Don't mock GetCurrentThreadId here, define it in the test file
static inline void OutputDebugStringW(const wchar_t* lpOutputString) { (void)lpOutputString; }
static inline void GetLocalTime(SYSTEMTIME* lpSystemTime) { (void)lpSystemTime; }
static inline BOOL CreatePipe(HANDLE* hReadPipe, HANDLE* hWritePipe, SECURITY_ATTRIBUTES* lpPipeAttributes, DWORD nSize) {
    (void)lpPipeAttributes; (void)nSize; *hReadPipe=(HANDLE)1; *hWritePipe=(HANDLE)2; return TRUE;
}
static inline BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags) {
    (void)hObject; (void)dwMask; (void)dwFlags; return TRUE;
}
static inline BOOL CreateProcessW(const wchar_t* lpApplicationName, wchar_t* lpCommandLine, SECURITY_ATTRIBUTES* lpProcessAttributes, SECURITY_ATTRIBUTES* lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, const wchar_t* lpCurrentDirectory, STARTUPINFOW* lpStartupInfo, PROCESS_INFORMATION* lpProcessInformation) {
    (void)lpApplicationName; (void)lpCommandLine; (void)lpProcessAttributes; (void)lpThreadAttributes; (void)bInheritHandles; (void)dwCreationFlags; (void)lpEnvironment; (void)lpCurrentDirectory; (void)lpStartupInfo; (void)lpProcessInformation; return TRUE;
}
static inline DWORD GetLastError(void) { return 0; }
static inline BOOL GetExitCodeProcess(HANDLE hProcess, DWORD* lpExitCode) {
    (void)hProcess; *lpExitCode = 0; return TRUE;
}
static inline BOOL ResetEvent(HANDLE hEvent) { (void)hEvent; return TRUE; }
static inline BOOL SetEvent(HANDLE hEvent) { (void)hEvent; return TRUE; }
static inline BOOL GenerateConsoleCtrlEvent(DWORD dwCtrlEvent, DWORD dwProcessGroupId) {
    (void)dwCtrlEvent; (void)dwProcessGroupId; return TRUE;
}
static inline DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    (void)hHandle; (void)dwMilliseconds; return WAIT_OBJECT_0;
}
static inline BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode) {
    (void)hProcess; (void)uExitCode; return TRUE;
}
static inline BOOL PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize, DWORD* lpBytesRead, DWORD* lpTotalBytesAvail, DWORD* lpBytesLeftThisMessage) {
    (void)hNamedPipe; (void)lpBuffer; (void)nBufferSize; (void)lpBytesRead; (void)lpBytesLeftThisMessage; *lpTotalBytesAvail = 0; return TRUE;
}
static inline BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, DWORD* lpNumberOfBytesRead, void* lpOverlapped) {
    (void)hFile; (void)lpBuffer; (void)nNumberOfBytesToRead; (void)lpOverlapped; *lpNumberOfBytesRead = 0; return TRUE;
}
static inline int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, const char* lpMultiByteStr, int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar) {
    (void)CodePage; (void)dwFlags; (void)lpMultiByteStr; (void)cbMultiByte; (void)lpWideCharStr; (void)cchWideChar; return 0;
}
static inline DWORD GetTickCount(void) { return 0; }
static inline void Sleep(DWORD dwMilliseconds) { (void)dwMilliseconds; }
static inline HANDLE CreateThread(SECURITY_ATTRIBUTES* lpThreadAttributes, size_t dwStackSize, LPVOID lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, DWORD* lpThreadId) {
    (void)lpThreadAttributes; (void)dwStackSize; (void)lpStartAddress; (void)lpParameter; (void)dwCreationFlags; (void)lpThreadId; return (HANDLE)1;
}

#endif // MOCK_WINDOWS_H
