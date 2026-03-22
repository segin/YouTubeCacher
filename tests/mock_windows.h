#ifndef MOCK_WINDOWS_H
#define MOCK_WINDOWS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>

// Basic Windows types
typedef int BOOL;
typedef unsigned long DWORD;
typedef void* HANDLE;
typedef void* LPVOID;
typedef void* PVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;

#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL ((void*)0)
#endif

// Synchronization
typedef struct {
    int dummy;
} CRITICAL_SECTION;

#define WINAPI

static inline void InitializeCriticalSection(CRITICAL_SECTION* cs) { (void)cs; }
static inline void EnterCriticalSection(CRITICAL_SECTION* cs) { (void)cs; }
static inline void LeaveCriticalSection(CRITICAL_SECTION* cs) { (void)cs; }
static inline void DeleteCriticalSection(CRITICAL_SECTION* cs) { (void)cs; }
static inline DWORD GetCurrentThreadId() { return 1; }

// Process and Thread
typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES;

typedef struct _STARTUPINFOW {
    DWORD cb;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    short wShowWindow;
    short cbReserved2;
    unsigned char* lpReserved2;
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

// Time
typedef struct _SYSTEMTIME {
    short wYear;
    short wMonth;
    short wDayOfWeek;
    short wDay;
    short wHour;
    short wMinute;
    short wSecond;
    short wMilliseconds;
} SYSTEMTIME;

static inline void GetLocalTime(SYSTEMTIME* st) {
    st->wYear = 2023;
    st->wMonth = 1;
    st->wDay = 1;
    st->wHour = 12;
    st->wMinute = 0;
    st->wSecond = 0;
}

#endif
typedef void* HWND;
