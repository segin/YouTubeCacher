#ifndef THREADING_H
#define THREADING_H

#include <windows.h>

// Forward declarations will be defined below where needed

// Progress callback function type
typedef void (*ProgressCallback)(int percentage, const wchar_t* status, void* userData);

// Thread context structure for managing thread state and synchronization
typedef struct {
    HANDLE hThread;
    DWORD threadId;
    BOOL isRunning;
    BOOL cancelRequested;
    CRITICAL_SECTION criticalSection;
} ThreadContext;

// Thread context management functions
BOOL InitializeThreadContext(ThreadContext* threadContext);
void CleanupThreadContext(ThreadContext* threadContext);

// Thread synchronization functions
BOOL SetCancellationFlag(ThreadContext* threadContext);
BOOL IsCancellationRequested(const ThreadContext* threadContext);

// Progress callback functions
void SubprocessProgressCallback(int percentage, const wchar_t* status, void* userData);
void UnifiedDownloadProgressCallback(int percentage, const wchar_t* status, void* userData);
void MainWindowProgressCallback(int percentage, const wchar_t* status, void* userData);

// HandleDownloadCompletion function declaration is in YouTubeCacher.h after type definitions

#endif // THREADING_H