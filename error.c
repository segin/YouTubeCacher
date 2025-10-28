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

/**
 * Create a new error context with automatic context population
 * Allocates and initializes an ErrorContext structure with basic information
 */
ErrorContext* CreateErrorContext(StandardErrorCode code, ErrorSeverity severity,
                                const wchar_t* function, const wchar_t* file, int line) {
    ErrorContext* context = (ErrorContext*)malloc(sizeof(ErrorContext));
    if (!context) {
        return NULL;
    }

    // Initialize all fields to zero
    ZeroMemory(context, sizeof(ErrorContext));

    // Set basic error information
    context->errorCode = code;
    context->severity = severity;
    context->lineNumber = line;
    context->systemErrorCode = GetLastError();
    context->threadId = GetCurrentThreadId();
    context->contextVariableCount = 0;

    // Get current timestamp
    GetSystemTime(&context->timestamp);

    // Copy function name safely
    if (function) {
        wcsncpy(context->functionName, function, 127);
        context->functionName[127] = L'\0';
    } else {
        wcscpy(context->functionName, L"Unknown");
    }

    // Copy file name safely (extract just the filename, not full path)
    if (file) {
        const wchar_t* fileName = wcsrchr(file, L'\\');
        if (fileName) {
            fileName++; // Skip the backslash
        } else {
            fileName = file;
        }
        wcsncpy(context->fileName, fileName, 255);
        context->fileName[255] = L'\0';
    } else {
        wcscpy(context->fileName, L"Unknown");
    }

    // Set default technical message based on error code
    const wchar_t* errorDescription = GetErrorCodeString(code);
    wcsncpy(context->technicalMessage, errorDescription, 511);
    context->technicalMessage[511] = L'\0';

    // Set default user message (will be overridden by SetUserFriendlyMessage if called)
    wcscpy(context->userMessage, L"An error occurred. Please try again.");

    // Initialize additional context with basic system information
    swprintf(context->additionalContext, 1024,
        L"Process ID: %lu\r\n"
        L"Thread ID: %lu\r\n"
        L"System Error: %lu\r\n"
        L"Timestamp: %04d-%02d-%02d %02d:%02d:%02d.%03d UTC\r\n",
        GetCurrentProcessId(),
        context->threadId,
        context->systemErrorCode,
        context->timestamp.wYear, context->timestamp.wMonth, context->timestamp.wDay,
        context->timestamp.wHour, context->timestamp.wMinute, context->timestamp.wSecond,
        context->timestamp.wMilliseconds);

    return context;
}

/**
 * Add a context variable to the error context for debugging
 * Stores name-value pairs that can be used for detailed error analysis
 */
void AddContextVariable(ErrorContext* context, const wchar_t* name, const wchar_t* value) {
    if (!context || !name || !value) {
        return;
    }

    // Check if we have room for more variables
    if (context->contextVariableCount >= 16) {
        return; // No more room
    }

    int index = context->contextVariableCount;

    // Copy name safely
    wcsncpy(context->contextVariables[index].name, name, 63);
    context->contextVariables[index].name[63] = L'\0';

    // Copy value safely
    wcsncpy(context->contextVariables[index].value, value, 255);
    context->contextVariables[index].value[255] = L'\0';

    context->contextVariableCount++;

    // Update additional context with the new variable
    wchar_t variableInfo[384];
    swprintf(variableInfo, 384, L"%ls: %ls\r\n", name, value);

    // Append to additional context if there's room
    size_t currentLen = wcslen(context->additionalContext);
    size_t newLen = wcslen(variableInfo);
    if (currentLen + newLen < 1023) {
        wcscat(context->additionalContext, variableInfo);
    }
}

/**
 * Set a user-friendly message for dialog display
 * Replaces the default user message with a more descriptive, user-facing message
 */
void SetUserFriendlyMessage(ErrorContext* context, const wchar_t* message) {
    if (!context || !message) {
        return;
    }

    wcsncpy(context->userMessage, message, 511);
    context->userMessage[511] = L'\0';
}

/**
 * Capture call stack information for debugging
 * Uses Windows API to capture the current call stack and store it in the context
 */
void CaptureCallStack(ErrorContext* context) {
    if (!context) {
        return;
    }

    // Initialize call stack buffer
    wcscpy(context->callStack, L"Call Stack:\r\n");

    // Use Windows API to capture stack trace
    // Note: This is a simplified implementation. A full implementation would use
    // SymInitialize, SymFromAddr, etc. from dbghelp.dll for detailed stack traces
    
    // For now, we'll capture basic information about the current function context
    wchar_t stackInfo[1024];
    swprintf(stackInfo, 1024,
        L"  Function: %ls\r\n"
        L"  File: %ls:%d\r\n"
        L"  Thread: %lu\r\n"
        L"  Process: %lu\r\n",
        context->functionName,
        context->fileName,
        context->lineNumber,
        context->threadId,
        GetCurrentProcessId());

    // Append to call stack if there's room
    size_t currentLen = wcslen(context->callStack);
    size_t newLen = wcslen(stackInfo);
    if (currentLen + newLen < 2047) {
        wcscat(context->callStack, stackInfo);
    }

    // Add a note about limited stack trace capability
    const wchar_t* note = L"  (Detailed stack trace requires debug symbols)\r\n";
    currentLen = wcslen(context->callStack);
    if (currentLen + wcslen(note) < 2047) {
        wcscat(context->callStack, note);
    }
}

/**
 * Free an error context and clean up its memory
 * Safely deallocates the ErrorContext structure
 */
void FreeErrorContext(ErrorContext* context) {
    if (context) {
        // Clear sensitive information before freeing
        ZeroMemory(context, sizeof(ErrorContext));
        free(context);
    }
}