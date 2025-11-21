#ifndef URI_H
#define URI_H

#include <windows.h>

// Function to check if a URL is a YouTube URL
BOOL IsYouTubeURL(const wchar_t* url);

// Function to check if input contains multiple URLs (space-separated)
BOOL ContainsMultipleURLs(const wchar_t* input);

// Function to check if a URL is a YouTube playlist URL
BOOL IsYouTubePlaylistURL(const wchar_t* url);

#endif // URI_H