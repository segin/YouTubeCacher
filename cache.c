#include "cache.h"
#include "YouTubeCacher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <shlwapi.h>

// Initialize the cache manager
BOOL InitializeCacheManager(CacheManager* manager, const wchar_t* downloadPath) {
    if (!manager || !downloadPath) return FALSE;
    
    memset(manager, 0, sizeof(CacheManager));
    
    // Initialize critical section for thread safety
    InitializeCriticalSection(&manager->lock);
    
    // Build cache file path
    swprintf(manager->cacheFilePath, MAX_PATH, L"%ls\\%ls", downloadPath, CACHE_FILE_NAME);
    
    // Load existing cache from file
    LoadCacheFromFile(manager);
    
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
        
        // Parse title
        token = wcstok(NULL, L"|", &context);
        if (token) entry->title = _wcsdup(token);
        
        // Parse duration
        token = wcstok(NULL, L"|", &context);
        if (token) entry->duration = _wcsdup(token);
        
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
        } else {
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
            fwprintf(file, L"%ls|%ls|%ls|%ls|%d",
                    current->videoId ? current->videoId : L"",
                    current->title ? current->title : L"",
                    current->duration ? current->duration : L"",
                    current->mainVideoFile ? current->mainVideoFile : L"",
                    current->subtitleCount);
            
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
    
    // Save to file
    SaveCacheToFile(manager);
    
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

// Delete all files associated with a cache entry
BOOL DeleteCacheEntryFiles(CacheManager* manager, const wchar_t* videoId) {
    if (!manager || !videoId) return FALSE;
    
    EnterCriticalSection(&manager->lock);
    
    CacheEntry* entry = FindCacheEntry(manager, videoId);
    if (!entry) {
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }
    
    BOOL success = TRUE;
    
    // Delete main video file
    if (entry->mainVideoFile) {
        if (!DeleteFileW(entry->mainVideoFile)) {
            success = FALSE;
        }
    }
    
    // Delete subtitle files
    for (int i = 0; i < entry->subtitleCount; i++) {
        if (entry->subtitleFiles && entry->subtitleFiles[i]) {
            if (!DeleteFileW(entry->subtitleFiles[i])) {
                success = FALSE;
            }
        }
    }
    
    LeaveCriticalSection(&manager->lock);
    
    // Remove from cache if files were deleted successfully
    if (success) {
        RemoveCacheEntry(manager, videoId);
    }
    
    return success;
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

// Refresh the cache list in the UI
void RefreshCacheList(HWND hListBox, CacheManager* manager) {
    if (!hListBox || !manager) return;
    
    // Clean up existing item data before clearing
    CleanupListboxItemData(hListBox);
    
    // Clear existing items
    SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
    
    // Ensure listbox is properly configured for scrolling
    EnsureListboxScrollable(hListBox);
    
    EnterCriticalSection(&manager->lock);
    
    CacheEntry* current = manager->entries;
    while (current) {
        if (ValidateCacheEntry(current)) {
            wchar_t* displayText = FormatCacheEntryDisplay(current);
            if (displayText) {
                int index = (int)SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)displayText);
                // Store video ID as item data
                if (current->videoId && index != LB_ERR) {
                    wchar_t* videoIdCopy = _wcsdup(current->videoId);
                    SendMessageW(hListBox, LB_SETITEMDATA, index, (LPARAM)videoIdCopy);
                }
                free(displayText);
            }
        }
        current = current->next;
    }
    
    LeaveCriticalSection(&manager->lock);
    
    // Set horizontal extent for proper horizontal scrolling
    SetListboxHorizontalExtent(hListBox, manager);
}

// Extract video ID from YouTube URL
wchar_t* ExtractVideoIdFromUrl(const wchar_t* url) {
    if (!url) return NULL;
    
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
                return videoId;
            }
        }
    }
    
    return NULL;
}

// Validate a cache entry
BOOL ValidateCacheEntry(const CacheEntry* entry) {
    if (!entry || !entry->videoId || !entry->mainVideoFile) {
        return FALSE;
    }
    
    // Check if main video file exists
    DWORD attributes = GetFileAttributesW(entry->mainVideoFile);
    return (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
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
    wchar_t baseName[MAX_PATH];
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
        wchar_t subtitlePath[MAX_PATH];
        swprintf(subtitlePath, MAX_PATH, L"%ls%ls", baseName, subtitleExts[i]);
        
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

// Scan download folder for existing videos (for initial cache population)
BOOL ScanDownloadFolderForVideos(CacheManager* manager, const wchar_t* downloadPath) {
    if (!manager || !downloadPath) return FALSE;
    
    wchar_t searchPattern[MAX_PATH];
    swprintf(searchPattern, MAX_PATH, L"%ls\\*.mp4", downloadPath);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPattern, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return TRUE; // No files found, but that's okay
    }
    
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            wchar_t fullPath[MAX_PATH];
            swprintf(fullPath, MAX_PATH, L"%ls\\%ls", downloadPath, findData.cFileName);
            
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

// Get the video ID of the currently selected item
wchar_t* GetSelectedVideoId(HWND hListBox) {
    if (!hListBox) return NULL;
    
    int selectedIndex = (int)SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
    if (selectedIndex == LB_ERR) return NULL;
    
    wchar_t* videoId = (wchar_t*)SendMessageW(hListBox, LB_GETITEMDATA, selectedIndex, 0);
    return videoId;
}

// Set horizontal extent for proper horizontal scrolling
BOOL SetListboxHorizontalExtent(HWND hListBox, CacheManager* manager) {
    if (!hListBox || !manager) return FALSE;
    
    HDC hdc = GetDC(hListBox);
    if (!hdc) return FALSE;
    
    HFONT hFont = (HFONT)SendMessageW(hListBox, WM_GETFONT, 0, 0);
    HFONT hOldFont = NULL;
    if (hFont) {
        hOldFont = (HFONT)SelectObject(hdc, hFont);
    }
    
    int maxWidth = 0;
    
    EnterCriticalSection(&manager->lock);
    
    CacheEntry* current = manager->entries;
    while (current) {
        if (ValidateCacheEntry(current)) {
            wchar_t* displayText = FormatCacheEntryDisplay(current);
            if (displayText) {
                SIZE textSize;
                if (GetTextExtentPoint32W(hdc, displayText, (int)wcslen(displayText), &textSize)) {
                    if (textSize.cx > maxWidth) {
                        maxWidth = textSize.cx;
                    }
                }
                free(displayText);
            }
        }
        current = current->next;
    }
    
    LeaveCriticalSection(&manager->lock);
    
    if (hOldFont) {
        SelectObject(hdc, hOldFont);
    }
    ReleaseDC(hListBox, hdc);
    
    // Add some padding and set the horizontal extent
    maxWidth += 20;
    SendMessageW(hListBox, LB_SETHORIZONTALEXTENT, maxWidth, 0);
    
    return TRUE;
}

// Ensure listbox is properly configured for scrolling
void EnsureListboxScrollable(HWND hListBox) {
    if (!hListBox) return;
    
    // Get current window style
    LONG style = GetWindowLongW(hListBox, GWL_STYLE);
    
    // Ensure scrollbars are enabled
    style |= WS_VSCROLL | WS_HSCROLL;
    
    // Ensure proper listbox styles for scrolling
    style |= LBS_NOINTEGRALHEIGHT; // Allow partial items to be visible
    
    SetWindowLongW(hListBox, GWL_STYLE, style);
    
    // Force window to update its appearance
    SetWindowPos(hListBox, NULL, 0, 0, 0, 0, 
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

// Clean up item data when clearing the listbox
void CleanupListboxItemData(HWND hListBox) {
    if (!hListBox) return;
    
    int count = (int)SendMessageW(hListBox, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; i++) {
        wchar_t* videoId = (wchar_t*)SendMessageW(hListBox, LB_GETITEMDATA, i, 0);
        if (videoId && videoId != (wchar_t*)LB_ERR) {
            free(videoId);
        }
    }
}