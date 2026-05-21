#include "mock_windows.h"
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
void FormatDuration(wchar_t* duration, size_t bufferSize) {
    if (!duration || bufferSize == 0) return;

    // Remove any whitespace
    wchar_t* src = duration;
    wchar_t* dst = duration;
    while (*src) {
        if (!iswspace(*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = L'\0';
    size_t len = wcslen(duration);

    // If empty, leave as is
    if (len == 0) return;

    // Check if it's already in time format (contains colon)
    if (wcschr(duration, L':') != NULL) {
        // Parse existing time format and reformat with proper leading zeros
        wchar_t* firstColon = wcschr(duration, L':');
        wchar_t* secondColon = wcschr(firstColon + 1, L':');

        if (secondColon != NULL) {
            // Format: H:M:S or HH:MM:SS - parse and reformat
            int hours = 0, minutes = 0, seconds = 0;
            if (swscanf(duration, L"%d:%d:%d", &hours, &minutes, &seconds) == 3) {
                wchar_t temp[32];
                swprintf(temp, 32, L"%02d:%02d:%02d", hours, minutes, seconds);
                wcsncpy(duration, temp, bufferSize - 1);
                duration[bufferSize - 1] = L'\0';
            }
        } else {
            // Format: M:S or MM:SS - parse and reformat
            int minutes = 0, seconds = 0;
            if (swscanf(duration, L"%d:%d", &minutes, &seconds) == 2) {
                wchar_t temp[32];
                swprintf(temp, 32, L"%02d:%02d", minutes, seconds);
                wcsncpy(duration, temp, bufferSize - 1);
                duration[bufferSize - 1] = L'\0';
            }
        }
        return;
    }

    // Check if it's a pure number (seconds)
    BOOL isNumber = TRUE;
    for (size_t i = 0; i < len; i++) {
        if (!iswdigit(duration[i])) {
            isNumber = FALSE;
            break;
        }
    }

    if (isNumber) {
        int totalSeconds = _wtoi(duration);

        if (totalSeconds < 0) {
            // Invalid number, leave as is
            return;
        }

        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;

        wchar_t formatted[32];

        if (hours > 0) {
            // Format as HH:MM:SS (leading zeros for all segments, hours can go past 99)
            swprintf(formatted, 32, L"%02d:%02d:%02d", hours, minutes, seconds);
        } else {
            // Format as MM:SS (leading zeros for minutes and seconds)
            swprintf(formatted, 32, L"%02d:%02d", minutes, seconds);
        }

        wcsncpy(duration, formatted, bufferSize - 1);
        duration[bufferSize - 1] = L'\0';
    } else {
        // Not a pure number, check for single digit case
        if (len == 1 && iswdigit(duration[0])) {
            // Single digit - treat as seconds and format as "00:0X"
            wchar_t temp[32];
            swprintf(temp, 32, L"00:0%lc", (wint_t)duration[0]);
            wcsncpy(duration, temp, bufferSize - 1);
            duration[bufferSize - 1] = L'\0';
        }
        // For other non-numeric formats, leave as is
    }
}
