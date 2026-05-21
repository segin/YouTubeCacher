#include "YouTubeCacher.h"

BOOL IsYouTubeURL(const wchar_t* url) {
    if (url == NULL) {
        return FALSE;
    }

    // Skip protocol
    const wchar_t* domain = NULL;
    if (wcsncmp(url, L"https://", 8) == 0) {
        domain = url + 8;
    } else if (wcsncmp(url, L"http://", 7) == 0) {
        domain = url + 7;
    } else {
        return FALSE;
    }

    // Identify the end of the domain. Delimiters are /, ?, or #
    const wchar_t* path = wcschr(domain, L'/');
    const wchar_t* query = wcschr(domain, L'?');
    const wchar_t* fragment = wcschr(domain, L'#');

    const wchar_t* end = path;
    if (query != NULL && (end == NULL || query < end)) end = query;
    if (fragment != NULL && (end == NULL || fragment < end)) end = fragment;

    size_t domainLen = end ? (size_t)(end - domain) : wcslen(domain);

    // Common YouTube domains
    const wchar_t* validDomains[] = {
        L"www.youtube.com",
        L"youtube.com",
        L"m.youtube.com",
        L"music.youtube.com",
        L"youtu.be",
        L"www.youtube-nocookie.com",
        L"youtube-nocookie.com"
    };

    BOOL domainValid = FALSE;
    for (size_t i = 0; i < sizeof(validDomains) / sizeof(validDomains[0]); i++) {
        if (wcslen(validDomains[i]) == domainLen &&
            _wcsnicmp(domain, validDomains[i], domainLen) == 0) {
            domainValid = TRUE;
            break;
        }
    }

    return domainValid;
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
    if (url == NULL || !IsYouTubeURL(url)) {
        return FALSE;
    }
    
    // Check for playlist indicators in the URL
    // Playlists contain "list=" parameter, properly delimited by ? or &
    const wchar_t* listParam = wcsstr(url, L"?list=");
    if (listParam == NULL) {
        listParam = wcsstr(url, L"&list=");
    }

    if (listParam != NULL) {
        // Ensure the list parameter is not empty and not just followed by other params
        // listParam[6] is the character after 'list='
        wchar_t nextChar = listParam[6];
        if (nextChar != L'\0' && nextChar != L'&' && nextChar != L'#') {
            return TRUE;
        }
    }
    
    // Check for playlist-specific URL patterns
    const wchar_t* playlistPath = wcsstr(url, L"/playlist/");
    if (playlistPath != NULL) {
        // Ensure there is something after /playlist/
        // playlistPath[10] is the character after '/playlist/'
        wchar_t nextChar = playlistPath[10];
        if (nextChar != L'\0' && nextChar != L'?' && nextChar != L'#') {
            return TRUE;
        }
    }

    return FALSE;
}
