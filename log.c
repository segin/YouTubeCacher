#include "YouTubeCacher.h"

// Function to write debug message to logfile
void WriteToLogfile(const wchar_t* message) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    FILE* logFile = _wfopen(L"YouTubeCacher-log.txt", L"a");
    if (logFile) {
        // Get current timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        fwprintf(logFile, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] %ls\r\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                message);
        
        fclose(logFile);
    }
}

// Function to write session start marker to logfile
void WriteSessionStartToLogfile(void) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    FILE* logFile = _wfopen(L"YouTubeCacher-log.txt", L"a");
    if (logFile) {
        // Get current timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        fwprintf(logFile, L"\r\n");
        fwprintf(logFile, L"=== YouTubeCacher Session Started: %04d-%02d-%02d %02d:%02d:%02d ===\r\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);
        fwprintf(logFile, L"=== Version: %ls ===\r\n", APP_VERSION);
        fwprintf(logFile, L"\r\n");
        
        fclose(logFile);
    }
}

// Function to write session end marker to logfile
void WriteSessionEndToLogfile(const wchar_t* reason) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    FILE* logFile = _wfopen(L"YouTubeCacher-log.txt", L"a");
    if (logFile) {
        // Get current timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        fwprintf(logFile, L"\r\n");
        fwprintf(logFile, L"=== YouTubeCacher Session Ended: %04d-%02d-%02d %02d:%02d:%02d ===\r\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);
        if (reason) {
            fwprintf(logFile, L"=== Reason: %ls ===\r\n", reason);
        }
        fwprintf(logFile, L"\r\n");
        
        fclose(logFile);
    }
}

// Enhanced debug output function
void DebugOutput(const wchar_t* message) {
    // Always output to debug console
    OutputDebugStringW(message);
    
    // Now we can safely check debug state without circular dependency
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    
    if (enableLogfile) {
        WriteToLogfile(message);
    }
}

// Write structured error information to logfile with Windows line endings
void WriteStructuredErrorToLogfile(const wchar_t* severity, int errorCode, 
                                  const wchar_t* function, const wchar_t* file, 
                                  int line, const wchar_t* message) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    FILE* logFile = _wfopen(L"YouTubeCacher-log.txt", L"a");
    if (logFile) {
        // Get current timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        // Write structured error entry with Windows line endings
        fwprintf(logFile, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%ls] [%d] %ls\r\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                severity, errorCode, message);
        
        fwprintf(logFile, L"  Function: %ls\r\n", function ? function : L"Unknown");
        fwprintf(logFile, L"  File: %ls:%d\r\n", file ? file : L"Unknown", line);
        fwprintf(logFile, L"  Thread: %lu\r\n", GetCurrentThreadId());
        fwprintf(logFile, L"\r\n");
        
        fclose(logFile);
    }
}

// Write complete error context to logfile with detailed information
void WriteErrorContextToLogfile(const ErrorContext* context) {
    if (!context) return;
    
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    FILE* logFile = _wfopen(L"YouTubeCacher-log.txt", L"a");
    if (logFile) {
        // Write error context header with Windows line endings
        fwprintf(logFile, L"=== ERROR CONTEXT ===\r\n");
        
        // Basic error information
        fwprintf(logFile, L"Error Code: %d\r\n", context->errorCode);
        fwprintf(logFile, L"Severity: %d\r\n", context->severity);
        fwprintf(logFile, L"Function: %ls\r\n", context->functionName);
        fwprintf(logFile, L"File: %ls:%d\r\n", context->fileName, context->lineNumber);
        fwprintf(logFile, L"Thread ID: %lu\r\n", context->threadId);
        fwprintf(logFile, L"System Error: %lu\r\n", context->systemErrorCode);
        
        // Timestamp
        fwprintf(logFile, L"Timestamp: %04d-%02d-%02d %02d:%02d:%02d.%03d UTC\r\n",
                context->timestamp.wYear, context->timestamp.wMonth, context->timestamp.wDay,
                context->timestamp.wHour, context->timestamp.wMinute, 
                context->timestamp.wSecond, context->timestamp.wMilliseconds);
        
        // Messages
        fwprintf(logFile, L"Technical Message: %ls\r\n", context->technicalMessage);
        fwprintf(logFile, L"User Message: %ls\r\n", context->userMessage);
        
        // Additional context
        if (wcslen(context->additionalContext) > 0) {
            fwprintf(logFile, L"Additional Context:\r\n%ls\r\n", context->additionalContext);
        }
        
        // Context variables
        if (context->contextVariableCount > 0) {
            fwprintf(logFile, L"Context Variables:\r\n");
            for (int i = 0; i < context->contextVariableCount; i++) {
                fwprintf(logFile, L"  %ls: %ls\r\n", 
                        context->contextVariables[i].name,
                        context->contextVariables[i].value);
            }
        }
        
        // Call stack
        if (wcslen(context->callStack) > 0) {
            fwprintf(logFile, L"Call Stack:\r\n%ls\r\n", context->callStack);
        }
        
        fwprintf(logFile, L"=== END ERROR CONTEXT ===\r\n\r\n");
        
        fclose(logFile);
    }
}

// Enhanced debug output with error context
void DebugOutputWithContext(const ErrorContext* context) {
    if (!context) return;
    
    // Create a formatted debug message
    wchar_t debugMessage[2048];
    swprintf(debugMessage, 2048, 
            L"[ERROR] %ls in %ls (%ls:%d) - %ls",
            GetErrorCodeString(context->errorCode),
            context->functionName,
            context->fileName,
            context->lineNumber,
            context->technicalMessage);
    
    // Output to debug console
    OutputDebugStringW(debugMessage);
    OutputDebugStringW(L"\r\n");
    
    // Write full context to logfile
    WriteErrorContextToLogfile(context);
}