#include "YouTubeCacher.h"

// Custom window messages (should match main.c and parser.c)
#define WM_UNIFIED_DOWNLOAD_UPDATE (WM_USER + 113)

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

// Progress callback for updating progress dialog
void SubprocessProgressCallback(int percentage, const wchar_t* status, void* userData) {
    ProgressDialog* progress = (ProgressDialog*)userData;
    if (progress) {
        UpdateProgressDialog(progress, percentage, status);
    }
}

// Progress callback for unified download
void UnifiedDownloadProgressCallback(int percentage, const wchar_t* status, void* userData) {
    HWND hDlg = (HWND)userData;
    if (!hDlg) return;
    
    PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, percentage); // Progress update
    if (status) {
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)_wcsdup(status)); // Status update
    }
}

// Progress callback for the main window (thread-safe) - Legacy function, kept for compatibility
void MainWindowProgressCallback(int percentage, const wchar_t* status, void* userData) {
    HWND hDlg = (HWND)userData;
    if (!hDlg) return;
    
    // Use unified download update message
    PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, percentage); // Progress update
    if (status) {
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)_wcsdup(status)); // Status update
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
                                    if (bestVideoFile) free(bestVideoFile);
                                    bestVideoFile = _wcsdup(fullVideoPath);
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
                            if (subtitleFiles[i]) free(subtitleFiles[i]);
                        }
                        free(subtitleFiles);
                    }
                    
                    free(bestVideoFile);
                } else {
                    OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - No video file found with enhanced detection\n");
                }
            }
            
            free(videoId);
            
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
                    wchar_t* enhancedDiag = (wchar_t*)malloc(newDiagSize * sizeof(wchar_t));
                    if (enhancedDiag) {
                        swprintf(enhancedDiag, newDiagSize,
                            L"%ls\n\n=== WINDOWS API ERROR ===\n"
                            L"Error Code: %lu (0x%08lX)\n"
                            L"Error Message: %ls\n",
                            result->diagnostics ? result->diagnostics : L"No diagnostic information available",
                            errorCode, errorCode, windowsError);
                        
                        // Replace the diagnostics
                        if (result->diagnostics) free(result->diagnostics);
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
    free(downloadContext);
}