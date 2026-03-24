#include <wchar.h>
#include <stdio.h>
int ParseDurationToSeconds(const wchar_t* duration) {
    if (!duration) return 0;
    
    int hours = 0, minutes = 0, seconds = 0;
    int parts = swscanf(duration, L"%d:%d:%d", &hours, &minutes, &seconds);
    
    if (parts == 3) {
        // HH:MM:SS format
        return hours * 3600 + minutes * 60 + seconds;
    } else if (parts == 2) {
        // MM:SS format (hours variable contains minutes, minutes variable contains seconds)
        return hours * 60 + minutes;
    }
    
    return 0;
}
