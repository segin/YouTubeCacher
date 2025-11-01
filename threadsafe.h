#ifndef THREADSAFE_H
#define THREADSAFE_H

#include <windows.h>

// Note: Types ErrorHandler, MemoryManager, and ApplicationState are defined 
// in error.h, memory.h, and appstate.h respectively and included via YouTubeCacher.h

// Thread safety system initialization and cleanup
BOOL InitializeThreadSafety(void);
void CleanupThreadSafety(void);

// Thread-safe access to global error handler
void LockErrorHandler(void);
void UnlockErrorHandler(void);
ErrorHandler* GetErrorHandler(void);

// Thread-safe access to global memory manager
void LockMemoryManager(void);
void UnlockMemoryManager(void);
MemoryManager* GetMemoryManager(void);

// Thread-safe access to global application state
void LockAppState(void);
void UnlockAppState(void);
ApplicationState* GetAppState(void);

// Thread safety status
BOOL IsThreadSafetyInitialized(void);

// Thread-safe logging functions
void ThreadSafeDebugOutput(const wchar_t* message);
void ThreadSafeDebugOutputF(const wchar_t* format, ...);

// Forward declarations - these types are defined in other headers
// ThreadSafeSubprocessContext is defined in threading.h
// YtDlpConfig, YtDlpRequest, YtDlpResult, SubprocessContext are defined in YouTubeCacher.h

#endif // THREADSAFE_H