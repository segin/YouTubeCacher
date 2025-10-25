#include "YouTubeCacher.h"

// Custom window messages - centralized definitions
#define WM_DOWNLOAD_COMPLETE (WM_USER + 102)
#define WM_UNIFIED_DOWNLOAD_UPDATE (WM_USER + 113)

// Global IPC context for the application
static IPCContext g_ipcContext = {0};
static BOOL g_ipcInitialized = FALSE;

// Initialize thread context with critical section for thread safety
BOOL InitializeThreadContext(ThreadContext* threadContext) {
    if (!threadContext) return FALSE;
    
    memset(threadContext, 0, sizeof(ThreadContext));
    
    // Initialize critical section for thread-safe access
    // Note: InitializeCriticalSection can raise exceptions on low memory,
    // but we'll handle this with standard error checking
    InitializeCriticalSection(&threadContext->criticalSection);
    return TRUE;
}

// Clean up thread context and wait for thread completion
void CleanupThreadContext(ThreadContext* threadContext) {
    if (!threadContext) return;
    
    // Wait for thread to complete if still running
    if (threadContext->hThread && threadContext->isRunning) {
        // Signal cancellation first
        EnterCriticalSection(&threadContext->criticalSection);
        threadContext->cancelRequested = TRUE;
        LeaveCriticalSection(&threadContext->criticalSection);
        
        // Wait up to 5 seconds for graceful shutdown
        if (WaitForSingleObject(threadContext->hThread, 5000) == WAIT_TIMEOUT) {
            // Force terminate if thread doesn't respond
            TerminateThread(threadContext->hThread, 1);
        }
        
        CloseHandle(threadContext->hThread);
        threadContext->hThread = NULL;
    }
    
    DeleteCriticalSection(&threadContext->criticalSection);
    threadContext->isRunning = FALSE;
}

// Thread-safe cancellation flag setting
BOOL SetCancellationFlag(ThreadContext* threadContext) {
    if (!threadContext) return FALSE;
    
    EnterCriticalSection(&threadContext->criticalSection);
    threadContext->cancelRequested = TRUE;
    LeaveCriticalSection(&threadContext->criticalSection);
    
    return TRUE;
}

// Thread-safe cancellation check
BOOL IsCancellationRequested(const ThreadContext* threadContext) {
    if (!threadContext) return FALSE;
    
    BOOL cancelled = FALSE;
    EnterCriticalSection((CRITICAL_SECTION*)&threadContext->criticalSection);
    cancelled = threadContext->cancelRequested;
    LeaveCriticalSection((CRITICAL_SECTION*)&threadContext->criticalSection);
    
    return cancelled;
}

// ============================================================================
// IPC System Implementation
// ============================================================================

// IPC Worker Thread - processes messages from the queue and sends them to target windows
DWORD WINAPI IPCWorkerThread(LPVOID lpParam) {
    IPCContext* context = (IPCContext*)lpParam;
    if (!context || !context->queue) return 1;
    
    while (!context->shutdown) {
        IPCMessage message;
        
        // Wait for messages in the queue
        if (DequeueIPCMessage(context->queue, &message)) {
            DWORD startTime = GetTickCount();
            message.processedTimestamp = startTime;
            
            // Process the message based on type
            switch (message.type) {
                case IPC_MSG_PROGRESS_UPDATE:
                    PostMessageW(message.targetWindow, WM_UNIFIED_DOWNLOAD_UPDATE, 3, message.data.progress.percentage);
                    break;
                    
                case IPC_MSG_STATUS_UPDATE:
                    if (message.data.status.text) {
                        PostMessageW(message.targetWindow, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)message.data.status.text);
                        // Don't free here - the receiving window will free it
                        message.data.status.text = NULL; // Prevent double-free
                    }
                    break;
                    
                case IPC_MSG_TITLE_UPDATE:
                    if (message.data.title.title) {
                        PostMessageW(message.targetWindow, WM_UNIFIED_DOWNLOAD_UPDATE, 1, (LPARAM)message.data.title.title);
                        message.data.title.title = NULL; // Prevent double-free
                    }
                    break;
                    
                case IPC_MSG_DURATION_UPDATE:
                    if (message.data.duration.duration) {
                        PostMessageW(message.targetWindow, WM_UNIFIED_DOWNLOAD_UPDATE, 2, (LPARAM)message.data.duration.duration);
                        message.data.duration.duration = NULL; // Prevent double-free
                    }
                    break;
                    
                case IPC_MSG_MARQUEE_START:
                    PostMessageW(message.targetWindow, WM_UNIFIED_DOWNLOAD_UPDATE, 4, 0);
                    break;
                    
                case IPC_MSG_MARQUEE_STOP:
                    PostMessageW(message.targetWindow, WM_UNIFIED_DOWNLOAD_UPDATE, 6, 0);
                    break;
                    
                case IPC_MSG_DOWNLOAD_COMPLETE:
                    PostMessageW(message.targetWindow, WM_DOWNLOAD_COMPLETE, 
                               (WPARAM)message.data.completion.result, 
                               (LPARAM)message.data.completion.context);
                    break;
                    
                case IPC_MSG_DOWNLOAD_FAILED:
                    PostMessageW(message.targetWindow, WM_UNIFIED_DOWNLOAD_UPDATE, 7, 0);
                    break;
                    
                case IPC_MSG_OPERATION_CANCELLED:
                    PostMessageW(message.targetWindow, WM_UNIFIED_DOWNLOAD_UPDATE, 8, 0);
                    break;
                    
                case IPC_MSG_VIDEO_INFO_COMPLETE:
                    PostMessageW(message.targetWindow, WM_USER + 101, 0, (LPARAM)message.data.completion.context);
                    break;
                    
                case IPC_MSG_METADATA_COMPLETE:
                    PostMessageW(message.targetWindow, WM_USER + 103, 
                               (WPARAM)message.data.metadata.success, 
                               (LPARAM)message.data.metadata.metadata);
                    break;
                    
                default:
                    // Unknown message type - ignore
                    break;
            }
            
            // Update performance statistics
            if (context->enableStatistics) {
                DWORD processingTime = GetTickCount() - startTime;
                
                EnterCriticalSection(&context->lock);
                context->stats.totalMessagesProcessed++;
                
                // Update average processing time (rolling average)
                if (context->stats.averageProcessingTimeMs == 0) {
                    context->stats.averageProcessingTimeMs = processingTime;
                } else {
                    context->stats.averageProcessingTimeMs = 
                        (context->stats.averageProcessingTimeMs * 9 + processingTime) / 10;
                }
                
                // Update max processing time
                if (processingTime > context->stats.maxProcessingTimeMs) {
                    context->stats.maxProcessingTimeMs = processingTime;
                }
                
                // Reset statistics if interval has passed
                DWORD currentTime = GetTickCount();
                if (currentTime - context->stats.lastResetTime > context->statisticsResetInterval) {
                    context->stats.maxProcessingTimeMs = 0;
                    context->stats.lastResetTime = currentTime;
                }
                
                LeaveCriticalSection(&context->lock);
            }
            
            // Clean up the message
            FreeIPCMessage(&message);
        } else {
            // No messages available, wait a bit
            Sleep(10);
        }
    }
    
    return 0;
}

// Initialize the IPC system
BOOL InitializeIPC(IPCContext* context, size_t queueCapacity) {
    if (!context) return FALSE;
    
    memset(context, 0, sizeof(IPCContext));
    
    // Initialize critical section
    InitializeCriticalSection(&context->lock);
    
    // Create message queue
    context->queue = CreateIPCMessageQueue(queueCapacity);
    if (!context->queue) {
        DeleteCriticalSection(&context->lock);
        return FALSE;
    }
    
    // Initialize performance monitoring
    context->enableStatistics = TRUE;
    context->statisticsResetInterval = 60000; // Reset every minute
    context->stats.lastResetTime = GetTickCount();
    
    // Create worker thread
    context->workerThread = CreateThread(NULL, 0, IPCWorkerThread, context, 0, &context->workerThreadId);
    if (!context->workerThread) {
        DestroyIPCMessageQueue(context->queue);
        DeleteCriticalSection(&context->lock);
        return FALSE;
    }
    
    return TRUE;
}

// Cleanup the IPC system
void CleanupIPC(IPCContext* context) {
    if (!context) return;
    
    // Signal shutdown
    EnterCriticalSection(&context->lock);
    context->shutdown = TRUE;
    LeaveCriticalSection(&context->lock);
    
    // Wait for worker thread to finish
    if (context->workerThread) {
        WaitForSingleObject(context->workerThread, 5000);
        CloseHandle(context->workerThread);
        context->workerThread = NULL;
    }
    
    // Cleanup queue
    if (context->queue) {
        DestroyIPCMessageQueue(context->queue);
        context->queue = NULL;
    }
    
    DeleteCriticalSection(&context->lock);
}

// Send a generic IPC message
BOOL SendIPCMessage(IPCContext* context, const IPCMessage* message) {
    if (!context || !context->queue || !message) return FALSE;
    
    IPCMessage msgCopy = *message;
    msgCopy.threadId = GetCurrentThreadId();
    
    EnterCriticalSection(&context->lock);
    BOOL result = !context->shutdown && EnqueueIPCMessage(context->queue, &msgCopy);
    
    if (context->enableStatistics) {
        if (result) {
            context->stats.totalMessagesSent++;
        } else {
            context->stats.totalMessagesDropped++;
        }
    }
    
    LeaveCriticalSection(&context->lock);
    
    return result;
}

// Convenience functions for common message types
BOOL SendProgressUpdate(IPCContext* context, HWND targetWindow, int percentage) {
    IPCMessage message = {0};
    message.type = IPC_MSG_PROGRESS_UPDATE;
    message.targetWindow = targetWindow;
    message.data.progress.percentage = percentage;
    message.timestamp = GetTickCount();
    
    return SendIPCMessage(context, &message);
}

BOOL SendStatusUpdate(IPCContext* context, HWND targetWindow, const wchar_t* status) {
    if (!status) return FALSE;
    
    IPCMessage message = {0};
    message.type = IPC_MSG_STATUS_UPDATE;
    message.targetWindow = targetWindow;
    message.data.status.text = SAFE_WCSDUP(status);
    message.timestamp = GetTickCount();
    message.autoFreeStrings = TRUE;
    
    return SendIPCMessage(context, &message);
}

BOOL SendTitleUpdate(IPCContext* context, HWND targetWindow, const wchar_t* title) {
    if (!title) return FALSE;
    
    IPCMessage message = {0};
    message.type = IPC_MSG_TITLE_UPDATE;
    message.targetWindow = targetWindow;
    message.data.title.title = SAFE_WCSDUP(title);
    message.timestamp = GetTickCount();
    message.autoFreeStrings = TRUE;
    
    return SendIPCMessage(context, &message);
}

BOOL SendDurationUpdate(IPCContext* context, HWND targetWindow, const wchar_t* duration) {
    if (!duration) return FALSE;
    
    IPCMessage message = {0};
    message.type = IPC_MSG_DURATION_UPDATE;
    message.targetWindow = targetWindow;
    message.data.duration.duration = SAFE_WCSDUP(duration);
    message.timestamp = GetTickCount();
    message.autoFreeStrings = TRUE;
    
    return SendIPCMessage(context, &message);
}

BOOL SendMarqueeControl(IPCContext* context, HWND targetWindow, BOOL start) {
    IPCMessage message = {0};
    message.type = start ? IPC_MSG_MARQUEE_START : IPC_MSG_MARQUEE_STOP;
    message.targetWindow = targetWindow;
    message.timestamp = GetTickCount();
    
    return SendIPCMessage(context, &message);
}

BOOL SendDownloadComplete(IPCContext* context, HWND targetWindow, void* result, void* downloadContext) {
    IPCMessage message = {0};
    message.type = IPC_MSG_DOWNLOAD_COMPLETE;
    message.targetWindow = targetWindow;
    message.data.completion.result = result;
    message.data.completion.context = downloadContext;
    message.timestamp = GetTickCount();
    
    return SendIPCMessage(context, &message);
}

BOOL SendDownloadFailed(IPCContext* context, HWND targetWindow) {
    IPCMessage message = {0};
    message.type = IPC_MSG_DOWNLOAD_FAILED;
    message.targetWindow = targetWindow;
    message.timestamp = GetTickCount();
    
    return SendIPCMessage(context, &message);
}

BOOL SendOperationCancelled(IPCContext* context, HWND targetWindow) {
    IPCMessage message = {0};
    message.type = IPC_MSG_OPERATION_CANCELLED;
    message.targetWindow = targetWindow;
    message.timestamp = GetTickCount();
    
    return SendIPCMessage(context, &message);
}

// ============================================================================
// Legacy Progress Callback Functions (Updated to use IPC)
// ============================================================================

// Progress callback for updating progress dialog
void SubprocessProgressCallback(int percentage, const wchar_t* status, void* userData) {
    ProgressDialog* progress = (ProgressDialog*)userData;
    if (progress) {
        UpdateProgressDialog(progress, percentage, status);
    }
}

// Progress callback for unified download - now uses IPC system
void UnifiedDownloadProgressCallback(int percentage, const wchar_t* status, void* userData) {
    HWND hDlg = (HWND)userData;
    if (!hDlg) return;
    
    IPCContext* ipc = GetGlobalIPCContext();
    if (ipc) {
        // Use the new IPC system for better performance and reliability
        SendProgressUpdate(ipc, hDlg, percentage);
        if (status) {
            SendStatusUpdate(ipc, hDlg, status);
        }
    } else {
        // Fallback to direct PostMessage if IPC is not available
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, percentage);
        if (status) {
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)SAFE_WCSDUP(status));
        }
    }
}

// Progress callback for the main window - now uses IPC system
void MainWindowProgressCallback(int percentage, const wchar_t* status, void* userData) {
    HWND hDlg = (HWND)userData;
    if (!hDlg) return;
    
    IPCContext* ipc = GetGlobalIPCContext();
    if (ipc) {
        // Use the new IPC system for better performance and reliability
        SendProgressUpdate(ipc, hDlg, percentage);
        if (status) {
            SendStatusUpdate(ipc, hDlg, status);
        }
    } else {
        // Fallback to direct PostMessage if IPC is not available
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, percentage);
        if (status) {
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)SAFE_WCSDUP(status));
        }
    }
}

// Handle download completion
void HandleDownloadCompletion(HWND hDlg, YtDlpResult* result, NonBlockingDownloadContext* downloadContext) {
    if (!hDlg || !downloadContext) return;
    
    if (result && result->success) {
        UpdateMainProgressBar(hDlg, 100, L"Download completed successfully");
        // Re-enable UI controls
        SetDownloadUIState(hDlg, FALSE);
        // Keep progress bar visible - don't hide it
        
        // Add to cache - extract video ID from URL
        OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Extracting video ID from URL\n");
        wchar_t* videoId = ExtractVideoIdFromUrl(downloadContext->url);
        OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - ExtractVideoIdFromUrl returned\n");
        
        if (videoId) {
            wchar_t debugMsg[256];
            swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Video ID: %ls\n", videoId);
            OutputDebugStringW(debugMsg);
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Getting UI text fields\n");
            
            // Get video title and duration from UI if available
            wchar_t title[512] = {0};
            wchar_t duration[64] = {0};
            GetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, title, 512);
            GetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, duration, 64);
            
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - UI text fields retrieved\n");
            
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Title will be base64 encoded for cache storage\n");
            
            // Get download path from config
            wchar_t downloadPath[MAX_EXTENDED_PATH];
            if (LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Download path: %ls\n", downloadPath);
                OutputDebugStringW(debugMsg);
                // Enhanced video file detection - look for files with video ID and any video extension
                wchar_t videoPattern[MAX_EXTENDED_PATH];
                swprintf(videoPattern, MAX_EXTENDED_PATH, L"%ls\\*%ls*", downloadPath, videoId);
                swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Searching pattern: %ls\n", videoPattern);
                OutputDebugStringW(debugMsg);
                
                WIN32_FIND_DATAW findData;
                HANDLE hFind = FindFirstFileW(videoPattern, &findData);
                
                wchar_t* bestVideoFile = NULL;
                FILETIME latestTime = {0};
                
                if (hFind != INVALID_HANDLE_VALUE) {
                    OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Found files matching pattern\n");
                    do {
                        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                            // Check if it's a video file with enhanced extension detection
                            wchar_t* ext = wcsrchr(findData.cFileName, L'.');
                            if (ext && IsVideoFileExtension(ext)) {
                                wchar_t fullVideoPath[MAX_EXTENDED_PATH];
                                swprintf(fullVideoPath, MAX_EXTENDED_PATH, L"%ls\\%ls", downloadPath, findData.cFileName);
                                
                                swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Found video file: %ls (ext: %ls)\n", 
                                        findData.cFileName, ext);
                                OutputDebugStringW(debugMsg);
                                
                                // Select the most recent video file (in case of multiple formats)
                                if (!bestVideoFile || CompareFileTime(&findData.ftLastWriteTime, &latestTime) > 0) {
                                    if (bestVideoFile) SAFE_FREE(bestVideoFile);
                                    bestVideoFile = SAFE_WCSDUP(fullVideoPath);
                                    latestTime = findData.ftLastWriteTime;
                                }
                            }
                        }
                    } while (FindNextFileW(hFind, &findData));
                    FindClose(hFind);
                }
                
                // If we found a video file, add it to cache
                if (bestVideoFile) {
                    // Find subtitle files
                    wchar_t** subtitleFiles = NULL;
                    int subtitleCount = 0;
                    FindSubtitleFiles(bestVideoFile, &subtitleFiles, &subtitleCount);
                    
                    // Add to cache
                    swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Adding to cache: %ls\n", bestVideoFile);
                    OutputDebugStringW(debugMsg);
                    
                    AddCacheEntry(GetCacheManager(), videoId, 
                                wcslen(title) > 0 ? title : ExtractFileNameFromPath(bestVideoFile),
                                wcslen(duration) > 0 ? duration : L"Unknown",
                                bestVideoFile, subtitleFiles, subtitleCount);
                    
                    OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Cache entry added successfully\n");
                    
                    // Clean up subtitle files array
                    if (subtitleFiles) {
                        for (int i = 0; i < subtitleCount; i++) {
                            if (subtitleFiles[i]) SAFE_FREE(subtitleFiles[i]);
                        }
                        SAFE_FREE(subtitleFiles);
                    }
                    
                    SAFE_FREE(bestVideoFile);
                } else {
                    OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - No video file found with enhanced detection\n");
                }
            }
            
            SAFE_FREE(videoId);
            
            // Refresh the cache list UI
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Refreshing cache list UI\n");
            RefreshCacheList(GetDlgItem(hDlg, IDC_LIST), GetCacheManager());
            UpdateCacheListStatus(hDlg, GetCacheManager());
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Cache list refreshed\n");
        }
    } else {
        UpdateMainProgressBar(hDlg, 0, L"Download failed");
        // Re-enable UI controls
        SetDownloadUIState(hDlg, FALSE);
        Sleep(500);
        ShowMainProgressBar(hDlg, FALSE);
        
        // Show enhanced error dialog with actual yt-dlp output and Windows API errors
        if (result) {
            // Enhance error details with Windows API error information if applicable
            if (result->exitCode != 0 && result->exitCode > 1000) {
                // This looks like a Windows error code
                wchar_t windowsError[512];
                DWORD errorCode = result->exitCode;
                
                if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                  windowsError, 512, NULL)) {
                    
                    // Remove trailing newlines
                    size_t len = wcslen(windowsError);
                    while (len > 0 && (windowsError[len-1] == L'\n' || windowsError[len-1] == L'\r')) {
                        windowsError[len-1] = L'\0';
                        len--;
                    }
                    
                    // Enhance the diagnostics with Windows error information
                    size_t newDiagSize = (result->diagnostics ? wcslen(result->diagnostics) : 0) + 1024;
                    wchar_t* enhancedDiag = (wchar_t*)SAFE_MALLOC(newDiagSize * sizeof(wchar_t));
                    if (enhancedDiag) {
                        swprintf(enhancedDiag, newDiagSize,
                            L"%ls\n\n=== WINDOWS API ERROR ===\n"
                            L"Error Code: %lu (0x%08lX)\n"
                            L"Error Message: %ls\n",
                            result->diagnostics ? result->diagnostics : L"No diagnostic information available",
                            errorCode, errorCode, windowsError);
                        
                        // Replace the diagnostics
                        if (result->diagnostics) SAFE_FREE(result->diagnostics);
                        result->diagnostics = enhancedDiag;
                    }
                }
            }
            
            ShowYtDlpError(hDlg, result, downloadContext->request);
        } else {
            ShowConfigurationError(hDlg, L"Download operation failed to initialize properly. Please check your yt-dlp configuration.");
        }
    }
    
    // Cleanup resources
    if (downloadContext->tempDir[0] != L'\0') {
        CleanupTempDirectory(downloadContext->tempDir);
    }
    if (result) FreeYtDlpResult(result);
    if (downloadContext->request) FreeYtDlpRequest(downloadContext->request);
    CleanupYtDlpConfig(&downloadContext->config);
    SAFE_FREE(downloadContext);
}

// ============================================================================
// Message Queue Implementation
// ============================================================================

// Create a new IPC message queue
IPCMessageQueue* CreateIPCMessageQueue(size_t capacity) {
    if (capacity == 0) capacity = 100; // Default capacity
    
    IPCMessageQueue* queue = (IPCMessageQueue*)SAFE_MALLOC(sizeof(IPCMessageQueue));
    if (!queue) return NULL;
    
    memset(queue, 0, sizeof(IPCMessageQueue));
    
    // Allocate message buffer
    queue->messages = (IPCMessage*)SAFE_MALLOC(sizeof(IPCMessage) * capacity);
    if (!queue->messages) {
        SAFE_FREE(queue);
        return NULL;
    }
    
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    
    // Initialize synchronization objects
    InitializeCriticalSection(&queue->lock);
    queue->notEmpty = CreateEventW(NULL, FALSE, FALSE, NULL); // Auto-reset event
    queue->notFull = CreateEventW(NULL, FALSE, TRUE, NULL);   // Auto-reset event, initially signaled
    
    if (!queue->notEmpty || !queue->notFull) {
        if (queue->notEmpty) CloseHandle(queue->notEmpty);
        if (queue->notFull) CloseHandle(queue->notFull);
        DeleteCriticalSection(&queue->lock);
        SAFE_FREE(queue->messages);
        SAFE_FREE(queue);
        return NULL;
    }
    
    return queue;
}

// Destroy an IPC message queue
void DestroyIPCMessageQueue(IPCMessageQueue* queue) {
    if (!queue) return;
    
    EnterCriticalSection(&queue->lock);
    
    // Free any remaining messages
    while (queue->count > 0) {
        FreeIPCMessage(&queue->messages[queue->head]);
        queue->head = (queue->head + 1) % queue->capacity;
        queue->count--;
    }
    
    LeaveCriticalSection(&queue->lock);
    
    // Cleanup synchronization objects
    if (queue->notEmpty) CloseHandle(queue->notEmpty);
    if (queue->notFull) CloseHandle(queue->notFull);
    DeleteCriticalSection(&queue->lock);
    
    // Free memory
    SAFE_FREE(queue->messages);
    SAFE_FREE(queue);
}

// Add a message to the queue
BOOL EnqueueIPCMessage(IPCMessageQueue* queue, const IPCMessage* message) {
    if (!queue || !message) return FALSE;
    
    EnterCriticalSection(&queue->lock);
    
    // Check if queue is full
    if (queue->count >= queue->capacity) {
        LeaveCriticalSection(&queue->lock);
        return FALSE; // Queue is full
    }
    
    // Copy message to queue
    memcpy(&queue->messages[queue->tail], message, sizeof(IPCMessage));
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    // Signal that queue is not empty
    SetEvent(queue->notEmpty);
    
    LeaveCriticalSection(&queue->lock);
    
    return TRUE;
}

// Remove a message from the queue
BOOL DequeueIPCMessage(IPCMessageQueue* queue, IPCMessage* message) {
    if (!queue || !message) return FALSE;
    
    EnterCriticalSection(&queue->lock);
    
    // Check if queue is empty
    if (queue->count == 0) {
        LeaveCriticalSection(&queue->lock);
        
        // Wait for messages with timeout
        if (WaitForSingleObject(queue->notEmpty, 100) != WAIT_OBJECT_0) {
            return FALSE; // Timeout or error
        }
        
        EnterCriticalSection(&queue->lock);
        if (queue->count == 0) {
            LeaveCriticalSection(&queue->lock);
            return FALSE; // Still empty
        }
    }
    
    // Copy message from queue
    memcpy(message, &queue->messages[queue->head], sizeof(IPCMessage));
    memset(&queue->messages[queue->head], 0, sizeof(IPCMessage)); // Clear the slot
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    // Signal that queue is not full
    SetEvent(queue->notFull);
    
    LeaveCriticalSection(&queue->lock);
    
    return TRUE;
}

// Free resources associated with an IPC message
void FreeIPCMessage(IPCMessage* message) {
    if (!message) return;
    
    if (message->autoFreeStrings) {
        switch (message->type) {
            case IPC_MSG_STATUS_UPDATE:
                if (message->data.status.text) {
                    SAFE_FREE(message->data.status.text);
                    message->data.status.text = NULL;
                }
                break;
                
            case IPC_MSG_TITLE_UPDATE:
                if (message->data.title.title) {
                    SAFE_FREE(message->data.title.title);
                    message->data.title.title = NULL;
                }
                break;
                
            case IPC_MSG_DURATION_UPDATE:
                if (message->data.duration.duration) {
                    SAFE_FREE(message->data.duration.duration);
                    message->data.duration.duration = NULL;
                }
                break;
                
            default:
                // No strings to free for other message types
                break;
        }
    }
    
    memset(message, 0, sizeof(IPCMessage));
}

// ============================================================================
// Global IPC System Management
// ============================================================================

// Initialize the global IPC system
BOOL InitializeGlobalIPC(void) {
    if (g_ipcInitialized) return TRUE;
    
    if (InitializeIPC(&g_ipcContext, 200)) { // 200 message capacity
        g_ipcInitialized = TRUE;
        return TRUE;
    }
    
    return FALSE;
}

// Cleanup the global IPC system
void CleanupGlobalIPC(void) {
    if (g_ipcInitialized) {
        CleanupIPC(&g_ipcContext);
        g_ipcInitialized = FALSE;
    }
}

// Get the global IPC context
IPCContext* GetGlobalIPCContext(void) {
    return g_ipcInitialized ? &g_ipcContext : NULL;
}

// ============================================================================
// Advanced IPC Functions
// ============================================================================

// Send a priority IPC message (higher priority messages are processed first)
BOOL SendPriorityIPCMessage(IPCContext* context, const IPCMessage* message, DWORD priority) {
    if (!context || !context->queue || !message) return FALSE;
    
    IPCMessage priorityMessage = *message;
    priorityMessage.priority = priority;
    priorityMessage.threadId = GetCurrentThreadId();
    
    EnterCriticalSection(&context->lock);
    BOOL result = !context->shutdown && EnqueueIPCMessage(context->queue, &priorityMessage);
    if (result && context->enableStatistics) {
        context->stats.totalMessagesSent++;
    }
    LeaveCriticalSection(&context->lock);
    
    return result;
}

// Flush all pending messages in the IPC queue
BOOL FlushIPCQueue(IPCContext* context) {
    if (!context || !context->queue) return FALSE;
    
    EnterCriticalSection(&context->lock);
    
    // Wait for queue to be empty with timeout
    DWORD startTime = GetTickCount();
    while (context->queue->count > 0 && (GetTickCount() - startTime) < 5000) {
        LeaveCriticalSection(&context->lock);
        Sleep(10);
        EnterCriticalSection(&context->lock);
    }
    
    BOOL success = (context->queue->count == 0);
    LeaveCriticalSection(&context->lock);
    
    return success;
}

// Get current IPC performance statistics
void GetIPCStatistics(IPCContext* context, IPCStatistics* stats) {
    if (!context || !stats) return;
    
    EnterCriticalSection(&context->lock);
    *stats = context->stats;
    
    // Add current queue size as high water mark if it's higher
    if (context->queue && context->queue->count > stats->queueHighWaterMark) {
        stats->queueHighWaterMark = context->queue->count;
        context->stats.queueHighWaterMark = context->queue->count;
    }
    
    LeaveCriticalSection(&context->lock);
}

// Reset IPC performance statistics
void ResetIPCStatistics(IPCContext* context) {
    if (!context) return;
    
    EnterCriticalSection(&context->lock);
    memset(&context->stats, 0, sizeof(IPCStatistics));
    context->stats.lastResetTime = GetTickCount();
    LeaveCriticalSection(&context->lock);
}

// Enable or disable IPC statistics collection
BOOL SetIPCStatisticsEnabled(IPCContext* context, BOOL enabled) {
    if (!context) return FALSE;
    
    EnterCriticalSection(&context->lock);
    context->enableStatistics = enabled;
    if (!enabled) {
        memset(&context->stats, 0, sizeof(IPCStatistics));
    } else {
        context->stats.lastResetTime = GetTickCount();
    }
    LeaveCriticalSection(&context->lock);
    
    return TRUE;
}

// Send both progress and status update in a single optimized operation
BOOL SendBatchProgressUpdate(IPCContext* context, HWND targetWindow, int percentage, const wchar_t* status) {
    if (!context) return FALSE;
    
    BOOL success = TRUE;
    
    // Send progress update
    success &= SendProgressUpdate(context, targetWindow, percentage);
    
    // Send status update if provided
    if (status && wcslen(status) > 0) {
        success &= SendStatusUpdate(context, targetWindow, status);
    }
    
    return success;
}

// Send both title and duration update in a single optimized operation
BOOL SendBatchMetadataUpdate(IPCContext* context, HWND targetWindow, const wchar_t* title, const wchar_t* duration) {
    if (!context) return FALSE;
    
    BOOL success = TRUE;
    
    // Send title update if provided
    if (title && wcslen(title) > 0) {
        success &= SendTitleUpdate(context, targetWindow, title);
    }
    
    // Send duration update if provided
    if (duration && wcslen(duration) > 0) {
        success &= SendDurationUpdate(context, targetWindow, duration);
    }
    
    return success;
}