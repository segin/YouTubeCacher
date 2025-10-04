#include "uri.h"
#include <string.h>

BOOL IsYouTubeURL(const char* url) {
    if (url == NULL) {
        return FALSE;
    }
    
    return (strstr(url, "https://www.youtube.com/watch") != NULL ||
            strstr(url, "https://youtu.be/") != NULL ||
            strstr(url, "https://m.youtube.com/watch") != NULL ||
            strstr(url, "https://youtube.com/watch") != NULL);
}