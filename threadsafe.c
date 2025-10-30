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