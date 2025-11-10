#include "YouTubeCacher.h"

// Function to write debug message to logfile
void WriteToLogfile(const wchar_t* message) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    // Open log file for append using Windows API
    HANDLE hLogFile = CreateFileW(L"YouTubeCacher-log.txt",
                                  GENERIC_WRITE,
                                  FILE_SHARE_READ,
                                  NULL,
                                  OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    
    if (hLogFile != INVALID_HANDLE_VALUE) {
        // Move to end of file
        SetFilePointer(hLogFile, 0, NULL, FILE_END);
        
        // Get current timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        // Create a copy of the message to strip trailing newlines
        wchar_t* cleanMessage = SAFE_WCSDUP(message);
        if (cleanMessage) {
            // Strip trailing \r\n, \n, or \r
            size_t len = wcslen(cleanMessage);
            while (len > 0 && (cleanMessage[len-1] == L'\r' || cleanMessage[len-1] == L'\n')) {
                cleanMessage[--len] = L'\0';
            }
            
            // Format log entry with Windows line endings
            wchar_t logEntry[4096];
            swprintf(logEntry, 4096, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] %ls\r\n",
                    st.wYear, st.wMonth, st.wDay,
                    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                    cleanMessage);
            
            // Convert to UTF-8 for file writing
            int utf8Size = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, NULL, 0, NULL, NULL);
            if (utf8Size > 0) {
                char* utf8Buffer = (char*)SAFE_MALLOC(utf8Size);
                if (utf8Buffer) {
                    WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
                    
                    DWORD bytesWritten;
                    WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
                    FlushFileBuffers(hLogFile);
                    
                    SAFE_FREE(utf8Buffer);
                }
            }
            
            SAFE_FREE(cleanMessage);
        }
        
        CloseHandle(hLogFile);
    }
}

// Function to write session start marker to logfile
void WriteSessionStartToLogfile(void) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    // Open log file for append using Windows API
    HANDLE hLogFile = CreateFileW(L"YouTubeCacher-log.txt",
                                  GENERIC_WRITE,
                                  FILE_SHARE_READ,
                                  NULL,
                                  OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    
    if (hLogFile != INVALID_HANDLE_VALUE) {
        // Move to end of file
        SetFilePointer(hLogFile, 0, NULL, FILE_END);
        
        // Get current timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        // Format log entry with Windows line endings
        wchar_t logEntry[1024];
        swprintf(logEntry, 1024, 
                L"=== YouTubeCacher Session Started: %04d-%02d-%02d %02d:%02d:%02d ===\r\n"
                L"=== Version: %ls ===\r\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond,
                APP_VERSION);
        
        // Convert to UTF-8 for file writing
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, NULL, 0, NULL, NULL);
        if (utf8Size > 0) {
            char* utf8Buffer = (char*)SAFE_MALLOC(utf8Size);
            if (utf8Buffer) {
                WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
                
                DWORD bytesWritten;
                WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
                FlushFileBuffers(hLogFile);
                
                SAFE_FREE(utf8Buffer);
            }
        }
        
        CloseHandle(hLogFile);
    }
}

// Function to write session end marker to logfile
void WriteSessionEndToLogfile(const wchar_t* reason) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    // Open log file for append using Windows API
    HANDLE hLogFile = CreateFileW(L"YouTubeCacher-log.txt",
                                  GENERIC_WRITE,
                                  FILE_SHARE_READ,
                                  NULL,
                                  OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    
    if (hLogFile != INVALID_HANDLE_VALUE) {
        // Move to end of file
        SetFilePointer(hLogFile, 0, NULL, FILE_END);
        
        // Get current timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        // Format log entry with Windows line endings
        wchar_t logEntry[1024];
        if (reason) {
            swprintf(logEntry, 1024,
                    L"=== YouTubeCacher Session Ended: %04d-%02d-%02d %02d:%02d:%02d ===\r\n"
                    L"=== Reason: %ls ===\r\n",
                    st.wYear, st.wMonth, st.wDay,
                    st.wHour, st.wMinute, st.wSecond,
                    reason);
        } else {
            swprintf(logEntry, 1024,
                    L"=== YouTubeCacher Session Ended: %04d-%02d-%02d %02d:%02d:%02d ===\r\n",
                    st.wYear, st.wMonth, st.wDay,
                    st.wHour, st.wMinute, st.wSecond);
        }
        
        // Convert to UTF-8 for file writing
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, NULL, 0, NULL, NULL);
        if (utf8Size > 0) {
            char* utf8Buffer = (char*)SAFE_MALLOC(utf8Size);
            if (utf8Buffer) {
                WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
                
                DWORD bytesWritten;
                WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
                FlushFileBuffers(hLogFile);
                
                SAFE_FREE(utf8Buffer);
            }
        }
        
        CloseHandle(hLogFile);
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
    
    // Open log file for append using Windows API
    HANDLE hLogFile = CreateFileW(L"YouTubeCacher-log.txt",
                                  GENERIC_WRITE,
                                  FILE_SHARE_READ,
                                  NULL,
                                  OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    
    if (hLogFile != INVALID_HANDLE_VALUE) {
        // Move to end of file
        SetFilePointer(hLogFile, 0, NULL, FILE_END);
        
        // Get current timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        // Format log entry with Windows line endings
        wchar_t logEntry[4096];
        swprintf(logEntry, 4096,
                L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%ls] [%d] %ls\r\n"
                L"  Function: %ls\r\n"
                L"  File: %ls:%d\r\n"
                L"  Thread: %lu\r\n"
                L"\r\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                severity, errorCode, message,
                function ? function : L"Unknown",
                file ? file : L"Unknown", line,
                GetCurrentThreadId());
        
        // Convert to UTF-8 for file writing
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, NULL, 0, NULL, NULL);
        if (utf8Size > 0) {
            char* utf8Buffer = (char*)SAFE_MALLOC(utf8Size);
            if (utf8Buffer) {
                WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
                
                DWORD bytesWritten;
                WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
                FlushFileBuffers(hLogFile);
                
                SAFE_FREE(utf8Buffer);
            }
        }
        
        CloseHandle(hLogFile);
    }
}

// Write complete error context to logfile with detailed information
void WriteErrorContextToLogfile(const ErrorContext* context) {
    if (!context) return;
    
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (!enableLogfile) return;
    
    // Open log file for append using Windows API
    HANDLE hLogFile = CreateFileW(L"YouTubeCacher-log.txt",
                                  GENERIC_WRITE,
                                  FILE_SHARE_READ,
                                  NULL,
                                  OPEN_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    
    if (hLogFile != INVALID_HANDLE_VALUE) {
        // Move to end of file
        SetFilePointer(hLogFile, 0, NULL, FILE_END);
        
        // Build the complete log entry with Windows line endings
        wchar_t logEntry[16384];
        int offset = 0;
        
        offset += swprintf(logEntry + offset, 16384 - offset, L"=== ERROR CONTEXT ===\r\n");
        
        // Basic error information
        offset += swprintf(logEntry + offset, 16384 - offset, L"Error Code: %d\r\n", context->errorCode);
        offset += swprintf(logEntry + offset, 16384 - offset, L"Severity: %d\r\n", context->severity);
        offset += swprintf(logEntry + offset, 16384 - offset, L"Function: %ls\r\n", context->functionName);
        offset += swprintf(logEntry + offset, 16384 - offset, L"File: %ls:%d\r\n", context->fileName, context->lineNumber);
        offset += swprintf(logEntry + offset, 16384 - offset, L"Thread ID: %lu\r\n", context->threadId);
        offset += swprintf(logEntry + offset, 16384 - offset, L"System Error: %lu\r\n", context->systemErrorCode);
        
        // Timestamp
        offset += swprintf(logEntry + offset, 16384 - offset,
                L"Timestamp: %04d-%02d-%02d %02d:%02d:%02d.%03d UTC\r\n",
                context->timestamp.wYear, context->timestamp.wMonth, context->timestamp.wDay,
                context->timestamp.wHour, context->timestamp.wMinute, 
                context->timestamp.wSecond, context->timestamp.wMilliseconds);
        
        // Messages
        offset += swprintf(logEntry + offset, 16384 - offset, L"Technical Message: %ls\r\n", context->technicalMessage);
        offset += swprintf(logEntry + offset, 16384 - offset, L"User Message: %ls\r\n", context->userMessage);
        
        // Additional context
        if (wcslen(context->additionalContext) > 0) {
            offset += swprintf(logEntry + offset, 16384 - offset, L"Additional Context:\r\n%ls\r\n", context->additionalContext);
        }
        
        // Context variables
        if (context->contextVariableCount > 0) {
            offset += swprintf(logEntry + offset, 16384 - offset, L"Context Variables:\r\n");
            for (int i = 0; i < context->contextVariableCount && offset < 16000; i++) {
                offset += swprintf(logEntry + offset, 16384 - offset, L"  %ls: %ls\r\n", 
                        context->contextVariables[i].name,
                        context->contextVariables[i].value);
            }
        }
        
        // Call stack
        if (wcslen(context->callStack) > 0) {
            offset += swprintf(logEntry + offset, 16384 - offset, L"Call Stack:\r\n%ls\r\n", context->callStack);
        }
        
        offset += swprintf(logEntry + offset, 16384 - offset, L"=== END ERROR CONTEXT ===\r\n\r\n");
        
        // Convert to UTF-8 for file writing
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, NULL, 0, NULL, NULL);
        if (utf8Size > 0) {
            char* utf8Buffer = (char*)SAFE_MALLOC(utf8Size);
            if (utf8Buffer) {
                WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
                
                DWORD bytesWritten;
                WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
                FlushFileBuffers(hLogFile);
                
                SAFE_FREE(utf8Buffer);
            }
        }
        
        CloseHandle(hLogFile);
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