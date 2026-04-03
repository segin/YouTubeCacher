#ifndef WINDOWS_MOCK_H
#define WINDOWS_MOCK_H

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>

// Mock basic Windows types
typedef uint32_t DWORD;
typedef int BOOL;
typedef unsigned char BYTE;
typedef wchar_t WCHAR;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HMENU;
typedef void* HDC;
typedef void* HBRUSH;
typedef void* HFONT;
typedef void* HICON;
typedef void* HMONITOR;
typedef uint32_t UINT;
typedef uintptr_t WPARAM;
typedef uintptr_t LPARAM;
typedef intptr_t LRESULT;
typedef intptr_t INT_PTR;
typedef uint32_t COLORREF;
typedef const wchar_t* LPCWSTR;
typedef void* LPVOID;
typedef void* (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);

typedef struct { int x; int y; } POINT;
typedef struct { int left; int top; int right; int bottom; } RECT;
typedef struct { int dummy; } SYSTEMTIME;
typedef struct { int dummy; } CRITICAL_SECTION;
typedef struct { int dummy; } FILETIME;
typedef struct { int dummy; } LOGFONTW;

#define CALLBACK
#define WINAPI
#define TRUE 1
#define FALSE 0
#define CP_UTF8 65001
#define MAX_PATH 260

// Mock memory macros for base64.c
#define SAFE_MALLOC(size) malloc(size)
#define SAFE_FREE(ptr) free(ptr)
#define SAFE_REALLOC(ptr, size) realloc(ptr, size)

static inline wchar_t* wcsdup_mock(const wchar_t* str) {
    if (!str) return NULL;
    size_t len = wcslen(str) + 1;
    wchar_t* dup = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (dup) wcscpy(dup, str);
    return dup;
}

#define SAFE_WCSDUP(str) wcsdup_mock(str)

// Mock Windows functions for UTF-8 conversion
static inline int WideCharToMultiByte(uint32_t codePage, uint32_t flags, const wchar_t* lpWideCharStr, int cchWideChar, char* lpMultiByteStr, int cbMultiByte, const char* lpDefaultChar, BOOL* lpUsedDefaultChar) {
    (void)codePage; (void)flags; (void)lpDefaultChar; (void)lpUsedDefaultChar;
    if (cchWideChar == -1) cchWideChar = wcslen(lpWideCharStr) + 1;
    if (cbMultiByte == 0) return cchWideChar;
    for (int i = 0; i < cchWideChar && i < cbMultiByte; i++) {
        lpMultiByteStr[i] = (char)lpWideCharStr[i];
    }
    return cchWideChar;
}

static inline int MultiByteToWideChar(uint32_t codePage, uint32_t flags, const char* lpMultiByteStr, int cbMultiByte, wchar_t* lpWideCharStr, int cchWideChar) {
    (void)codePage; (void)flags;
    if (cbMultiByte == -1) cbMultiByte = strlen(lpMultiByteStr) + 1;
    if (cchWideChar == 0) return cbMultiByte;
    for (int i = 0; i < cbMultiByte && i < cchWideChar; i++) {
        lpWideCharStr[i] = (wchar_t)(unsigned char)lpMultiByteStr[i];
    }
    return cbMultiByte;
}

#endif // WINDOWS_MOCK_H
