#include "YouTubeCacher.h"

// Initialize the cache manager
BOOL InitializeCacheManager(CacheManager* manager, const wchar_t* downloadPath) {
    OutputDebugStringW(L"YouTubeCacher: InitializeCacheManager - ENTRY\n");
    
    if (!manager || !downloadPath) {
        OutputDebugStringW(L"YouTubeCacher: InitializeCacheManager - NULL parameters\n");
        return FALSE;
    }
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: InitializeCacheManager - downloadPath: %ls\n", downloadPath);
    OutputDebugStringW(debugMsg);
    
    memset(manager, 0, sizeof(CacheManager));
    
    // Initialize critical section for thread safety
    InitializeCriticalSection(&manager->lock);
    OutputDebugStringW(L"YouTubeCacher: InitializeCacheManager - Critical section initialized\n");
    
    // Build cache file path
    swprintf(manager->cacheFilePath, MAX_EXTENDED_PATH, L"%ls\\%ls", downloadPath, CACHE_FILE_NAME);
    swprintf(debugMsg, 512, L"YouTubeCacher: InitializeCacheManager - cacheFilePath: %ls\n", manager->cacheFilePath);
    OutputDebugStringW(debugMsg);
    
    // Load existing cache from file
    OutputDebugStringW(L"YouTubeCacher: InitializeCacheManager - Loading cache from file\n");
    LoadCacheFromFile(manager);
    
    swprintf(debugMsg, 512, L"YouTubeCacher: InitializeCacheManager - SUCCESS, loaded %d entries\n", manager->totalEntries);
    OutputDebugStringW(debugMsg);
    
    return TRUE;
}

// Clean up the cache manager
void CleanupCacheManager(CacheManager* manager) {
    if (!manager) return;
    
    EnterCriticalSection(&manager->lock);
    
    // Free all cache entries
    CacheEntry* current = manager->entries;
    while (current) {
        CacheEntry* next = current->next;
        FreeCacheEntry(current);
        current = next;
    }
    
    manager->entries = NULL;
    manager->totalEntries = 0;
    
    LeaveCriticalSection(&manager->lock);
    DeleteCriticalSection(&manager->lock);
}

// Free a single cache entry
void FreeCacheEntry(CacheEntry* entry) {
    if (!entry) return;
    
    if (entry->videoId) free(entry->videoId);
    if (entry->title) free(entry->title);
    if (entry->duration) free(entry->duration);
    if (entry->mainVideoFile) free(entry->mainVideoFile);
    
    if (entry->subtitleFiles) {
        for (int i = 0; i < entry->subtitleCount; i++) {
            if (entry->subtitleFiles[i]) {
                free(entry->subtitleFiles[i]);
            }
        }
        free(entry->subtitleFiles);
    }
    
    free(entry);
}

// Load cache from file
BOOL LoadCacheFromFile(CacheManager* manager) {
    if (!manager) return FALSE;
    
    FILE* file = _wfopen(manager->cacheFilePath, L"r,ccs=UTF-8");
    if (!file) {
        // File doesn't exist yet, that's okay
        return TRUE;
    }
    
    wchar_t line[MAX_CACHE_LINE_LENGTH];
    wchar_t version[32];
    
    // Read and verify version header
    if (!fgetws(version, 32, file) || wcsncmp(version, L"CACHE_VERSION=", 14) != 0) {
        fclose(file);
        return FALSE;
    }
    
    EnterCriticalSection(&manager->lock);
    
    // Read cache entries
    while (fgetws(line, MAX_CACHE_LINE_LENGTH, file)) {
        // Remove newline
        wchar_t* newline = wcschr(line, L'\n');
        if (newline) *newline = L'\0';
        
        // Skip empty lines
        if (wcslen(line) == 0) continue;
        
        // Parse cache entry line format:
        // VIDEO_ID|TITLE|DURATION|MAIN_FILE|SUBTITLE_COUNT|SUBTITLE1|SUBTITLE2|...
        wchar_t* context = NULL;
        wchar_t* token = wcstok(line, L"|", &context);
        if (!token) continue;
        
        CacheEntry* entry = (CacheEntry*)malloc(sizeof(CacheEntry));
        if (!entry) continue;
        
        memset(entry, 0, sizeof(CacheEntry));
        
        // Parse video ID
        entry->videoId = _wcsdup(token);
        
        // Parse title (base64 encoded)
        token = wcstok(NULL, L"|", &context);
        if (token && wcslen(token) > 0) {
            entry->title = Base64DecodeWide(token);
            if (entry->title) {
                wchar_t debugMsg[1024];
                swprintf(debugMsg, 1024, L"YouTubeCacher: LoadCacheFromFile - Decoded title: %ls (length: %zu)\n", 
                        entry->title, wcslen(entry->title));
                OutputDebugStringW(debugMsg);
            } else {
                OutputDebugStringW(L"YouTubeCacher: LoadCacheFromFile - ERROR: Base64DecodeWide returned NULL\n");
            }
        }
        
        // Parse duration
        token = wcstok(NULL, L"|", &context);
        if (token) {
            entry->duration = _wcsdup(token);
            // Format the duration properly (fix legacy single-number durations)
            if (entry->duration) {
                // Create a temporary buffer for formatting
                size_t len = wcslen(entry->duration);
                wchar_t* tempDuration = (wchar_t*)malloc((len + 32) * sizeof(wchar_t));
                if (tempDuration) {
                    wcscpy(tempDuration, entry->duration);
                    FormatDuration(tempDuration, len + 32);
                    
                    // Replace the original if it changed
                    if (wcscmp(entry->duration, tempDuration) != 0) {
                        free(entry->duration);
                        entry->duration = _wcsdup(tempDuration);
                    }
                    free(tempDuration);
                }
            }
        }
        
        // Parse main video file
        token = wcstok(NULL, L"|", &context);
        if (token) entry->mainVideoFile = _wcsdup(token);
        
        // Parse subtitle count
        token = wcstok(NULL, L"|", &context);
        if (token) {
            entry->subtitleCount = _wtoi(token);
            if (entry->subtitleCount > 0) {
                entry->subtitleFiles = (wchar_t**)malloc(entry->subtitleCount * sizeof(wchar_t*));
                if (entry->subtitleFiles) {
                    for (int i = 0; i < entry->subtitleCount; i++) {
                        token = wcstok(NULL, L"|", &context);
                        if (token) {
                            entry->subtitleFiles[i] = _wcsdup(token);
                        } else {
                            entry->subtitleFiles[i] = NULL;
                        }
                    }
                }
            }
        }
        
        // Get file info for the main video file
        if (entry->mainVideoFile) {
            GetVideoFileInfo(entry->mainVideoFile, &entry->fileSize, &entry->downloadTime);
        }
        
        // Validate entry before adding
        if (ValidateCacheEntry(entry)) {
            // Add to linked list
            entry->next = manager->entries;
            manager->entries = entry;
            manager->totalEntries++;
            OutputDebugStringW(L"YouTubeCacher: LoadCacheFromFile - Entry validated and added\n");
        } else {
            OutputDebugStringW(L"YouTubeCacher: LoadCacheFromFile - Entry validation FAILED, freeing entry\n");
            FreeCacheEntry(entry);
        }
    }
    
    LeaveCriticalSection(&manager->lock);
    fclose(file);
    return TRUE;
}

// Save cache to file
BOOL SaveCacheToFile(CacheManager* manager) {
    if (!manager) return FALSE;
    
    FILE* file = _wfopen(manager->cacheFilePath, L"w,ccs=UTF-8");
    if (!file) return FALSE;
    
    // Write version header
    fwprintf(file, L"CACHE_VERSION=%ls\n", CACHE_VERSION);
    
    EnterCriticalSection(&manager->lock);
    
    // Write each cache entry
    CacheEntry* current = manager->entries;
    while (current) {
        if (ValidateCacheEntry(current)) {
            // Encode title as base64 to handle all Unicode characters safely
            wchar_t* encodedTitle = current->title ? Base64EncodeWide(current->title) : NULL;
            
            fwprintf(file, L"%ls|%ls|%ls|%ls|%d",
                    current->videoId ? current->videoId : L"",
                    encodedTitle ? encodedTitle : L"",
                    current->duration ? current->duration : L"",
                    current->mainVideoFile ? current->mainVideoFile : L"",
                    current->subtitleCount);
            
            if (encodedTitle) free(encodedTitle);
            
            // Write subtitle files
            for (int i = 0; i < current->subtitleCount; i++) {
                if (current->subtitleFiles && current->subtitleFiles[i]) {
                    fwprintf(file, L"|%ls", current->subtitleFiles[i]);
                } else {
                    fwprintf(file, L"|");
                }
            }
            
            fwprintf(file, L"\n");
        }
        current = current->next;
    }
    
    LeaveCriticalSection(&manager->lock);
    fclose(file);
    return TRUE;
}

// Add a new cache entry
BOOL AddCacheEntry(CacheManager* manager, const wchar_t* videoId, const wchar_t* title, 
                   const wchar_t* duration, const wchar_t* mainVideoFile, 
                   wchar_t** subtitleFiles, int subtitleCount) {
    if (!manager || !videoId || !mainVideoFile) return FALSE;
    
    // Debug logging
    OutputDebugStringW(L"YouTubeCacher: AddCacheEntry - Starting\n");
    if (title) {
        wchar_t debugMsg[1024];
        swprintf(debugMsg, 1024, L"YouTubeCacher: AddCacheEntry - Title: %ls (length: %zu)\n", title, wcslen(title));
        OutputDebugStringW(debugMsg);
    }
    
    EnterCriticalSection(&manager->lock);
    
    // Check if entry already exists
    CacheEntry* existing = FindCacheEntry(manager, videoId);
    if (existing) {
        LeaveCriticalSection(&manager->lock);
        return FALSE; // Already exists
    }
    
    // Create new entry
    CacheEntry* entry = (CacheEntry*)malloc(sizeof(CacheEntry));
    if (!entry) {
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }
    
    memset(entry, 0, sizeof(CacheEntry));
    
    entry->videoId = _wcsdup(videoId);
    entry->title = title ? _wcsdup(title) : NULL;
    entry->duration = duration ? _wcsdup(duration) : NULL;
    entry->mainVideoFile = _wcsdup(mainVideoFile);
    entry->subtitleCount = subtitleCount;
    
    // Copy subtitle files
    if (subtitleCount > 0 && subtitleFiles) {
        entry->subtitleFiles = (wchar_t**)malloc(subtitleCount * sizeof(wchar_t*));
        if (entry->subtitleFiles) {
            for (int i = 0; i < subtitleCount; i++) {
                entry->subtitleFiles[i] = subtitleFiles[i] ? _wcsdup(subtitleFiles[i]) : NULL;
            }
        }
    }
    
    // Get file info
    GetVideoFileInfo(mainVideoFile, &entry->fileSize, &entry->downloadTime);
    
    // Add to linked list
    entry->next = manager->entries;
    manager->entries = entry;
    manager->totalEntries++;
    
    LeaveCriticalSection(&manager->lock);
    
    OutputDebugStringW(L"YouTubeCacher: AddCacheEntry - Entry added to memory, saving to file\n");
    
    // Save to file
    BOOL saveResult = SaveCacheToFile(manager);
    
    if (saveResult) {
        OutputDebugStringW(L"YouTubeCacher: AddCacheEntry - Successfully saved to file\n");
    } else {
        OutputDebugStringW(L"YouTubeCacher: AddCacheEntry - ERROR: Failed to save to file\n");
    }
    
    return TRUE;
}

// Remove a cache entry
BOOL RemoveCacheEntry(CacheManager* manager, const wchar_t* videoId) {
    if (!manager || !videoId) return FALSE;
    
    EnterCriticalSection(&manager->lock);
    
    CacheEntry* current = manager->entries;
    CacheEntry* previous = NULL;
    
    while (current) {
        if (current->videoId && wcscmp(current->videoId, videoId) == 0) {
            // Remove from linked list
            if (previous) {
                previous->next = current->next;
            } else {
                manager->entries = current->next;
            }
            
            manager->totalEntries--;
            FreeCacheEntry(current);
            
            LeaveCriticalSection(&manager->lock);
            
            // Save updated cache
            SaveCacheToFile(manager);
            return TRUE;
        }
        
        previous = current;
        current = current->next;
    }
    
    LeaveCriticalSection(&manager->lock);
    return FALSE;
}

// Find a cache entry by video ID
CacheEntry* FindCacheEntry(CacheManager* manager, const wchar_t* videoId) {
    if (!manager || !videoId) return NULL;
    
    CacheEntry* current = manager->entries;
    while (current) {
        if (current->videoId && wcscmp(current->videoId, videoId) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

// Delete all files associated with a cache entry (simple version)
BOOL DeleteCacheEntryFiles(CacheManager* manager, const wchar_t* videoId) {
    DeleteResult* result = DeleteCacheEntryFilesDetailed(manager, videoId);
    if (!result) return FALSE;
    
    BOOL success = (result->errorCount == 0);
    FreeDeleteResult(result);
    return success;
}

// Delete all files associated with a cache entry with detailed error reporting
DeleteResult* DeleteCacheEntryFilesDetailed(CacheManager* manager, const wchar_t* videoId) {
    if (!manager || !videoId) return NULL;
    
    EnterCriticalSection(&manager->lock);
    
    CacheEntry* entry = FindCacheEntry(manager, videoId);
    if (!entry) {
        LeaveCriticalSection(&manager->lock);
        return NULL;
    }
    
    // Create result structure
    DeleteResult* result = (DeleteResult*)malloc(sizeof(DeleteResult));
    if (!result) {
        LeaveCriticalSection(&manager->lock);
        return NULL;
    }
    
    memset(result, 0, sizeof(DeleteResult));
    
    // Calculate total files to delete
    result->totalFiles = (entry->mainVideoFile ? 1 : 0) + entry->subtitleCount;
    
    // Allocate error array (worst case: all files fail)
    if (result->totalFiles > 0) {
        result->errors = (FileDeleteError*)malloc(result->totalFiles * sizeof(FileDeleteError));
        if (!result->errors) {
            free(result);
            LeaveCriticalSection(&manager->lock);
            return NULL;
        }
        memset(result->errors, 0, result->totalFiles * sizeof(FileDeleteError));
    }
    
    // Delete main video file
    if (entry->mainVideoFile) {
        if (!DeleteFileW(entry->mainVideoFile)) {
            DWORD error = GetLastError();
            result->errors[result->errorCount].fileName = _wcsdup(entry->mainVideoFile);
            result->errors[result->errorCount].errorCode = error;
            result->errorCount++;
        } else {
            result->successfulDeletes++;
        }
    }
    
    // Delete subtitle files
    for (int i = 0; i < entry->subtitleCount; i++) {
        if (entry->subtitleFiles && entry->subtitleFiles[i]) {
            if (!DeleteFileW(entry->subtitleFiles[i])) {
                DWORD error = GetLastError();
                result->errors[result->errorCount].fileName = _wcsdup(entry->subtitleFiles[i]);
                result->errors[result->errorCount].errorCode = error;
                result->errorCount++;
            } else {
                result->successfulDeletes++;
            }
        }
    }
    
    LeaveCriticalSection(&manager->lock);
    
    // Remove from cache if all files were deleted successfully
    if (result->errorCount == 0) {
        RemoveCacheEntry(manager, videoId);
    }
    
    return result;
}

// Free delete result structure
void FreeDeleteResult(DeleteResult* result) {
    if (!result) return;
    
    if (result->errors) {
        for (int i = 0; i < result->errorCount; i++) {
            if (result->errors[i].fileName) {
                free(result->errors[i].fileName);
            }
        }
        free(result->errors);
    }
    
    free(result);
}

// Format delete error details for display
wchar_t* FormatDeleteErrorDetails(const DeleteResult* result) {
    if (!result) return NULL;
    
    // Calculate required buffer size
    size_t bufferSize = 1024; // Base size for headers
    for (int i = 0; i < result->errorCount; i++) {
        if (result->errors[i].fileName) {
            bufferSize += wcslen(result->errors[i].fileName) + 100; // File name + error info
        }
    }
    
    wchar_t* details = (wchar_t*)malloc(bufferSize * sizeof(wchar_t));
    if (!details) return NULL;
    
    // Format summary
    swprintf(details, bufferSize, 
        L"Delete Operation Summary:\n"
        L"Total files: %d\n"
        L"Successfully deleted: %d\n"
        L"Failed to delete: %d\n\n",
        result->totalFiles, result->successfulDeletes, result->errorCount);
    
    if (result->errorCount > 0) {
        wcscat(details, L"Failed Files:\n");
        wcscat(details, L"=============\n\n");
        
        for (int i = 0; i < result->errorCount; i++) {
            wchar_t errorInfo[1024];
            wchar_t* fileName = result->errors[i].fileName;
            DWORD errorCode = result->errors[i].errorCode;
            
            // Get just the filename without full path for display
            wchar_t* displayName = wcsrchr(fileName, L'\\');
            if (displayName) {
                displayName++; // Skip the backslash
            } else {
                displayName = fileName;
            }
            
            // Format error message
            wchar_t errorMessage[256] = L"Unknown error";
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          errorMessage, 256, NULL);
            
            // Remove trailing newlines from error message
            size_t len = wcslen(errorMessage);
            while (len > 0 && (errorMessage[len-1] == L'\n' || errorMessage[len-1] == L'\r')) {
                errorMessage[len-1] = L'\0';
                len--;
            }
            
            swprintf(errorInfo, 1024, 
                L"File: %ls\n"
                L"Error Code: %lu (0x%08lX)\n"
                L"Error: %ls\n"
                L"Full Path: %ls\n\n",
                displayName, errorCode, errorCode, errorMessage, fileName);
            
            // Check if we have enough space
            if (wcslen(details) + wcslen(errorInfo) < bufferSize - 1) {
                wcscat(details, errorInfo);
            } else {
                wcscat(details, L"... (additional errors truncated)\n");
                break;
            }
        }
    }
    
    return details;
}

// Play a cached video
BOOL PlayCacheEntry(CacheManager* manager, const wchar_t* videoId, const wchar_t* playerPath) {
    if (!manager || !videoId || !playerPath) return FALSE;
    
    EnterCriticalSection(&manager->lock);
    
    CacheEntry* entry = FindCacheEntry(manager, videoId);
    if (!entry || !entry->mainVideoFile) {
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }
    
    // Check if video file still exists
    DWORD attributes = GetFileAttributesW(entry->mainVideoFile);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }
    
    // Build command line to launch player
    size_t cmdLineLen = wcslen(playerPath) + wcslen(entry->mainVideoFile) + 10;
    wchar_t* cmdLine = (wchar_t*)malloc(cmdLineLen * sizeof(wchar_t));
    if (!cmdLine) {
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }
    
    swprintf(cmdLine, cmdLineLen, L"\"%ls\" \"%ls\"", playerPath, entry->mainVideoFile);
    
    LeaveCriticalSection(&manager->lock);
    
    // Launch player
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    BOOL result = CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    
    if (result) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    free(cmdLine);
    return result;
}

// Initialize ListView with columns
void InitializeCacheListView(HWND hListView) {
    OutputDebugStringW(L"YouTubeCacher: InitializeCacheListView - ENTRY\n");
    
    if (!hListView) {
        OutputDebugStringW(L"YouTubeCacher: InitializeCacheListView - NULL hListView\n");
        return;
    }
    
    OutputDebugStringW(L"YouTubeCacher: InitializeCacheListView - Setting extended styles\n");
    
    // Set extended styles for better appearance
    DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
    SendMessageW(hListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, exStyle);
    
    // Add columns
    LVCOLUMNW column = {0};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    
    OutputDebugStringW(L"YouTubeCacher: InitializeCacheListView - Adding Title column\n");
    
    // Title column (will resize with window)
    column.pszText = L"Title";
    column.cx = 300; // Initial width
    column.iSubItem = 0;
    int result1 = SendMessageW(hListView, LVM_INSERTCOLUMNW, 0, (LPARAM)&column);
    
    OutputDebugStringW(L"YouTubeCacher: InitializeCacheListView - Adding Duration column\n");
    
    // Duration column (fixed width)
    column.pszText = L"Duration";
    column.cx = 80; // Fixed width
    column.iSubItem = 1;
    int result2 = SendMessageW(hListView, LVM_INSERTCOLUMNW, 1, (LPARAM)&column);
    
    wchar_t debugMsg[256];
    swprintf(debugMsg, 256, L"YouTubeCacher: InitializeCacheListView - Column results: %d, %d\n", result1, result2);
    OutputDebugStringW(debugMsg);
    
    OutputDebugStringW(L"YouTubeCacher: InitializeCacheListView - COMPLETE\n");
}

// Resize ListView columns
void ResizeCacheListViewColumns(HWND hListView, int totalWidth) {
    if (!hListView) return;
    
    const int durationColumnWidth = 80; // Fixed width for duration
    const int scrollbarWidth = GetSystemMetrics(SM_CXVSCROLL);
    const int borderWidth = 4; // Account for borders
    
    int titleColumnWidth = totalWidth - durationColumnWidth - scrollbarWidth - borderWidth;
    if (titleColumnWidth < 100) titleColumnWidth = 100; // Minimum width
    
    // Resize title column
    SendMessageW(hListView, LVM_SETCOLUMNWIDTH, 0, titleColumnWidth);
    
    // Duration column stays fixed
    SendMessageW(hListView, LVM_SETCOLUMNWIDTH, 1, durationColumnWidth);
}

// Refresh the cache list in the UI
void RefreshCacheList(HWND hListView, CacheManager* manager) {
    if (!hListView || !manager) return;
    
    // Clean up existing item data before clearing
    CleanupListViewItemData(hListView);
    
    // Clear existing items
    SendMessageW(hListView, LVM_DELETEALLITEMS, 0, 0);
    
    EnterCriticalSection(&manager->lock);
    
    CacheEntry* current = manager->entries;
    int itemIndex = 0;
    
    while (current) {
        if (ValidateCacheEntry(current)) {
            LVITEMW item = {0};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = itemIndex;
            
            // Title column
            item.iSubItem = 0;
            item.pszText = current->title ? current->title : L"Unknown Title";
            item.lParam = (LPARAM)_wcsdup(current->videoId); // Store video ID
            
            int insertedIndex = (int)SendMessageW(hListView, LVM_INSERTITEMW, 0, (LPARAM)&item);
            
            if (insertedIndex != -1) {
                // Duration column
                LVITEMW subItem = {0};
                subItem.mask = LVIF_TEXT;
                subItem.iItem = insertedIndex;
                subItem.iSubItem = 1;
                subItem.pszText = current->duration ? current->duration : L"Unknown";
                
                SendMessageW(hListView, LVM_SETITEMW, 0, (LPARAM)&subItem);
                itemIndex++;
            }
        }
        current = current->next;
    }
    
    LeaveCriticalSection(&manager->lock);
}



// Extract video ID from YouTube URL
wchar_t* ExtractVideoIdFromUrl(const wchar_t* url) {
    if (!url) return NULL;
    
    // Debug: Log the URL being processed
    wchar_t debugMsg[1024];
    swprintf(debugMsg, 1024, L"YouTubeCacher: ExtractVideoIdFromUrl - Processing URL: %ls\n", url);
    OutputDebugStringW(debugMsg);
    
    // Look for v= parameter
    const wchar_t* vParam = wcsstr(url, L"v=");
    if (vParam) {
        vParam += 2; // Skip "v="
        
        // Find end of video ID (11 characters or until & or end)
        const wchar_t* end = vParam;
        int count = 0;
        while (*end && *end != L'&' && *end != L'#' && count < 11) {
            end++;
            count++;
        }
        
        if (count == 11) {
            wchar_t* videoId = (wchar_t*)malloc(12 * sizeof(wchar_t));
            if (videoId) {
                wcsncpy(videoId, vParam, 11);
                videoId[11] = L'\0';
                swprintf(debugMsg, 1024, L"YouTubeCacher: ExtractVideoIdFromUrl - Found video ID (v= format): %ls\n", videoId);
                OutputDebugStringW(debugMsg);
                return videoId;
            }
        }
    }
    
    // Look for youtu.be/ format
    const wchar_t* shortUrl = wcsstr(url, L"youtu.be/");
    if (shortUrl) {
        shortUrl += 9; // Skip "youtu.be/"
        
        const wchar_t* end = shortUrl;
        int count = 0;
        while (*end && *end != L'?' && *end != L'&' && *end != L'#' && count < 11) {
            end++;
            count++;
        }
        
        if (count == 11) {
            wchar_t* videoId = (wchar_t*)malloc(12 * sizeof(wchar_t));
            if (videoId) {
                wcsncpy(videoId, shortUrl, 11);
                videoId[11] = L'\0';
                swprintf(debugMsg, 1024, L"YouTubeCacher: ExtractVideoIdFromUrl - Found video ID (youtu.be format): %ls\n", videoId);
                OutputDebugStringW(debugMsg);
                return videoId;
            }
        }
    }
    
    // Look for YouTube Shorts format: youtube.com/shorts/VIDEO_ID
    const wchar_t* shortsUrl = wcsstr(url, L"/shorts/");
    if (shortsUrl) {
        shortsUrl += 8; // Skip "/shorts/"
        
        const wchar_t* end = shortsUrl;
        int count = 0;
        while (*end && *end != L'?' && *end != L'&' && *end != L'#' && count < 11) {
            end++;
            count++;
        }
        
        if (count == 11) {
            wchar_t* videoId = (wchar_t*)malloc(12 * sizeof(wchar_t));
            if (videoId) {
                wcsncpy(videoId, shortsUrl, 11);
                videoId[11] = L'\0';
                swprintf(debugMsg, 1024, L"YouTubeCacher: ExtractVideoIdFromUrl - Found video ID (shorts format): %ls\n", videoId);
                OutputDebugStringW(debugMsg);
                return videoId;
            }
        }
    }
    
    OutputDebugStringW(L"YouTubeCacher: ExtractVideoIdFromUrl - No video ID found in URL\n");
    return NULL;
}

// Validate a cache entry
BOOL ValidateCacheEntry(const CacheEntry* entry) {
    if (!entry || !entry->videoId || !entry->mainVideoFile) {
        OutputDebugStringW(L"YouTubeCacher: ValidateCacheEntry - NULL entry or missing required fields\n");
        return FALSE;
    }
    
    // Check if main video file exists
    DWORD attributes = GetFileAttributesW(entry->mainVideoFile);
    BOOL fileExists = (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
    
    if (!fileExists) {
        wchar_t debugMsg[1024];
        swprintf(debugMsg, 1024, L"YouTubeCacher: ValidateCacheEntry - File does not exist: %ls\n", entry->mainVideoFile);
        OutputDebugStringW(debugMsg);
    }
    
    return fileExists;
}

// Get video file information
BOOL GetVideoFileInfo(const wchar_t* filePath, DWORD* fileSize, FILETIME* modTime) {
    if (!filePath) return FALSE;
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(filePath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    if (fileSize) {
        *fileSize = findData.nFileSizeLow; // Simplified for files < 4GB
    }
    
    if (modTime) {
        *modTime = findData.ftLastWriteTime;
    }
    
    FindClose(hFind);
    return TRUE;
}

// Find subtitle files for a video
BOOL FindSubtitleFiles(const wchar_t* videoFilePath, wchar_t*** subtitleFiles, int* count) {
    if (!videoFilePath || !subtitleFiles || !count) return FALSE;
    
    *subtitleFiles = NULL;
    *count = 0;
    
    // Get base name without extension
    wchar_t baseName[MAX_EXTENDED_PATH];
    wcscpy(baseName, videoFilePath);
    wchar_t* lastDot = wcsrchr(baseName, L'.');
    if (lastDot) *lastDot = L'\0';
    
    // Search for subtitle files with common extensions
    const wchar_t* subtitleExts[] = {L".srt", L".vtt", L".ass", L".ssa", L".sub"};
    int numExts = sizeof(subtitleExts) / sizeof(subtitleExts[0]);
    
    wchar_t** tempFiles = (wchar_t**)malloc(numExts * sizeof(wchar_t*));
    if (!tempFiles) return FALSE;
    
    int foundCount = 0;
    
    for (int i = 0; i < numExts; i++) {
        wchar_t subtitlePath[MAX_EXTENDED_PATH];
        swprintf(subtitlePath, MAX_EXTENDED_PATH, L"%ls%ls", baseName, subtitleExts[i]);
        
        DWORD attributes = GetFileAttributesW(subtitlePath);
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            tempFiles[foundCount] = _wcsdup(subtitlePath);
            foundCount++;
        }
    }
    
    if (foundCount > 0) {
        *subtitleFiles = (wchar_t**)realloc(tempFiles, foundCount * sizeof(wchar_t*));
        *count = foundCount;
        return TRUE;
    } else {
        free(tempFiles);
        return FALSE;
    }
}

// Format file size for display
wchar_t* FormatFileSize(DWORD sizeBytes) {
    wchar_t* result = (wchar_t*)malloc(32 * sizeof(wchar_t));
    if (!result) return NULL;
    
    if (sizeBytes < 1024) {
        swprintf(result, 32, L"%lu B", sizeBytes);
    } else if (sizeBytes < 1024 * 1024) {
        swprintf(result, 32, L"%.1f KB", sizeBytes / 1024.0);
    } else if (sizeBytes < 1024 * 1024 * 1024) {
        swprintf(result, 32, L"%.1f MB", sizeBytes / (1024.0 * 1024.0));
    } else {
        swprintf(result, 32, L"%.1f GB", sizeBytes / (1024.0 * 1024.0 * 1024.0));
    }
    
    return result;
}

// Format cache entry for display in list
wchar_t* FormatCacheEntryDisplay(const CacheEntry* entry) {
    if (!entry) return NULL;
    
    wchar_t* result = (wchar_t*)malloc(512 * sizeof(wchar_t));
    if (!result) return NULL;
    
    wchar_t* sizeStr = FormatFileSize(entry->fileSize);
    
    swprintf(result, 512, L"%ls - %ls (%ls)",
            entry->title ? entry->title : L"Unknown Title",
            entry->duration ? entry->duration : L"Unknown Duration",
            sizeStr ? sizeStr : L"Unknown Size");
    
    if (sizeStr) free(sizeStr);
    
    return result;
}

// Add dummy video for debugging purposes
BOOL AddDummyVideo(CacheManager* manager, const wchar_t* downloadPath) {
    if (!manager || !downloadPath) return FALSE;
    
    static int dummyCounter = 1;
    
    // Generate dummy video data
    wchar_t videoId[12];
    wchar_t title[256];
    wchar_t duration[32];
    wchar_t filename[MAX_EXTENDED_PATH];
    wchar_t fullPath[MAX_EXTENDED_PATH];
    
    swprintf(videoId, 12, L"DUMMY%06d", dummyCounter);
    swprintf(title, 256, L"Debug Video %d - Sample Content for Testing", dummyCounter);
    swprintf(duration, 32, L"%d:%02d", (dummyCounter * 3) / 60, (dummyCounter * 3) % 60);
    swprintf(filename, MAX_EXTENDED_PATH, L"debug_video_%d [%ls].mp4", dummyCounter, videoId);
    swprintf(fullPath, MAX_EXTENDED_PATH, L"%ls\\%ls", downloadPath, filename);
    
    // Create a dummy file (0 bytes) to simulate the video file
    HANDLE hFile = CreateFileW(fullPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        // Write some dummy data to make it look like a real file
        const char dummyData[] = "This is a dummy video file for debugging purposes.";
        DWORD bytesWritten;
        WriteFile(hFile, dummyData, sizeof(dummyData), &bytesWritten, NULL);
        CloseHandle(hFile);
    }
    
    // Create dummy subtitle files
    wchar_t** subtitleFiles = (wchar_t**)malloc(2 * sizeof(wchar_t*));
    if (subtitleFiles) {
        // Create .srt file
        wchar_t srtPath[MAX_EXTENDED_PATH];
        swprintf(srtPath, MAX_EXTENDED_PATH, L"%ls\\debug_video_%d [%ls].en.srt", downloadPath, dummyCounter, videoId);
        subtitleFiles[0] = _wcsdup(srtPath);
        
        HANDLE hSrt = CreateFileW(srtPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hSrt != INVALID_HANDLE_VALUE) {
            const char srtData[] = "1\n00:00:00,000 --> 00:00:05,000\nThis is a dummy subtitle file.\n\n";
            DWORD bytesWritten;
            WriteFile(hSrt, srtData, sizeof(srtData), &bytesWritten, NULL);
            CloseHandle(hSrt);
        }
        
        // Create .vtt file
        wchar_t vttPath[MAX_EXTENDED_PATH];
        swprintf(vttPath, MAX_EXTENDED_PATH, L"%ls\\debug_video_%d [%ls].en.vtt", downloadPath, dummyCounter, videoId);
        subtitleFiles[1] = _wcsdup(vttPath);
        
        HANDLE hVtt = CreateFileW(vttPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hVtt != INVALID_HANDLE_VALUE) {
            const char vttData[] = "WEBVTT\n\n00:00:00.000 --> 00:00:05.000\nThis is a dummy WebVTT subtitle.\n\n";
            DWORD bytesWritten;
            WriteFile(hVtt, vttData, sizeof(vttData), &bytesWritten, NULL);
            CloseHandle(hVtt);
        }
    }
    
    // Add to cache
    BOOL result = AddCacheEntry(manager, videoId, title, duration, fullPath, subtitleFiles, 2);
    
    // Clean up
    if (subtitleFiles) {
        if (subtitleFiles[0]) free(subtitleFiles[0]);
        if (subtitleFiles[1]) free(subtitleFiles[1]);
        free(subtitleFiles);
    }
    
    dummyCounter++;
    return result;
}

// Scan download folder for existing videos (for initial cache population)
BOOL ScanDownloadFolderForVideos(CacheManager* manager, const wchar_t* downloadPath) {
    if (!manager || !downloadPath) return FALSE;
    
    wchar_t searchPattern[MAX_EXTENDED_PATH];
    swprintf(searchPattern, MAX_EXTENDED_PATH, L"%ls\\*.mp4", downloadPath);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPattern, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return TRUE; // No files found, but that's okay
    }
    
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            wchar_t fullPath[MAX_EXTENDED_PATH];
            swprintf(fullPath, MAX_EXTENDED_PATH, L"%ls\\%ls", downloadPath, findData.cFileName);
            
            // Try to extract video ID from filename (if it follows yt-dlp naming)
            wchar_t* videoId = NULL;
            wchar_t* bracket = wcschr(findData.cFileName, L'[');
            if (bracket) {
                wchar_t* closeBracket = wcschr(bracket, L']');
                if (closeBracket && (closeBracket - bracket) == 12) {
                    videoId = (wchar_t*)malloc(12 * sizeof(wchar_t));
                    if (videoId) {
                        wcsncpy(videoId, bracket + 1, 11);
                        videoId[11] = L'\0';
                    }
                }
            }
            
            if (videoId) {
                // Find subtitle files
                wchar_t** subtitleFiles = NULL;
                int subtitleCount = 0;
                FindSubtitleFiles(fullPath, &subtitleFiles, &subtitleCount);
                
                // Add to cache (title and duration will be unknown)
                AddCacheEntry(manager, videoId, findData.cFileName, L"Unknown", 
                             fullPath, subtitleFiles, subtitleCount);
                
                // Clean up
                if (subtitleFiles) {
                    for (int i = 0; i < subtitleCount; i++) {
                        if (subtitleFiles[i]) free(subtitleFiles[i]);
                    }
                    free(subtitleFiles);
                }
                
                free(videoId);
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    return TRUE;
}

// UI helper functions for better listbox management

// Update the status labels with current cache information
void UpdateCacheListStatus(HWND hDlg, CacheManager* manager) {
    if (!hDlg || !manager) return;
    
    wchar_t statusText[256];
    wchar_t itemsText[64];
    
    EnterCriticalSection(&manager->lock);
    
    // Calculate total size
    DWORD totalSize = 0;
    CacheEntry* current = manager->entries;
    while (current) {
        if (ValidateCacheEntry(current)) {
            totalSize += current->fileSize;
        }
        current = current->next;
    }
    
    // Format status text
    wchar_t* sizeStr = FormatFileSize(totalSize);
    swprintf(statusText, 256, L"Status: Ready - Total size: %ls", sizeStr ? sizeStr : L"0 B");
    if (sizeStr) free(sizeStr);
    
    // Format items count
    swprintf(itemsText, 64, L"Items: %d", manager->totalEntries);
    
    LeaveCriticalSection(&manager->lock);
    
    // Update UI labels
    SetDlgItemTextW(hDlg, IDC_LABEL2, statusText);
    SetDlgItemTextW(hDlg, IDC_LABEL3, itemsText);
}

// Get the video ID of the currently selected item (single selection)
wchar_t* GetSelectedVideoId(HWND hListView) {
    if (!hListView) return NULL;
    
    int selectedIndex = (int)SendMessageW(hListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
    if (selectedIndex == -1) return NULL;
    
    LVITEMW item = {0};
    item.mask = LVIF_PARAM;
    item.iItem = selectedIndex;
    
    if (SendMessageW(hListView, LVM_GETITEMW, 0, (LPARAM)&item)) {
        return (wchar_t*)item.lParam;
    }
    
    return NULL;
}

// Get all selected video IDs (multiple selection)
wchar_t** GetSelectedVideoIds(HWND hListView, int* count) {
    if (!hListView || !count) return NULL;
    
    *count = 0;
    
    // First pass: count selected items
    int selectedCount = 0;
    int index = -1;
    while ((index = (int)SendMessageW(hListView, LVM_GETNEXTITEM, index, LVNI_SELECTED)) != -1) {
        selectedCount++;
    }
    
    if (selectedCount == 0) return NULL;
    
    // Allocate array for video IDs
    wchar_t** videoIds = (wchar_t**)malloc(selectedCount * sizeof(wchar_t*));
    if (!videoIds) return NULL;
    
    // Second pass: collect video IDs
    int currentIndex = 0;
    index = -1;
    while ((index = (int)SendMessageW(hListView, LVM_GETNEXTITEM, index, LVNI_SELECTED)) != -1) {
        LVITEMW item = {0};
        item.mask = LVIF_PARAM;
        item.iItem = index;
        
        if (SendMessageW(hListView, LVM_GETITEMW, 0, (LPARAM)&item)) {
            wchar_t* videoId = (wchar_t*)item.lParam;
            if (videoId) {
                videoIds[currentIndex] = _wcsdup(videoId);
                currentIndex++;
            }
        }
    }
    
    *count = currentIndex;
    return videoIds;
}

// Free array of selected video IDs
void FreeSelectedVideoIds(wchar_t** videoIds, int count) {
    if (!videoIds) return;
    
    for (int i = 0; i < count; i++) {
        if (videoIds[i]) {
            free(videoIds[i]);
        }
    }
    
    free(videoIds);
}

// Clean up item data when clearing the ListView
void CleanupListViewItemData(HWND hListView) {
    if (!hListView) return;
    
    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    for (int i = 0; i < count; i++) {
        LVITEMW item = {0};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        
        if (SendMessageW(hListView, LVM_GETITEMW, 0, (LPARAM)&item)) {
            wchar_t* videoId = (wchar_t*)item.lParam;
            if (videoId) {
                free(videoId);
            }
        }
    }
}