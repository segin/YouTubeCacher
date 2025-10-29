#ifndef THREADSAFE_H
#define THREADSAFE_H

#include <windows.h>

// Forward declarations for types that will be defined in other headers
// Note: These types are defined in error.h, memory.h, and appstate.h respectively

// Thread safety system initialization and cleanup
BOOL InitializeThreadSafety(void);
void CleanupThreadSafety(void);

// Thread-safe access to global error handler
void LockErrorHandler(void);
void UnlockErrorHandler(void);
void* GetErrorHandler(void);

// Thread-safe access to global memory manager
void LockMemoryManager(void);
void UnlockMemoryManager(void);
void* GetMemoryManager(void);

// Thread-safe access to global application state
void LockAppState(void);
void UnlockAppState(void);
void* GetAppState(void);

// Thread safety status
BOOL IsThreadSafetyInitialized(void);

#endif // THREADSAFE_H