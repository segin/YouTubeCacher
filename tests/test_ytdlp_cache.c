#include "mock_windows.h"

// These will be extracted from ytdlp.c and included via ytdlp_cache_logic.c
typedef struct {
    wchar_t* title;
    wchar_t* duration;
    wchar_t* id;
    BOOL success;
} VideoMetadata;

typedef struct {
    wchar_t* url;
    VideoMetadata metadata;
    BOOL isValid;
} CachedVideoMetadata;

// Forward declaration of the function under test
BOOL IsCachedMetadataValid(const CachedVideoMetadata* cached, const wchar_t* url);

int g_tests_run = 0;
int g_tests_failed = 0;

void assert_bool(const char* name, BOOL expected, BOOL actual) {
    g_tests_run++;
    if (expected != actual) {
        g_tests_failed++;
        printf("FAILED: %s\n  Expected: %s, Actual: %s\n",
               name, expected ? "TRUE" : "FALSE", actual ? "TRUE" : "FALSE");
    } else {
        printf("PASSED: %s\n", name);
    }
}

void test_is_cached_metadata_valid() {
    printf("Running IsCachedMetadataValid tests...\n");

    CachedVideoMetadata cached;
    wchar_t* test_url = L"https://www.youtube.com/watch?v=dQw4w9WgXcQ";
    wchar_t* other_url = L"https://www.youtube.com/watch?v=abc12345678";

    // 1. NULL cached pointer
    assert_bool("NULL cached pointer", FALSE, IsCachedMetadataValid(NULL, test_url));

    // 2. NULL url pointer
    memset(&cached, 0, sizeof(cached));
    assert_bool("NULL url pointer", FALSE, IsCachedMetadataValid(&cached, NULL));

    // 3. isValid is FALSE
    cached.isValid = FALSE;
    cached.url = test_url;
    assert_bool("isValid is FALSE", FALSE, IsCachedMetadataValid(&cached, test_url));

    // 4. cached->url is NULL
    cached.isValid = TRUE;
    cached.url = NULL;
    assert_bool("cached->url is NULL", FALSE, IsCachedMetadataValid(&cached, test_url));

    // 5. Matching URL
    cached.isValid = TRUE;
    cached.url = test_url;
    assert_bool("Matching URL", TRUE, IsCachedMetadataValid(&cached, test_url));

    // 6. Mismatched URL
    cached.isValid = TRUE;
    cached.url = test_url;
    assert_bool("Mismatched URL", FALSE, IsCachedMetadataValid(&cached, other_url));
}

int main() {
    test_is_cached_metadata_valid();

    printf("\nTests run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}

// Include the logic after main or use a separate file if needed by Makefile
#include "ytdlp_cache_logic.c"
