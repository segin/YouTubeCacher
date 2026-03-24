#include "mock_defs.h"

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

int main() {
    test_playlist_detection();
    test_multiple_urls_detection();

    printf("\nTests run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}
