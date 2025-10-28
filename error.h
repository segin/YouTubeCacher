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

#endif // ERROR_H