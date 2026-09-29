// Tests for the registry readers in settings.c, run against a mocked registry
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <wchar.h>

typedef int BOOL;
typedef unsigned char BYTE;
typedef BYTE* LPBYTE;
typedef uint32_t DWORD;
typedef DWORD* LPDWORD;
typedef long LONG;
typedef void* HKEY;
typedef HKEY* PHKEY;
typedef const wchar_t* LPCWSTR;

#define TRUE 1
#define FALSE 0
#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_MORE_DATA 234L
#define REG_SZ 1
#define REG_DWORD 4
#define REG_BINARY 3
#define KEY_READ 0x20019
#define HKEY_CURRENT_USER ((HKEY)(uintptr_t)0x80000001)
#define REGISTRY_KEY L"Software\\Test"

// A single mocked registry value, returned for any value name
static BOOL g_valueExists;
static DWORD g_valueType;
static BYTE g_valueData[256];
static DWORD g_valueSize;

static void SetMockValue(DWORD type, const void* data, DWORD size) {
    assert(size <= sizeof(g_valueData));
    g_valueExists = TRUE;
    g_valueType = type;
    memcpy(g_valueData, data, size);
    g_valueSize = size;
}

static LONG RegOpenKeyExW(HKEY hKey, LPCWSTR subKey, DWORD options, DWORD sam, PHKEY result) {
    (void)hKey; (void)subKey; (void)options; (void)sam;
    *result = (HKEY)(uintptr_t)1;
    return ERROR_SUCCESS;
}

static LONG RegCloseKey(HKEY hKey) {
    (void)hKey;
    return ERROR_SUCCESS;
}

// Behaves like RegQueryValueExW: copies the stored bytes as-is, without adding a terminator
static LONG RegQueryValueExW(HKEY hKey, LPCWSTR valueName, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD dataSize) {
    (void)hKey; (void)valueName; (void)reserved;
    if (!g_valueExists) {
        return ERROR_FILE_NOT_FOUND;
    }
    if (type) {
        *type = g_valueType;
    }
    if (data && dataSize && *dataSize < g_valueSize) {
        *dataSize = g_valueSize;
        return ERROR_MORE_DATA;
    }
    if (data) {
        memcpy(data, g_valueData, g_valueSize);
    }
    if (dataSize) {
        *dataSize = g_valueSize;
    }
    return ERROR_SUCCESS;
}

#include "settings_registry_logic.c"

// Fill a buffer with non-zero garbage so a missing terminator is detectable
static void FillGarbage(wchar_t* buffer, size_t count) {
    for (size_t i = 0; i < count; i++) {
        buffer[i] = L'X';
    }
}

static void test_load_string_setting(void) {
    printf("Running LoadSettingFromRegistry tests...\n");

    wchar_t buffer[8];

    // Terminated value that fits
    SetMockValue(REG_SZ, L"abc", 4 * sizeof(wchar_t));
    FillGarbage(buffer, 8);
    assert(LoadSettingFromRegistry(L"v", buffer, 8));
    assert(wcscmp(buffer, L"abc") == 0);

    // Unterminated value shorter than the buffer gets a terminator
    SetMockValue(REG_SZ, L"abc", 3 * sizeof(wchar_t));
    FillGarbage(buffer, 8);
    assert(LoadSettingFromRegistry(L"v", buffer, 8));
    assert(wcscmp(buffer, L"abc") == 0);

    // Unterminated value exactly filling the buffer is rejected, and the buffer is cleared
    SetMockValue(REG_SZ, L"abcdefgh", 8 * sizeof(wchar_t));
    FillGarbage(buffer, 8);
    assert(!LoadSettingFromRegistry(L"v", buffer, 8));
    assert(buffer[0] == L'\0');

    // Terminated value exactly filling the buffer is accepted
    SetMockValue(REG_SZ, L"abcdefg", 8 * sizeof(wchar_t));
    FillGarbage(buffer, 8);
    assert(LoadSettingFromRegistry(L"v", buffer, 8));
    assert(wcscmp(buffer, L"abcdefg") == 0);

    // Value too large for the buffer
    SetMockValue(REG_SZ, L"abcdefghij", 11 * sizeof(wchar_t));
    FillGarbage(buffer, 8);
    assert(!LoadSettingFromRegistry(L"v", buffer, 8));
    assert(buffer[0] == L'\0');

    // Zero-length value reads as an empty string
    SetMockValue(REG_SZ, L"", 0);
    FillGarbage(buffer, 8);
    assert(LoadSettingFromRegistry(L"v", buffer, 8));
    assert(buffer[0] == L'\0');

    // Wrong type is rejected
    DWORD dw = 1;
    SetMockValue(REG_DWORD, &dw, sizeof(dw));
    FillGarbage(buffer, 8);
    assert(!LoadSettingFromRegistry(L"v", buffer, 8));
    assert(buffer[0] == L'\0');

    // Missing value
    g_valueExists = FALSE;
    FillGarbage(buffer, 8);
    assert(!LoadSettingFromRegistry(L"v", buffer, 8));
    assert(buffer[0] == L'\0');

    // Zero-size buffer is refused without writing
    SetMockValue(REG_SZ, L"abc", 4 * sizeof(wchar_t));
    FillGarbage(buffer, 8);
    assert(!LoadSettingFromRegistry(L"v", buffer, 0));
    assert(buffer[0] == L'X');

    printf("All LoadSettingFromRegistry tests passed!\n");
}

static void test_load_bool_setting(void) {
    printf("Running LoadBoolSettingFromRegistry tests...\n");

    DWORD dw;

    // REG_DWORD, the type the Settings dialog writes
    dw = 1;
    SetMockValue(REG_DWORD, &dw, sizeof(dw));
    assert(LoadBoolSettingFromRegistry(L"v", FALSE) == TRUE);
    dw = 0;
    SetMockValue(REG_DWORD, &dw, sizeof(dw));
    assert(LoadBoolSettingFromRegistry(L"v", TRUE) == FALSE);

    // REG_SZ "0"/"1" left by older versions
    SetMockValue(REG_SZ, L"1", 2 * sizeof(wchar_t));
    assert(LoadBoolSettingFromRegistry(L"v", FALSE) == TRUE);
    SetMockValue(REG_SZ, L"0", 2 * sizeof(wchar_t));
    assert(LoadBoolSettingFromRegistry(L"v", TRUE) == FALSE);

    // Unterminated REG_SZ "1"
    SetMockValue(REG_SZ, L"1", 1 * sizeof(wchar_t));
    assert(LoadBoolSettingFromRegistry(L"v", FALSE) == TRUE);

    // Missing value, oversized value and unexpected types fall back to the default
    g_valueExists = FALSE;
    assert(LoadBoolSettingFromRegistry(L"v", TRUE) == TRUE);
    assert(LoadBoolSettingFromRegistry(L"v", FALSE) == FALSE);
    SetMockValue(REG_SZ, L"11111111111111111111", 21 * sizeof(wchar_t));
    assert(LoadBoolSettingFromRegistry(L"v", TRUE) == TRUE);
    BYTE bin[2] = {1, 0};
    SetMockValue(REG_BINARY, bin, sizeof(bin));
    assert(LoadBoolSettingFromRegistry(L"v", TRUE) == TRUE);

    printf("All LoadBoolSettingFromRegistry tests passed!\n");
}

int main(void) {
    test_load_string_setting();
    test_load_bool_setting();
    return 0;
}
