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

BOOL ContainsMultipleURLs(const wchar_t* input) {
    if (input == NULL) {
        return FALSE;
    }
    
    // Check if input contains spaces, which would indicate multiple URLs
    // We look for space characters that are not at the beginning or end
    const wchar_t* ptr = input;
    BOOL foundNonSpace = FALSE;
    
    while (*ptr) {
        if (*ptr == L' ') {
            // If we've found non-space characters before this space,
            // and there are non-space characters after it, we have multiple items
            if (foundNonSpace) {
                const wchar_t* nextPtr = ptr + 1;
                // Skip any additional spaces
                while (*nextPtr == L' ') {
                    nextPtr++;
                }
                // If there's content after the space(s), we have multiple URLs
                if (*nextPtr != L'\0') {
                    return TRUE;
                }
            }
        } else {
            foundNonSpace = TRUE;
        }
        ptr++;
    }
    
    return FALSE;
}

BOOL IsYouTubePlaylistURL(const wchar_t* url) {
    if (url == NULL) {
        return FALSE;
    }
    
    // Check for playlist indicators in the URL
    // Playlists contain "list=" parameter
    if (wcsstr(url, L"list=") != NULL) {
        return TRUE;
    }
    
    // Check for playlist-specific URL patterns
    if (wcsstr(url, L"/playlist?") != NULL) {
        return TRUE;
    }
    
    return FALSE;
}