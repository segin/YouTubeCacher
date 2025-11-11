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
 * Releases all resources and critical sections
 */
void CleanupThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context) {
    if (!context) {
        return;
    }
    
    // Check if already cleaned up to prevent double-cleanup
    if (!context->initialized) {
        return;
    }
    
    // Mark as not initialized immediately to prevent re-entry
    context->initialized = FALSE;

    // Only cancel if the process is still running
    // If it's already completed, no need to cancel
    if (!context->processCompleted) {
        // Cancel any running process
        CancelThreadSafeSubprocess(context);

        // Wait for process to complete with timeout
        WaitForThreadSafeSubprocessCompletion(context, 5000);

        // Force terminate if still running
        ForceKillThreadSafeSubprocess(context);
    }

    // Clean up configuration strings
    EnterCriticalSection(&context->configLock);
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
    LeaveCriticalSection(&context->configLock);

    // Clean up output buffer
    EnterCriticalSection(&context->outputLock);
    if (context->outputBuffer) {
        SAFE_FREE(context->outputBuffer);
        context->outputBuffer = NULL;
    }
    context->outputBufferSize = 0;
    context->outputLength = 0;
    LeaveCriticalSection(&context->outputLock);

    // Close handles
    EnterCriticalSection(&context->processStateLock);
    if (context->hProcess) {
        CloseHandle(context->hProcess);
        context->hProcess = NULL;
    }
    if (context->hThread) {
        CloseHandle(context->hThread);
        context->hThread = NULL;
    }
    if (context->hOutputRead) {
        CloseHandle(context->hOutputRead);
        context->hOutputRead = NULL;
    }
    if (context->hOutputWrite) {
        CloseHandle(context->hOutputWrite);
        context->hOutputWrite = NULL;
    }
    LeaveCriticalSection(&context->processStateLock);

    // Close cancellation event
    if (context->cancellationEvent) {
        CloseHandle(context->cancellationEvent);
        context->cancellationEvent = NULL;
    }

    // Delete critical sections
    // Note: initialized was already set to FALSE at the start of this function
    DeleteCriticalSection(&context->processStateLock);
    DeleteCriticalSection(&context->outputLock);
    DeleteCriticalSection(&context->configLock);
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

    // Log the command being executed for debugging
    wchar_t logMsg[8192];
    int logLen = swprintf(logMsg, 8192, L"StartThreadSafeSubprocess: Executing command: %ls", cmdLine);
    if (logLen > 0 && logLen < 8192) {
        ThreadSafeDebugOutput(logMsg);
    } else {
        // Command too long, log truncated version
        swprintf(logMsg, 8192, L"StartThreadSafeSubprocess: Executing command (truncated): %.500ls...", cmdLine);
        ThreadSafeDebugOutput(logMsg);
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
    
    // Safely set cancellation event
    if (context->cancellationEvent && context->cancellationEvent != INVALID_HANDLE_VALUE) {
        SetEvent(context->cancellationEvent);
    }

    // Try graceful termination - but don't crash if we can't get the lock
    // The process will be force-killed later in cleanup anyway
    if (context->processRunning && context->hProcess) {
        // Try to send CTRL+C without using the lock
        // This might race but it's better than crashing
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, context->processId);
    }

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
    BOOL success = TRUE;

    // Accumulator for incomplete UTF-8 sequences
    static char utf8Accumulator[8] = {0};
    static size_t accumulatorLength = 0;

    while (success) {
        // Check for cancellation
        if (context->cancellationRequested) {
            ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Cancellation requested, exiting");
            break;
        }

        // Check if process is still running
        if (!IsThreadSafeSubprocessRunning(context)) {
            ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Process no longer running");
            break;
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
            } else {
                ThreadSafeDebugOutputF(L"SubprocessOutputReaderThread: PeekNamedPipe failed with error %lu", error);
                Sleep(100); // Wait a bit before retrying
                continue;
            }
        }

        if (bytesAvailable == 0) {
            Sleep(50); // No data available, wait a bit
            continue;
        }

        // Read available data
        success = ReadFile(hOutputRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
        if (!success) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Pipe broken during read, process ended");
                break;
            } else {
                ThreadSafeDebugOutputF(L"SubprocessOutputReaderThread: ReadFile failed with error %lu", error);
                break;
            }
        }

        if (bytesRead == 0) {
            continue; // No data read, continue loop
        }

        buffer[bytesRead] = '\0'; // Null-terminate

        // Combine with any accumulated bytes from previous incomplete UTF-8 sequences
        size_t totalBytes = accumulatorLength + bytesRead;
        char* processBuffer = buffer;
        
        if (accumulatorLength > 0) {
            // We have incomplete UTF-8 from previous read
            if (totalBytes < sizeof(utf8Accumulator)) {
                memcpy(utf8Accumulator + accumulatorLength, buffer, bytesRead);
                utf8Accumulator[totalBytes] = '\0';
                processBuffer = utf8Accumulator;
                bytesRead = (DWORD)totalBytes;
                accumulatorLength = 0; // Reset accumulator
            } else {
                // Too much data, process what we have and continue
                accumulatorLength = 0;
            }
        }

        // Process complete lines only to avoid partial UTF-8 sequences
        char* lineStart = processBuffer;
        char* lineEnd;
        
        while ((lineEnd = strchr(lineStart, '\n')) != NULL) {
            *lineEnd = '\0'; // Null-terminate the line
            
            // Remove \r if present (Windows line endings)
            size_t lineLength = lineEnd - lineStart;
            if (lineLength > 0 && lineStart[lineLength - 1] == '\r') {
                lineStart[lineLength - 1] = '\0';
                lineLength--;
            }

            // Convert UTF-8 line to wide characters
            if (lineLength > 0) {
                wchar_t wideBuffer[2048];
                int converted = MultiByteToWideChar(CP_UTF8, 0, lineStart, (int)lineLength, wideBuffer, 2047);
                if (converted > 0) {
                    wideBuffer[converted] = L'\0';
                    
                    // Append to output buffer with Windows line endings
                    wchar_t lineWithEnding[2050];
                    swprintf(lineWithEnding, 2050, L"%ls\r\n", wideBuffer);
                    
                    if (!AppendToThreadSafeSubprocessOutput(context, lineWithEnding, wcslen(lineWithEnding))) {
                        ThreadSafeDebugOutput(L"SubprocessOutputReaderThread: Failed to append output");
                    }

                    // Call progress callback if available
                    if (context->progressCallback) {
                        // Parse progress information from the line if it looks like progress
                        if (wcsstr(wideBuffer, L"%") || wcsstr(wideBuffer, L"download") || wcsstr(wideBuffer, L"Downloading")) {
                            context->progressCallback(-1, wideBuffer, context->callbackUserData);
                        }
                    }
                }
            }

            lineStart = lineEnd + 1; // Move to next line
        }

        // Handle any remaining incomplete line (save for next iteration)
        size_t remainingLength = strlen(lineStart);
        if (remainingLength > 0 && remainingLength < sizeof(utf8Accumulator)) {
            memcpy(utf8Accumulator, lineStart, remainingLength);
            utf8Accumulator[remainingLength] = '\0';
            accumulatorLength = remainingLength;
        }
    }

    // Process any final accumulated data
    if (accumulatorLength > 0) {
        wchar_t wideBuffer[2048];
        int converted = MultiByteToWideChar(CP_UTF8, 0, utf8Accumulator, (int)accumulatorLength, wideBuffer, 2047);
        if (converted > 0) {
            wideBuffer[converted] = L'\0';
            AppendToThreadSafeSubprocessOutput(context, wideBuffer, converted);
        }
        accumulatorLength = 0;
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

    // Store the thread handle for cleanup (we'll close it immediately since we don't need to wait for it)
    CloseHandle(hOutputThread);
    
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
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
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
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
        legacyContext->completed = TRUE;
        return 1;
    }

    // Wait for completion
    if (!WaitForThreadSafeSubprocessWithOutputCompletion(context, 300000)) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Subprocess timed out");
        CancelThreadSafeSubprocess(context);
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
        legacyContext->completed = TRUE;
        return 1;
    }

    // Create result structure
    YtDlpResult* result = (YtDlpResult*)SAFE_MALLOC(sizeof(YtDlpResult));
    if (!result) {
        ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Failed to allocate result");
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
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
    CleanupThreadSafeSubprocessContext(context);
    SAFE_FREE(context);

    ThreadSafeDebugOutput(L"ThreadSafeSubprocessWorkerThread: Thread-safe worker completed successfully");
    return 0;
}