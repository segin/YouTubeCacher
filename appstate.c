#include "YouTubeCacher.h"

// Global application state instance
static ApplicationState g_appState = {0};

// State change callback structure
typedef struct StateChangeCallbackNode {
    StateChangeCallback callback;
    void* userData;
    struct StateChangeCallbackNode* next;
} StateChangeCallbackNode;

static StateChangeCallbackNode* g_callbackList = NULL;

// Initialize the application state
BOOL InitializeApplicationState(ApplicationState* state) {
    if (!state) return FALSE;
    
    // Clear the structure
    memset(state, 0, sizeof(ApplicationState));
    
    // Initialize critical section for thread safety
    InitializeCriticalSection(&state->stateLock);
    
    // Initialize UI state flags
    state->isDownloading = FALSE;
    state->programmaticChange = FALSE;
    state->manualPaste = FALSE;
    state->downloadAfterInfo = FALSE;
    
    // Initialize configuration state with defaults
    state->enableDebug = FALSE;
    state->enableLogfile = FALSE;
    state->enableAutopaste = TRUE;  // Default to enabled
    
    // Create UI brushes for text field colors
    state->hBrushWhite = CreateSolidBrush(COLOR_WHITE);
    state->hBrushLightGreen = CreateSolidBrush(COLOR_LIGHT_GREEN);
    state->hBrushLightBlue = CreateSolidBrush(COLOR_LIGHT_BLUE);
    state->hBrushLightTeal = CreateSolidBrush(COLOR_LIGHT_TEAL);
    state->hCurrentBrush = state->hBrushWhite;  // Default to white
    
    // Allocate and initialize cache manager
    state->cacheManager = (CacheManager*)SAFE_MALLOC(sizeof(CacheManager));
    if (state->cacheManager) {
        memset(state->cacheManager, 0, sizeof(CacheManager));
        // Note: Debug output deferred until logging system is initialized
    } else {
        // Critical error - use direct output since logging may not be ready
        OutputDebugStringW(L"YouTubeCacher: InitializeApplicationState - ERROR: Failed to allocate cache manager");
    }
    
    // Allocate and initialize cached video metadata
    state->cachedVideoMetadata = (CachedVideoMetadata*)SAFE_MALLOC(sizeof(CachedVideoMetadata));
    if (state->cachedVideoMetadata) {
        memset(state->cachedVideoMetadata, 0, sizeof(CachedVideoMetadata));
        // Note: Debug output deferred until logging system is initialized
    } else {
        // Critical error - use direct output since logging may not be ready
        OutputDebugStringW(L"YouTubeCacher: InitializeApplicationState - ERROR: Failed to allocate cached video metadata");
    }
    
    // Initialize download tracking
    state->isDownloadActive = FALSE;
    state->hDownloadProcess = NULL;
    state->downloadProcessId = 0;
    state->downloadTempDir[0] = L'\0';
    state->downloadCancelled = FALSE;
    
    // Initialize window procedure pointer
    state->originalTextFieldProc = NULL;
    
    // Initialize command line URL as empty
    state->cmdLineURL[0] = L'\0';
    
    // Initialize yt-dlp output buffer
    state->ytdlpOutputBufferSize = 64 * 1024; // 64KB initial size
    state->ytdlpOutputBuffer = (wchar_t*)SAFE_MALLOC(state->ytdlpOutputBufferSize * sizeof(wchar_t));
    if (state->ytdlpOutputBuffer) {
        state->ytdlpOutputBuffer[0] = L'\0';
        InitializeCriticalSection(&state->ytdlpOutputLock);
    } else {
        OutputDebugStringW(L"YouTubeCacher: InitializeApplicationState - ERROR: Failed to allocate yt-dlp output buffer");
        state->ytdlpOutputBufferSize = 0;
    }
    
    // Initialize yt-dlp session logs (in-memory only, separate from disk logging)
    state->ytdlpSessionLogAllSize = 256 * 1024; // 256KB initial size for all logs
    state->ytdlpSessionLogAll = (wchar_t*)SAFE_MALLOC(state->ytdlpSessionLogAllSize * sizeof(wchar_t));
    state->ytdlpSessionLogLastSize = 64 * 1024; // 64KB initial size for last run
    state->ytdlpSessionLogLast = (wchar_t*)SAFE_MALLOC(state->ytdlpSessionLogLastSize * sizeof(wchar_t));
    
    if (state->ytdlpSessionLogAll && state->ytdlpSessionLogLast) {
        state->ytdlpSessionLogAll[0] = L'\0';
        state->ytdlpSessionLogLast[0] = L'\0';
        InitializeCriticalSection(&state->ytdlpSessionLogLock);
    } else {
        OutputDebugStringW(L"YouTubeCacher: InitializeApplicationState - ERROR: Failed to allocate yt-dlp session log buffers");
        if (state->ytdlpSessionLogAll) {
            SAFE_FREE(state->ytdlpSessionLogAll);
            state->ytdlpSessionLogAll = NULL;
        }
        if (state->ytdlpSessionLogLast) {
            SAFE_FREE(state->ytdlpSessionLogLast);
            state->ytdlpSessionLogLast = NULL;
        }
        state->ytdlpSessionLogAllSize = 0;
        state->ytdlpSessionLogLastSize = 0;
    }
    
    // Mark as initialized
    state->isInitialized = TRUE;
    
    return TRUE;
}

// Clean up the application state
void CleanupApplicationState(ApplicationState* state) {
    if (!state || !state->isInitialized) return;
    
    // Enter critical section for cleanup
    EnterCriticalSection(&state->stateLock);
    
    // Clean up UI brushes
    if (state->hBrushWhite) {
        DeleteObject(state->hBrushWhite);
        state->hBrushWhite = NULL;
    }
    if (state->hBrushLightGreen) {
        DeleteObject(state->hBrushLightGreen);
        state->hBrushLightGreen = NULL;
    }
    if (state->hBrushLightBlue) {
        DeleteObject(state->hBrushLightBlue);
        state->hBrushLightBlue = NULL;
    }
    if (state->hBrushLightTeal) {
        DeleteObject(state->hBrushLightTeal);
        state->hBrushLightTeal = NULL;
    }
    state->hCurrentBrush = NULL;
    
    // Clean up cache manager
    if (state->cacheManager) {
        // Use direct debug output since logging system may be shutting down
        OutputDebugStringW(L"YouTubeCacher: CleanupApplicationState - Cleaning up cache manager");
        CleanupCacheManager(state->cacheManager);
        SAFE_FREE(state->cacheManager);
        state->cacheManager = NULL;
    }
    
    // Clean up cached video metadata
    if (state->cachedVideoMetadata) {
        // Use direct debug output since logging system may be shutting down
        OutputDebugStringW(L"YouTubeCacher: CleanupApplicationState - Cleaning up cached video metadata");
        FreeCachedMetadata(state->cachedVideoMetadata);
        SAFE_FREE(state->cachedVideoMetadata);
        state->cachedVideoMetadata = NULL;
    }
    
    // Clean up yt-dlp output buffer
    if (state->ytdlpOutputBuffer) {
        OutputDebugStringW(L"YouTubeCacher: CleanupApplicationState - Cleaning up yt-dlp output buffer");
        SAFE_FREE(state->ytdlpOutputBuffer);
        state->ytdlpOutputBuffer = NULL;
        state->ytdlpOutputBufferSize = 0;
        DeleteCriticalSection(&state->ytdlpOutputLock);
    }
    
    // Clean up yt-dlp session logs
    if (state->ytdlpSessionLogAll || state->ytdlpSessionLogLast) {
        OutputDebugStringW(L"YouTubeCacher: CleanupApplicationState - Cleaning up yt-dlp session logs");
        if (state->ytdlpSessionLogAll) {
            SAFE_FREE(state->ytdlpSessionLogAll);
            state->ytdlpSessionLogAll = NULL;
            state->ytdlpSessionLogAllSize = 0;
        }
        if (state->ytdlpSessionLogLast) {
            SAFE_FREE(state->ytdlpSessionLogLast);
            state->ytdlpSessionLogLast = NULL;
            state->ytdlpSessionLogLastSize = 0;
        }
        DeleteCriticalSection(&state->ytdlpSessionLogLock);
    }
    
    // Mark as uninitialized
    state->isInitialized = FALSE;
    
    // Leave critical section before deleting it
    LeaveCriticalSection(&state->stateLock);
    DeleteCriticalSection(&state->stateLock);
}

// Get the global application state instance
ApplicationState* GetApplicationState(void) {
    // Initialize on first access if not already initialized
    if (!g_appState.isInitialized) {
        InitializeApplicationState(&g_appState);
    }
    return &g_appState;
}

// Thread-safe downloading state functions
BOOL SetDownloadingState(BOOL isDownloading) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    EnterCriticalSection(&state->stateLock);
    BOOL oldValue = state->isDownloading;
    state->isDownloading = isDownloading;
    LeaveCriticalSection(&state->stateLock);
    
    // Notify state change
    if (oldValue != isDownloading) {
        NotifyStateChange("isDownloading", &isDownloading);
    }
    
    return TRUE;
}

BOOL GetDownloadingState(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    EnterCriticalSection(&state->stateLock);
    BOOL result = state->isDownloading;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// Thread-safe programmatic change flag functions
void SetProgrammaticChangeFlag(BOOL flag) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    state->programmaticChange = flag;
    LeaveCriticalSection(&state->stateLock);
    
    NotifyStateChange("programmaticChange", &flag);
}

BOOL GetProgrammaticChangeFlag(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    EnterCriticalSection(&state->stateLock);
    BOOL result = state->programmaticChange;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// Thread-safe manual paste flag functions
void SetManualPasteFlag(BOOL flag) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    state->manualPaste = flag;
    LeaveCriticalSection(&state->stateLock);
    
    NotifyStateChange("manualPaste", &flag);
}

BOOL GetManualPasteFlag(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    EnterCriticalSection(&state->stateLock);
    BOOL result = state->manualPaste;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// Thread-safe debug state functions
void SetDebugState(BOOL enableDebug, BOOL enableLogfile) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    BOOL oldDebug = state->enableDebug;
    BOOL oldLogfile = state->enableLogfile;
    state->enableDebug = enableDebug;
    state->enableLogfile = enableLogfile;
    LeaveCriticalSection(&state->stateLock);
    
    // Notify state changes
    if (oldDebug != enableDebug) {
        NotifyStateChange("enableDebug", &enableDebug);
    }
    if (oldLogfile != enableLogfile) {
        NotifyStateChange("enableLogfile", &enableLogfile);
    }
}

void GetDebugState(BOOL* enableDebug, BOOL* enableLogfile) {
    if (!enableDebug || !enableLogfile) return;
    
    // Check if state is initialized without triggering initialization
    if (!g_appState.isInitialized) {
        // Return default values during early initialization
        *enableDebug = FALSE;
        *enableLogfile = FALSE;
        return;
    }
    
    EnterCriticalSection(&g_appState.stateLock);
    *enableDebug = g_appState.enableDebug;
    *enableLogfile = g_appState.enableLogfile;
    LeaveCriticalSection(&g_appState.stateLock);
}

// Thread-safe autopaste state functions
void SetAutopasteState(BOOL enableAutopaste) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    BOOL oldValue = state->enableAutopaste;
    state->enableAutopaste = enableAutopaste;
    LeaveCriticalSection(&state->stateLock);
    
    // Notify state change
    if (oldValue != enableAutopaste) {
        NotifyStateChange("enableAutopaste", &enableAutopaste);
    }
}

BOOL GetAutopasteState(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return TRUE;  // Default to enabled
    
    EnterCriticalSection(&state->stateLock);
    BOOL result = state->enableAutopaste;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// UI resource access functions
HBRUSH GetBrush(int brushType) {
    ApplicationState* state = GetApplicationState();
    if (!state) return NULL;
    
    EnterCriticalSection(&state->stateLock);
    HBRUSH result = NULL;
    
    switch (brushType) {
        case BRUSH_WHITE:
            result = state->hBrushWhite;
            break;
        case BRUSH_LIGHT_GREEN:
            result = state->hBrushLightGreen;
            break;
        case BRUSH_LIGHT_BLUE:
            result = state->hBrushLightBlue;
            break;
        case BRUSH_LIGHT_TEAL:
            result = state->hBrushLightTeal;
            break;
        default:
            result = state->hBrushWhite;  // Default to white
            break;
    }
    
    LeaveCriticalSection(&state->stateLock);
    return result;
}

void SetCurrentBrush(HBRUSH brush) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    state->hCurrentBrush = brush;
    LeaveCriticalSection(&state->stateLock);
    
    NotifyStateChange("currentBrush", &brush);
}

HBRUSH GetCurrentBrush(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return NULL;
    
    EnterCriticalSection(&state->stateLock);
    HBRUSH result = state->hCurrentBrush;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// Window procedure access functions
void SetOriginalTextFieldProc(WNDPROC proc) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    state->originalTextFieldProc = proc;
    LeaveCriticalSection(&state->stateLock);
}

WNDPROC GetOriginalTextFieldProc(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return NULL;
    
    EnterCriticalSection(&state->stateLock);
    WNDPROC result = state->originalTextFieldProc;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// Command line URL functions
void SetCommandLineURL(const wchar_t* url) {
    ApplicationState* state = GetApplicationState();
    if (!state || !url) return;
    
    EnterCriticalSection(&state->stateLock);
    wcsncpy(state->cmdLineURL, url, sizeof(state->cmdLineURL) / sizeof(wchar_t) - 1);
    state->cmdLineURL[sizeof(state->cmdLineURL) / sizeof(wchar_t) - 1] = L'\0';
    LeaveCriticalSection(&state->stateLock);
    
    NotifyStateChange("cmdLineURL", (void*)url);
}

const wchar_t* GetCommandLineURL(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return L"";
    
    // Note: Returning pointer to internal buffer - caller should not modify
    // and should copy if needed for long-term storage
    EnterCriticalSection(&state->stateLock);
    const wchar_t* result = state->cmdLineURL;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// Cache and metadata access functions
CacheManager* GetCacheManager(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return NULL;
    
    EnterCriticalSection(&state->stateLock);
    CacheManager* result = state->cacheManager;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

CachedVideoMetadata* GetCachedVideoMetadata(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return NULL;
    
    EnterCriticalSection(&state->stateLock);
    CachedVideoMetadata* result = state->cachedVideoMetadata;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// State change notification system
void RegisterStateChangeCallback(StateChangeCallback callback, void* userData) {
    if (!callback) return;
    
    StateChangeCallbackNode* node = (StateChangeCallbackNode*)SAFE_MALLOC(sizeof(StateChangeCallbackNode));
    if (!node) return;
    
    node->callback = callback;
    node->userData = userData;
    node->next = g_callbackList;
    g_callbackList = node;
}

void UnregisterStateChangeCallback(StateChangeCallback callback) {
    if (!callback) return;
    
    StateChangeCallbackNode** current = &g_callbackList;
    while (*current) {
        if ((*current)->callback == callback) {
            StateChangeCallbackNode* toDelete = *current;
            *current = (*current)->next;
            SAFE_FREE(toDelete);
            return;
        }
        current = &(*current)->next;
    }
}

void NotifyStateChange(const char* stateType, void* newValue) {
    if (!stateType) return;
    
    StateChangeCallbackNode* current = g_callbackList;
    while (current) {
        if (current->callback) {
            current->callback(stateType, newValue, current->userData);
        }
        current = current->next;
    }
}

// Thread-safe download-after-info flag functions
BOOL SetDownloadAfterInfoFlag(BOOL flag) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    EnterCriticalSection(&state->stateLock);
    state->downloadAfterInfo = flag;
    LeaveCriticalSection(&state->stateLock);
    
    NotifyStateChange("downloadAfterInfo", &flag);
    return TRUE;
}

BOOL GetDownloadAfterInfoFlag(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    EnterCriticalSection(&state->stateLock);
    BOOL result = state->downloadAfterInfo;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// Download management functions
BOOL SetActiveDownload(HANDLE hProcess, DWORD processId, const wchar_t* tempDir) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    EnterCriticalSection(&state->stateLock);
    
    // Clear any existing download first
    if (state->isDownloadActive && state->hDownloadProcess) {
        CloseHandle(state->hDownloadProcess);
    }
    
    state->isDownloadActive = TRUE;
    state->hDownloadProcess = hProcess;
    state->downloadProcessId = processId;
    state->downloadCancelled = FALSE;
    
    if (tempDir) {
        wcscpy(state->downloadTempDir, tempDir);
    } else {
        state->downloadTempDir[0] = L'\0';
    }
    
    LeaveCriticalSection(&state->stateLock);
    return TRUE;
}

void ClearActiveDownload(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    
    if (state->hDownloadProcess && state->hDownloadProcess != INVALID_HANDLE_VALUE) {
        // Defensive: Validate handle before closing
        HANDLE hTest = NULL;
        if (DuplicateHandle(GetCurrentProcess(), state->hDownloadProcess, 
                           GetCurrentProcess(), &hTest, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            CloseHandle(hTest);
            CloseHandle(state->hDownloadProcess);
        }
        state->hDownloadProcess = NULL;
    }
    
    state->isDownloadActive = FALSE;
    state->downloadProcessId = 0;
    state->downloadTempDir[0] = L'\0';
    state->downloadCancelled = FALSE;
    
    LeaveCriticalSection(&state->stateLock);
}

BOOL CancelActiveDownload(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    BOOL success = FALSE;
    
    EnterCriticalSection(&state->stateLock);
    
    if (state->isDownloadActive && state->hDownloadProcess) {
        // Mark as cancelled first
        state->downloadCancelled = TRUE;
        
        // Terminate the process
        if (TerminateProcess(state->hDownloadProcess, 1)) {
            success = TRUE;
            DebugOutput(L"YouTubeCacher: CancelActiveDownload - Process terminated successfully");
        } else {
            DWORD error = GetLastError();
            wchar_t debugMsg[256];
            swprintf(debugMsg, 256, L"YouTubeCacher: CancelActiveDownload - Failed to terminate process, error: %lu", error);
            DebugOutput(debugMsg);
        }
        
        // Clean up temporary files if temp directory is set
        if (state->downloadTempDir[0] != L'\0') {
            wchar_t debugMsg[512];
            swprintf(debugMsg, 512, L"YouTubeCacher: CancelActiveDownload - Cleaning up temp directory: %ls", state->downloadTempDir);
            DebugOutput(debugMsg);
            
            // Remove temporary files
            CleanupTempDirectory(state->downloadTempDir);
        }
    }
    
    LeaveCriticalSection(&state->stateLock);
    return success;
}

BOOL IsDownloadActive(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    BOOL active;
    EnterCriticalSection(&state->stateLock);
    active = state->isDownloadActive;
    LeaveCriticalSection(&state->stateLock);
    
    return active;
}

BOOL IsDownloadCancelled(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    BOOL cancelled;
    EnterCriticalSection(&state->stateLock);
    cancelled = state->downloadCancelled;
    LeaveCriticalSection(&state->stateLock);
    
    return cancelled;
}

// yt-dlp output buffer management functions
void ClearYtDlpOutputBuffer(void) {
    ApplicationState* state = GetApplicationState();
    if (!state || !state->ytdlpOutputBuffer) return;
    
    EnterCriticalSection(&state->ytdlpOutputLock);
    state->ytdlpOutputBuffer[0] = L'\0';
    LeaveCriticalSection(&state->ytdlpOutputLock);
}

void AppendToYtDlpOutputBuffer(const wchar_t* output) {
    ApplicationState* state = GetApplicationState();
    if (!state || !state->ytdlpOutputBuffer || !output) return;
    
    EnterCriticalSection(&state->ytdlpOutputLock);
    
    size_t currentLen = wcslen(state->ytdlpOutputBuffer);
    size_t outputLen = wcslen(output);
    size_t requiredSize = currentLen + outputLen + 1;
    
    // Check if we need to expand the buffer
    if (requiredSize > state->ytdlpOutputBufferSize) {
        // Double the buffer size or use required size, whichever is larger
        size_t newSize = (state->ytdlpOutputBufferSize * 2 > requiredSize) ? 
                         state->ytdlpOutputBufferSize * 2 : requiredSize;
        
        wchar_t* newBuffer = (wchar_t*)SAFE_REALLOC(state->ytdlpOutputBuffer, newSize * sizeof(wchar_t));
        if (newBuffer) {
            state->ytdlpOutputBuffer = newBuffer;
            state->ytdlpOutputBufferSize = newSize;
        } else {
            // Reallocation failed, truncate to fit in current buffer
            size_t maxAppend = state->ytdlpOutputBufferSize - currentLen - 1;
            if (maxAppend > 0) {
                wcsncat(state->ytdlpOutputBuffer, output, maxAppend);
            }
            LeaveCriticalSection(&state->ytdlpOutputLock);
            return;
        }
    }
    
    // Append the output
    wcscat(state->ytdlpOutputBuffer, output);
    
    LeaveCriticalSection(&state->ytdlpOutputLock);
}

const wchar_t* GetYtDlpOutputBuffer(void) {
    ApplicationState* state = GetApplicationState();
    if (!state || !state->ytdlpOutputBuffer) return L"";
    
    // Note: This returns a pointer to the buffer. The caller should not modify it
    // and should be aware that the content may change if other threads call
    // AppendToYtDlpOutputBuffer or ClearYtDlpOutputBuffer
    return state->ytdlpOutputBuffer;
}

size_t GetYtDlpOutputBufferSize(void) {
    ApplicationState* state = GetApplicationState();
    if (!state || !state->ytdlpOutputBuffer) return 0;
    
    EnterCriticalSection(&state->ytdlpOutputLock);
    size_t len = wcslen(state->ytdlpOutputBuffer);
    LeaveCriticalSection(&state->ytdlpOutputLock);
    
    return len;
}

// yt-dlp session log management functions (in-memory only, separate from disk logging)
void StartNewYtDlpInvocation(void) {
    ApplicationState* state = GetApplicationState();
    if (!state || !state->ytdlpSessionLogLast) return;
    
    EnterCriticalSection(&state->ytdlpSessionLogLock);
    
    // Clear the "last run" log to prepare for new invocation
    state->ytdlpSessionLogLast[0] = L'\0';
    
    LeaveCriticalSection(&state->ytdlpSessionLogLock);
}

void AppendToYtDlpSessionLog(const wchar_t* output) {
    ApplicationState* state = GetApplicationState();
    if (!state || !state->ytdlpSessionLogAll || !state->ytdlpSessionLogLast || !output) return;
    
    EnterCriticalSection(&state->ytdlpSessionLogLock);
    
    // Append to "all logs" buffer
    size_t currentAllLen = wcslen(state->ytdlpSessionLogAll);
    size_t outputLen = wcslen(output);
    size_t requiredAllSize = currentAllLen + outputLen + 1;
    
    if (requiredAllSize > state->ytdlpSessionLogAllSize) {
        size_t newSize = (state->ytdlpSessionLogAllSize * 2 > requiredAllSize) ? 
                         state->ytdlpSessionLogAllSize * 2 : requiredAllSize;
        
        wchar_t* newBuffer = (wchar_t*)SAFE_REALLOC(state->ytdlpSessionLogAll, newSize * sizeof(wchar_t));
        if (newBuffer) {
            state->ytdlpSessionLogAll = newBuffer;
            state->ytdlpSessionLogAllSize = newSize;
        } else {
            // Reallocation failed, truncate
            size_t maxAppend = state->ytdlpSessionLogAllSize - currentAllLen - 1;
            if (maxAppend > 0) {
                wcsncat(state->ytdlpSessionLogAll, output, maxAppend);
            }
        }
    }
    
    if (requiredAllSize <= state->ytdlpSessionLogAllSize) {
        wcscat(state->ytdlpSessionLogAll, output);
    }
    
    // Append to "last run" buffer
    size_t currentLastLen = wcslen(state->ytdlpSessionLogLast);
    size_t requiredLastSize = currentLastLen + outputLen + 1;
    
    if (requiredLastSize > state->ytdlpSessionLogLastSize) {
        size_t newSize = (state->ytdlpSessionLogLastSize * 2 > requiredLastSize) ? 
                         state->ytdlpSessionLogLastSize * 2 : requiredLastSize;
        
        wchar_t* newBuffer = (wchar_t*)SAFE_REALLOC(state->ytdlpSessionLogLast, newSize * sizeof(wchar_t));
        if (newBuffer) {
            state->ytdlpSessionLogLast = newBuffer;
            state->ytdlpSessionLogLastSize = newSize;
        } else {
            // Reallocation failed, truncate
            size_t maxAppend = state->ytdlpSessionLogLastSize - currentLastLen - 1;
            if (maxAppend > 0) {
                wcsncat(state->ytdlpSessionLogLast, output, maxAppend);
            }
        }
    }
    
    if (requiredLastSize <= state->ytdlpSessionLogLastSize) {
        wcscat(state->ytdlpSessionLogLast, output);
    }
    
    LeaveCriticalSection(&state->ytdlpSessionLogLock);
    
    // Notify log viewer window if it's open (real-time update)
    extern HWND g_hLogViewerDialog;
    if (g_hLogViewerDialog && IsWindow(g_hLogViewerDialog)) {
        PostMessageW(g_hLogViewerDialog, WM_LOG_VIEWER_UPDATE, 0, 0);
    }
}

const wchar_t* GetYtDlpSessionLogAll(void) {
    ApplicationState* state = GetApplicationState();
    if (!state || !state->ytdlpSessionLogAll) return L"";
    return state->ytdlpSessionLogAll;
}

const wchar_t* GetYtDlpSessionLogLast(void) {
    ApplicationState* state = GetApplicationState();
    if (!state || !state->ytdlpSessionLogLast) return L"";
    return state->ytdlpSessionLogLast;
}
