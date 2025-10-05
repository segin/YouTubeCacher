#include "uri.h"
#include <wchar.h>

BOOL IsYouTubeURL(const wchar_t* url) {
    if (url == NULL) {
        return FALSE;
    }
    
    return (wcsstr(url, L"https://www.youtube.com/watch") != NULL ||
            wcsstr(url, L"https://youtu.be/") != NULL ||
            wcsstr(url, L"https://m.youtube.com/watch") != NULL ||
            wcsstr(url, L"https://youtube.com/watch") != NULL);
}