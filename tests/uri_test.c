#include <stdio.h>
#include <wchar.h>
#include <string.h>

// Mock Windows types
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL ((void*)0)
#endif

// Function prototype
BOOL ContainsMultipleURLs(const wchar_t* input);

int tests_run = 0;
int tests_failed = 0;

void assert_bool(const char* test_name, BOOL expected, BOOL actual) {
    tests_run++;
    if (expected != actual) {
        printf("FAIL: %s (Expected %s, got %s)\n", test_name,
               expected ? "TRUE" : "FALSE", actual ? "TRUE" : "FALSE");
        tests_failed++;
    } else {
        printf("PASS: %s\n", test_name);
    }
}

int main() {
    printf("Running ContainsMultipleURLs tests...\n");

    // NULL input
    assert_bool("NULL input", FALSE, ContainsMultipleURLs(NULL));

    // Empty string
    assert_bool("Empty string", FALSE, ContainsMultipleURLs(L""));

    // Only spaces
    assert_bool("Single space", FALSE, ContainsMultipleURLs(L" "));
    assert_bool("Multiple spaces", FALSE, ContainsMultipleURLs(L"   "));

    // Single URL
    assert_bool("Single URL", FALSE, ContainsMultipleURLs(L"https://www.youtube.com/watch?v=dQw4w9WgXcQ"));

    // Single URL with leading/trailing spaces
    assert_bool("Single URL leading space", FALSE, ContainsMultipleURLs(L" https://www.youtube.com/watch?v=dQw4w9WgXcQ"));
    assert_bool("Single URL trailing space", FALSE, ContainsMultipleURLs(L"https://www.youtube.com/watch?v=dQw4w9WgXcQ "));
    assert_bool("Single URL leading and trailing space", FALSE, ContainsMultipleURLs(L"  https://www.youtube.com/watch?v=dQw4w9WgXcQ  "));

    // Multiple URLs
    assert_bool("Two URLs", TRUE, ContainsMultipleURLs(L"https://youtu.be/123 https://youtu.be/456"));
    assert_bool("Two URLs with multiple spaces", TRUE, ContainsMultipleURLs(L"https://youtu.be/123   https://youtu.be/456"));
    assert_bool("Three URLs", TRUE, ContainsMultipleURLs(L"url1 url2 url3"));

    // Multiple URLs with leading/trailing spaces
    assert_bool("Multiple URLs with leading space", TRUE, ContainsMultipleURLs(L" url1 url2"));
    assert_bool("Multiple URLs with trailing space", TRUE, ContainsMultipleURLs(L"url1 url2 "));

    printf("\nTest Summary: %d run, %d failed\n", tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
