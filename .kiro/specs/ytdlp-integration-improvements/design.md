# Design Document

## Overview

This design improves the yt-dlp integration in YouTubeCacher by implementing robust error handling, better process management, and enhanced user experience. The current implementation successfully launches yt-dlp but lacks comprehensive error handling for runtime issues, proper temporary directory management, and user feedback during operations.

The design focuses on creating a more resilient integration that can handle various failure scenarios gracefully while providing clear feedback to users about what's happening and how to resolve issues.

## Architecture

### Current Architecture Analysis

The existing implementation has these components:
- **Path Management**: `GetDefaultYtDlpPath()` and `ValidateYtDlpExecutable()`
- **Process Execution**: Direct `CreateProcessW()` calls with pipe-based output capture
- **Error Handling**: Basic Windows error code reporting
- **UI Integration**: Simple message boxes for results and errors

### Proposed Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    UI Layer (main.c)                       │
├─────────────────────────────────────────────────────────────┤
│                YtDlp Manager Module                         │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │   Validation    │  │    Execution    │  │   Progress  │ │
│  │    Service      │  │     Service     │  │   Dialog    │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                Process Management Layer                     │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │    Process      │  │   Temp Dir      │  │   Output    │ │
│  │   Wrapper       │  │   Manager       │  │  Capture    │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                 Error Handling Layer                        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │  Error Parser   │  │  Diagnostics    │  │  Recovery   │ │
│  │                 │  │   Generator     │  │  Advisor    │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## Components and Interfaces

### 1. YtDlp Manager Module

**Purpose**: Central coordination of all yt-dlp operations

**Interface**:
```c
typedef enum {
    YTDLP_OP_GET_INFO,
    YTDLP_OP_DOWNLOAD,
    YTDLP_OP_VALIDATE
} YtDlpOperation;

typedef struct {
    YtDlpOperation operation;
    wchar_t* url;
    wchar_t* outputPath;
    wchar_t* tempDir;
    BOOL useCustomArgs;
    wchar_t* customArgs;
} YtDlpRequest;

typedef struct {
    BOOL success;
    DWORD exitCode;
    wchar_t* output;
    wchar_t* errorMessage;
    wchar_t* diagnostics;
} YtDlpResult;

BOOL ExecuteYtDlpOperation(const YtDlpRequest* request, YtDlpResult* result);
void FreeYtDlpResult(YtDlpResult* result);
```

### 2. Validation Service

**Purpose**: Comprehensive yt-dlp executable and environment validation

**Interface**:
```c
typedef enum {
    VALIDATION_OK,
    VALIDATION_NOT_FOUND,
    VALIDATION_NOT_EXECUTABLE,
    VALIDATION_MISSING_DEPENDENCIES,
    VALIDATION_VERSION_INCOMPATIBLE,
    VALIDATION_PERMISSION_DENIED
} ValidationResult;

typedef struct {
    ValidationResult result;
    wchar_t* version;
    wchar_t* errorDetails;
    wchar_t* suggestions;
} ValidationInfo;

BOOL ValidateYtDlpComprehensive(const wchar_t* path, ValidationInfo* info);
BOOL TestYtDlpFunctionality(const wchar_t* path);
void FreeValidationInfo(ValidationInfo* info);
```

### 3. Process Wrapper

**Purpose**: Robust process management with timeout and cleanup

**Interface**:
```c
typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hStdOut;
    HANDLE hStdErr;
    DWORD processId;
    BOOL isRunning;
} ProcessHandle;

typedef struct {
    DWORD timeoutMs;
    BOOL captureOutput;
    BOOL hideWindow;
    wchar_t* workingDirectory;
    wchar_t* environment;
} ProcessOptions;

BOOL CreateYtDlpProcess(const wchar_t* cmdLine, const ProcessOptions* options, ProcessHandle* handle);
BOOL WaitForProcessCompletion(ProcessHandle* handle, DWORD timeoutMs, DWORD* exitCode);
BOOL TerminateYtDlpProcess(ProcessHandle* handle);
void CleanupProcessHandle(ProcessHandle* handle);
```

### 4. Temporary Directory Manager

**Purpose**: Intelligent temporary directory selection and management

**Interface**:
```c
typedef enum {
    TEMP_DIR_SYSTEM,
    TEMP_DIR_DOWNLOAD,
    TEMP_DIR_CUSTOM,
    TEMP_DIR_APPDATA
} TempDirStrategy;

BOOL CreateYtDlpTempDir(wchar_t* tempPath, size_t pathSize, TempDirStrategy strategy);
BOOL CleanupYtDlpTempDir(const wchar_t* tempPath);
BOOL ValidateTempDirAccess(const wchar_t* tempPath);
```

### 5. Progress Dialog

**Purpose**: User feedback during long-running operations

**Interface**:
```c
typedef struct {
    HWND hDialog;
    HWND hProgressBar;
    HWND hStatusText;
    HWND hCancelButton;
    BOOL cancelled;
} ProgressDialog;

BOOL ShowProgressDialog(HWND parent, const wchar_t* title, ProgressDialog* dialog);
void UpdateProgress(ProgressDialog* dialog, int percentage, const wchar_t* status);
BOOL IsProgressCancelled(const ProgressDialog* dialog);
void CloseProgressDialog(ProgressDialog* dialog);
```

### 6. Error Parser and Diagnostics

**Purpose**: Intelligent error analysis and user guidance

**Interface**:
```c
typedef enum {
    ERROR_TYPE_TEMP_DIR,
    ERROR_TYPE_NETWORK,
    ERROR_TYPE_PERMISSIONS,
    ERROR_TYPE_DEPENDENCIES,
    ERROR_TYPE_URL_INVALID,
    ERROR_TYPE_DISK_SPACE,
    ERROR_TYPE_UNKNOWN
} ErrorType;

typedef struct {
    ErrorType type;
    wchar_t* description;
    wchar_t* solution;
    wchar_t* technicalDetails;
} ErrorAnalysis;

BOOL AnalyzeYtDlpError(const wchar_t* output, DWORD exitCode, ErrorAnalysis* analysis);
void GenerateDiagnosticReport(const YtDlpRequest* request, const YtDlpResult* result, wchar_t* report, size_t reportSize);
void FreeErrorAnalysis(ErrorAnalysis* analysis);
```

## Data Models

### Configuration Structure
```c
typedef struct {
    wchar_t ytDlpPath[MAX_EXTENDED_PATH];
    wchar_t defaultTempDir[MAX_EXTENDED_PATH];
    wchar_t defaultArgs[1024];
    DWORD timeoutSeconds;
    BOOL enableVerboseLogging;
    BOOL autoRetryOnFailure;
    TempDirStrategy tempDirStrategy;
} YtDlpConfig;
```

### Operation Context
```c
typedef struct {
    YtDlpConfig config;
    YtDlpRequest request;
    ProcessHandle process;
    ProgressDialog progress;
    wchar_t tempDir[MAX_EXTENDED_PATH];
    BOOL operationActive;
} YtDlpContext;
```

## Error Handling

### Error Classification System

1. **Configuration Errors**: Missing executable, invalid paths
2. **Runtime Errors**: Process creation failures, permission issues
3. **yt-dlp Specific Errors**: Temporary directory, network, URL validation
4. **System Errors**: Memory allocation, disk space, Windows API failures

### Error Recovery Strategies

1. **Automatic Recovery**: Alternative temp directories, fallback arguments
2. **User-Guided Recovery**: Clear instructions for manual fixes
3. **Graceful Degradation**: Simplified operations when full functionality unavailable

### Error Message Enhancement

```c
typedef struct {
    wchar_t* primaryMessage;    // User-friendly description
    wchar_t* technicalDetails;  // Technical information for advanced users
    wchar_t* suggestedActions;  // Step-by-step resolution guide
    wchar_t* diagnosticInfo;    // System state and configuration details
} EnhancedErrorMessage;
```

## Testing Strategy

### Unit Testing Approach

1. **Validation Functions**: Test with various executable states and paths
2. **Process Management**: Test timeout handling, cleanup, and error scenarios
3. **Error Parsing**: Test with known yt-dlp error outputs
4. **Temp Directory Management**: Test fallback strategies and cleanup

### Integration Testing

1. **End-to-End Operations**: Full yt-dlp execution cycles
2. **Error Scenarios**: Simulate various failure conditions
3. **UI Integration**: Test progress dialogs and error displays
4. **Resource Management**: Verify proper cleanup and no leaks

### Test Data Requirements

1. **Mock yt-dlp Executables**: Various versions and states
2. **Sample Error Outputs**: Known yt-dlp error messages
3. **Test URLs**: Valid and invalid YouTube URLs
4. **System State Simulation**: Restricted permissions, full disks

### Performance Testing

1. **Timeout Handling**: Verify proper process termination
2. **Memory Usage**: Monitor allocation and cleanup
3. **Concurrent Operations**: Test operation queuing and rejection
4. **Large Output Handling**: Test with verbose yt-dlp output

## Implementation Notes

### Backward Compatibility

- Maintain existing function signatures where possible
- Preserve current registry settings and configuration
- Ensure existing UI behavior remains consistent

### Security Considerations

- Validate all user inputs and file paths
- Use secure process creation with minimal privileges
- Sanitize command line arguments to prevent injection
- Implement proper cleanup to prevent information leakage

### Performance Optimizations

- Cache validation results to avoid repeated checks
- Use asynchronous operations for UI responsiveness
- Implement efficient output buffering for large captures
- Minimize memory allocations during critical operations

### Internationalization Support

- Use Unicode throughout for proper character handling
- Design error messages for easy localization
- Consider different locale-specific temporary directory conventions