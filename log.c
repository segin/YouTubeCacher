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
        
        fwprintf(logFile, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] %ls\n",
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
        
        fwprintf(logFile, L"\n");
        fwprintf(logFile, L"=== YouTubeCacher Session Started: %04d-%02d-%02d %02d:%02d:%02d ===\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);
        fwprintf(logFile, L"=== Version: %ls ===\n", APP_VERSION);
        fwprintf(logFile, L"\n");
        
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
        
        fwprintf(logFile, L"\n");
        fwprintf(logFile, L"=== YouTubeCacher Session Ended: %04d-%02d-%02d %02d:%02d:%02d ===\n",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);
        if (reason) {
            fwprintf(logFile, L"=== Reason: %ls ===\n", reason);
        }
        fwprintf(logFile, L"\n");
        
        fclose(logFile);
    }
}

// Enhanced debug output function
void DebugOutput(const wchar_t* message) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    if (enableDebug) {
        OutputDebugStringW(message);
    }
    WriteToLogfile(message);
}