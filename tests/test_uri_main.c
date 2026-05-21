#include "mock_defs.h"

// These will be extracted from uri_main.c and included via uri_main_functions.c
#include "uri_main_functions.c"

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

void test_is_youtube_url() {
    printf("Running IsYouTubeURL tests...\n");

    // Standard URLs
    assert_bool("Valid watch URL", TRUE, IsYouTubeURL(L"https://www.youtube.com/watch?v=abc"), L"https://www.youtube.com/watch?v=abc");
    assert_bool("Valid shorts URL", TRUE, IsYouTubeURL(L"https://www.youtube.com/shorts/abc"), L"https://www.youtube.com/shorts/abc");
    assert_bool("Valid playlist URL", TRUE, IsYouTubeURL(L"https://www.youtube.com/playlist?list=PL123"), L"https://www.youtube.com/playlist?list=PL123");
    assert_bool("Valid youtu.be URL", TRUE, IsYouTubeURL(L"https://youtu.be/abc"), L"https://youtu.be/abc");

    // Mobile URLs
    assert_bool("Valid mobile watch URL", TRUE, IsYouTubeURL(L"https://m.youtube.com/watch?v=abc"), L"https://m.youtube.com/watch?v=abc");
    assert_bool("Valid mobile shorts URL", TRUE, IsYouTubeURL(L"https://m.youtube.com/shorts/abc"), L"https://m.youtube.com/shorts/abc");

    // HTTP URLs
    assert_bool("Valid http watch URL", TRUE, IsYouTubeURL(L"http://www.youtube.com/watch?v=abc"), L"http://www.youtube.com/watch?v=abc");
    assert_bool("Valid http youtu.be URL", TRUE, IsYouTubeURL(L"http://youtu.be/abc"), L"http://youtu.be/abc");

    // Negative cases
    assert_bool("Non-YouTube URL", FALSE, IsYouTubeURL(L"https://example.com"), L"https://example.com");
    assert_bool("Incomplete URL", FALSE, IsYouTubeURL(L"https://youtube.com/"), L"https://youtube.com/");
    assert_bool("NULL input", FALSE, IsYouTubeURL(NULL), NULL);
    assert_bool("Empty string", FALSE, IsYouTubeURL(L""), L"");
}

void test_playlist_detection() {
    printf("Running IsYouTubePlaylistURL tests...\n");

    // Happy paths
    assert_bool("Valid playlist URL", TRUE, IsYouTubePlaylistURL(L"https://www.youtube.com/playlist?list=PL123"), L"https://www.youtube.com/playlist?list=PL123");
    assert_bool("Watch URL with list parameter", TRUE, IsYouTubePlaylistURL(L"https://www.youtube.com/watch?v=abc&list=PL123"), L"https://www.youtube.com/watch?v=abc&list=PL123");

    // Negative cases
    assert_bool("Single video watch URL", FALSE, IsYouTubePlaylistURL(L"https://www.youtube.com/watch?v=abc"), L"https://www.youtube.com/watch?v=abc");
    assert_bool("Non-YouTube URL with list=", FALSE, IsYouTubePlaylistURL(L"https://example.com/watch?list=PL123"), L"https://example.com/watch?list=PL123");

    // Error conditions
    assert_bool("NULL input", FALSE, IsYouTubePlaylistURL(NULL), NULL);
}

void test_multiple_urls_detection() {
    printf("Running ContainsMultipleURLs tests...\n");

    // Basic cases
    assert_bool("NULL input", FALSE, ContainsMultipleURLs(NULL), NULL);
    assert_bool("Single URL", FALSE, ContainsMultipleURLs(L"https://youtu.be/abc"), L"https://youtu.be/abc");

    // Positive cases
    assert_bool("Two URLs", TRUE, ContainsMultipleURLs(L"https://youtu.be/123 https://youtu.be/456"), L"https://youtu.be/123 https://youtu.be/456");
    assert_bool("Two URLs with leading space", TRUE, ContainsMultipleURLs(L" url1 url2"), L" url1 url2");
}

int main() {
    test_is_youtube_url();
    test_playlist_detection();
    test_multiple_urls_detection();

    printf("\nTests run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}
