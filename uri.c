#include "YouTubeCacher.h"

BOOL IsYouTubeURL(const wchar_t* url) {
    if (url == NULL) {
        return FALSE;
    }
    
    // Check if URL begins with any of the YouTube URL prefixes
    return (wcsncmp(url, L"https://www.youtube.com/watch", 29) == 0 ||
            wcsncmp(url, L"https://www.youtube.com/shorts/", 31) == 0 ||
            wcsncmp(url, L"https://youtu.be/", 17) == 0 ||
            wcsncmp(url, L"https://m.youtube.com/watch", 27) == 0 ||
            wcsncmp(url, L"https://m.youtube.com/shorts/", 29) == 0 ||
            wcsncmp(url, L"https://youtube.com/watch", 25) == 0 ||
            wcsncmp(url, L"https://youtube.com/shorts/", 27) == 0 ||
            wcsncmp(url, L"http://www.youtube.com/watch", 28) == 0 ||
            wcsncmp(url, L"http://www.youtube.com/shorts/", 30) == 0 ||
            wcsncmp(url, L"http://youtu.be/", 16) == 0 ||
            wcsncmp(url, L"http://m.youtube.com/watch", 26) == 0 ||
            wcsncmp(url, L"http://m.youtube.com/shorts/", 28) == 0 ||
            wcsncmp(url, L"http://youtube.com/watch", 24) == 0 ||
            wcsncmp(url, L"http://youtube.com/shorts/", 26) == 0);
}