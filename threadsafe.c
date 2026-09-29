#include "YouTubeCacher.h"
#include <stdarg.h>

// Global synchronization objects for coordinated access to global systems
static CRITICAL_SECTION g_errorHandlerLock;
static CRITICAL_SECTION g_memoryManagerLock;
static CRITICAL_SECTION g_appStateLock;
static CRITICAL_SECTION g_debugOutputLock;
static BOOL g_threadSafetyInitialized = FALSE;

// External global variable declarations (for direct access when needed)
extern ErrorHandler g_ErrorHandler;

/**
 * Initialize the thread safety system
 * Sets up critical sections for protecting global variables
 */
BOOL InitializeThreadSafety(void) {
    if (g_threadSafetyInitialized) {
        return TRUE; // Already initialized
    }

    // Initialize critical sections for each global variable
    InitializeCriticalSection(&g_errorHandlerLock);
    InitializeCriticalSection(&g_memoryManagerLock);
    InitializeCriticalSection(&g_appStateLock);
    InitializeCriticalSection(&g_debugOutputLock);

    g_threadSafetyInitialized = TRUE;
    return TRUE;
}

/**
 * Clean up the thread safety system
 * Releases all critical sections
 */
void CleanupThreadSafety(void) {
    if (!g_threadSafetyInitialized) {
        return;
    }

    // Delete critical sections
    DeleteCriticalSection(&g_errorHandlerLock);
    DeleteCriticalSection(&g_memoryManagerLock);
    DeleteCriticalSection(&g_appStateLock);
    DeleteCriticalSection(&g_debugOutputLock);

    g_threadSafetyInitialized = FALSE;
}

/**
 * Lock the global error handler for exclusive access
 */
void LockErrorHandler(void) {
    if (!g_threadSafetyInitialized) {
        InitializeThreadSafety();
    }
    EnterCriticalSection(&g_errorHandlerLock);
}

/**
 * Unlock the global error handler
 */
void UnlockErrorHandler(void) {
    if (g_threadSafetyInitialized) {
        LeaveCriticalSection(&g_errorHandlerLock);
    }
}

/**
 * Get a pointer to the global error handler
 * Note: Caller must call LockErrorHandler() before using and UnlockErrorHandler() when done
 */
ErrorHandler* GetErrorHandler(void) {
    return &g_ErrorHandler;
}

/**
 * Lock the global memory manager for coordinated access
 * This provides an additional layer of synchronization for operations that need
 * to coordinate between memory management and other systems
 */
void LockMemoryManager(void) {
    if (!g_threadSafetyInitialized) {
        InitializeThreadSafety();
    }
    EnterCriticalSection(&g_memoryManagerLock);
}

/**
 * Unlock the global memory manager
 */
void UnlockMemoryManager(void) {
    if (g_threadSafetyInitialized) {
        LeaveCriticalSection(&g_memoryManagerLock);
    }
}

/**
 * Get access to memory manager
 * Note: The memory manager is accessed through its API functions (SAFE_MALLOC, etc.)
 * This function provides coordination locking only and returns NULL
 * Caller must call LockMemoryManager() before coordinated operations and UnlockMemoryManager() when done
 */
MemoryManager* GetMemoryManager(void) {
    // Memory manager is accessed through its API functions, not direct pointer access
    // This function exists for interface consistency and coordination locking
    return NULL;
}

/**
 * Lock the global application state for coordinated access
 * This provides an additional layer of synchronization for operations that need
 * to coordinate between application state and other systems
 */
void LockAppState(void) {
    if (!g_threadSafetyInitialized) {
        InitializeThreadSafety();
    }
    EnterCriticalSection(&g_appStateLock);
}

/**
 * Unlock the global application state
 */
void UnlockAppState(void) {
    if (g_threadSafetyInitialized) {
        LeaveCriticalSection(&g_appStateLock);
    }
}

/**
 * Get a pointer to the global application state
 * Note: Caller must call LockAppState() before using and UnlockAppState() when done
 * This provides coordinated access - the application state has its own internal locking
 */
ApplicationState* GetAppState(void) {
    return GetApplicationState();
}

/**
 * Check if the thread safety system is initialized
 */
BOOL IsThreadSafetyInitialized(void) {
    return g_threadSafetyInitialized;
}

/**
 * Thread-safe debug output function
 * Protects debug output operations with critical section and includes thread ID
 */
void ThreadSafeDebugOutput(const wchar_t* message) {
    if (!message) {
        return;
    }

    // Initialize thread safety if not already done
    if (!g_threadSafetyInitialized) {
        InitializeThreadSafety();
    }

    // Enter critical section to ensure atomic debug output
    EnterCriticalSection(&g_debugOutputLock);

    // Create message with thread ID for debugging
    DWORD threadId = GetCurrentThreadId();
    wchar_t threadedMessage[2048];
    swprintf(threadedMessage, 2048, L"[Thread %lu] %ls", threadId, message);

    // Call the original DebugOutput function
    DebugOutput(threadedMessage);

    // Leave critical section
    LeaveCriticalSection(&g_debugOutputLock);
}

/**
 * Thread-safe formatted debug output function
 * Protects debug output operations with critical section and includes thread ID
 */
void ThreadSafeDebugOutputF(const wchar_t* format, ...) {
    if (!format) {
        return;
    }

    // Initialize thread safety if not already done
    if (!g_threadSafetyInitialized) {
        InitializeThreadSafety();
    }

    // Format the message using variable arguments
    va_list args;
    va_start(args, format);
    
    wchar_t formattedMessage[2048];
    vswprintf(formattedMessage, 2048, format, args);
    
    va_end(args);

    // Enter critical section to ensure atomic debug output
    EnterCriticalSection(&g_debugOutputLock);

    // Create message with thread ID for debugging
    DWORD threadId = GetCurrentThreadId();
    wchar_t threadedMessage[2048];
    swprintf(threadedMessage, 2048, L"[Thread %lu] %ls", threadId, formattedMessage);

    // Call the original DebugOutput function
    DebugOutput(threadedMessage);

    // Leave critical section
    LeaveCriticalSection(&g_debugOutputLock);
}

// Thread-safe subprocess context implementation

/**
 * Initialize a thread-safe subprocess context
 * Sets up all critical sections and initializes state
 */
BOOL InitializeThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context) {
    if (!context) {
        return FALSE;
    }

    // Zero out the structure
    memset(context, 0, sizeof(ThreadSafeSubprocessContext));

    // Initialize critical sections
    InitializeCriticalSection(&context->processStateLock);
    InitializeCriticalSection(&context->outputLock);
    InitializeCriticalSection(&context->configLock);

    // Create cancellation event
    context->cancellationEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!context->cancellationEvent) {
        DeleteCriticalSection(&context->processStateLock);
        DeleteCriticalSection(&context->outputLock);
        DeleteCriticalSection(&context->configLock);
        return FALSE;
    }

    // Initialize output buffer
    EnterCriticalSection(&context->outputLock);
    context->outputBufferSize = 8192; // Start with 8KB buffer
    context->outputBuffer = (wchar_t*)SAFE_MALLOC(context->outputBufferSize * sizeof(wchar_t));
    if (context->outputBuffer) {
        context->outputBuffer[0] = L'\0';
        context->outputLength = 0;
    }
    LeaveCriticalSection(&context->outputLock);

    if (!context->outputBuffer) {
        CloseHandle(context->cancellationEvent);
        context->cancellationEvent = NULL;
        DeleteCriticalSection(&context->processStateLock);
        DeleteCriticalSection(&context->outputLock);
        DeleteCriticalSection(&context->configLock);
        return FALSE;
    }

    // Set default timeout
    EnterCriticalSection(&context->configLock);
    context->timeoutMs = 300000; // 5 minutes default
    LeaveCriticalSection(&context->configLock);

    context->initialized = TRUE;
    return TRUE;
}

/**
 * Clean up a thread-safe subprocess context
 * Stops the child process and joins the output reader thread, then releases
 * all resources and critical sections.
 * Returns FALSE if the reader thread did not exit: the context is then left
 * initialized and allocated for that thread, and the caller must not free it.
 */
BOOL CleanupThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context) {
    if (!context) {
        return TRUE;
    }

    // Defensive: Check if context memory looks valid
    // If initialized flag is corrupted (not 0 or 1), bail out
    if (context->initialized != TRUE && context->initialized != FALSE) {
        OutputDebugStringW(L"CleanupThreadSafeSubprocessContext: Context appears corrupted, skipping cleanup\r\n");
        return FALSE;
    }

    // Check if already cleaned up to prevent double-cleanup
    if (!context->initialized) {
        return TRUE;
    }

    // Stop the child and the reader while the context is still initialized:
    // the cancel, wait and kill functions do nothing on an uninitialized context

    // Only stop the process if it is still running
    // If it's already completed, no need to stop it
    if (!context->processCompleted) {
        EnterCriticalSection(&context->processStateLock);
        HANDLE hProcess = context->hProcess;
        LeaveCriticalSection(&context->processStateLock);

        // There is no graceful stop to try first: a GUI process has no console
        // to deliver Ctrl+C to a CREATE_NO_WINDOW child. Terminate it, then
        // wait for it to exit (TerminateProcess is asynchronous).
        ForceKillThreadSafeSubprocess(context);
        if (hProcess) {
            WaitForSingleObject(hProcess, 5000);
        }
    }

    // Join the output reader thread before anything it uses is freed.
    // Give it time to drain the pipe first, then ask it to stop.
    if (context->hReaderThread) {
        if (WaitForSingleObject(context->hReaderThread, 2000) == WAIT_TIMEOUT) {
            context->cancellationRequested = TRUE;
            if (WaitForSingleObject(context->hReaderThread, 5000) == WAIT_TIMEOUT) {
                // Fallback: leave the context to the still-running reader.
                // Leaking it is safe; freeing it under the reader is not.
                ThreadSafeDebugOutput(L"CleanupThreadSafeSubprocessContext: Output reader did not exit, leaving context allocated");
                return FALSE;
            }
        }
        CloseHandle(context->hReaderThread);
        context->hReaderThread = NULL;
    }

    // Nothing else uses the context now; mark it cleaned up
    context->initialized = FALSE;

    // Clean up configuration strings - skip if critical sections are corrupted
    // Just free the memory directly without using locks since we're cleaning up anyway
    if (context->executablePath) {
        SAFE_FREE(context->executablePath);
        context->executablePath = NULL;
    }
    if (context->arguments) {
        SAFE_FREE(context->arguments);
        context->arguments = NULL;
    }
    if (context->workingDirectory) {
        SAFE_FREE(context->workingDirectory);
        context->workingDirectory = NULL;
    }

    // Clean up output buffer - no lock needed during cleanup
    if (context->outputBuffer) {
        SAFE_FREE(context->outputBuffer);
        context->outputBuffer = NULL;
    }
    context->outputBufferSize = 0;
    context->outputLength = 0;

    // Note: Handles are closed by the worker thread, so we just NULL them out here
    // Attempting to close them again causes STATUS_INVALID_HANDLE exceptions
    context->hProcess = NULL;
    context->hThread = NULL;
    context->hOutputRead = NULL;
    context->hOutputWrite = NULL;

    // Close cancellation event
    if (context->cancellationEvent && context->cancellationEvent != INVALID_HANDLE_VALUE) {
        // Defensive: Check if handle is still valid before closing
        // Use DuplicateHandle to test validity without side effects
        HANDLE hTest = NULL;
        if (DuplicateHandle(GetCurrentProcess(), context->cancellationEvent, 
                           GetCurrentProcess(), &hTest, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            // Handle is valid, close both the test and original
            CloseHandle(hTest);
            CloseHandle(context->cancellationEvent);
        }
        // Always NULL it out regardless
        context->cancellationEvent = NULL;
    }

    // Delete critical sections
    // Note: initialized was already set to FALSE above
    // The initialized check at the start of this function should prevent double-deletion
    DeleteCriticalSection(&context->processStateLock);
    DeleteCriticalSection(&context->outputLock);
    DeleteCriticalSection(&context->configLock);
    return TRUE;
}

/**
 * Set the executable path for the subprocess
 */
BOOL SetSubprocessExecutable(ThreadSafeSubprocessContext* context, const wchar_t* path) {
    if (!context || !context->initialized || !path) {
        return FALSE;
    }

    EnterCriticalSection(&context->configLock);
    
    // Free existing path
    if (context->executablePath) {
        SAFE_FREE(context->executablePath);
    }
    
    // Copy new path
    context->executablePath = SAFE_WCSDUP(path);
    BOOL success = (context->executablePath != NULL);
    
    LeaveCriticalSection(&context->configLock);
    return success;
}

/**
 * Set the command line arguments for the subprocess
 */
BOOL SetSubprocessArguments(ThreadSafeSubprocessContext* context, const wchar_t* args) {
    if (!context || !context->initialized || !args) {
        return FALSE;
    }

    EnterCriticalSection(&context->configLock);
    
    // Free existing arguments
    if (context->arguments) {
        SAFE_FREE(context->arguments);
    }
    
    // Copy new arguments
    context->arguments = SAFE_WCSDUP(args);
    BOOL success = (context->arguments != NULL);
    
    LeaveCriticalSection(&context->configLock);
    return success;
}

/**
 * Set the working directory for the subprocess
 */
BOOL SetSubprocessWorkingDirectory(ThreadSafeSubprocessContext* context, const wchar_t* dir) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    EnterCriticalSection(&context->configLock);
    
    // Free existing directory
    if (context->workingDirectory) {
        SAFE_FREE(context->workingDirectory);
    }
    
    // Copy new directory (can be NULL)
    if (dir) {
        context->workingDirectory = SAFE_WCSDUP(dir);
    } else {
        context->workingDirectory = NULL;
    }
    
    LeaveCriticalSection(&context->configLock);
    return TRUE;
}

/**
 * Set the timeout for subprocess execution
 */
BOOL SetSubprocessTimeout(ThreadSafeSubprocessContext* context, DWORD timeoutMs) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    EnterCriticalSection(&context->configLock);
    context->timeoutMs = timeoutMs;
    LeaveCriticalSection(&context->configLock);
    return TRUE;
}

/**
 * Set the progress callback for the subprocess
 */
BOOL SetSubprocessProgressCallback(ThreadSafeSubprocessContext* context, ProgressCallback callback, void* userData) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    // Progress callback doesn't need locking as it's set before process starts
    context->progressCallback = callback;
    context->callbackUserData = userData;
    return TRUE;
}

/**
 * Set the parent window for the subprocess
 */
BOOL SetSubprocessParentWindow(ThreadSafeSubprocessContext* context, HWND parentWindow) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    // Parent window doesn't need locking as it's set before process starts
    context->parentWindow = parentWindow;
    return TRUE;
}

/**
 * Sanitize a command line by redacting sensitive information like authentication tokens in URLs
 * Returns a new allocated string that must be freed with SAFE_FREE
 */
static wchar_t* SanitizeCommandLine(const wchar_t* cmdLine) {
    if (!cmdLine) {
        return NULL;
    }

    wchar_t* sanitized = SAFE_WCSDUP(cmdLine);
    if (!sanitized) {
        return NULL;
    }

    // List of sensitive parameter names to redact
    static const wchar_t* sensitiveParams[] = {
        L"token", L"auth", L"key", L"sig", L"signature",
        L"pass", L"password", L"secret", L"access_token",
        L"auth_token", L"sid", L"session", L"cookie"
    };
    const int numSensitiveParams = sizeof(sensitiveParams) / sizeof(sensitiveParams[0]);

    // Search for URL-like strings (starting with http:// or https://)
    wchar_t* currentPos = sanitized;
    while (currentPos && *currentPos) {
        wchar_t* urlStartHttp = wcsstr(currentPos, L"http://");
        wchar_t* urlStartHttps = wcsstr(currentPos, L"https://");
        wchar_t* urlStart = NULL;

        if (urlStartHttp && urlStartHttps) {
            urlStart = (urlStartHttp < urlStartHttps) ? urlStartHttp : urlStartHttps;
        } else {
            urlStart = urlStartHttp ? urlStartHttp : urlStartHttps;
        }

        if (!urlStart) {
            break;
        }

        // Find end of URL (space, end of string, or common delimiters in command lines)
        wchar_t* urlEnd = urlStart;
        while (*urlEnd && *urlEnd != L' ' && *urlEnd != L'"' && *urlEnd != L'\'') {
            urlEnd++;
        }

        // Process query parameters in this URL
        wchar_t* queryStart = wcschr(urlStart, L'?');
        if (queryStart && queryStart < urlEnd) {
            wchar_t* paramPtr = queryStart + 1;
            while (paramPtr < urlEnd) {
                // Find '=' which separates key from value
                wchar_t* equalSign = wcschr(paramPtr, L'=');
                if (!equalSign || equalSign >= urlEnd) {
                    break; // No more key-value pairs
                }

                // Find end of this parameter (next '&' or end of URL)
                wchar_t* nextParam = wcschr(paramPtr, L'&');
                if (!nextParam || nextParam > urlEnd) {
                    nextParam = urlEnd;
                }

                // Check if this parameter is sensitive
                size_t keyLen = equalSign - paramPtr;
                BOOL isSensitive = FALSE;
                for (int i = 0; i < numSensitiveParams; i++) {
                    if (keyLen == wcslen(sensitiveParams[i]) &&
                        _wcsnicmp(paramPtr, sensitiveParams[i], keyLen) == 0) {
                        isSensitive = TRUE;
                        break;
                    }
                }

                if (isSensitive) {
                    // Redact the value part
                    wchar_t* valuePtr = equalSign + 1;
                    while (valuePtr < nextParam) {
                        *valuePtr = L'*';
                        valuePtr++;
                    }
                }

                // Move to next parameter
                if (nextParam < urlEnd) {
                    paramPtr = nextParam + 1;
                } else {
                    break;
                }
            }
        }

        currentPos = urlEnd;
    }

    return sanitized;
}

/**
 * Start the subprocess with thread-safe execution
 */
BOOL StartThreadSafeSubprocess(ThreadSafeSubprocessContext* context) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    // Check if already running
    EnterCriticalSection(&context->processStateLock);
    if (context->processRunning) {
        LeaveCriticalSection(&context->processStateLock);
        return FALSE; // Already running
    }
    LeaveCriticalSection(&context->processStateLock);

    // Get configuration safely
    EnterCriticalSection(&context->configLock);
    if (!context->executablePath || !context->arguments) {
        LeaveCriticalSection(&context->configLock);
        return FALSE; // Missing required configuration
    }

    // Build command line
    size_t cmdLineLen = wcslen(context->executablePath) + wcslen(context->arguments) + 10;
    wchar_t* cmdLine = (wchar_t*)SAFE_MALLOC(cmdLineLen * sizeof(wchar_t));
    if (!cmdLine) {
        LeaveCriticalSection(&context->configLock);
        return FALSE;
    }
    swprintf(cmdLine, cmdLineLen, L"\"%ls\" %ls", context->executablePath, context->arguments);

    // Sanitize command line for logging
    wchar_t* sanitizedCmdLine = SanitizeCommandLine(cmdLine);

    // Log the command being executed for debugging
    wchar_t logMsg[8192];
    int logLen = swprintf(logMsg, 8192, L"StartThreadSafeSubprocess: Executing command: %ls",
                         sanitizedCmdLine ? sanitizedCmdLine : L"[Sanitization Failed]");
    if (logLen > 0 && logLen < 8192) {
        ThreadSafeDebugOutput(logMsg);
    } else {
        // Command too long, log truncated version
        swprintf(logMsg, 8192, L"StartThreadSafeSubprocess: Executing command (truncated): %.500ls...",
                 sanitizedCmdLine ? sanitizedCmdLine : L"[Sanitization Failed]");
        ThreadSafeDebugOutput(logMsg);
    }
    
    // Log timestamp and command-line to session log
    SYSTEMTIME st;
    GetLocalTime(&st);

    AppendToYtDlpSessionLog(L"\r\n========================================\r\n");

    wchar_t timestampMsg[512];
    swprintf(timestampMsg, 512, L"yt-dlp invocation started: %04d-%02d-%02d %02d:%02d:%02d\r\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    AppendToYtDlpSessionLog(timestampMsg);

    AppendToYtDlpSessionLog(L"Command: ");
    AppendToYtDlpSessionLog(sanitizedCmdLine ? sanitizedCmdLine : L"[Sanitization Failed]");
    AppendToYtDlpSessionLog(L"\r\n========================================\r\n");

    if (sanitizedCmdLine) {
        SAFE_FREE(sanitizedCmdLine);
    }

    // Copy working directory if set
    wchar_t* workDir = NULL;
    if (context->workingDirectory) {
        workDir = SAFE_WCSDUP(context->workingDirectory);
    }
    LeaveCriticalSection(&context->configLock);

    // Create pipes for output capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hOutputRead = NULL, hOutputWrite = NULL;
    
    if (!CreatePipe(&hOutputRead, &hOutputWrite, &sa, 0)) {
        SAFE_FREE(cmdLine);
        if (workDir) SAFE_FREE(workDir);
        return FALSE;
    }
    
    // Make sure the read handle is not inherited
    SetHandleInformation(hOutputRead, HANDLE_FLAG_INHERIT, 0);

    // Set up process startup info
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutputWrite;
    si.hStdError = hOutputWrite;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi = {0};

    // Create the process
    BOOL processCreated = CreateProcessW(
        NULL,           // No module name (use command line)
        cmdLine,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        TRUE,           // Set handle inheritance to TRUE for pipes
        CREATE_NO_WINDOW, // No window
        NULL,           // Use parent's environment block
        workDir,        // Working directory
        &si,            // Pointer to STARTUPINFO structure
        &pi             // Pointer to PROCESS_INFORMATION structure
    );

    // Clean up temporary allocations
    SAFE_FREE(cmdLine);
    if (workDir) SAFE_FREE(workDir);
    CloseHandle(hOutputWrite); // Close write handle in parent process

    if (!processCreated) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        swprintf(errorMsg, 256, L"StartThreadSafeSubprocess: CreateProcessW failed with error %lu", error);
        ThreadSafeDebugOutput(errorMsg);
        CloseHandle(hOutputRead);
        return FALSE;
    }
    
    ThreadSafeDebugOutputF(L"StartThreadSafeSubprocess: Process created successfully, PID=%lu", pi.dwProcessId);

    // Store process information safely
    EnterCriticalSection(&context->processStateLock);
    context->hProcess = pi.hProcess;
    context->hThread = pi.hThread;
    context->processId = pi.dwProcessId;
    context->threadId = pi.dwThreadId;
    context->processRunning = TRUE;
    context->processCompleted = FALSE;
    context->exitCode = 0;
    context->hOutputRead = hOutputRead;
    context->hOutputWrite = NULL; // Already closed
    LeaveCriticalSection(&context->processStateLock);

    // Reset cancellation
    ResetEvent(context->cancellationEvent);
    context->cancellationRequested = FALSE;

    return TRUE;
}

/**
 * Check if the subprocess is currently running
 */
BOOL IsThreadSafeSubprocessRunning(ThreadSafeSubprocessContext* context) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    EnterCriticalSection(&context->processStateLock);
    BOOL running = context->processRunning && !context->processCompleted;
    
    // If we think it's running, double-check with the actual process
    if (running && context->hProcess) {
        DWORD exitCode;
        if (GetExitCodeProcess(context->hProcess, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                // Process has actually completed
                context->processRunning = FALSE;
                context->processCompleted = TRUE;
                context->exitCode = exitCode;
                running = FALSE;
            }
        }
    }
    
    LeaveCriticalSection(&context->processStateLock);
    return running;
}

/**
 * Request cancellation of the subprocess
 */
BOOL CancelThreadSafeSubprocess(ThreadSafeSubprocessContext* context) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    // Set cancellation flag (atomic operation, no lock needed)
    context->cancellationRequested = TRUE;
    
    // Log cancellation to session log
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t cancelMsg[512];
    swprintf(cancelMsg, 512,
             L"========================================\r\n"
             L"yt-dlp invocation CANCELLED by user: %04d-%02d-%02d %02d:%02d:%02d\r\n"
             L"========================================\r\n\r\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    AppendToYtDlpSessionLog(cancelMsg);
    
    // Safely set cancellation event
    if (context->cancellationEvent && context->cancellationEvent != INVALID_HANDLE_VALUE) {
        SetEvent(context->cancellationEvent);
    }

    // No Ctrl+C is sent: GenerateConsoleCtrlEvent cannot reach a CREATE_NO_WINDOW
    // child from a GUI process with no console. Cleanup terminates the process.

    return TRUE;
}

/**
 * Wait for subprocess completion with timeout
 */
BOOL WaitForThreadSafeSubprocessCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    EnterCriticalSection(&context->processStateLock);
    HANDLE hProcess = context->hProcess;
    BOOL alreadyCompleted = context->processCompleted;
    LeaveCriticalSection(&context->processStateLock);

    if (alreadyCompleted || !hProcess) {
        return TRUE; // Already completed or never started
    }

    // Wait for process completion
    DWORD waitResult = WaitForSingleObject(hProcess, timeoutMs);
    
    if (waitResult == WAIT_OBJECT_0) {
        // Process completed, update state
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode)) {
            EnterCriticalSection(&context->processStateLock);
            context->processRunning = FALSE;
            context->processCompleted = TRUE;
            context->exitCode = exitCode;
            LeaveCriticalSection(&context->processStateLock);
        }
        return TRUE;
    }

    return FALSE; // Timeout or error
}

/**
 * Get the exit code of the completed subprocess
 */
DWORD GetThreadSafeSubprocessExitCode(ThreadSafeSubprocessContext* context) {
    if (!context || !context->initialized) {
        return (DWORD)-1;
    }

    EnterCriticalSection(&context->processStateLock);
    DWORD exitCode = context->exitCode;
    LeaveCriticalSection(&context->processStateLock);
    
    return exitCode;
}

/**
 * Get the current output from the subprocess
 */
BOOL GetThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context, wchar_t** output, size_t* length) {
    if (!context || !context->initialized || !output || !length) {
        return FALSE;
    }

    EnterCriticalSection(&context->outputLock);
    
    if (context->outputBuffer && context->outputLength > 0) {
        // Create a copy of the output
        *output = SAFE_WCSDUP(context->outputBuffer);
        *length = context->outputLength;
    } else {
        *output = NULL;
        *length = 0;
    }
    
    LeaveCriticalSection(&context->outputLock);
    return (*output != NULL || *length == 0);
}

/**
 * Append data to the subprocess output buffer
 */
BOOL AppendToThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context, const wchar_t* data, size_t length) {
    if (!context || !context->initialized || !data || length == 0) {
        return FALSE;
    }

    EnterCriticalSection(&context->outputLock);
    
    // Check if we need to expand the buffer
    size_t requiredSize = context->outputLength + length + 1; // +1 for null terminator
    if (requiredSize > context->outputBufferSize) {
        // Expand buffer (double the size or required size, whichever is larger)
        size_t newSize = (context->outputBufferSize * 2 > requiredSize) ? 
                         context->outputBufferSize * 2 : requiredSize;
        
        wchar_t* newBuffer = (wchar_t*)SAFE_REALLOC(context->outputBuffer, newSize * sizeof(wchar_t));
        if (!newBuffer) {
            LeaveCriticalSection(&context->outputLock);
            return FALSE;
        }
        
        context->outputBuffer = newBuffer;
        context->outputBufferSize = newSize;
    }
    
    // Append the data
    wcsncpy(context->outputBuffer + context->outputLength, data, length);
    context->outputLength += length;
    context->outputBuffer[context->outputLength] = L'\0';
    
    LeaveCriticalSection(&context->outputLock);
    return TRUE;
}

/**
 * Clear the subprocess output buffer
 */
void ClearThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context) {
    if (!context || !context->initialized) {
        return;
    }

    EnterCriticalSection(&context->outputLock);
    if (context->outputBuffer) {
        context->outputBuffer[0] = L'\0';
        context->outputLength = 0;
    }
    LeaveCriticalSection(&context->outputLock);
}

/**
 * Terminate the subprocess with specified exit code
 */
BOOL TerminateThreadSafeSubprocess(ThreadSafeSubprocessContext* context, DWORD exitCode) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    EnterCriticalSection(&context->processStateLock);
    BOOL success = FALSE;
    
    if (context->processRunning && context->hProcess) {
        success = TerminateProcess(context->hProcess, exitCode);
        if (success) {
            context->processRunning = FALSE;
            context->processCompleted = TRUE;
            context->exitCode = exitCode;
        }
    }
    
    LeaveCriticalSection(&context->processStateLock);
    return success;
}

/**
 * Force kill the subprocess (last resort)
 */
BOOL ForceKillThreadSafeSubprocess(ThreadSafeSubprocessContext* context) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    return TerminateThreadSafeSubprocess(context, 9); // Use exit code 9 for forced termination
}

// Thread-safe subprocess output collection implementation

/**
 * Append raw bytes to a reader thread's growable line buffer
 */
static BOOL AppendToLineBuffer(char** line, size_t* length, size_t* capacity, const char* data, size_t count) {
    if (count == 0) {
        return TRUE;
    }

    if (*length + count > *capacity) {
        size_t newCapacity = (*capacity > 0) ? *capacity : 1024;
        while (newCapacity < *length + count) {
            newCapacity *= 2;
        }

        char* newLine = (char*)SAFE_REALLOC(*line, newCapacity);
        if (!newLine) {
            return FALSE;
        }

        *line = newLine;
        *capacity = newCapacity;
    }

    memcpy(*line + *length, data, count);
    *length += count;
    return TRUE;
}

/**
 * Convert one complete UTF-8 output line (without its '\n') and deliver it to
 * the output buffer, the session log and the progress callback
 */
static void ProcessSubprocessOutputLine(ThreadSafeSubprocessContext* context, const char* line, size_t lineLength) {
    // Remove \r if present (Windows line endings)
    if (lineLength > 0 && line[lineLength - 1] == '\r') {
        lineLength--;
    }

    if (lineLength == 0 || lineLength > 0x7FFFFFFF) {
        return;
    }

    // Convert UTF-8 line to wide characters, sized for the whole line
    int wideLength = MultiByteToWideChar(CP_UTF8, 0, line, (int)lineLength, NULL, 0);
    if (wideLength <= 0) {
        return;
    }

    wchar_t* wideLine = (wchar_t*)SAFE_MALLOC(((size_t)wideLength + 3) * sizeof(wchar_t));
    if (!wideLine) {
        ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Failed to allocate line buffer");
        return;
    }

    int converted = MultiByteToWideChar(CP_UTF8, 0, line, (int)lineLength, wideLine, wideLength);
    if (converted > 0) {
        // Append to output buffer with Windows line endings
        wideLine[converted] = L'\r';
        wideLine[converted + 1] = L'\n';
        wideLine[converted + 2] = L'\0';

        if (!AppendToThreadSafeSubprocessOutput(context, wideLine, (size_t)converted + 2)) {
            ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Failed to append output");
        }

        // Also append to session log (in-memory only, separate from disk logging)
        AppendToYtDlpSessionLog(wideLine);

        // Call progress callback if available
        wideLine[converted] = L'\0';
        if (context->progressCallback) {
            // Parse progress information from the line if it looks like progress
            if (wcsstr(wideLine, L"%") || wcsstr(wideLine, L"download") || wcsstr(wideLine, L"Downloading")) {
                context->progressCallback(-1, wideLine, context->callbackUserData);
            }
        }
    }

    SAFE_FREE(wideLine);
}

/**
 * Worker thread function for collecting subprocess output
 * Runs in background to continuously read from subprocess output pipe
 */
static DWORD WINAPI SubprocessOutputReaderThread(LPVOID lpParam) {
    ThreadSafeSubprocessContext* context = (ThreadSafeSubprocessContext*)lpParam;
    if (!context || !context->initialized) {
        return 1;
    }

    ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Starting output collection");

    char buffer[4096];
    DWORD bytesRead;

    // Bytes of the current line that has no '\n' yet. Local to this thread
    // (concurrent readers must not share line state) and grown as needed, so a
    // line of any length is kept whole across any number of reads.
    char* line = NULL;
    size_t lineLength = 0;
    size_t lineCapacity = 0;

    BOOL processExited = FALSE;

    while (TRUE) {
        // Check for cancellation
        if (context->cancellationRequested) {
            ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Cancellation requested, exiting");
            break;
        }

        // Note an exit before peeking: by then everything the process wrote is
        // in the pipe, so a later empty peek means the pipe is drained
        if (!processExited && !IsThreadSafeSubprocessRunning(context)) {
            ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Process no longer running, draining pipe");
            processExited = TRUE;
        }

        // Read from output pipe with timeout
        EnterCriticalSection(&context->processStateLock);
        HANDLE hOutputRead = context->hOutputRead;
        LeaveCriticalSection(&context->processStateLock);

        if (!hOutputRead) {
            ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: No output handle available");
            break;
        }

        // Use PeekNamedPipe to check if data is available (non-blocking)
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(hOutputRead, NULL, 0, NULL, &bytesAvailable, NULL)) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Pipe broken, process ended");
                break;
            } else if (processExited) {
                ThreadSafeDebugOutputF(L"SubprocessOutputReaderThread: PeekNamedPipe failed with error %lu after exit", error);
                break;
            } else {
                ThreadSafeDebugOutputF(L"SubprocessOutputReaderThread: PeekNamedPipe failed with error %lu", error);
                Sleep(100); // Wait a bit before retrying
                continue;
            }
        }

        if (bytesAvailable == 0) {
            if (processExited) {
                ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Pipe drained after process exit");
                break;
            }
            Sleep(50); // No data available, wait a bit
            continue;
        }

        // Read available data
        if (!ReadFile(hOutputRead, buffer, sizeof(buffer), &bytesRead, NULL)) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Pipe broken during read, process ended");
            } else {
                ThreadSafeDebugOutputF(L"SubprocessOutputReaderThread: ReadFile failed with error %lu", error);
            }
            break;
        }

        if (bytesRead == 0) {
            continue; // No data read, continue loop
        }

        // Deliver each complete line; keep the unterminated tail for the next read.
        // Lines are split on '\n' only, so UTF-8 sequences are never cut apart.
        DWORD segmentStart = 0;
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buffer[i] != '\n') {
                continue;
            }

            if (lineLength > 0) {
                // The line started in an earlier read
                if (AppendToLineBuffer(&line, &lineLength, &lineCapacity, buffer + segmentStart, i - segmentStart)) {
                    ProcessSubprocessOutputLine(context, line, lineLength);
                } else {
                    ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Failed to grow line buffer, line dropped");
                }
                lineLength = 0;
            } else {
                ProcessSubprocessOutputLine(context, buffer + segmentStart, i - segmentStart);
            }

            segmentStart = i + 1;
        }

        if (segmentStart < bytesRead &&
            !AppendToLineBuffer(&line, &lineLength, &lineCapacity, buffer + segmentStart, bytesRead - segmentStart)) {
            ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Failed to grow line buffer, partial line dropped");
            lineLength = 0;
        }
    }

    // Deliver a final line that had no terminator
    if (lineLength > 0) {
        ProcessSubprocessOutputLine(context, line, lineLength);
    }
    if (line) {
        SAFE_FREE(line);
    }

    // Mark output as complete
    EnterCriticalSection(&context->outputLock);
    context->outputComplete = TRUE;
    LeaveCriticalSection(&context->outputLock);

    ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Output collection completed");
    return 0;
}

/**
 * Start the output collection thread for a running subprocess
 */
BOOL StartThreadSafeSubprocessOutputCollection(ThreadSafeSubprocessContext* context) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    // Check if subprocess is running
    if (!IsThreadSafeSubprocessRunning(context)) {
        return FALSE;
    }

    // One reader per context: cleanup joins the one handle it keeps
    if (context->hReaderThread) {
        return FALSE;
    }

    // Create output reader thread
    HANDLE hOutputThread = CreateThread(
        NULL,                           // Default security attributes
        0,                              // Default stack size
        SubprocessOutputReaderThread,   // Thread function
        context,                        // Thread parameter
        0,                              // Default creation flags
        NULL                            // Don't need thread ID
    );

    if (!hOutputThread) {
        ThreadSafeDebugOutputF(L"StartThreadSafeSubprocessOutputCollection: Failed to create output thread, error %lu", GetLastError());
        return FALSE;
    }

    // Keep the thread handle: cleanup must join the reader before freeing the context
    context->hReaderThread = hOutputThread;
    
    ThreadSafeDebugOutput(L"StartThreadSafeSubprocessOutputCollection: Output collection thread started");
    return TRUE;
}

/**
 * Enhanced subprocess execution that combines process creation with output collection
 */
BOOL ExecuteThreadSafeSubprocessWithOutput(ThreadSafeSubprocessContext* context) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    ThreadSafeDebugOutput(L"ExecuteThreadSafeSubprocessWithOutput: Starting subprocess execution");

    // Start the subprocess
    if (!StartThreadSafeSubprocess(context)) {
        ThreadSafeDebugOutput(L"ExecuteThreadSafeSubprocessWithOutput: Failed to start subprocess");
        return FALSE;
    }

    // Start output collection
    if (!StartThreadSafeSubprocessOutputCollection(context)) {
        ThreadSafeDebugOutput(L"ExecuteThreadSafeSubprocessWithOutput: Failed to start output collection");
        // Process started but output collection failed - continue anyway
    }

    ThreadSafeDebugOutput(L"ExecuteThreadSafeSubprocessWithOutput: Subprocess and output collection started successfully");
    return TRUE;
}

/**
 * Wait for subprocess completion and ensure all output is collected
 */
BOOL WaitForThreadSafeSubprocessWithOutputCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs) {
    if (!context || !context->initialized) {
        return FALSE;
    }

    ThreadSafeDebugOutput(L"WaitForThreadSafeSubprocessWithOutputCompletion: Waiting for subprocess completion");

    // Wait for process completion
    BOOL processCompleted = WaitForThreadSafeSubprocessCompletion(context, timeoutMs);
    
    if (!processCompleted) {
        ThreadSafeDebugOutput(L"WaitForThreadSafeSubprocessWithOutputCompletion: Process did not complete within timeout");
        return FALSE;
    }

    // Give output collection thread a moment to finish reading any remaining data
    DWORD outputWaitStart = GetTickCount();
    const DWORD OUTPUT_COLLECTION_TIMEOUT = 2000; // 2 seconds max for output collection

    while (GetTickCount() - outputWaitStart < OUTPUT_COLLECTION_TIMEOUT) {
        EnterCriticalSection(&context->outputLock);
        BOOL outputComplete = context->outputComplete;
        LeaveCriticalSection(&context->outputLock);

        if (outputComplete) {
            break;
        }

        Sleep(100); // Check every 100ms
    }

    // Log completion with exit code and timestamp
    EnterCriticalSection(&context->processStateLock);
    DWORD exitCode = context->exitCode;
    LeaveCriticalSection(&context->processStateLock);
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t completionMsg[512];
    swprintf(completionMsg, 512,
             L"========================================\r\n"
             L"yt-dlp invocation completed: %04d-%02d-%02d %02d:%02d:%02d\r\n"
             L"Exit code: %lu\r\n"
             L"========================================\r\n\r\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             exitCode);
    AppendToYtDlpSessionLog(completionMsg);

    ThreadSafeDebugOutput(L"WaitForThreadSafeSubprocessWithOutputCompletion: Subprocess and output collection completed");
    return TRUE;
}

/**
 * Get the final output after subprocess completion
 */
BOOL GetFinalThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context, wchar_t** output, size_t* length, DWORD* exitCode) {
    if (!context || !context->initialized || !output || !length) {
        return FALSE;
    }

    // Ensure process has completed
    EnterCriticalSection(&context->processStateLock);
    BOOL completed = context->processCompleted;
    DWORD code = context->exitCode;
    LeaveCriticalSection(&context->processStateLock);

    if (!completed) {
        ThreadSafeDebugOutput(L"GetFinalThreadSafeSubprocessOutput: Process has not completed yet");
        return FALSE;
    }

    // Get the output
    BOOL success = GetThreadSafeSubprocessOutput(context, output, length);
    
    if (exitCode) {
        *exitCode = code;
    }

    ThreadSafeDebugOutputF(L"GetFinalThreadSafeSubprocessOutput: Retrieved %zu characters of output, exit code %lu", 
                          length ? *length : 0, code);
    
    return success;
}

// Legacy adapter functions for compatibility with existing ytdlp.c code

/**
 * Thread-safe worker thread function for legacy SubprocessContext
 * This adapts the old SubprocessContext to use the new thread-safe implementation
 */
DWORD WINAPI ThreadSafeSubprocessWorkerThread(LPVOID lpParam) {
    SubprocessContext* legacyContext = (SubprocessContext*)lpParam;
    if (!legacyContext || !legacyContext->config || !legacyContext->request) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Invalid legacy context");
        return 1;
    }

    ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Starting thread-safe worker for legacy context");

    // Mark legacy context thread as running
    EnterCriticalSection(&legacyContext->threadContext.criticalSection);
    legacyContext->threadContext.isRunning = TRUE;
    LeaveCriticalSection(&legacyContext->threadContext.criticalSection);

    // Create a thread-safe subprocess context
    ThreadSafeSubprocessContext* context = (ThreadSafeSubprocessContext*)SAFE_MALLOC(sizeof(ThreadSafeSubprocessContext));
    if (!context) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Failed to allocate thread-safe context");
        legacyContext->completed = TRUE;
        return 1;
    }

    // Initialize the thread-safe context
    if (!InitializeThreadSafeSubprocessContext(context)) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Failed to initialize thread-safe context");
        SAFE_FREE(context);
        legacyContext->completed = TRUE;
        return 1;
    }

    // Build command line arguments
    wchar_t arguments[4096];
    if (!GetYtDlpArgsForOperation(legacyContext->request->operation, legacyContext->request->url, 
                                 legacyContext->request->outputPath, legacyContext->config, arguments, 4096)) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Failed to build arguments");
        if (CleanupThreadSafeSubprocessContext(context)) {
            SAFE_FREE(context);
        }
        legacyContext->completed = TRUE;
        return 1;
    }

    // Configure the thread-safe context
    SetSubprocessExecutable(context, legacyContext->config->ytDlpPath);
    SetSubprocessArguments(context, arguments);
    SetSubprocessTimeout(context, 300000); // 5 minutes
    SetSubprocessProgressCallback(context, legacyContext->progressCallback, legacyContext->callbackUserData);
    SetSubprocessParentWindow(context, legacyContext->parentWindow);

    // Execute the subprocess with output collection
    if (!ExecuteThreadSafeSubprocessWithOutput(context)) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Failed to start subprocess");
        if (CleanupThreadSafeSubprocessContext(context)) {
            SAFE_FREE(context);
        }
        legacyContext->completed = TRUE;
        return 1;
    }

    // Wait for completion
    if (!WaitForThreadSafeSubprocessWithOutputCompletion(context, 300000)) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Subprocess timed out");
        CancelThreadSafeSubprocess(context);
        if (CleanupThreadSafeSubprocessContext(context)) {
            SAFE_FREE(context);
        }
        legacyContext->completed = TRUE;
        return 1;
    }

    // Create result structure
    YtDlpResult* result = (YtDlpResult*)SAFE_MALLOC(sizeof(YtDlpResult));
    if (!result) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Failed to allocate result");
        if (CleanupThreadSafeSubprocessContext(context)) {
            SAFE_FREE(context);
        }
        legacyContext->completed = TRUE;
        return 1;
    }

    memset(result, 0, sizeof(YtDlpResult));

    // Get final output and exit code
    wchar_t* output = NULL;
    size_t outputLength = 0;
    DWORD exitCode = 0;
    
    if (GetFinalThreadSafeSubprocessOutput(context, &output, &outputLength, &exitCode)) {
        result->success = (exitCode == 0);
        result->exitCode = exitCode;
        result->output = output;

        // Create error message if failed
        if (!result->success) {
            result->errorMessage = CreateUserFriendlyYtDlpError(exitCode, output, legacyContext->request->url);
        }
    } else {
        result->success = FALSE;
        result->exitCode = (DWORD)-1;
        result->output = SAFE_WCSDUP(L"Failed to retrieve subprocess output");
        result->errorMessage = CreateUserFriendlyYtDlpError((DWORD)-1, NULL, legacyContext->request->url);
    }

    // Store result in legacy context
    legacyContext->result = result;
    legacyContext->completed = TRUE;
    legacyContext->completionTime = GetTickCount();

    // Final progress update
    if (legacyContext->progressCallback) {
        if (result->success) {
            legacyContext->progressCallback(100, L"Completed successfully", legacyContext->callbackUserData);
        } else {
            legacyContext->progressCallback(100, L"Operation failed", legacyContext->callbackUserData);
        }
    }

    // Mark thread as no longer running
    EnterCriticalSection(&legacyContext->threadContext.criticalSection);
    legacyContext->threadContext.isRunning = FALSE;
    LeaveCriticalSection(&legacyContext->threadContext.criticalSection);

    // Clean up thread-safe context
    if (CleanupThreadSafeSubprocessContext(context)) {
        SAFE_FREE(context);
    }

    ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Thread-safe worker completed successfully");
    return 0;
}