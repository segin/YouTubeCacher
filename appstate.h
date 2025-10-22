#ifndef APPSTATE_H
#define APPSTATE_H

#include <windows.h>
#include "cache.h"

// Application state structure containing all global variables
typedef struct {
    // Command line state
    wchar_t cmdLineURL[1024];  // MAX_URL_LENGTH
    
    // UI state flags
    BOOL isDownloading;
    BOOL programmaticChange;
    BOOL manualPaste;
    
    // Configuration state
    BOOL enableDebug;
    BOOL enableLogfile;
    BOOL enableAutopaste;
    
    // UI resources (brushes for text field colors)
    HBRUSH hBrushWhite;
    HBRUSH hBrushLightGreen;
    HBRUSH hBrushLightBlue;
    HBRUSH hBrushLightTeal;
    HBRUSH hCurrentBrush;
    
    // Cache and metadata
    CacheManager* cacheManager;
    CachedVideoMetadata* cachedVideoMetadata;
    
    // Original window procedures for subclassing
    WNDPROC originalTextFieldProc;
    
    // Thread synchronization
    CRITICAL_SECTION stateLock;
    BOOL isInitialized;
} ApplicationState;

// State management functions
BOOL InitializeApplicationState(ApplicationState* state);
void CleanupApplicationState(ApplicationState* state);
ApplicationState* GetApplicationState(void);

// Thread-safe state access functions
BOOL SetDownloadingState(BOOL isDownloading);
BOOL GetDownloadingState(void);
void SetProgrammaticChangeFlag(BOOL flag);
BOOL GetProgrammaticChangeFlag(void);
void SetManualPasteFlag(BOOL flag);
BOOL GetManualPasteFlag(void);

// Configuration state functions
void SetDebugState(BOOL enableDebug, BOOL enableLogfile);
void GetDebugState(BOOL* enableDebug, BOOL* enableLogfile);
void SetAutopasteState(BOOL enableAutopaste);
BOOL GetAutopasteState(void);

// UI resource access functions
HBRUSH GetBrush(int brushType);
void SetCurrentBrush(HBRUSH brush);
HBRUSH GetCurrentBrush(void);

// Window procedure access functions
void SetOriginalTextFieldProc(WNDPROC proc);
WNDPROC GetOriginalTextFieldProc(void);

// Command line URL functions
void SetCommandLineURL(const wchar_t* url);
const wchar_t* GetCommandLineURL(void);

// Cache and metadata access functions
CacheManager* GetCacheManager(void);
CachedVideoMetadata* GetCachedVideoMetadata(void);

// State change notification
typedef void (*StateChangeCallback)(const char* stateType, void* newValue, void* userData);
void RegisterStateChangeCallback(StateChangeCallback callback, void* userData);
void UnregisterStateChangeCallback(StateChangeCallback callback);
void NotifyStateChange(const char* stateType, void* newValue);

// Brush type constants
#define BRUSH_WHITE         0
#define BRUSH_LIGHT_GREEN   1
#define BRUSH_LIGHT_BLUE    2
#define BRUSH_LIGHT_TEAL    3

// Color definitions for text field backgrounds
#define COLOR_WHITE         RGB(255, 255, 255)
#define COLOR_LIGHT_GREEN   RGB(220, 255, 220)
#define COLOR_LIGHT_BLUE    RGB(220, 220, 255)
#define COLOR_LIGHT_TEAL    RGB(220, 255, 255)

#endif // APPSTATE_H