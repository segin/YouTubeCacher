# Error Handling Standardization Design

## Overview

This design document outlines the architecture for standardizing error handling throughout the YouTubeCacher application. The current codebase exhibits inconsistent error handling patterns with mixed return codes, NULL returns, and ad-hoc error reporting. This design establishes a unified error handling system that provides consistent error propagation, comprehensive error context collection, and user-friendly error dialog population.

## Architecture

### Core Components

The error handling system consists of four main components:

1. **Error Handler Core** - Central error management system
2. **Error Context System** - Error information collection and preservation
3. **Error Dialog Manager** - User-facing error presentation
4. **Validation Framework** - Consistent input validation with standardized error reporting

### System Integration

The error handling system integrates with existing YouTubeCacher components:
- **Application State** - Error statistics and recovery state tracking
- **Logging System** - Structured error logging with context
- **UI System** - Error dialog presentation and user feedback
- **Threading System** - Thread-safe error handling for concurrent operations

## Components and Interfaces

### 1. Error Handler Core

```c
// Error severity levels
typedef enum {
    ERROR_SEVERITY_INFO = 0,
    ERROR_SEVERITY_WARNING = 1,
    ERROR_SEVERITY_ERROR = 2,
    ERROR_SEVERITY_FATAL = 3
} ErrorSeverity;

// Standardized error codes
typedef enum {
    ERROR_SUCCESS = 0,
    ERROR_MEMORY_ALLOCATION = 1001,
    ERROR_FILE_NOT_FOUND = 1002,
    ERROR_INVALID_PARAMETER = 1003,
    ERROR_NETWORK_FAILURE = 1004,
    ERROR_YTDLP_EXECUTION = 1005,
    ERROR_CACHE_OPERATION = 1006,
    ERROR_THREAD_CREATION = 1007,
    ERROR_VALIDATION_FAILED = 1008,
    ERROR_SUBPROCESS_FAILED = 1009,
    ERROR_DIALOG_CREATION = 1010
} StandardErrorCode;

// Error handler interface
typedef struct ErrorHandler {
    CRITICAL_SECTION lock;
    ErrorStatistics* stats;
    RecoveryStrategy* strategies;
    BOOL initialized;
} ErrorHandler;

// Core functions
BOOL InitializeErrorHandler(ErrorHandler* handler);
void CleanupErrorHandler(ErrorHandler* handler);
StandardErrorCode ReportError(ErrorSeverity severity, StandardErrorCode code, 
                             const wchar_t* function, const wchar_t* file, 
                             int line, const wchar_t* message);
```

### 2. Error Context System

```c
// Error context structure
typedef struct ErrorContext {
    StandardErrorCode errorCode;
    ErrorSeverity severity;
    wchar_t functionName[128];
    wchar_t fileName[256];
    int lineNumber;
    wchar_t technicalMessage[512];
    wchar_t userMessage[512];
    wchar_t additionalContext[1024];
    DWORD systemErrorCode;
    SYSTEMTIME timestamp;
    DWORD threadId;
    wchar_t callStack[2048];
} ErrorContext;

// Context collection functions
ErrorContext* CreateErrorContext(StandardErrorCode code, ErrorSeverity severity,
                                const wchar_t* function, const wchar_t* file, int line);
void AddContextVariable(ErrorContext* context, const wchar_t* name, const wchar_t* value);
void SetUserFriendlyMessage(ErrorContext* context, const wchar_t* message);
void CaptureCallStack(ErrorContext* context);
void FreeErrorContext(ErrorContext* context);
```

### 3. Error Dialog Manager (Unified Dialog Integration)

```c
// Error dialog integration with existing UnifiedDialog system
typedef struct ErrorDialogBuilder {
    wchar_t* title;
    wchar_t* message;
    wchar_t* technicalDetails;
    wchar_t* diagnostics;
    wchar_t* solutions;
    UnifiedDialogType dialogType;
    BOOL showCopyButton;
    BOOL showDetailsButton;
} ErrorDialogBuilder;

// Dialog management functions that integrate with UnifiedDialog
ErrorDialogBuilder* CreateErrorDialogBuilder(const ErrorContext* context);
UnifiedDialogConfig* BuildUnifiedDialogConfig(const ErrorDialogBuilder* builder);
INT_PTR ShowErrorDialog(HWND parent, const ErrorContext* context);
void FreeErrorDialogBuilder(ErrorDialogBuilder* builder);

// Utility functions for dialog content generation
UnifiedDialogType MapSeverityToDialogType(ErrorSeverity severity);
void FormatTechnicalDetails(const ErrorContext* context, wchar_t* buffer, size_t bufferSize);
void FormatDiagnosticInfo(const ErrorContext* context, wchar_t* buffer, size_t bufferSize);
void FormatSolutionSuggestions(StandardErrorCode errorCode, wchar_t* buffer, size_t bufferSize);
```

### 4. Validation Framework

```c
// Validation result structure
typedef struct ValidationResult {
    StandardErrorCode errorCode;
    wchar_t errorMessage[512];
    wchar_t fieldName[128];
    BOOL isValid;
} ValidationResult;

// Validation functions
ValidationResult ValidatePointer(const void* ptr, const wchar_t* paramName);
ValidationResult ValidateString(const wchar_t* str, const wchar_t* paramName, 
                               size_t maxLength);
ValidationResult ValidateFilePath(const wchar_t* path, const wchar_t* paramName);
ValidationResult ValidateURL(const wchar_t* url, const wchar_t* paramName);
ValidationResult ValidateBufferSize(size_t size, size_t minSize, size_t maxSize);
```

## Data Models

### Error Statistics

```c
typedef struct ErrorStatistics {
    DWORD totalErrors;
    DWORD errorsByCode[100];  // Track frequency of each error code
    DWORD errorsBySeverity[4]; // Track by severity level
    SYSTEMTIME lastError;
    DWORD consecutiveErrors;
    BOOL recoveryInProgress;
} ErrorStatistics;
```

### Recovery Strategy

```c
typedef struct RecoveryAction {
    StandardErrorCode triggerCode;
    BOOL (*recoveryFunction)(const ErrorContext* context);
    wchar_t description[256];
    int maxAttempts;
    int currentAttempts;
} RecoveryAction;

typedef struct RecoveryStrategy {
    RecoveryAction actions[50];
    int actionCount;
    CRITICAL_SECTION lock;
} RecoveryStrategy;
```

## Error Handling Macros

### Common Error Checking Patterns

```c
// Memory allocation checking
#define SAFE_ALLOC(ptr, size, cleanup_label) \
    do { \
        (ptr) = malloc(size); \
        if (!(ptr)) { \
            ReportError(ERROR_SEVERITY_ERROR, ERROR_MEMORY_ALLOCATION, \
                       __FUNCTIONW__, __FILEW__, __LINE__, \
                       L"Memory allocation failed"); \
            goto cleanup_label; \
        } \
    } while(0)

// Function parameter validation
#define VALIDATE_PARAM(condition, param_name, cleanup_label) \
    do { \
        if (!(condition)) { \
            ReportError(ERROR_SEVERITY_ERROR, ERROR_INVALID_PARAMETER, \
                       __FUNCTIONW__, __FILEW__, __LINE__, \
                       L"Invalid parameter: " param_name); \
            goto cleanup_label; \
        } \
    } while(0)

// System call error checking
#define CHECK_SYSTEM_CALL(call, cleanup_label) \
    do { \
        if (!(call)) { \
            DWORD sysError = GetLastError(); \
            wchar_t msg[256]; \
            swprintf(msg, 256, L"System call failed: %ls (Error: %lu)", \
                    L#call, sysError); \
            ReportError(ERROR_SEVERITY_ERROR, ERROR_SUBPROCESS_FAILED, \
                       __FUNCTIONW__, __FILEW__, __LINE__, msg); \
            goto cleanup_label; \
        } \
    } while(0)

// Error propagation
#define PROPAGATE_ERROR(result, cleanup_label) \
    do { \
        if ((result) != ERROR_SUCCESS) { \
            ReportError(ERROR_SEVERITY_ERROR, (result), \
                       __FUNCTIONW__, __FILEW__, __LINE__, \
                       L"Error propagated from called function"); \
            goto cleanup_label; \
        } \
    } while(0)
```

## Error Handling

### Error Classification

Errors are classified into categories for appropriate handling:

1. **Recoverable Errors** - Memory allocation failures, network timeouts, file access issues
2. **User Errors** - Invalid URLs, missing files, configuration problems
3. **System Errors** - Threading failures, subprocess creation failures
4. **Fatal Errors** - Critical system failures requiring application termination

### Error Recovery Strategies

1. **Automatic Recovery** - Retry operations with exponential backoff
2. **Graceful Degradation** - Disable features when dependencies fail
3. **User Intervention** - Prompt user for corrective action
4. **Safe Termination** - Save state and exit cleanly for fatal errors

### Error Dialog Population (UnifiedDialog Integration)

The system automatically populates the existing UnifiedDialog system with structured error information:

1. **Main Message Tab** - User-friendly error description and immediate context
2. **Technical Details Tab** - Function name, file, line number, call stack, and variable values  
3. **Diagnostics Tab** - System state information, memory usage, thread status, file system state
4. **Solutions Tab** - Actionable guidance and specific steps users can take to resolve issues
5. **Dialog Type Mapping** - Automatic mapping of error severity to UnifiedDialogType (INFO, WARNING, ERROR, SUCCESS)
6. **Copy Button Integration** - Leverages existing copy functionality for technical support

## Testing Strategy

### Unit Testing

1. **Error Context Creation** - Verify proper context structure population
2. **Error Code Mapping** - Test conversion from system errors to standard codes
3. **Dialog Data Generation** - Validate user-friendly message creation
4. **Macro Functionality** - Test error checking macros in various scenarios

### Integration Testing

1. **Cross-Module Error Propagation** - Test error flow between components
2. **Threading Error Handling** - Verify thread-safe error reporting
3. **Recovery Strategy Execution** - Test automatic recovery mechanisms
4. **Dialog Display Integration** - Test error dialog presentation with UI system

### Error Simulation Testing

1. **Memory Allocation Failures** - Simulate low memory conditions
2. **File System Errors** - Test with read-only directories, full disks
3. **Network Failures** - Simulate connection timeouts and failures
4. **Subprocess Failures** - Test yt-dlp execution error scenarios

## Implementation Considerations

### Thread Safety

All error handling components use critical sections for thread safety:
- Error statistics updates are atomic
- Error context creation is thread-safe
- Dialog data population handles concurrent access

### Performance Impact

The error handling system is designed for minimal performance overhead:
- Error context creation only occurs when errors happen
- Statistics updates use efficient data structures
- Macro-based error checking has zero runtime cost when successful

### Memory Management

Consistent memory management patterns:
- All error structures use standardized allocation/deallocation
- Automatic cleanup on error propagation
- Resource leak prevention through RAII-style patterns

### Backward Compatibility

The new error handling system maintains compatibility with existing code:
- Existing return codes can be mapped to standard error codes
- Gradual migration path for legacy error handling
- Wrapper functions for existing error reporting mechanisms