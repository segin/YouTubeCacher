#include "error.h"
#include <stdio.h>
#include <wchar.h>
#include <time.h>

// Global error handler instance
ErrorHandler g_ErrorHandler = {0};

// Error code to string mapping
static const struct {
    StandardErrorCode code;
    const wchar_t* description;
} ErrorCodeStrings[] = {
    {YTC_ERROR_SUCCESS, L"Success"},
    {YTC_ERROR_MEMORY_ALLOCATION, L"Memory allocation failed"},
    {YTC_ERROR_FILE_NOT_FOUND, L"File not found"},
    {YTC_ERROR_INVALID_PARAMETER, L"Invalid parameter"},
    {YTC_ERROR_NETWORK_FAILURE, L"Network operation failed"},
    {YTC_ERROR_YTDLP_EXECUTION, L"yt-dlp execution failed"},
    {YTC_ERROR_CACHE_OPERATION, L"Cache operation failed"},
    {YTC_ERROR_THREAD_CREATION, L"Thread creation failed"},
    {YTC_ERROR_VALIDATION_FAILED, L"Validation failed"},
    {YTC_ERROR_SUBPROCESS_FAILED, L"Subprocess execution failed"},
    {YTC_ERROR_DIALOG_CREATION, L"Dialog creation failed"},
    {YTC_ERROR_FILE_ACCESS, L"File access denied"},
    {YTC_ERROR_BUFFER_OVERFLOW, L"Buffer overflow detected"},
    {YTC_ERROR_URL_INVALID, L"Invalid URL format"},
    {YTC_ERROR_PERMISSION_DENIED, L"Permission denied"},
    {YTC_ERROR_DISK_FULL, L"Disk full"},
    {YTC_ERROR_TIMEOUT, L"Operation timed out"},
    {YTC_ERROR_AUTHENTICATION, L"Authentication failed"},
    {YTC_ERROR_CONFIGURATION, L"Configuration error"},
    {YTC_ERROR_INITIALIZATION, L"Initialization failed"},
    {YTC_ERROR_CLEANUP, L"Cleanup failed"}
};

// Severity level to string mapping
static const wchar_t* SeverityStrings[] = {
    L"INFO",
    L"WARNING", 
    L"ERROR",
    L"FATAL"
};

/**
 * Initialize the error handler system
 * Sets up critical sections, allocates statistics and recovery structures
 */
BOOL InitializeErrorHandler(ErrorHandler* handler) {
    if (!handler) {
        return FALSE;
    }

    // Initialize critical section for thread safety
    InitializeCriticalSection(&handler->lock);

    // Allocate error statistics structure
    handler->stats = (ErrorStatistics*)malloc(sizeof(ErrorStatistics));
    if (!handler->stats) {
        DeleteCriticalSection(&handler->lock);
        return FALSE;
    }

    // Initialize statistics
    ZeroMemory(handler->stats, sizeof(ErrorStatistics));
    GetSystemTime(&handler->stats->lastError);

    // Allocate recovery strategy structure
    handler->strategies = (RecoveryStrategy*)malloc(sizeof(RecoveryStrategy));
    if (!handler->strategies) {
        free(handler->stats);
        handler->stats = NULL;
        DeleteCriticalSection(&handler->lock);
        return FALSE;
    }

    // Initialize recovery strategies
    ZeroMemory(handler->strategies, sizeof(RecoveryStrategy));
    InitializeCriticalSection(&handler->strategies->lock);

    // Set up log file path
    GetModuleFileNameW(NULL, handler->logPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(handler->logPath, L'\\');
    if (lastSlash) {
        wcscpy(lastSlash + 1, L"error.log");
    } else {
        wcscpy(handler->logPath, L"error.log");
    }

    handler->logFile = INVALID_HANDLE_VALUE;
    handler->initialized = TRUE;

    return TRUE;
}

/**
 * Clean up the error handler system
 * Releases all allocated resources and critical sections
 */
void CleanupErrorHandler(ErrorHandler* handler) {
    if (!handler || !handler->initialized) {
        return;
    }

    EnterCriticalSection(&handler->lock);

    // Close log file if open
    if (handler->logFile != INVALID_HANDLE_VALUE) {
        CloseHandle(handler->logFile);
        handler->logFile = INVALID_HANDLE_VALUE;
    }

    // Clean up recovery strategies
    if (handler->strategies) {
        DeleteCriticalSection(&handler->strategies->lock);
        free(handler->strategies);
        handler->strategies = NULL;
    }

    // Clean up statistics
    if (handler->stats) {
        free(handler->stats);
        handler->stats = NULL;
    }

    handler->initialized = FALSE;

    LeaveCriticalSection(&handler->lock);
    DeleteCriticalSection(&handler->lock);
}

/**
 * Report an error with full context information
 * Logs the error and updates statistics
 */
StandardErrorCode ReportError(ErrorSeverity severity, StandardErrorCode code, 
                             const wchar_t* function, const wchar_t* file, 
                             int line, const wchar_t* message) {
    if (!g_ErrorHandler.initialized) {
        // Try to initialize if not already done
        if (!InitializeErrorHandler(&g_ErrorHandler)) {
            return code; // Return original error if we can't initialize
        }
    }

    EnterCriticalSection(&g_ErrorHandler.lock);

    // Update error statistics
    UpdateErrorStatistics(&g_ErrorHandler, code, severity);

    // Create log entry
    SYSTEMTIME currentTime;
    GetSystemTime(&currentTime);

    wchar_t logEntry[2048];
    swprintf(logEntry, 2048, 
        L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%ls] [%d] %ls\r\n"
        L"  Function: %ls\r\n"
        L"  File: %ls:%d\r\n"
        L"  Message: %ls\r\n"
        L"  Thread: %lu\r\n\r\n",
        currentTime.wYear, currentTime.wMonth, currentTime.wDay,
        currentTime.wHour, currentTime.wMinute, currentTime.wSecond, currentTime.wMilliseconds,
        GetSeverityString(severity), code, GetErrorCodeString(code),
        function ? function : L"Unknown",
        file ? file : L"Unknown", line,
        message ? message : L"No message provided",
        GetCurrentThreadId());

    // Write to log file
    if (g_ErrorHandler.logFile == INVALID_HANDLE_VALUE) {
        g_ErrorHandler.logFile = CreateFileW(
            g_ErrorHandler.logPath,
            GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        
        if (g_ErrorHandler.logFile != INVALID_HANDLE_VALUE) {
            SetFilePointer(g_ErrorHandler.logFile, 0, NULL, FILE_END);
        }
    }

    if (g_ErrorHandler.logFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        char utf8Buffer[4096];
        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, 
                                           utf8Buffer, sizeof(utf8Buffer), NULL, NULL);
        if (utf8Length > 0) {
            WriteFile(g_ErrorHandler.logFile, utf8Buffer, utf8Length - 1, &bytesWritten, NULL);
            FlushFileBuffers(g_ErrorHandler.logFile);
        }
    }

    LeaveCriticalSection(&g_ErrorHandler.lock);

    // Attempt recovery if available
    if (IsRecoverableError(code)) {
        ExecuteRecoveryStrategy(&g_ErrorHandler, code, NULL);
    }

    return code;
}

/**
 * Update error statistics with new error occurrence
 */
void UpdateErrorStatistics(ErrorHandler* handler, StandardErrorCode code, ErrorSeverity severity) {
    if (!handler || !handler->stats) {
        return;
    }

    handler->stats->totalErrors++;
    
    // Update error count by code (use modulo to stay within array bounds)
    int codeIndex = code % 100;
    handler->stats->errorsByCode[codeIndex]++;
    
    // Update error count by severity
    if (severity < 4) {
        handler->stats->errorsBySeverity[severity]++;
    }

    // Update last error time
    GetSystemTime(&handler->stats->lastError);

    // Track consecutive errors
    if (severity >= YTC_SEVERITY_ERROR) {
        handler->stats->consecutiveErrors++;
    } else {
        handler->stats->consecutiveErrors = 0;
    }
}

/**
 * Get string representation of error code
 */
const wchar_t* GetErrorCodeString(StandardErrorCode code) {
    for (size_t i = 0; i < sizeof(ErrorCodeStrings) / sizeof(ErrorCodeStrings[0]); i++) {
        if (ErrorCodeStrings[i].code == code) {
            return ErrorCodeStrings[i].description;
        }
    }
    return L"Unknown error code";
}

/**
 * Get string representation of error severity
 */
const wchar_t* GetSeverityString(ErrorSeverity severity) {
    if (severity < sizeof(SeverityStrings) / sizeof(SeverityStrings[0])) {
        return SeverityStrings[severity];
    }
    return L"UNKNOWN";
}

/**
 * Check if an error code represents a recoverable error
 */
BOOL IsRecoverableError(StandardErrorCode code) {
    switch (code) {
        case YTC_ERROR_MEMORY_ALLOCATION:
        case YTC_ERROR_NETWORK_FAILURE:
        case YTC_ERROR_FILE_ACCESS:
        case YTC_ERROR_TIMEOUT:
        case YTC_ERROR_CACHE_OPERATION:
            return TRUE;
        default:
            return FALSE;
    }
}

/**
 * Reset error statistics
 */
void ResetErrorStatistics(ErrorHandler* handler) {
    if (!handler || !handler->stats) {
        return;
    }

    EnterCriticalSection(&handler->lock);
    ZeroMemory(handler->stats, sizeof(ErrorStatistics));
    GetSystemTime(&handler->stats->lastError);
    LeaveCriticalSection(&handler->lock);
}

/**
 * Get error count for specific error code
 */
DWORD GetErrorCount(const ErrorHandler* handler, StandardErrorCode code) {
    if (!handler || !handler->stats) {
        return 0;
    }

    int codeIndex = code % 100;
    return handler->stats->errorsByCode[codeIndex];
}

/**
 * Get total error count
 */
DWORD GetTotalErrorCount(const ErrorHandler* handler) {
    if (!handler || !handler->stats) {
        return 0;
    }

    return handler->stats->totalErrors;
}

/**
 * Add a recovery strategy for a specific error code
 */
BOOL AddRecoveryStrategy(ErrorHandler* handler, StandardErrorCode code, 
                        BOOL (*recoveryFunc)(const void*), const wchar_t* description) {
    if (!handler || !handler->strategies || !recoveryFunc) {
        return FALSE;
    }

    EnterCriticalSection(&handler->strategies->lock);

    if (handler->strategies->actionCount >= 50) {
        LeaveCriticalSection(&handler->strategies->lock);
        return FALSE; // No more room for strategies
    }

    RecoveryAction* action = &handler->strategies->actions[handler->strategies->actionCount];
    action->triggerCode = code;
    action->recoveryFunction = recoveryFunc;
    action->maxAttempts = 3;
    action->currentAttempts = 0;
    
    if (description) {
        wcsncpy(action->description, description, 255);
        action->description[255] = L'\0';
    } else {
        wcscpy(action->description, L"Automatic recovery");
    }

    handler->strategies->actionCount++;

    LeaveCriticalSection(&handler->strategies->lock);
    return TRUE;
}

/**
 * Execute recovery strategy for a specific error code
 */
BOOL ExecuteRecoveryStrategy(ErrorHandler* handler, StandardErrorCode code, const void* context) {
    if (!handler || !handler->strategies) {
        return FALSE;
    }

    EnterCriticalSection(&handler->strategies->lock);

    for (int i = 0; i < handler->strategies->actionCount; i++) {
        RecoveryAction* action = &handler->strategies->actions[i];
        if (action->triggerCode == code && action->currentAttempts < action->maxAttempts) {
            action->currentAttempts++;
            
            BOOL result = action->recoveryFunction(context);
            
            if (result) {
                action->currentAttempts = 0; // Reset on successful recovery
            }
            
            LeaveCriticalSection(&handler->strategies->lock);
            return result;
        }
    }

    LeaveCriticalSection(&handler->strategies->lock);
    return FALSE;
}

/**
 * Clear all recovery strategies
 */
void ClearRecoveryStrategies(ErrorHandler* handler) {
    if (!handler || !handler->strategies) {
        return;
    }

    EnterCriticalSection(&handler->strategies->lock);
    ZeroMemory(handler->strategies->actions, sizeof(handler->strategies->actions));
    handler->strategies->actionCount = 0;
    LeaveCriticalSection(&handler->strategies->lock);
}