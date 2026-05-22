#define CreateProcessW MockCreateProcessW
#include "mock_windows.h"

// Define missing types
typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef void ErrorContext;

#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ACCESS_DENIED 5

#include "../cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock data
static wchar_t last_cmd_line[2048] = {0};
static CacheEntry mock_entry;

// Mock implementations
CacheEntry* FindCacheEntry(CacheManager* manager, const wchar_t* videoId) {
    (void)manager;
    if (wcscmp(videoId, L"test_id") == 0) {
        return &mock_entry;
    }
    return NULL;
}

#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
DWORD GetFileAttributesW(LPCWSTR lpFileName) {
    (void)lpFileName;
    return 0; // Success
}

// Mock CreateProcessW to capture cmdLine
BOOL MockCreateProcessW(LPCWSTR name, LPWSTR cmd, LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa, BOOL inherit, DWORD flags, LPVOID env, LPCWSTR dir, LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi) {
    (void)name; (void)psa; (void)tsa; (void)inherit; (void)flags; (void)env; (void)dir; (void)si; (void)pi;
    if (cmd) {
        wcscpy(last_cmd_line, cmd);
    }
    return TRUE;
}

// Mock error handling functions
typedef enum {
    YTC_ERROR_FILE_NOT_FOUND,
    YTC_SEVERITY_ERROR
} MockErrorType;

void* CREATE_ERROR_CONTEXT(int type, int severity) { (void)type; (void)severity; return NULL; }
void AddContextVariable(void* ctx, const wchar_t* name, const wchar_t* value) { (void)ctx; (void)name; (void)value; }
void SetUserFriendlyMessage(void* ctx, const wchar_t* msg) { (void)ctx; (void)msg; }
void FreeErrorContext(void* ctx) { (void)ctx; }

// Include required logic
wchar_t* EscapeCommandLineArgument(const wchar_t* arg) {
    if (!arg) return NULL;

    size_t len = wcslen(arg);
    // Rough estimate for escaped length: 2x original + quotes
    size_t escapedCapacity = (len * 2) + 3;
    wchar_t* escaped = (wchar_t*)SAFE_MALLOC(escapedCapacity * sizeof(wchar_t));
    if (!escaped) return NULL;

    size_t j = 0;
    escaped[j++] = L'\"';

    for (size_t i = 0; i < len; i++) {
        size_t backslashes = 0;
        while (i < len && arg[i] == L'\\') {
            backslashes++;
            i++;
        }

        if (i == len) {
            // End of string, escape all backslashes
            for (size_t k = 0; k < backslashes * 2; k++) {
                escaped[j++] = L'\\';
            }
            break;
        } else if (arg[i] == L'\"') {
            // Escape backslashes and the quote
            for (size_t k = 0; k < backslashes * 2 + 1; k++) {
                escaped[j++] = L'\\';
            }
            escaped[j++] = L'\"';
        } else {
            // Just the backslashes and the character
            for (size_t k = 0; k < backslashes; k++) {
                escaped[j++] = L'\\';
            }
            escaped[j++] = arg[i];
        }
    }

    escaped[j++] = L'\"';
    escaped[j] = L'\0';

    return escaped;
}

BOOL PlayCacheEntry(CacheManager* manager, const wchar_t* videoId, const wchar_t* playerPath) {
    if (!manager || !videoId || !playerPath) return FALSE;

    EnterCriticalSection(&manager->lock);

    CacheEntry* entry = FindCacheEntry(manager, videoId);
    if (!entry || !entry->mainVideoFile) {
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }

    // Check if video file still exists with enhanced error handling
    DWORD attributes = GetFileAttributesW(entry->mainVideoFile);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();

        // Report file access failure for playback
        ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_NOT_FOUND, YTC_SEVERITY_ERROR);
        if (ctx) {
            AddContextVariable(ctx, L"FilePath", entry->mainVideoFile);
            AddContextVariable(ctx, L"Operation", L"Check file for playback");
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
                SetUserFriendlyMessage(ctx, L"The video file no longer exists at the expected location.\r\nIt may have been moved, deleted, or the storage device may be disconnected.");
            } else if (error == ERROR_ACCESS_DENIED) {
                SetUserFriendlyMessage(ctx, L"Cannot access the video file due to insufficient permissions.\r\nPlease check file permissions or run as administrator.");
            } else {
                SetUserFriendlyMessage(ctx, L"Unable to access the video file due to a system error.\r\nThe storage device may have issues or be disconnected.");
            }
            FreeErrorContext(ctx);
        }

        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }

    // Escape arguments to prevent command injection
    wchar_t* escapedPlayerPath = EscapeCommandLineArgument(playerPath);
    wchar_t* escapedVideoFile = EscapeCommandLineArgument(entry->mainVideoFile);

    if (!escapedPlayerPath || !escapedVideoFile) {
        SAFE_FREE(escapedPlayerPath);
        SAFE_FREE(escapedVideoFile);
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }

    // Build command line to launch player
    size_t cmdLineLen = wcslen(escapedPlayerPath) + wcslen(escapedVideoFile) + 2;
    wchar_t* cmdLine = (wchar_t*)SAFE_MALLOC(cmdLineLen * sizeof(wchar_t));
    if (!cmdLine) {
        SAFE_FREE(escapedPlayerPath);
        SAFE_FREE(escapedVideoFile);
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }

    swprintf(cmdLine, cmdLineLen, L"%ls %ls", escapedPlayerPath, escapedVideoFile);

    SAFE_FREE(escapedPlayerPath);
    SAFE_FREE(escapedVideoFile);

    LeaveCriticalSection(&manager->lock);

    // Launch player
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};

    BOOL result = CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);

    if (result) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    SAFE_FREE(cmdLine);
    return result;
}


int main() {
    CacheManager manager = {0};
    InitializeCriticalSection(&manager.lock);

    // Case 1: Normal paths
    mock_entry.videoId = L"test_id";
    mock_entry.mainVideoFile = L"C:\\Videos\\video.mp4";
    wchar_t* playerPath = L"C:\\Program Files\\Player\\player.exe";

    printf("Test 1: Normal paths\n");
    PlayCacheEntry(&manager, L"test_id", playerPath);
    printf("Generated cmdLine: %ls\n", last_cmd_line);

    // Case 2: Path with spaces
    mock_entry.mainVideoFile = L"C:\\My Videos\\video 1.mp4";
    printf("\nTest 2: Path with spaces\n");
    PlayCacheEntry(&manager, L"test_id", playerPath);
    printf("Generated cmdLine: %ls\n", last_cmd_line);

    // Case 3: Path with double quotes (Vulnerability Fix Check)
    mock_entry.mainVideoFile = L"video.mp4\" & calc.exe & \"";
    printf("\nTest 3: Path with double quotes (Exploit attempt mitigation)\n");
    PlayCacheEntry(&manager, L"test_id", playerPath);
    printf("Generated cmdLine: %ls\n", last_cmd_line);

    return 0;
}
