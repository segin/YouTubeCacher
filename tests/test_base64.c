#include "windows_mock.h"
#include <stdio.h>
#include <assert.h>

// Define guard to prevent inclusion of real YouTubeCacher.h
#define YOUTUBECACHER_H

// Include the source file instead of header to access static functions if needed
#include "../base64.c"

void test_base64_decode_wide_happy_path() {
    printf("Running test_base64_decode_wide_happy_path...\n");

    // "Hello" in Base64 is "SGVsbG8="
    const wchar_t* input = L"SGVsbG8=";
    wchar_t* output = Base64DecodeWide(input);

    assert(output != NULL);
    assert(wcscmp(output, L"Hello") == 0);

    free(output);
    printf("Passed!\n");
}

void test_base64_decode_wide_null() {
    printf("Running test_base64_decode_wide_null...\n");

    wchar_t* output = Base64DecodeWide(NULL);
    assert(output == NULL);

    printf("Passed!\n");
}

void test_base64_decode_wide_empty() {
    printf("Running test_base64_decode_wide_empty...\n");

    // An empty Base64 string is just ""
    const wchar_t* input = L"";
    wchar_t* output = Base64DecodeWide(input);

    // After fix it should return NULL or empty consistently
    assert(output == NULL);

    printf("Passed!\n");
}

void test_base64_decode_wide_invalid() {
    printf("Running test_base64_decode_wide_invalid...\n");

    // Invalid length
    assert(Base64DecodeWide(L"ABC") == NULL);

    // Invalid characters
    assert(Base64DecodeWide(L"ABC$") == NULL);

    printf("Passed!\n");
}

void test_base64_round_trip() {
    printf("Running test_base64_round_trip...\n");

    const wchar_t* original = L"YouTube Cacher Test 123 !@#";
    wchar_t* encoded = Base64EncodeWide(original);
    assert(encoded != NULL);

    wchar_t* decoded = Base64DecodeWide(encoded);
    assert(decoded != NULL);
    assert(wcscmp(original, decoded) == 0);

    free(encoded);
    free(decoded);
    printf("Passed!\n");
}

int main() {
    test_base64_decode_wide_happy_path();
    test_base64_decode_wide_null();
    test_base64_decode_wide_empty();
    test_base64_decode_wide_invalid();
    test_base64_round_trip();

    printf("All Base64 tests passed!\n");
    return 0;
}
