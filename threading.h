#ifndef THREADING_H
#define THREADING_H

#include <windows.h>

// Forward declarations will be defined below where needed

// Progress callback function type
typedef void (*ProgressCallback)(int percentage, const wchar_t* status, void* userData);

// IPC Message Types for efficient cross-thread communication
typedef enum {
    IPC_MSG_PROGRESS_UPDATE = 1,
    IPC_MSG_STATUS_UPDATE = 2,
    IPC_MSG_TITLE_UPDATE = 3,
    IPC_MSG_DURATION_UPDATE = 4,
    IPC_MSG_MARQUEE_START = 5,
    IPC_MSG_MARQUEE_STOP = 6,
    IPC_MSG_DOWNLOAD_COMPLETE = 7,
    IPC_MSG_DOWNLOAD_FAILED = 8,
    IPC_MSG_OPERATION_CANCELLED = 9,
    IPC_MSG_VIDEO_INFO_COMPLETE = 10,
    IPC_MSG_METADATA_COMPLETE = 11
} IPCMessageType;

// IPC Message structure for type-safe communication
typedef struct {
    IPCMessageType type;
    HWND targetWindow;
    union {
        struct {
            int percentage;
        } progress;
        struct {
            wchar_t* text;  // Automatically managed memory
        } status;
        struct {
            wchar_t* title;
        } title;
        struct {
            wchar_t* duration;
        } duration;
        struct {
            void* result;
            void* context;
        } completion;
        struct {
            void* metadata;
            BOOL success;
        } metadata;
    } data;
    
    // Internal management and performance tracking
    DWORD timestamp;
    DWORD processedTimestamp;
    DWORD threadId;
    BOOL autoFreeStrings;
    DWORD priority;  // 0 = normal, 1 = high, 2 = critical
} IPCMessage;

// IPC Message Queue for buffering and batch processing
typedef struct IPCMessageQueue {
    IPCMessage* messages;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
    CRITICAL_SECTION lock;
    HANDLE notEmpty;
    HANDLE notFull;
} IPCMessageQueue;

// IPC Performance Statistics
typedef struct {
    DWORD totalMessagesSent;
    DWORD totalMessagesProcessed;
    DWORD totalMessagesDropped;
    DWORD averageProcessingTimeMs;
    DWORD maxProcessingTimeMs;
    DWORD queueHighWaterMark;
    DWORD lastResetTime;
} IPCStatistics;

// IPC Context for managing communication channels
typedef struct {
    IPCMessageQueue* queue;
    HANDLE workerThread;
    DWORD workerThreadId;
    BOOL shutdown;
    CRITICAL_SECTION lock;
    
    // Performance monitoring
    IPCStatistics stats;
    BOOL enableStatistics;
    DWORD statisticsResetInterval; // Reset stats every N milliseconds
} IPCContext;

// Thread context structure for managing thread state and synchronization
typedef struct {
    HANDLE hThread;
    DWORD threadId;
    BOOL isRunning;
    BOOL cancelRequested;
    CRITICAL_SECTION criticalSection;
    
    // Enhanced lifecycle management fields
    DWORD timeoutMs;
    SYSTEMTIME startTime;
    wchar_t threadName[64];
} ThreadContext;

// Thread context management functions
BOOL InitializeThreadContext(ThreadContext* threadContext);
void CleanupThreadContext(ThreadContext* threadContext);

// Enhanced thread management functions
BOOL CreateManagedThread(ThreadContext* context, LPTHREAD_START_ROUTINE function, LPVOID data, const wchar_t* name, DWORD timeoutMs);
BOOL WaitForThreadCompletion(ThreadContext* context, DWORD timeoutMs);
void ForceTerminateThread(ThreadContext* context);

// Thread synchronization functions
BOOL SetCancellationFlag(ThreadContext* threadContext);
BOOL IsCancellationRequested(const ThreadContext* threadContext);

// IPC System Functions
BOOL InitializeIPC(IPCContext* context, size_t queueCapacity);
void CleanupIPC(IPCContext* context);
BOOL SendIPCMessage(IPCContext* context, const IPCMessage* message);
BOOL SendProgressUpdate(IPCContext* context, HWND targetWindow, int percentage);
BOOL SendStatusUpdate(IPCContext* context, HWND targetWindow, const wchar_t* status);
BOOL SendTitleUpdate(IPCContext* context, HWND targetWindow, const wchar_t* title);
BOOL SendDurationUpdate(IPCContext* context, HWND targetWindow, const wchar_t* duration);
BOOL SendMarqueeControl(IPCContext* context, HWND targetWindow, BOOL start);
BOOL SendDownloadComplete(IPCContext* context, HWND targetWindow, void* result, void* downloadContext);
BOOL SendDownloadFailed(IPCContext* context, HWND targetWindow);
BOOL SendOperationCancelled(IPCContext* context, HWND targetWindow);

// Message Queue Functions
IPCMessageQueue* CreateIPCMessageQueue(size_t capacity);
void DestroyIPCMessageQueue(IPCMessageQueue* queue);
BOOL EnqueueIPCMessage(IPCMessageQueue* queue, const IPCMessage* message);
BOOL DequeueIPCMessage(IPCMessageQueue* queue, IPCMessage* message);
void FreeIPCMessage(IPCMessage* message);

// Global IPC System Management
BOOL InitializeGlobalIPC(void);
void CleanupGlobalIPC(void);
IPCContext* GetGlobalIPCContext(void);

// Advanced IPC Functions
BOOL SendPriorityIPCMessage(IPCContext* context, const IPCMessage* message, DWORD priority);
BOOL FlushIPCQueue(IPCContext* context);
void GetIPCStatistics(IPCContext* context, IPCStatistics* stats);
void ResetIPCStatistics(IPCContext* context);
BOOL SetIPCStatisticsEnabled(IPCContext* context, BOOL enabled);

// Batch IPC Operations for better performance
BOOL SendBatchProgressUpdate(IPCContext* context, HWND targetWindow, int percentage, const wchar_t* status);
BOOL SendBatchMetadataUpdate(IPCContext* context, HWND targetWindow, const wchar_t* title, const wchar_t* duration);

// Progress callback functions (legacy - now use IPC internally)
void SubprocessProgressCallback(int percentage, const wchar_t* status, void* userData);
void UnifiedDownloadProgressCallback(int percentage, const wchar_t* status, void* userData);
void MainWindowProgressCallback(int percentage, const wchar_t* status, void* userData);

// Thread-safe subprocess context structure
typedef struct {
    // Critical sections for thread safety
    CRITICAL_SECTION processStateLock;
    CRITICAL_SECTION outputLock;
    CRITICAL_SECTION configLock;
    
    // Process state (protected by processStateLock)
    HANDLE hProcess;
    HANDLE hThread;
    DWORD processId;
    DWORD threadId;
    BOOL processRunning;
    BOOL processCompleted;
    DWORD exitCode;
    
    // Output management (protected by outputLock)
    HANDLE hOutputRead;
    HANDLE hOutputWrite;
    wchar_t* outputBuffer;
    size_t outputBufferSize;
    size_t outputLength;
    BOOL outputComplete;
    
    // Configuration (protected by configLock)
    wchar_t* executablePath;
    wchar_t* arguments;
    wchar_t* workingDirectory;
    DWORD timeoutMs;
    
    // Progress and status
    ProgressCallback progressCallback;
    void* callbackUserData;
    HWND parentWindow;
    
    // Cancellation support
    BOOL cancellationRequested;
    HANDLE cancellationEvent;
    
    // Initialization state
    BOOL initialized;
} ThreadSafeSubprocessContext;

// Thread-safe subprocess management functions
BOOL InitializeThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context);
void CleanupThreadSafeSubprocessContext(ThreadSafeSubprocessContext* context);
BOOL SetSubprocessExecutable(ThreadSafeSubprocessContext* context, const wchar_t* path);
BOOL SetSubprocessArguments(ThreadSafeSubprocessContext* context, const wchar_t* args);
BOOL SetSubprocessWorkingDirectory(ThreadSafeSubprocessContext* context, const wchar_t* dir);
BOOL SetSubprocessTimeout(ThreadSafeSubprocessContext* context, DWORD timeoutMs);
BOOL SetSubprocessProgressCallback(ThreadSafeSubprocessContext* context, ProgressCallback callback, void* userData);
BOOL SetSubprocessParentWindow(ThreadSafeSubprocessContext* context, HWND parentWindow);

// Thread-safe process control functions
BOOL StartThreadSafeSubprocess(ThreadSafeSubprocessContext* context);
BOOL IsThreadSafeSubprocessRunning(ThreadSafeSubprocessContext* context);
BOOL CancelThreadSafeSubprocess(ThreadSafeSubprocessContext* context);
BOOL WaitForThreadSafeSubprocessCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs);
DWORD GetThreadSafeSubprocessExitCode(ThreadSafeSubprocessContext* context);

// Thread-safe output collection functions
BOOL GetThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context, wchar_t** output, size_t* length);
BOOL AppendToThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context, const wchar_t* data, size_t length);
void ClearThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context);

// Thread-safe process termination functions
BOOL TerminateThreadSafeSubprocess(ThreadSafeSubprocessContext* context, DWORD exitCode);
BOOL ForceKillThreadSafeSubprocess(ThreadSafeSubprocessContext* context);

// Thread-safe subprocess output collection functions
BOOL StartThreadSafeSubprocessOutputCollection(ThreadSafeSubprocessContext* context);
BOOL ExecuteThreadSafeSubprocessWithOutput(ThreadSafeSubprocessContext* context);
BOOL WaitForThreadSafeSubprocessWithOutputCompletion(ThreadSafeSubprocessContext* context, DWORD timeoutMs);
BOOL GetFinalThreadSafeSubprocessOutput(ThreadSafeSubprocessContext* context, wchar_t** output, size_t* length, DWORD* exitCode);

// Adapter functions for integrating with existing ytdlp.c code
ThreadSafeSubprocessContext* CreateThreadSafeSubprocessFromYtDlp(const YtDlpConfig* config, const YtDlpRequest* request);
YtDlpResult* ExecuteYtDlpRequestThreadSafe(const YtDlpConfig* config, const YtDlpRequest* request);
ThreadSafeSubprocessContext* CreateThreadSafeSubprocessWithCallback(const YtDlpConfig* config, const YtDlpRequest* request, 
                                                                   ProgressCallback progressCallback, void* callbackUserData, HWND parentWindow);

// Legacy context adapter functions - these will be declared properly in YouTubeCacher.h after SubprocessContext is defined
// Thread-safe worker thread function
DWORD WINAPI ThreadSafeSubprocessWorkerThread(LPVOID lpParam);

// Forward declaration for download completion handling (defined in YouTubeCacher.h after type definitions)
// void HandleDownloadCompletion(HWND hDlg, YtDlpResult* result, NonBlockingDownloadContext* downloadContext);

#endif // THREADING_H