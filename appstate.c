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
    
    // Initialize cache and metadata pointers to NULL
    state->cacheManager = NULL;
    state->cachedVideoMetadata = NULL;
    
    // Initialize window procedure pointer
    state->originalTextFieldProc = NULL;
    
    // Initialize command line URL as empty
    state->cmdLineURL[0] = L'\0';
    
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
    
    // Note: Cache manager and cached video metadata cleanup should be handled
    // by their respective modules when they are implemented
    
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
    ApplicationState* state = GetApplicationState();
    if (!state || !enableDebug || !enableLogfile) return;
    
    EnterCriticalSection(&state->stateLock);
    *enableDebug = state->enableDebug;
    *enableLogfile = state->enableLogfile;
    LeaveCriticalSection(&state->stateLock);
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
    
    StateChangeCallbackNode* node = (StateChangeCallbackNode*)malloc(sizeof(StateChangeCallbackNode));
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
            free(toDelete);
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