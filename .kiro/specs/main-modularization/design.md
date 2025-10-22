# Design Document

## Overview

This design addresses the modularization of the monolithic main.c file (5,804 lines) in YouTubeCacher by splitting it into focused, maintainable modules. The current implementation handles UI management, yt-dlp integration, settings management, threading, and application state all within a single file, making it difficult to maintain and extend.

The modularization will create separate modules with clear responsibilities while maintaining all existing functionality. The design emphasizes clean interfaces, minimal coupling, and proper separation of concerns.

## Architecture

### Current Architecture Analysis

The existing main.c file contains:
- **UI Management**: Window procedures, dialog handling, control management (~1,500 lines)
- **YtDlp Integration**: Process execution, output parsing, error handling (~1,200 lines)
- **Settings Management**: Registry operations, configuration loading/saving (~300 lines)
- **Threading**: Thread management, synchronization, progress tracking (~800 lines)
- **Application State**: Global variables, state management (~200 lines)
- **Utility Functions**: Path handling, validation, formatting (~1,000 lines)
- **Entry Points**: WinMain, initialization, cleanup (~200 lines)

### Proposed Modular Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    main.c (Entry Point)                    │
│              - WinMain/wWinMain                             │
│              - Application initialization                   │
│              - Message loop                                 │
│              - Global cleanup                               │
├─────────────────────────────────────────────────────────────┤
│                    ui.c (UI Module)                         │
│              - Window procedures                            │
│              - Dialog management                            │
│              - Control handling                             │
│              - Layout and theming                           │
├─────────────────────────────────────────────────────────────┤
│                  ytdlp.c (YtDlp Module)                     │
│              - Process execution                            │
│              - Output parsing                               │
│              - Error handling                               │
│              - Validation                                   │
├─────────────────────────────────────────────────────────────┤
│                settings.c (Settings Module)                │
│              - Registry operations                          │
│              - Configuration management                     │
│              - Path handling                                │
├─────────────────────────────────────────────────────────────┤
│               threading.c (Threading Module)               │
│              - Thread management                            │
│              - Synchronization                              │
│              - Progress tracking                            │
├─────────────────────────────────────────────────────────────┤
│               appstate.c (Application State)               │
│              - Global state management                      │
│              - State synchronization                        │
│              - Event notification                           │
└─────────────────────────────────────────────────────────────┘
```

## Components and Interfaces

### 1. Application State Module (appstate.c/appstate.h)

**Purpose**: Centralized management of application state and global variables

**Interface**:
```c
// Application state structure
typedef struct {
    // UI state
    BOOL isDownloading;
    BOOL programmaticChange;
    BOOL manualPaste;
    
    // Configuration state
    BOOL enableDebug;
    BOOL enableLogfile;
    BOOL enableAutopaste;
    
    // UI resources
    HBRUSH hBrushWhite;
    HBRUSH hBrushLightGreen;
    HBRUSH hBrushLightBlue;
    HBRUSH hBrushLightTeal;
    HBRUSH hCurrentBrush;
    
    // Cache and metadata
    CacheManager cacheManager;
    CachedVideoMetadata cachedVideoMetadata;
    
    // Command line state
    wchar_t cmdLineURL[MAX_URL_LENGTH];
    
    // Original window procedures
    WNDPROC originalTextFieldProc;
} ApplicationState;

// State management functions
BOOL InitializeApplicationState(ApplicationState* state);
void CleanupApplicationState(ApplicationState* state);
ApplicationState* GetApplicationState(void);
BOOL SetDownloadingState(BOOL isDownloading);
BOOL GetDownloadingState(void);
void SetDebugState(BOOL enableDebug, BOOL enableLogfile);
void NotifyStateChange(const char* stateType, void* newValue);
```

### 2. UI Module (ui.c/ui.h)

**Purpose**: All user interface related functionality

**Interface**:
```c
// UI management functions
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ProgressDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TextFieldSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// UI utility functions
void ResizeControls(HWND hDlg);
void ApplyModernThemeToDialog(HWND hDlg);
HWND CreateThemedDialog(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc);
void UpdateDebugControlVisibility(HWND hDlg);
void CheckClipboardForYouTubeURL(HWND hDlg);

// Progress and status functions
void UpdateVideoInfoUI(HWND hDlg, const wchar_t* title, const wchar_t* duration);
void SetDownloadUIState(HWND hDlg, BOOL isDownloading);
void UpdateMainProgressBar(HWND hDlg, int percentage, const wchar_t* status);
void ShowMainProgressBar(HWND hDlg, BOOL show);
void SetProgressBarMarquee(HWND hDlg, BOOL enable);

// Dialog management
BOOL ShowUnifiedDialog(HWND parent, const UnifiedDialogConfig* config);
void UpdateProgressDialog(ProgressDialog* dialog, int progress, const wchar_t* status);
BOOL IsProgressDialogCancelled(const ProgressDialog* dialog);
void DestroyProgressDialog(ProgressDialog* dialog);
```

### 3. YtDlp Module (ytdlp.c/ytdlp.h)

**Purpose**: All yt-dlp integration and process management

**Interface**:
```c
// YtDlp configuration and validation
BOOL InitializeYtDlpConfig(YtDlpConfig* config);
void CleanupYtDlpConfig(YtDlpConfig* config);
BOOL ValidateYtDlpExecutable(const wchar_t* path);
BOOL ValidateYtDlpComprehensive(const wchar_t* path, ValidationInfo* info);
void FreeValidationInfo(ValidationInfo* info);

// YtDlp operations
BOOL GetVideoTitleAndDurationSync(const wchar_t* url, wchar_t* title, size_t titleSize, wchar_t* duration, size_t durationSize);
BOOL GetVideoTitleAndDuration(HWND hDlg, const wchar_t* url, wchar_t* title, size_t titleSize, wchar_t* duration, size_t durationSize);
BOOL StartUnifiedDownload(HWND hDlg, const wchar_t* url);
BOOL StartNonBlockingDownload(YtDlpConfig* config, YtDlpRequest* request, HWND parentWindow);

// Process management
BOOL StartSubprocessExecution(SubprocessContext* context);
BOOL IsSubprocessRunning(const SubprocessContext* context);
BOOL CancelSubprocessExecution(SubprocessContext* context);
BOOL WaitForSubprocessCompletion(SubprocessContext* context, DWORD timeoutMs);
void FreeSubprocessContext(SubprocessContext* context);

// Temporary directory management
BOOL CreateYtDlpTempDirWithFallback(wchar_t* tempPath, size_t pathSize);
BOOL CleanupTempDirectory(const wchar_t* tempDir);

// Error handling and analysis
void FreeYtDlpResult(YtDlpResult* result);
void FreeErrorAnalysis(ErrorAnalysis* analysis);
void InstallYtDlpWithWinget(HWND hParent);
```

### 4. Settings Module (settings.c/settings.h)

**Purpose**: Configuration and registry management

**Interface**:
```c
// Registry operations
BOOL LoadSettingFromRegistry(const wchar_t* valueName, wchar_t* buffer, DWORD bufferSize);
BOOL SaveSettingToRegistry(const wchar_t* valueName, const wchar_t* value);

// Settings management
void LoadSettings(HWND hDlg);
void SaveSettings(HWND hDlg);

// Path management
void GetDefaultDownloadPath(wchar_t* path, size_t pathSize);
void GetDefaultYtDlpPath(wchar_t* path, size_t pathSize);
BOOL CreateDownloadDirectoryIfNeeded(const wchar_t* path);

// Utility functions
void FormatDuration(wchar_t* duration, size_t bufferSize);
```

### 5. Threading Module (threading.c/threading.h)

**Purpose**: Thread management and synchronization

**Interface**:
```c
// Thread context management
BOOL InitializeThreadContext(ThreadContext* threadContext);
void CleanupThreadContext(ThreadContext* threadContext);

// Thread synchronization
BOOL SetCancellationFlag(ThreadContext* threadContext);
BOOL IsCancellationRequested(const ThreadContext* threadContext);

// Progress callbacks
void SubprocessProgressCallback(int percentage, const wchar_t* status, void* userData);
void UnifiedDownloadProgressCallback(int percentage, const wchar_t* status, void* userData);
void MainWindowProgressCallback(int percentage, const wchar_t* status, void* userData);

// Download completion handling
void HandleDownloadCompletion(HWND hDlg, YtDlpResult* result, NonBlockingDownloadContext* downloadContext);
```

### 6. Main Module (main.c - Simplified)

**Purpose**: Application entry point and coordination

**Interface**:
```c
// Entry points
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

// Debug and logging
void DebugOutput(const wchar_t* message);
void WriteToLogfile(const wchar_t* message);
void WriteSessionStartToLogfile(void);
void WriteSessionEndToLogfile(const wchar_t* reason);
```

## Data Models

### Module Dependencies

```c
// Dependency hierarchy (top depends on bottom)
main.c
├── ui.h
├── ytdlp.h  
├── settings.h
├── threading.h
└── appstate.h

ui.c
├── appstate.h
├── settings.h
└── threading.h

ytdlp.c
├── appstate.h
├── settings.h
└── threading.h

settings.c
└── appstate.h

threading.c
└── appstate.h

appstate.c
└── (no module dependencies)
```

### Shared Data Structures

The existing structures in YouTubeCacher.h will be reorganized:

- **Core structures** remain in YouTubeCacher.h
- **Module-specific structures** move to respective module headers
- **State structures** move to appstate.h

### Header File Organization

```c
// YouTubeCacher.h - Core application definitions
#include "appstate.h"
#include "ui.h"
#include "ytdlp.h"
#include "settings.h"
#include "threading.h"

// Each module header includes only what it needs
// appstate.h - No dependencies on other modules
// ui.h - Includes appstate.h
// ytdlp.h - Includes appstate.h, threading.h
// settings.h - Includes appstate.h
// threading.h - Includes appstate.h
```

## Error Handling

### Module Error Handling Strategy

1. **Consistent Return Values**: All modules use BOOL for success/failure
2. **Error Context**: Modules provide detailed error information through state
3. **Error Propagation**: Errors bubble up through the module hierarchy
4. **Cleanup Responsibility**: Each module cleans up its own resources

### Error Communication

```c
// Error information structure for inter-module communication
typedef struct {
    DWORD errorCode;
    wchar_t* errorMessage;
    wchar_t* moduleName;
    wchar_t* functionName;
} ModuleError;

// Error reporting functions in appstate module
void SetModuleError(const ModuleError* error);
const ModuleError* GetLastModuleError(void);
void ClearModuleError(void);
```

## Testing Strategy

### Unit Testing Approach

1. **Module Isolation**: Each module can be tested independently
2. **Mock Dependencies**: Use mock implementations for dependent modules
3. **State Testing**: Test application state management thoroughly
4. **Interface Testing**: Verify all module interfaces work correctly

### Integration Testing

1. **Module Interaction**: Test communication between modules
2. **State Consistency**: Verify state remains consistent across modules
3. **Error Propagation**: Test error handling across module boundaries
4. **Resource Management**: Verify proper cleanup in all scenarios

### Test Structure

```
tests/
├── test_appstate.c     - Application state tests
├── test_ui.c           - UI module tests
├── test_ytdlp.c        - YtDlp module tests
├── test_settings.c     - Settings module tests
├── test_threading.c    - Threading module tests
└── test_integration.c  - Cross-module integration tests
```

## Implementation Notes

### Migration Strategy

1. **Phase 1**: Create appstate.c and move global variables
2. **Phase 2**: Extract settings.c functionality
3. **Phase 3**: Extract threading.c functionality  
4. **Phase 4**: Extract ytdlp.c functionality
5. **Phase 5**: Extract ui.c functionality
6. **Phase 6**: Simplify main.c to entry point only

### Backward Compatibility

- All existing function signatures preserved during transition
- Registry settings and configuration remain unchanged
- UI behavior and appearance unchanged
- Performance characteristics maintained or improved

### Build System Updates

```makefile
# Updated Makefile structure
SOURCES = main.c ui.c ytdlp.c settings.c threading.c appstate.c
HEADERS = YouTubeCacher.h ui.h ytdlp.h settings.h threading.h appstate.h

# Module-specific object files
OBJECTS = main.o ui.o ytdlp.o settings.o threading.o appstate.o

# Dependency tracking for incremental builds
main.o: main.c YouTubeCacher.h ui.h ytdlp.h settings.h threading.h appstate.h
ui.o: ui.c ui.h appstate.h settings.h threading.h
ytdlp.o: ytdlp.c ytdlp.h appstate.h settings.h threading.h
settings.o: settings.c settings.h appstate.h
threading.o: threading.c threading.h appstate.h
appstate.o: appstate.c appstate.h
```

### Performance Considerations

- **Function Call Overhead**: Minimal impact due to compiler optimization
- **Memory Usage**: Slight reduction due to better organization
- **Compilation Time**: Improved incremental compilation
- **Debugging**: Easier to debug specific functionality

### Security Considerations

- **Module Boundaries**: Clear interfaces prevent accidental data exposure
- **State Protection**: Centralized state management improves security
- **Input Validation**: Each module validates its inputs
- **Resource Cleanup**: Proper cleanup prevents resource leaks

This modular design maintains all existing functionality while providing a much more maintainable and extensible codebase structure.