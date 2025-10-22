#include "YouTubeCacher.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Global application state instance
static ApplicationState g_appState = {0};
static BOOL g_stateInitialized = FALSE;

// State change callback management
#define MAX_CALLBACKS 10
static struct {
    StateChangeCallback callback;
    void* userData;
    BOOL inUse;
} g_callbacks[MAX_CALLBACKS] = {0};

// Initialize the application state
BOOL InitializeApplicationState(ApplicationState* state) {
    if (!state) {
        return FALSE;
    }
    
    // Initialize critical section for thread safety
    if (!InitializeCriticalSectionAndSpinCount(&state->stateLock, 0x00000400)) {
        return FALSE;
    }
    
    // Initialize command line URL
    memset(state->cmdLineURL, 0, sizeof(state->cmdLineURL));
    
    // Initialize UI state flags
    state->isDownloading = FALSE;
    state->programmaticChange = FALSE;
    state->manualPaste = FALSE;
    
    // Initialize configuration state with defaults
    state->enableDebug = FALSE;
    state->enableLogfile = FALSE;
    state->enableAutopaste = TRUE;  // Default to enabled
    
    // Initialize UI resources (brushes will be created when needed)
    state->hBrushWhite = NULL;
    state->hBrushLightGreen = NULL;
    state->hBrushLightBlue = NULL;
    state->hBrushLightTeal = NULL;
    state->hCurrentBrush = NULL;
    
    // Initialize cache and metadata pointers
    state->cacheManager = NULL;
    state->cachedVideoMetadata = NULL;
    
    // Initialize window procedure
    state->originalTextFieldProc = NULL;
    
    // Mark as initialized
    state->isInitialized = TRUE;
    
    return TRUE;
}

// Cleanup the application state
void CleanupApplicationState(ApplicationState* state) {
    if (!state || !state->isInitialized) {
        return;
    }
    
    // Enter critical section for cleanup
    EnterCriticalSection(&state->stateLock);
    
    // Cleanup UI resources
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
    
    // Clear current brush reference (don't delete as it's one of the above)
    state->hCurrentBrush = NULL;
    
    // Cleanup allocated cache and metadata
    if (state->cacheManager) {
        free(state->cacheManager);
        state->cacheManager = NULL;
    }
    if (state->cachedVideoMetadata) {
        free(state->cachedVideoMetadata);
        state->cachedVideoMetadata = NULL;
    }
    
    // Clear command line URL
    memset(state->cmdLineURL, 0, sizeof(state->cmdLineURL));
    
    // Reset flags
    state->isDownloading = FALSE;
    state->programmaticChange = FALSE;
    state->manualPaste = FALSE;
    state->enableDebug = FALSE;
    state->enableLogfile = FALSE;
    state->enableAutopaste = TRUE;
    
    // Clear window procedure
    state->originalTextFieldProc = NULL;
    
    // Mark as uninitialized
    state->isInitialized = FALSE;
    
    // Leave critical section before deleting it
    LeaveCriticalSection(&state->stateLock);
    
    // Delete critical section
    DeleteCriticalSection(&state->stateLock);
}

// Get the global application state instance
ApplicationState* GetApplicationState(void) {
    if (!g_stateInitialized) {
        if (InitializeApplicationState(&g_appState)) {
            g_stateInitialized = TRUE;
        } else {
            return NULL;
        }
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
        NotifyStateChange("downloading", &isDownloading);
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
}

BOOL GetManualPasteFlag(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return FALSE;
    
    EnterCriticalSection(&state->stateLock);
    BOOL result = state->manualPaste;
    LeaveCriticalSection(&state->stateLock);
    
    return result;
}

// Configuration state functions
void SetDebugState(BOOL enableDebug, BOOL enableLogfile) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    state->enableDebug = enableDebug;
    state->enableLogfile = enableLogfile;
    LeaveCriticalSection(&state->stateLock);
    
    // Notify state change
    NotifyStateChange("debug", &enableDebug);
    NotifyStateChange("logfile", &enableLogfile);
}

void GetDebugState(BOOL* enableDebug, BOOL* enableLogfile) {
    ApplicationState* state = GetApplicationState();
    if (!state || !enableDebug || !enableLogfile) return;
    
    EnterCriticalSection(&state->stateLock);
    *enableDebug = state->enableDebug;
    *enableLogfile = state->enableLogfile;
    LeaveCriticalSection(&state->stateLock);
}

void SetAutopasteState(BOOL enableAutopaste) {
    ApplicationState* state = GetApplicationState();
    if (!state) return;
    
    EnterCriticalSection(&state->stateLock);
    state->enableAutopaste = enableAutopaste;
    LeaveCriticalSection(&state->stateLock);
    
    // Notify state change
    NotifyStateChange("autopaste", &enableAutopaste);
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
            if (!state->hBrushWhite) {
                state->hBrushWhite = CreateSolidBrush(COLOR_WHITE);
            }
            result = state->hBrushWhite;
            break;
            
        case BRUSH_LIGHT_GREEN:
            if (!state->hBrushLightGreen) {
                state->hBrushLightGreen = CreateSolidBrush(COLOR_LIGHT_GREEN);
            }
            result = state->hBrushLightGreen;
            break;
            
        case BRUSH_LIGHT_BLUE:
            if (!state->hBrushLightBlue) {
                state->hBrushLightBlue = CreateSolidBrush(COLOR_LIGHT_BLUE);
            }
            result = state->hBrushLightBlue;
            break;
            
        case BRUSH_LIGHT_TEAL:
            if (!state->hBrushLightTeal) {
                state->hBrushLightTeal = CreateSolidBrush(COLOR_LIGHT_TEAL);
            }
            result = state->hBrushLightTeal;
            break;
            
        default:
            result = NULL;
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
}

const wchar_t* GetCommandLineURL(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return L"";
    
    // Note: Returning pointer to internal buffer - caller should not modify
    // and should use the value immediately or copy it
    return state->cmdLineURL;
}

// Cache and metadata access functions
CacheManager* GetCacheManager(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return NULL;
    
    // Lazy initialization - allocate if not already allocated
    if (!state->cacheManager) {
        state->cacheManager = (CacheManager*)malloc(sizeof(CacheManager));
        if (state->cacheManager) {
            memset(state->cacheManager, 0, sizeof(CacheManager));
        }
    }
    
    return state->cacheManager;
}

CachedVideoMetadata* GetCachedVideoMetadata(void) {
    ApplicationState* state = GetApplicationState();
    if (!state) return NULL;
    
    // Lazy initialization - allocate if not already allocated
    if (!state->cachedVideoMetadata) {
        state->cachedVideoMetadata = (CachedVideoMetadata*)malloc(sizeof(CachedVideoMetadata));
        if (state->cachedVideoMetadata) {
            memset(state->cachedVideoMetadata, 0, sizeof(CachedVideoMetadata));
            state->cachedVideoMetadata->isValid = FALSE;
            state->cachedVideoMetadata->url = NULL;
        }
    }
    
    return state->cachedVideoMetadata;
}

// State change notification system
void RegisterStateChangeCallback(StateChangeCallback callback, void* userData) {
    if (!callback) return;
    
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!g_callbacks[i].inUse) {
            g_callbacks[i].callback = callback;
            g_callbacks[i].userData = userData;
            g_callbacks[i].inUse = TRUE;
            break;
        }
    }
}

void UnregisterStateChangeCallback(StateChangeCallback callback) {
    if (!callback) return;
    
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (g_callbacks[i].inUse && g_callbacks[i].callback == callback) {
            g_callbacks[i].callback = NULL;
            g_callbacks[i].userData = NULL;
            g_callbacks[i].inUse = FALSE;
            break;
        }
    }
}

void NotifyStateChange(const char* stateType, void* newValue) {
    if (!stateType) return;
    
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (g_callbacks[i].inUse && g_callbacks[i].callback) {
            g_callbacks[i].callback(stateType, newValue, g_callbacks[i].userData);
        }
    }
}