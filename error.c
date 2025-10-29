#include "YouTubeCacher.h"
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

// ============================================================================
// Error Dialog Management System (UnifiedDialog Integration)
// ============================================================================

// Local structure for building error dialogs (not exposed in header)
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

// Local function declarations
static ErrorDialogBuilder* CreateErrorDialogBuilder(const ErrorContext* context);
static UnifiedDialogConfig* BuildUnifiedDialogConfig(const ErrorDialogBuilder* builder);
static void FreeErrorDialogBuilder(ErrorDialogBuilder* builder);
static UnifiedDialogType MapSeverityToDialogType(ErrorSeverity severity);

/**
 * Map error severity to UnifiedDialogType for proper visual styling
 */
static UnifiedDialogType MapSeverityToDialogType(ErrorSeverity severity) {
    switch (severity) {
        case YTC_SEVERITY_INFO:
            return UNIFIED_DIALOG_INFO;
        case YTC_SEVERITY_WARNING:
            return UNIFIED_DIALOG_WARNING;
        case YTC_SEVERITY_ERROR:
        case YTC_SEVERITY_FATAL:
            return UNIFIED_DIALOG_ERROR;
        default:
            return UNIFIED_DIALOG_ERROR;
    }
}

/**
 * Format technical details from error context for display in dialog
 */
void FormatTechnicalDetails(const ErrorContext* context, wchar_t* buffer, size_t bufferSize) {
    if (!context || !buffer || bufferSize == 0) {
        return;
    }

    swprintf(buffer, bufferSize,
        L"Error Code: %d (%ls)\r\n"
        L"Severity: %ls\r\n"
        L"Function: %ls\r\n"
        L"File: %ls (Line %d)\r\n"
        L"System Error: %lu\r\n"
        L"Thread ID: %lu\r\n"
        L"Timestamp: %04d-%02d-%02d %02d:%02d:%02d UTC\r\n"
        L"\r\n"
        L"Technical Message:\r\n"
        L"%ls\r\n",
        context->errorCode,
        GetErrorCodeString(context->errorCode),
        GetSeverityString(context->severity),
        context->functionName,
        context->fileName,
        context->lineNumber,
        context->systemErrorCode,
        context->threadId,
        context->timestamp.wYear, context->timestamp.wMonth, context->timestamp.wDay,
        context->timestamp.wHour, context->timestamp.wMinute, context->timestamp.wSecond,
        context->technicalMessage);

    // Add context variables if any
    if (context->contextVariableCount > 0) {
        wcscat(buffer, L"\r\nContext Variables:\r\n");
        for (int i = 0; i < context->contextVariableCount; i++) {
            wchar_t varInfo[384];
            swprintf(varInfo, 384, L"  %ls: %ls\r\n", 
                context->contextVariables[i].name, 
                context->contextVariables[i].value);
            wcscat(buffer, varInfo);
        }
    }
}

/**
 * Format diagnostic information for display in dialog
 */
void FormatDiagnosticInfo(const ErrorContext* context, wchar_t* buffer, size_t bufferSize) {
    if (!context || !buffer || bufferSize == 0) {
        return;
    }

    // Get memory status
    MEMORYSTATUSEX memStatus = {0};
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);

    // Get disk space for current directory
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    GetDiskFreeSpaceExW(L".", &freeBytesAvailable, &totalBytes, &totalFreeBytes);

    swprintf(buffer, bufferSize,
        L"System Diagnostics:\r\n"
        L"\r\n"
        L"Memory Status:\r\n"
        L"  Physical Memory: %llu MB used / %llu MB total\r\n"
        L"  Virtual Memory: %llu MB used / %llu MB total\r\n"
        L"  Memory Load: %lu%%\r\n"
        L"\r\n"
        L"Disk Space:\r\n"
        L"  Available: %llu MB\r\n"
        L"  Total: %llu MB\r\n"
        L"\r\n"
        L"Process Information:\r\n"
        L"  Process ID: %lu\r\n"
        L"  Thread ID: %lu\r\n"
        L"\r\n"
        L"Additional Context:\r\n"
        L"%ls\r\n"
        L"\r\n"
        L"Call Stack:\r\n"
        L"%ls",
        (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024),
        memStatus.ullTotalPhys / (1024 * 1024),
        (memStatus.ullTotalVirtual - memStatus.ullAvailVirtual) / (1024 * 1024),
        memStatus.ullTotalVirtual / (1024 * 1024),
        memStatus.dwMemoryLoad,
        freeBytesAvailable.QuadPart / (1024 * 1024),
        totalBytes.QuadPart / (1024 * 1024),
        GetCurrentProcessId(),
        context->threadId,
        context->additionalContext,
        context->callStack);
}

/**
 * Format solution suggestions based on error code
 */
void FormatSolutionSuggestions(StandardErrorCode errorCode, wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return;
    }

    const wchar_t* suggestions = L"";

    switch (errorCode) {
        case YTC_ERROR_MEMORY_ALLOCATION:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Close other applications to free memory\r\n"
                         L"• Restart YouTubeCacher to clear memory leaks\r\n"
                         L"• Check available system memory\r\n"
                         L"• Consider reducing cache size in settings";
            break;

        case YTC_ERROR_FILE_NOT_FOUND:
        case YTC_ERROR_FILE_ACCESS:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Verify the file path is correct\r\n"
                         L"• Check file permissions\r\n"
                         L"• Ensure the file is not in use by another program\r\n"
                         L"• Try running YouTubeCacher as administrator";
            break;

        case YTC_ERROR_NETWORK_FAILURE:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Check your internet connection\r\n"
                         L"• Verify proxy settings if applicable\r\n"
                         L"• Try again in a few minutes\r\n"
                         L"• Check if YouTube is accessible in your browser";
            break;

        case YTC_ERROR_YTDLP_EXECUTION:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Update yt-dlp to the latest version\r\n"
                         L"• Check if the video URL is valid and accessible\r\n"
                         L"• Verify yt-dlp is properly installed\r\n"
                         L"• Try a different video URL to test";
            break;

        case YTC_ERROR_DISK_FULL:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Free up disk space by deleting unnecessary files\r\n"
                         L"• Clear the cache directory\r\n"
                         L"• Move cache to a different drive with more space\r\n"
                         L"• Check disk cleanup utilities";
            break;

        case YTC_ERROR_PERMISSION_DENIED:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Run YouTubeCacher as administrator\r\n"
                         L"• Check folder permissions for cache directory\r\n"
                         L"• Ensure antivirus is not blocking the operation\r\n"
                         L"• Try changing the cache directory location";
            break;

        case YTC_ERROR_URL_INVALID:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Verify the URL is a valid YouTube link\r\n"
                         L"• Check for typos in the URL\r\n"
                         L"• Try copying the URL directly from YouTube\r\n"
                         L"• Ensure the video is publicly accessible";
            break;

        case YTC_ERROR_TIMEOUT:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Check your internet connection speed\r\n"
                         L"• Try again when network conditions improve\r\n"
                         L"• Consider increasing timeout settings\r\n"
                         L"• Verify the server is responding";
            break;

        default:
            suggestions = L"Suggested Actions:\r\n"
                         L"• Try the operation again\r\n"
                         L"• Restart YouTubeCacher if the problem persists\r\n"
                         L"• Check the log file for additional details\r\n"
                         L"• Contact support if the issue continues";
            break;
    }

    wcsncpy(buffer, suggestions, bufferSize - 1);
    buffer[bufferSize - 1] = L'\0';
}

/**
 * Create an ErrorDialogBuilder from an ErrorContext
 */
static ErrorDialogBuilder* CreateErrorDialogBuilder(const ErrorContext* context) {
    if (!context) {
        return NULL;
    }

    ErrorDialogBuilder* builder = (ErrorDialogBuilder*)malloc(sizeof(ErrorDialogBuilder));
    if (!builder) {
        return NULL;
    }

    ZeroMemory(builder, sizeof(ErrorDialogBuilder));

    // Allocate and set title
    builder->title = (wchar_t*)malloc(256 * sizeof(wchar_t));
    if (builder->title) {
        swprintf(builder->title, 256, L"%ls - %ls", 
            GetSeverityString(context->severity), 
            GetErrorCodeString(context->errorCode));
    }

    // Allocate and set main message (user-friendly)
    builder->message = (wchar_t*)malloc(1024 * sizeof(wchar_t));
    if (builder->message) {
        wcsncpy(builder->message, context->userMessage, 1023);
        builder->message[1023] = L'\0';
    }

    // Allocate and format technical details
    builder->technicalDetails = (wchar_t*)malloc(4096 * sizeof(wchar_t));
    if (builder->technicalDetails) {
        FormatTechnicalDetails(context, builder->technicalDetails, 4096);
    }

    // Allocate and format diagnostics
    builder->diagnostics = (wchar_t*)malloc(8192 * sizeof(wchar_t));
    if (builder->diagnostics) {
        FormatDiagnosticInfo(context, builder->diagnostics, 8192);
    }

    // Allocate and format solutions
    builder->solutions = (wchar_t*)malloc(2048 * sizeof(wchar_t));
    if (builder->solutions) {
        FormatSolutionSuggestions(context->errorCode, builder->solutions, 2048);
    }

    // Set dialog properties
    builder->dialogType = MapSeverityToDialogType(context->severity);
    builder->showCopyButton = TRUE;
    builder->showDetailsButton = TRUE;

    return builder;
}

/**
 * Build a UnifiedDialogConfig from an ErrorDialogBuilder
 */
static UnifiedDialogConfig* BuildUnifiedDialogConfig(const ErrorDialogBuilder* builder) {
    if (!builder) {
        return NULL;
    }

    UnifiedDialogConfig* config = (UnifiedDialogConfig*)malloc(sizeof(UnifiedDialogConfig));
    if (!config) {
        return NULL;
    }

    ZeroMemory(config, sizeof(UnifiedDialogConfig));

    // Set basic properties
    config->dialogType = builder->dialogType;
    config->title = builder->title;
    config->message = builder->message;
    config->details = builder->technicalDetails;
    config->showDetailsButton = builder->showDetailsButton;
    config->showCopyButton = builder->showCopyButton;

    // Set up tabs for additional information
    config->tab1_name = L"Technical Details";
    config->tab2_content = builder->diagnostics;
    config->tab2_name = L"Diagnostics";
    config->tab3_content = builder->solutions;
    config->tab3_name = L"Solutions";

    return config;
}

/**
 * Show an error dialog using the existing UnifiedDialog system
 */
INT_PTR ShowErrorDialog(HWND parent, const ErrorContext* context) {
    if (!context) {
        return IDCANCEL;
    }

    // Create dialog builder from error context
    ErrorDialogBuilder* builder = CreateErrorDialogBuilder(context);
    if (!builder) {
        return IDCANCEL;
    }

    // Build unified dialog config
    UnifiedDialogConfig* config = BuildUnifiedDialogConfig(builder);
    if (!config) {
        FreeErrorDialogBuilder(builder);
        return IDCANCEL;
    }

    // Show the dialog using existing UnifiedDialog system
    INT_PTR result = ShowUnifiedDialog(parent, config);

    // Clean up
    free(config);
    FreeErrorDialogBuilder(builder);

    return result;
}

/**
 * Free an ErrorDialogBuilder and its allocated memory
 */
static void FreeErrorDialogBuilder(ErrorDialogBuilder* builder) {
    if (!builder) {
        return;
    }

    // Free all allocated strings
    if (builder->title) {
        free(builder->title);
    }
    if (builder->message) {
        free(builder->message);
    }
    if (builder->technicalDetails) {
        free(builder->technicalDetails);
    }
    if (builder->diagnostics) {
        free(builder->diagnostics);
    }
    if (builder->solutions) {
        free(builder->solutions);
    }

    // Clear and free the builder itself
    ZeroMemory(builder, sizeof(ErrorDialogBuilder));
    free(builder);
}
// ============================================================================
// Validation Framework Implementation
// ============================================================================

/**
 * Validate a pointer parameter for NULL checking
 * Returns validation result with appropriate error information
 */
ParameterValidationResult ValidatePointer(const void* ptr, const wchar_t* paramName) {
    ParameterValidationResult result = {0};
    
    // Initialize result structure
    result.isValid = TRUE;
    result.errorCode = YTC_ERROR_SUCCESS;
    
    // Set field name
    if (paramName) {
        wcsncpy(result.fieldName, paramName, 127);
        result.fieldName[127] = L'\0';
    } else {
        wcscpy(result.fieldName, L"Unknown Parameter");
    }
    
    // Check for NULL pointer
    if (ptr == NULL) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_INVALID_PARAMETER;
        swprintf(result.errorMessage, 512, 
            L"Parameter '%ls' cannot be NULL", result.fieldName);
    } else {
        wcscpy(result.errorMessage, L"Parameter validation successful");
    }
    
    return result;
}

/**
 * Validate a string parameter for NULL, empty, and length constraints
 * Returns validation result with detailed error information
 */
ParameterValidationResult ValidateString(const wchar_t* str, const wchar_t* paramName, size_t maxLength) {
    ParameterValidationResult result = {0};
    
    // Initialize result structure
    result.isValid = TRUE;
    result.errorCode = YTC_ERROR_SUCCESS;
    
    // Set field name
    if (paramName) {
        wcsncpy(result.fieldName, paramName, 127);
        result.fieldName[127] = L'\0';
    } else {
        wcscpy(result.fieldName, L"String Parameter");
    }
    
    // Check for NULL pointer
    if (str == NULL) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_INVALID_PARAMETER;
        swprintf(result.errorMessage, 512, 
            L"String parameter '%ls' cannot be NULL", result.fieldName);
        return result;
    }
    
    // Check for empty string
    if (wcslen(str) == 0) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_INVALID_PARAMETER;
        swprintf(result.errorMessage, 512, 
            L"String parameter '%ls' cannot be empty", result.fieldName);
        return result;
    }
    
    // Check length constraint
    size_t actualLength = wcslen(str);
    if (maxLength > 0 && actualLength > maxLength) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_BUFFER_OVERFLOW;
        swprintf(result.errorMessage, 512, 
            L"String parameter '%ls' exceeds maximum length of %zu characters (actual: %zu)", 
            result.fieldName, maxLength, actualLength);
        return result;
    }
    
    // Validation successful
    swprintf(result.errorMessage, 512, 
        L"String parameter '%ls' validation successful (length: %zu)", 
        result.fieldName, actualLength);
    
    return result;
}

/**
 * Validate a file path parameter for format and accessibility
 * Returns validation result with file system specific error information
 */
ParameterValidationResult ValidateFilePath(const wchar_t* path, const wchar_t* paramName) {
    ParameterValidationResult result = {0};
    
    // Initialize result structure
    result.isValid = TRUE;
    result.errorCode = YTC_ERROR_SUCCESS;
    
    // Set field name
    if (paramName) {
        wcsncpy(result.fieldName, paramName, 127);
        result.fieldName[127] = L'\0';
    } else {
        wcscpy(result.fieldName, L"File Path");
    }
    
    // First validate as string
    ParameterValidationResult stringResult = ValidateString(path, paramName, MAX_PATH);
    if (!stringResult.isValid) {
        return stringResult; // Return the string validation error
    }
    
    // Check for invalid characters in Windows file paths
    const wchar_t* invalidChars = L"<>:\"|?*";
    for (const wchar_t* invalid = invalidChars; *invalid; invalid++) {
        if (wcschr(path, *invalid) != NULL) {
            result.isValid = FALSE;
            result.errorCode = YTC_ERROR_INVALID_PARAMETER;
            swprintf(result.errorMessage, 512, 
                L"File path '%ls' contains invalid character '%lc'", 
                result.fieldName, *invalid);
            return result;
        }
    }
    
    // Check for reserved names (CON, PRN, AUX, NUL, COM1-9, LPT1-9)
    wchar_t fileName[MAX_PATH];
    wcsncpy(fileName, path, MAX_PATH - 1);
    fileName[MAX_PATH - 1] = L'\0';
    
    // Extract just the filename part
    wchar_t* lastSlash = wcsrchr(fileName, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(fileName, L'/');
    wchar_t* actualFileName = fileName;
    
    if (lastSlash && (!lastForwardSlash || lastSlash > lastForwardSlash)) {
        actualFileName = lastSlash + 1;
    } else if (lastForwardSlash) {
        actualFileName = lastForwardSlash + 1;
    }
    
    // Remove extension for reserved name check
    wchar_t* dot = wcsrchr(actualFileName, L'.');
    if (dot) {
        *dot = L'\0';
    }
    
    // Convert to uppercase for comparison
    _wcsupr(actualFileName);
    
    const wchar_t* reservedNames[] = {
        L"CON", L"PRN", L"AUX", L"NUL",
        L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
        L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"
    };
    
    for (size_t i = 0; i < sizeof(reservedNames) / sizeof(reservedNames[0]); i++) {
        if (wcscmp(actualFileName, reservedNames[i]) == 0) {
            result.isValid = FALSE;
            result.errorCode = YTC_ERROR_INVALID_PARAMETER;
            swprintf(result.errorMessage, 512, 
                L"File path '%ls' uses reserved name '%ls'", 
                result.fieldName, reservedNames[i]);
            return result;
        }
    }
    
    // Check if path is too long
    if (wcslen(path) >= MAX_PATH) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_BUFFER_OVERFLOW;
        swprintf(result.errorMessage, 512, 
            L"File path '%ls' exceeds maximum path length of %d characters", 
            result.fieldName, MAX_PATH);
        return result;
    }
    
    // Validation successful
    swprintf(result.errorMessage, 512, 
        L"File path '%ls' validation successful", result.fieldName);
    
    return result;
}

/**
 * Validate a URL parameter for basic format and YouTube URL patterns
 * Returns validation result with URL-specific error information
 */
ParameterValidationResult ValidateURL(const wchar_t* url, const wchar_t* paramName) {
    ParameterValidationResult result = {0};
    
    // Initialize result structure
    result.isValid = TRUE;
    result.errorCode = YTC_ERROR_SUCCESS;
    
    // Set field name
    if (paramName) {
        wcsncpy(result.fieldName, paramName, 127);
        result.fieldName[127] = L'\0';
    } else {
        wcscpy(result.fieldName, L"URL");
    }
    
    // First validate as string with reasonable URL length limit
    ParameterValidationResult stringResult = ValidateString(url, paramName, 2048);
    if (!stringResult.isValid) {
        return stringResult; // Return the string validation error
    }
    
    // Convert to lowercase for case-insensitive comparison
    wchar_t lowerUrl[2049];
    wcsncpy(lowerUrl, url, 2048);
    lowerUrl[2048] = L'\0';
    _wcslwr(lowerUrl);
    
    // Check for basic URL format (must start with http:// or https://)
    if (wcsncmp(lowerUrl, L"http://", 7) != 0 && wcsncmp(lowerUrl, L"https://", 8) != 0) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_URL_INVALID;
        swprintf(result.errorMessage, 512, 
            L"URL '%ls' must start with 'http://' or 'https://'", result.fieldName);
        return result;
    }
    
    // Check for YouTube domain patterns
    BOOL isYouTubeURL = FALSE;
    const wchar_t* youtubeDomains[] = {
        L"youtube.com",
        L"www.youtube.com",
        L"m.youtube.com",
        L"youtu.be",
        L"www.youtu.be"
    };
    
    for (size_t i = 0; i < sizeof(youtubeDomains) / sizeof(youtubeDomains[0]); i++) {
        if (wcsstr(lowerUrl, youtubeDomains[i]) != NULL) {
            isYouTubeURL = TRUE;
            break;
        }
    }
    
    if (!isYouTubeURL) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_URL_INVALID;
        swprintf(result.errorMessage, 512, 
            L"URL '%ls' is not a valid YouTube URL", result.fieldName);
        return result;
    }
    
    // Check for minimum URL structure (domain + some path/query)
    const wchar_t* domainEnd = wcsstr(lowerUrl + 8, L"/"); // Skip https://
    if (!domainEnd) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_URL_INVALID;
        swprintf(result.errorMessage, 512, 
            L"URL '%ls' appears to be incomplete (missing path or video ID)", result.fieldName);
        return result;
    }
    
    // For YouTube URLs, check for video ID patterns
    if (wcsstr(lowerUrl, L"watch?v=") || wcsstr(lowerUrl, L"youtu.be/") || 
        wcsstr(lowerUrl, L"embed/") || wcsstr(lowerUrl, L"v/")) {
        // URL contains video ID patterns - this is good
    } else {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_URL_INVALID;
        swprintf(result.errorMessage, 512, 
            L"URL '%ls' does not appear to contain a valid YouTube video ID", result.fieldName);
        return result;
    }
    
    // Validation successful
    swprintf(result.errorMessage, 512, 
        L"URL '%ls' validation successful", result.fieldName);
    
    return result;
}

/**
 * Validate buffer size parameters for overflow prevention
 * Returns validation result with buffer size specific error information
 */
ParameterValidationResult ValidateBufferSize(size_t size, size_t minSize, size_t maxSize) {
    ParameterValidationResult result = {0};
    
    // Initialize result structure
    result.isValid = TRUE;
    result.errorCode = YTC_ERROR_SUCCESS;
    wcscpy(result.fieldName, L"Buffer Size");
    
    // Check minimum size constraint
    if (size < minSize) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_INVALID_PARAMETER;
        swprintf(result.errorMessage, 512, 
            L"Buffer size %zu is below minimum required size of %zu bytes", 
            size, minSize);
        return result;
    }
    
    // Check maximum size constraint
    if (maxSize > 0 && size > maxSize) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_BUFFER_OVERFLOW;
        swprintf(result.errorMessage, 512, 
            L"Buffer size %zu exceeds maximum allowed size of %zu bytes", 
            size, maxSize);
        return result;
    }
    
    // Check for reasonable upper bounds to prevent excessive allocations
    const size_t REASONABLE_MAX = 1024 * 1024 * 100; // 100 MB
    if (size > REASONABLE_MAX) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_BUFFER_OVERFLOW;
        swprintf(result.errorMessage, 512, 
            L"Buffer size %zu is unreasonably large (exceeds %zu bytes)", 
            size, REASONABLE_MAX);
        return result;
    }
    
    // Check for zero size (usually invalid)
    if (size == 0) {
        result.isValid = FALSE;
        result.errorCode = YTC_ERROR_INVALID_PARAMETER;
        wcscpy(result.errorMessage, L"Buffer size cannot be zero");
        return result;
    }
    
    // Validation successful
    swprintf(result.errorMessage, 512, 
        L"Buffer size %zu validation successful (min: %zu, max: %zu)", 
        size, minSize, maxSize);
    
    return result;
}