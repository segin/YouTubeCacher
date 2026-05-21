#include "mock_defs.h"

#undef SAFE_MALLOC
#define SAFE_MALLOC(sz) malloc(sz)
#undef SAFE_WCSDUP
static inline wchar_t* mock_wcsdup(const wchar_t* str) {
    if (!str) return NULL;
    size_t len = wcslen(str);
    wchar_t* dup = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (dup) wcscpy(dup, str);
    return dup;
}
#define SAFE_WCSDUP(str) mock_wcsdup(str)

// These will be extracted from uri.c and included via uri_functions.c
#include "uri_functions.c"

int g_tests_run = 0;
int g_tests_failed = 0;

void assert_bool(const char* name, BOOL expected, BOOL actual, const wchar_t* url) {
    g_tests_run++;
    if (expected != actual) {
        g_tests_failed++;
        printf("FAILED: %s\n  URL: %ls\n  Expected: %s, Actual: %s\n",
               name, url ? url : L"NULL", expected ? "TRUE" : "FALSE", actual ? "TRUE" : "FALSE");
    }
}

void test_playlist_detection() {
    printf("Running IsYouTubePlaylistURL tests...\n");

    // Happy paths
    assert_bool("Valid playlist URL", TRUE, IsYouTubePlaylistURL(L"https://www.youtube.com/playlist?list=PL38529C25393049F8"), L"https://www.youtube.com/playlist?list=PL38529C25393049F8");
    assert_bool("Watch URL with playlist", TRUE, IsYouTubePlaylistURL(L"https://www.youtube.com/watch?v=abc&list=PL123"), L"https://www.youtube.com/watch?v=abc&list=PL123");
    assert_bool("Watch URL with playlist (start)", TRUE, IsYouTubePlaylistURL(L"https://www.youtube.com/watch?list=PL123&v=abc"), L"https://www.youtube.com/watch?list=PL123&v=abc");
    assert_bool("Shorts with playlist", TRUE, IsYouTubePlaylistURL(L"https://www.youtube.com/shorts/abc?list=PL123"), L"https://www.youtube.com/shorts/abc?list=PL123");
    assert_bool("Mobile playlist", TRUE, IsYouTubePlaylistURL(L"https://m.youtube.com/playlist?list=PL123"), L"https://m.youtube.com/playlist?list=PL123");
    assert_bool("Playlist path format", TRUE, IsYouTubePlaylistURL(L"https://www.youtube.com/playlist/PL123"), L"https://www.youtube.com/playlist/PL123");

    // Negative cases
    assert_bool("Single video watch URL", FALSE, IsYouTubePlaylistURL(L"https://www.youtube.com/watch?v=abc"), L"https://www.youtube.com/watch?v=abc");
    assert_bool("Single shorts URL", FALSE, IsYouTubePlaylistURL(L"https://www.youtube.com/shorts/abc"), L"https://www.youtube.com/shorts/abc");
    assert_bool("Youtu.be single video", FALSE, IsYouTubePlaylistURL(L"https://youtu.be/abc"), L"https://youtu.be/abc");

    // Edge cases / Bugs
    assert_bool("URL with playlist= but no list=", FALSE, IsYouTubePlaylistURL(L"https://www.youtube.com/watch?v=abc&playlist=PL123"), L"https://www.youtube.com/watch?v=abc&playlist=PL123");
    assert_bool("URL with notlist=", FALSE, IsYouTubePlaylistURL(L"https://www.youtube.com/watch?v=abc&notlist=PL123"), L"https://www.youtube.com/watch?v=abc&notlist=PL123");
    assert_bool("Non-YouTube URL with list=", FALSE, IsYouTubePlaylistURL(L"https://example.com/watch?list=PL123"), L"https://example.com/watch?list=PL123");

    // Error conditions
    assert_bool("NULL input", FALSE, IsYouTubePlaylistURL(NULL), NULL);
    assert_bool("Empty string", FALSE, IsYouTubePlaylistURL(L""), L"");
}

void test_multiple_urls_detection() {
    printf("Running ContainsMultipleURLs tests...\n");

    // Basic cases
    assert_bool("NULL input", FALSE, ContainsMultipleURLs(NULL), NULL);
    assert_bool("Empty string", FALSE, ContainsMultipleURLs(L""), L"");
    assert_bool("Single space", FALSE, ContainsMultipleURLs(L" "), L" ");
    assert_bool("Single URL", FALSE, ContainsMultipleURLs(L"https://www.youtube.com/watch?v=abc"), L"https://www.youtube.com/watch?v=abc");

    // Positive cases
    assert_bool("Two URLs", TRUE, ContainsMultipleURLs(L"https://youtu.be/123 https://youtu.be/456"), L"https://youtu.be/123 https://youtu.be/456");
    assert_bool("Two URLs with extra spaces", TRUE, ContainsMultipleURLs(L"https://youtu.be/123   https://youtu.be/456"), L"https://youtu.be/123   https://youtu.be/456");
    assert_bool("Three tokens", TRUE, ContainsMultipleURLs(L"url1 url2 url3"), L"url1 url2 url3");

    // Mixed cases
    assert_bool("Leading space", TRUE, ContainsMultipleURLs(L" url1 url2"), L" url1 url2");
    assert_bool("Trailing space", TRUE, ContainsMultipleURLs(L"url1 url2 "), L"url1 url2 ");
}

void assert_str(const char* name, const wchar_t* expected, const wchar_t* actual) {
    g_tests_run++;
    if (expected == NULL || actual == NULL) {
        if (expected != actual) {
            g_tests_failed++;
            printf("FAILED: %s\n  Expected: %ls, Actual: %ls\n",
                   name, expected ? expected : L"NULL", actual ? actual : L"NULL");
        }
    } else if (wcscmp(expected, actual) != 0) {
        g_tests_failed++;
        printf("FAILED: %s\n  Expected: '%ls', Actual: '%ls'\n",
               name, expected, actual);
    }
}

void test_normalize_url() {
    printf("Running NormalizeURL tests...\n");

    // NULL input
    assert_str("NULL input", NULL, NormalizeURL(NULL));

    // Empty string
    wchar_t* res1 = NormalizeURL(L"");
    assert_str("Empty string", L"", res1);
    free(res1);

    // Only spaces
    wchar_t* res2 = NormalizeURL(L"   ");
    assert_str("Only spaces", L"", res2);
    free(res2);

    // No change
    wchar_t* res3 = NormalizeURL(L"https://youtube.com/watch?v=123");
    assert_str("No change", L"https://youtube.com/watch?v=123", res3);
    free(res3);

    // Leading spaces
    wchar_t* res4 = NormalizeURL(L"   https://youtube.com/watch?v=123");
    assert_str("Leading spaces", L"https://youtube.com/watch?v=123", res4);
    free(res4);

    // Trailing spaces
    wchar_t* res5 = NormalizeURL(L"https://youtube.com/watch?v=123   ");
    assert_str("Trailing spaces", L"https://youtube.com/watch?v=123", res5);
    free(res5);

    // Both leading and trailing spaces
    wchar_t* res6 = NormalizeURL(L"  https://youtube.com/watch?v=123  ");
    assert_str("Leading and trailing spaces", L"https://youtube.com/watch?v=123", res6);
    free(res6);
}

int main() {
    test_playlist_detection();
    test_multiple_urls_detection();
    test_normalize_url();

    printf("\nTests run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}
