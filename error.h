#ifndef ERROR_H
#define ERROR_H

#include <windows.h>
#include <stdbool.h>

// Error severity levels for classification
typedef enum {
    YTC_SEVERITY_INFO = 0,
    YTC_SEVERITY_WARNING = 1,
    YTC_SEVERITY_ERROR = 2,
    YTC_SEVERITY_FATAL = 3
} ErrorSeverity;

// Standardized error codes for all application-specific error conditions
typedef enum {
    YTC_ERROR_SUCCESS = 0,
    YTC_ERROR_MEMORY_ALLOCATION = 1001,
    YTC_ERROR_FILE_NOT_FOUND = 1002,
    YTC_ERROR_INVALID_PARAMETER = 1003,
    YTC_ERROR_NETWORK_FAILURE = 1004,
    YTC_ERROR_YTDLP_EXECUTION = 1005,
    YTC_ERROR_CACHE_OPERATION = 1006,
    YTC_ERROR_THREAD_CREATION = 1007,
    YTC_ERROR_VALIDATION_FAILED = 1008,
    YTC_ERROR_SUBPROCESS_FAILED = 1009,
    YTC_ERROR_DIALOG_CREATION = 1010,
    YTC_ERROR_FILE_ACCESS = 1011,
    YTC_ERROR_BUFFER_OVERFLOW = 1012,
    YTC_ERROR_URL_INVALID = 1013,
    YTC_ERROR_PERMISSION_DENIED = 1014,
    YTC_ERROR_DISK_FULL = 1015,
    YTC_ERROR_TIMEOUT = 1016,
    YTC_ERROR_AUTHENTICATION = 1017,
    YTC_ERROR_CONFIGURATION = 1018,
    YTC_ERROR_INITIALIZATION = 1019,
    YTC_ERROR_CLEANUP = 1020
} StandardErrorCode;

// Forward declarations
typedef struct ErrorStatistics ErrorStatistics;
typedef struct RecoveryStrategy RecoveryStrategy;
typedef struct ErrorContext ErrorContext;

// Error statistics structure for tracking and analysis
typedef struct ErrorStatistics {
    DWORD totalErrors;
    DWORD errorsByCode[100];      // Track frequency of each error code
    DWORD errorsBySeverity[4];    // Track by severity level
    SYSTEMTIME lastError;
    DWORD consecutiveErrors;
    BOOL recoveryInProgress;
} ErrorStatistics;

// Recovery action structure for automatic error recovery
typedef struct RecoveryAction {
    StandardErrorCode triggerCode;
    BOOL (*recoveryFunction)(const void* context);
    wchar_t description[256];
    int maxAttempts;
    int currentAttempts;
} RecoveryAction;

// Recovery strategy structure for managing recovery actions
typedef struct RecoveryStrategy {
    RecoveryAction actions[50];
    int actionCount;
    CRITICAL_SECTION lock;
} RecoveryStrategy;

// Main error handler structure
typedef struct ErrorHandler {
    CRITICAL_SECTION lock;
    ErrorStatistics* stats;
    RecoveryStrategy* strategies;
    BOOL initialized;
    HANDLE logFile;
    wchar_t logPath[MAX_PATH];
} ErrorHandler;

// Core error handler functions
BOOL InitializeErrorHandler(ErrorHandler* handler);
void CleanupErrorHandler(ErrorHandler* handler);
StandardErrorCode ReportError(ErrorSeverity severity, StandardErrorCode code, 
                             const wchar_t* function, const wchar_t* file, 
                             int line, const wchar_t* message);

// Error statistics functions
void UpdateErrorStatistics(ErrorHandler* handler, StandardErrorCode code, ErrorSeverity severity);
void ResetErrorStatistics(ErrorHandler* handler);
DWORD GetErrorCount(const ErrorHandler* handler, StandardErrorCode code);
DWORD GetTotalErrorCount(const ErrorHandler* handler);

// Recovery strategy functions
BOOL AddRecoveryStrategy(ErrorHandler* handler, StandardErrorCode code, 
                        BOOL (*recoveryFunc)(const void*), const wchar_t* description);
BOOL ExecuteRecoveryStrategy(ErrorHandler* handler, StandardErrorCode code, const void* context);
void ClearRecoveryStrategies(ErrorHandler* handler);

// Utility functions
const wchar_t* GetErrorCodeString(StandardErrorCode code);
const wchar_t* GetSeverityString(ErrorSeverity severity);
BOOL IsRecoverableError(StandardErrorCode code);

// Error context structure for detailed error information
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
    struct {
        wchar_t name[64];
        wchar_t value[256];
    } contextVariables[16];
    int contextVariableCount;
} ErrorContext;

// Error context management functions
ErrorContext* CreateErrorContext(StandardErrorCode code, ErrorSeverity severity,
                                const wchar_t* function, const wchar_t* file, int line);
void AddContextVariable(ErrorContext* context, const wchar_t* name, const wchar_t* value);
void SetUserFriendlyMessage(ErrorContext* context, const wchar_t* message);
void CaptureCallStack(ErrorContext* context);
void FreeErrorContext(ErrorContext* context);

// Error dialog functions (implemented in error.c, requires YouTubeCacher.h)
// These functions integrate with the existing UnifiedDialog system
INT_PTR ShowErrorDialog(HWND parent, const ErrorContext* context);
void FormatTechnicalDetails(const ErrorContext* context, wchar_t* buffer, size_t bufferSize);
void FormatDiagnosticInfo(const ErrorContext* context, wchar_t* buffer, size_t bufferSize);
void FormatSolutionSuggestions(StandardErrorCode errorCode, wchar_t* buffer, size_t bufferSize);

// Parameter validation result structure for consistent validation reporting
typedef struct ParameterValidationResult {
    StandardErrorCode errorCode;
    wchar_t errorMessage[512];
    wchar_t fieldName[128];
    BOOL isValid;
} ParameterValidationResult;

// Validation framework functions
ParameterValidationResult ValidatePointer(const void* ptr, const wchar_t* paramName);
ParameterValidationResult ValidateString(const wchar_t* str, const wchar_t* paramName, size_t maxLength);
ParameterValidationResult ValidateFilePath(const wchar_t* path, const wchar_t* paramName);
ParameterValidationResult ValidateURL(const wchar_t* url, const wchar_t* paramName);
ParameterValidationResult ValidateBufferSize(size_t size, size_t minSize, size_t maxSize);

// Global error handler instance
extern ErrorHandler g_ErrorHandler;

// Convenience macros for error reporting
#define REPORT_ERROR(severity, code, message) \
    ReportError((severity), (code), __FUNCTIONW__, __FILEW__, __LINE__, (message))

#define REPORT_FATAL_ERROR(code, message) \
    ReportError(YTC_SEVERITY_FATAL, (code), __FUNCTIONW__, __FILEW__, __LINE__, (message))

#define REPORT_ERROR_MSG(severity, code, format, ...) \
    do { \
        wchar_t _msg[512]; \
        swprintf(_msg, 512, (format), __VA_ARGS__); \
        ReportError((severity), (code), __FUNCTIONW__, __FILEW__, __LINE__, _msg); \
    } while(0)

// Convenience macro for showing error dialogs
#define SHOW_ERROR_DIALOG(parent, severity, code, message) \
    do { \
        ErrorContext* _ctx = CreateErrorContext((code), (severity), __FUNCTIONW__, __FILEW__, __LINE__); \
        if (_ctx) { \
            SetUserFriendlyMessage(_ctx, (message)); \
            ShowErrorDialog((parent), _ctx); \
            FreeErrorContext(_ctx); \
        } \
    } while(0)

// ============================================================================
// Error Handling Macros and Utilities
// ============================================================================

// Helper functions to convert __FUNCTION__ and __FILE__ to wide strings
static inline const wchar_t* GetWideFunction(const char* func) {
    static wchar_t buffer[256];
    MultiByteToWideChar(CP_UTF8, 0, func, -1, buffer, 256);
    return buffer;
}

static inline const wchar_t* GetWideFile(const char* file) {
    static wchar_t buffer[512];
    MultiByteToWideChar(CP_UTF8, 0, file, -1, buffer, 512);
    return buffer;
}

// Memory allocation checking macro with error handling and cleanup
#define SAFE_ALLOC(ptr, size, cleanup_label) \
    do { \
        (ptr) = malloc(size); \
        if (!(ptr)) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_MEMORY_ALLOCATION, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Memory allocation failed"); \
            goto cleanup_label; \
        } \
    } while(0)

// Memory allocation checking macro with zero initialization
#define SAFE_CALLOC_OR_CLEANUP(ptr, count, size, cleanup_label) \
    do { \
        (ptr) = calloc(count, size); \
        if (!(ptr)) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_MEMORY_ALLOCATION, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Memory allocation (calloc) failed"); \
            goto cleanup_label; \
        } \
    } while(0)

// Memory reallocation checking macro with error handling
#define SAFE_REALLOC_OR_CLEANUP(ptr, new_ptr, size, cleanup_label) \
    do { \
        void* _temp = realloc(ptr, size); \
        if (!_temp) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_MEMORY_ALLOCATION, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Memory reallocation failed"); \
            goto cleanup_label; \
        } \
        (new_ptr) = _temp; \
    } while(0)

// Parameter validation macro with error reporting and cleanup
#define VALIDATE_PARAM(condition, param_name, cleanup_label) \
    do { \
        if (!(condition)) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_INVALID_PARAMETER, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Parameter validation failed: " param_name); \
            goto cleanup_label; \
        } \
    } while(0)

// Pointer validation macro (checks for NULL)
#define VALIDATE_POINTER(ptr, cleanup_label) \
    do { \
        if (!(ptr)) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_INVALID_PARAMETER, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Pointer parameter cannot be NULL"); \
            goto cleanup_label; \
        } \
    } while(0)

// String validation macro (checks for NULL and empty)
#define VALIDATE_STRING(str, cleanup_label) \
    do { \
        if (!(str)) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_INVALID_PARAMETER, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"String parameter cannot be NULL"); \
            goto cleanup_label; \
        } \
        if (wcslen(str) == 0) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_INVALID_PARAMETER, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"String parameter cannot be empty"); \
            goto cleanup_label; \
        } \
    } while(0)

// Buffer size validation macro
#define VALIDATE_BUFFER_SIZE(size, min_size, max_size, cleanup_label) \
    do { \
        if ((size) < (min_size)) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_BUFFER_OVERFLOW, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Buffer size is below minimum required"); \
            goto cleanup_label; \
        } \
        if ((max_size) > 0 && (size) > (max_size)) { \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_BUFFER_OVERFLOW, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Buffer size exceeds maximum allowed"); \
            goto cleanup_label; \
        } \
    } while(0)

// Windows API system call error checking macro
#define CHECK_SYSTEM_CALL(call, cleanup_label) \
    do { \
        if (!(call)) { \
            DWORD _sysError = GetLastError(); \
            wchar_t _msg[512]; \
            swprintf(_msg, 512, L"System call failed (Error: %lu)", _sysError); \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_SUBPROCESS_FAILED, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, _msg); \
            goto cleanup_label; \
        } \
    } while(0)

// Windows API system call error checking with custom error code
#define CHECK_SYSTEM_CALL_EX(call, error_code, cleanup_label) \
    do { \
        if (!(call)) { \
            DWORD _sysError = GetLastError(); \
            wchar_t _msg[512]; \
            swprintf(_msg, 512, L"System call failed (Error: %lu)", _sysError); \
            ReportError(YTC_SEVERITY_ERROR, (error_code), \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, _msg); \
            goto cleanup_label; \
        } \
    } while(0)

// Handle validation macro for Windows handles
#define VALIDATE_HANDLE(handle, cleanup_label) \
    do { \
        if ((handle) == NULL || (handle) == INVALID_HANDLE_VALUE) { \
            DWORD _sysError = GetLastError(); \
            wchar_t _msg[512]; \
            swprintf(_msg, 512, L"Invalid handle (Error: %lu)", _sysError); \
            ReportError(YTC_SEVERITY_ERROR, YTC_ERROR_INVALID_PARAMETER, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, _msg); \
            goto cleanup_label; \
        } \
    } while(0)

// Error propagation macro for function return codes
#define PROPAGATE_ERROR(result, cleanup_label) \
    do { \
        if ((result) != YTC_ERROR_SUCCESS) { \
            ReportError(YTC_SEVERITY_ERROR, (result), \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Error propagated from called function"); \
            goto cleanup_label; \
        } \
    } while(0)

// Error propagation macro with custom message
#define PROPAGATE_ERROR_MSG(result, message, cleanup_label) \
    do { \
        if ((result) != YTC_ERROR_SUCCESS) { \
            ReportError(YTC_SEVERITY_ERROR, (result), \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, (message)); \
            goto cleanup_label; \
        } \
    } while(0)

// Boolean result checking macro
#define CHECK_BOOL_RESULT(call, error_code, cleanup_label) \
    do { \
        if (!(call)) { \
            ReportError(YTC_SEVERITY_ERROR, (error_code), \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, \
                       L"Boolean operation failed"); \
            goto cleanup_label; \
        } \
    } while(0)

// File operation error checking macro
#define CHECK_FILE_OPERATION(call, cleanup_label) \
    CHECK_SYSTEM_CALL_EX(call, YTC_ERROR_FILE_ACCESS, cleanup_label)

// Network operation error checking macro  
#define CHECK_NETWORK_OPERATION(call, cleanup_label) \
    CHECK_SYSTEM_CALL_EX(call, YTC_ERROR_NETWORK_FAILURE, cleanup_label)

// Thread operation error checking macro
#define CHECK_THREAD_OPERATION(call, cleanup_label) \
    CHECK_SYSTEM_CALL_EX(call, YTC_ERROR_THREAD_CREATION, cleanup_label)

// ============================================================================
// Resource Management and Cleanup Patterns
// ============================================================================

// Safe cleanup macro that checks for NULL before calling cleanup function
#define SAFE_CLEANUP(ptr, cleanup_func) \
    do { \
        if (ptr) { \
            cleanup_func(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)

// Safe free macro that checks for NULL and sets pointer to NULL
#define SAFE_FREE_AND_NULL(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)

// Safe handle close macro for Windows handles
#define SAFE_CLOSE_HANDLE(handle) \
    do { \
        if ((handle) != NULL && (handle) != INVALID_HANDLE_VALUE) { \
            CloseHandle(handle); \
            (handle) = INVALID_HANDLE_VALUE; \
        } \
    } while(0)

// Safe critical section cleanup macro
#define SAFE_DELETE_CRITICAL_SECTION(cs) \
    do { \
        __try { \
            DeleteCriticalSection(cs); \
        } \
        __except(EXCEPTION_EXECUTE_HANDLER) { \
            /* Critical section was already deleted or corrupted */ \
        } \
    } while(0)

// Resource cleanup label pattern macros for consistent error handling
#define DECLARE_CLEANUP_VARS() \
    BOOL _cleanup_needed = FALSE; \
    StandardErrorCode _error_result = YTC_ERROR_SUCCESS

#define SET_ERROR_AND_CLEANUP(error_code) \
    do { \
        _error_result = (error_code); \
        _cleanup_needed = TRUE; \
        goto cleanup; \
    } while(0)

#define RETURN_ON_ERROR() \
    do { \
        if (_cleanup_needed) { \
            return _error_result; \
        } \
        return YTC_ERROR_SUCCESS; \
    } while(0)

// Multi-resource cleanup pattern
#define BEGIN_CLEANUP_BLOCK() \
    cleanup: \
    do {

#define END_CLEANUP_BLOCK() \
    } while(0); \
    RETURN_ON_ERROR()

// ============================================================================
// Validation Framework Integration Macros
// ============================================================================

// Macro to use validation framework with automatic error reporting
#define VALIDATE_WITH_FRAMEWORK(validation_call, cleanup_label) \
    do { \
        ParameterValidationResult _result = (validation_call); \
        if (!_result.isValid) { \
            ReportError(YTC_SEVERITY_ERROR, _result.errorCode, \
                       GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__, _result.errorMessage); \
            goto cleanup_label; \
        } \
    } while(0)

// Specific validation macros using the framework
#define VALIDATE_POINTER_PARAM(ptr, param_name, cleanup_label) \
    VALIDATE_WITH_FRAMEWORK(ValidatePointer(ptr, param_name), cleanup_label)

#define VALIDATE_STRING_PARAM(str, param_name, max_len, cleanup_label) \
    VALIDATE_WITH_FRAMEWORK(ValidateString(str, param_name, max_len), cleanup_label)

#define VALIDATE_FILEPATH_PARAM(path, param_name, cleanup_label) \
    VALIDATE_WITH_FRAMEWORK(ValidateFilePath(path, param_name), cleanup_label)

#define VALIDATE_URL_PARAM(url, param_name, cleanup_label) \
    VALIDATE_WITH_FRAMEWORK(ValidateURL(url, param_name), cleanup_label)

#define VALIDATE_BUFFER_SIZE_PARAM(size, min_size, max_size, cleanup_label) \
    VALIDATE_WITH_FRAMEWORK(ValidateBufferSize(size, min_size, max_size), cleanup_label)

// ============================================================================
// Error Context Creation Macros
// ============================================================================

// Macro to create error context with automatic cleanup
#define CREATE_ERROR_CONTEXT(code, severity) \
    CreateErrorContext((code), (severity), GetWideFunction(__FUNCTION__), GetWideFile(__FILE__), __LINE__)

// Macro to create and show error dialog in one step
#define SHOW_ERROR_CONTEXT_DIALOG(parent, context) \
    do { \
        if (context) { \
            ShowErrorDialog((parent), (context)); \
            FreeErrorContext(context); \
            (context) = NULL; \
        } \
    } while(0)

// Macro for creating error context with user message
#define CREATE_ERROR_CONTEXT_WITH_MESSAGE(code, severity, user_msg) \
    ({ \
        ErrorContext* _ctx = CREATE_ERROR_CONTEXT(code, severity); \
        if (_ctx && (user_msg)) { \
            SetUserFriendlyMessage(_ctx, (user_msg)); \
        } \
        _ctx; \
    })

#endif // ERROR_H