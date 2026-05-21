#include "mock_windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Mock malloc to simulate failure
static int g_fail_malloc = 0;
void* mock_malloc(size_t size) {
    if (g_fail_malloc) return NULL;
    return malloc(size);
}

// Redefine SAFE_MALLOC and SAFE_WCSDUP for testing
#undef SAFE_MALLOC
#define SAFE_MALLOC(sz) mock_malloc(sz)

#undef SAFE_WCSDUP
wchar_t* mock_wcsdup(const wchar_t* str) {
    if (g_fail_malloc) return NULL;
    if (!str) return NULL;
    size_t len = wcslen(str);
    wchar_t* dup = (wchar_t*)mock_malloc((len + 1) * sizeof(wchar_t));
    if (dup) wcscpy(dup, str);
    return dup;
}
#define SAFE_WCSDUP(str) mock_wcsdup(str)

// Include the implementation directly
#include "uri_functions.c"

int main() {
    printf("Running NormalizeURL memory failure tests...\n");

    // Test 1: SAFE_WCSDUP failure for only spaces
    g_fail_malloc = 1;
    wchar_t* res1 = NormalizeURL(L"   ");
    if (res1 == NULL) {
        printf("PASS: NormalizeURL returned NULL on SAFE_WCSDUP failure\n");
    } else {
        printf("FAIL: NormalizeURL did not return NULL on SAFE_WCSDUP failure\n");
        free(res1);
        return 1;
    }

    // Test 2: SAFE_MALLOC failure for normal string
    g_fail_malloc = 1;
    wchar_t* res2 = NormalizeURL(L"https://youtube.com");
    if (res2 == NULL) {
        printf("PASS: NormalizeURL returned NULL on SAFE_MALLOC failure\n");
    } else {
        printf("FAIL: NormalizeURL did not return NULL on SAFE_MALLOC failure\n");
        free(res2);
        return 1;
    }

    // Test 3: Normal operation when malloc succeeds
    g_fail_malloc = 0;
    wchar_t* res3 = NormalizeURL(L"  url  ");
    if (res3 != NULL && wcscmp(res3, L"url") == 0) {
        printf("PASS: NormalizeURL works when malloc succeeds\n");
        free(res3);
    } else {
        printf("FAIL: NormalizeURL failed even when malloc succeeds\n");
        if (res3) free(res3);
        return 1;
    }

    printf("All memory failure tests passed!\n");
    return 0;
}
