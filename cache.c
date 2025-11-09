#include "YouTubeCacher.h"

// Enhanced file operation error handling macro for cache operations
#define CHECK_FILE_OPERATION_WITH_CONTEXT(call, operation_name, file_path, cleanup_label) \
    do { \
        if (!(call)) { \
            DWORD _error = GetLastError(); \
            ErrorContext* _ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_ERROR); \
            if (_ctx) { \
                AddContextVariable(_ctx, L"FilePath", (file_path)); \
                AddContextVariable(_ctx, L"Operation", (operation_name)); \
                wchar_t _errorStr[32]; \
                swprintf(_errorStr, 32, L"%lu", _error); \
                AddContextVariable(_ctx, L"SystemError", _errorStr); \
                \
                if (_error == ERROR_ACCESS_DENIED) { \
                    SetUserFriendlyMessage(_ctx, L"Access denied. Please check file permissions or run as administrator."); \
                } else if (_error == ERROR_SHARING_VIOLATION) { \
                    SetUserFriendlyMessage(_ctx, L"File is currently in use by another program. Please close any programs using the file and try again."); \
                } else if (_error == ERROR_FILE_NOT_FOUND || _error == ERROR_PATH_NOT_FOUND) { \
                    SetUserFriendlyMessage(_ctx, L"File or path not found. The file may have been moved or deleted."); \
                } else if (_error == ERROR_DISK_FULL) { \
                    SetUserFriendlyMessage(_ctx, L"Disk is full. Please free up disk space and try again."); \
                } else { \
                    SetUserFriendlyMessage(_ctx, L"File operation failed due to a system error. The storage device may have issues."); \
                } \
                FreeErrorContext(_ctx); \
            } \
            goto cleanup_label; \
        } \
    } while(0)

// Utility function to safely check if a file exists using new error handling
BOOL SafeFileExists(const wchar_t* filePath) {
    VALIDATE_POINTER_PARAM(filePath, L"filePath", cleanup);
    
    DWORD attributes = GetFileAttributesW(filePath);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        
        // Only report non-file-not-found errors as these are unexpected
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_INFO);
            if (ctx) {
                AddContextVariable(ctx, L"FilePath", filePath);
                AddContextVariable(ctx, L"Operation", L"Check file existence");
                wchar_t errorCodeStr[32];
                swprintf(errorCodeStr, 32, L"%lu", error);
                AddContextVariable(ctx, L"SystemError", errorCodeStr);
                
                if (error == ERROR_ACCESS_DENIED) {
                    SetUserFriendlyMessage(ctx, L"Cannot check if file exists due to insufficient permissions.");
                } else {
                    SetUserFriendlyMessage(ctx, L"Cannot check if file exists due to a system error.");
                }
                FreeErrorContext(ctx);
            }
        }
        return FALSE;
    }
    
    // Return TRUE only if it's a file (not a directory)
    return !(attributes & FILE_ATTRIBUTE_DIRECTORY);

cleanup:
    return FALSE;
}

// Initialize the cache manager
BOOL InitializeCacheManager(CacheManager* manager, const wchar_t* downloadPath) {
    ThreadSafeDebugOutput(L"YouTubeCacher: InitializeCacheManager - ENTRY");
    
    if (!manager || !downloadPath) {
        ThreadSafeDebugOutput(L"YouTubeCacher: InitializeCacheManager - NULL parameters");
        return FALSE;
    }
    
    ThreadSafeDebugOutputF(L"YouTubeCacher: InitializeCacheManager - downloadPath: %ls", downloadPath);
    
    memset(manager, 0, sizeof(CacheManager));
    
    // Initialize critical section for thread safety
    InitializeCriticalSection(&manager->lock);
    ThreadSafeDebugOutput(L"YouTubeCacher: InitializeCacheManager - Critical section initialized");
    
    // Build cache file path
    swprintf(manager->cacheFilePath, MAX_EXTENDED_PATH, L"%ls\\%ls", downloadPath, CACHE_FILE_NAME);
    ThreadSafeDebugOutputF(L"YouTubeCacher: InitializeCacheManager - cacheFilePath: %ls", manager->cacheFilePath);
    
    // Load existing cache from file
    ThreadSafeDebugOutput(L"YouTubeCacher: InitializeCacheManager - Loading cache from file");
    LoadCacheFromFile(manager);
    
    ThreadSafeDebugOutputF(L"YouTubeCacher: InitializeCacheManager - SUCCESS, loaded %d entries", manager->totalEntries);
    
    return TRUE;
}

// Clean up the cache manager
void CleanupCacheManager(CacheManager* manager) {
    if (!manager) return;
    
    // Save cache to disk before cleanup
    DebugOutput(L"YouTubeCacher: CleanupCacheManager - Saving cache before cleanup");
    SaveCacheToFile(manager);
    
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
    
    DebugOutput(L"YouTubeCacher: CleanupCacheManager - Cleanup complete");
}

// Free a single cache entry
void FreeCacheEntry(CacheEntry* entry) {
    if (!entry) return;
    
    if (entry->videoId) SAFE_FREE(entry->videoId);
    if (entry->title) SAFE_FREE(entry->title);
    if (entry->duration) SAFE_FREE(entry->duration);
    if (entry->mainVideoFile) SAFE_FREE(entry->mainVideoFile);
    
    if (entry->subtitleFiles) {
        for (int i = 0; i < entry->subtitleCount; i++) {
            if (entry->subtitleFiles[i]) {
                SAFE_FREE(entry->subtitleFiles[i]);
            }
        }
        SAFE_FREE(entry->subtitleFiles);
    }
    
    SAFE_FREE(entry);
}

// Optimized cache loading: Load entire file into memory and process in-place
BOOL LoadCacheFromFile(CacheManager* manager) {
    if (!manager) return FALSE;
    
    wchar_t debugMsg[1024];
    DebugOutput(L"YouTubeCacher: LoadCacheFromFile - ENTRY (optimized version)");
    
    // Step 1: Use Windows API to get file size (equivalent to stat())
    HANDLE hFile = CreateFileW(manager->cacheFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            DebugOutput(L"YouTubeCacher: LoadCacheFromFile - Cache file does not exist, starting with empty cache");
            return TRUE; // Not an error - just no cache file yet
        }
        swprintf(debugMsg, 1024, L"YouTubeCacher: LoadCacheFromFile - ERROR: Cannot open file (error %lu)", error);
        DebugOutput(debugMsg);
        return FALSE;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        DebugOutput(L"YouTubeCacher: LoadCacheFromFile - ERROR: Cannot get file size");
        CloseHandle(hFile);
        return FALSE;
    }
    
    if (fileSize.QuadPart == 0) {
        DebugOutput(L"YouTubeCacher: LoadCacheFromFile - File is empty, starting with empty cache");
        CloseHandle(hFile);
        return TRUE;
    }
    
    if (fileSize.QuadPart > 50 * 1024 * 1024) { // 50MB limit for safety
        DebugOutput(L"YouTubeCacher: LoadCacheFromFile - ERROR: File too large for in-memory processing");
        CloseHandle(hFile);
        return FALSE;
    }
    
    swprintf(debugMsg, 1024, L"YouTubeCacher: LoadCacheFromFile - File size: %lld bytes", fileSize.QuadPart);
    DebugOutput(debugMsg);
    
    // Step 2: Allocate buffer to load entire file (use ANSI for simplicity, convert to wide later)
    DWORD fileSizeBytes = (DWORD)fileSize.QuadPart;
    char* fileBuffer = (char*)SAFE_MALLOC(fileSizeBytes + 1); // +1 for null terminator
    if (!fileBuffer) {
        DebugOutput(L"YouTubeCacher: LoadCacheFromFile - ERROR: Cannot allocate file buffer");
        CloseHandle(hFile);
        return FALSE;
    }
    
    // Read entire file in one operation (or 4K chunks if needed)
    DWORD totalBytesRead = 0;
    while (totalBytesRead < fileSizeBytes) {
        DWORD chunkSize = min(4096, fileSizeBytes - totalBytesRead);
        DWORD bytesRead;
        if (!ReadFile(hFile, fileBuffer + totalBytesRead, chunkSize, &bytesRead, NULL) || bytesRead == 0) {
            DebugOutput(L"YouTubeCacher: LoadCacheFromFile - ERROR: Failed to read file");
            SAFE_FREE(fileBuffer);
            CloseHandle(hFile);
            return FALSE;
        }
        totalBytesRead += bytesRead;
    }
    
    CloseHandle(hFile);
    fileBuffer[fileSizeBytes] = '\0'; // Null terminate
    
    DebugOutput(L"YouTubeCacher: LoadCacheFromFile - File loaded into memory successfully");
    
    // Step 3: Scan for newlines and count them
    int newlineCount = 0;
    for (DWORD i = 0; i < fileSizeBytes; i++) {
        if (fileBuffer[i] == '\n') {
            newlineCount++;
        }
    }
    
    swprintf(debugMsg, 1024, L"YouTubeCacher: LoadCacheFromFile - Found %d lines", newlineCount);
    DebugOutput(debugMsg);
    
    if (newlineCount == 0) {
        DebugOutput(L"YouTubeCacher: LoadCacheFromFile - No lines found in file");
        SAFE_FREE(fileBuffer);
        return TRUE;
    }
    
    // Step 4: Allocate pointer array (2 more than newline count for safety)
    char** lines = (char**)SAFE_MALLOC((newlineCount + 2) * sizeof(char*));
    if (!lines) {
        DebugOutput(L"YouTubeCacher: LoadCacheFromFile - ERROR: Cannot allocate line pointer array");
        SAFE_FREE(fileBuffer);
        return FALSE;
    }
    
    // Step 5: Replace newlines with null bytes and build pointer array
    int lineIndex = 0;
    lines[lineIndex++] = fileBuffer; // First line starts at beginning
    
    for (DWORD i = 0; i < fileSizeBytes; i++) {
        if (fileBuffer[i] == '\r') {
            fileBuffer[i] = '\0'; // Null out \r
            if (i + 1 < fileSizeBytes && fileBuffer[i + 1] == '\n') {
                fileBuffer[i + 1] = '\0'; // Null out \n in \r\n sequence
                i++; // Skip the \n
                if (i + 1 < fileSizeBytes) {
                    lines[lineIndex++] = &fileBuffer[i + 1]; // Next line starts after \r\n
                }
            } else if (i + 1 < fileSizeBytes) {
                lines[lineIndex++] = &fileBuffer[i + 1]; // Next line starts after \r
            }
        } else if (fileBuffer[i] == '\n') {
            fileBuffer[i] = '\0'; // Null out \n
            if (i + 1 < fileSizeBytes) {
                lines[lineIndex++] = &fileBuffer[i + 1]; // Next line starts after \n
            }
        }
    }
    
    int totalLines = lineIndex;
    swprintf(debugMsg, 1024, L"YouTubeCacher: LoadCacheFromFile - Processed into %d lines", totalLines);
    DebugOutput(debugMsg);
    
    // Step 6: Process cache entries in-place
    EnterCriticalSection(&manager->lock);
    
    int validEntries = 0;
    int invalidEntries = 0;
    BOOL versionValidated = FALSE;
    
    for (int i = 0; i < totalLines; i++) {
        char* line = lines[i];
        if (!line || strlen(line) == 0) continue; // Skip empty lines
        if (line[0] == '#') continue; // Skip comments
        
        // Convert line to wide string for processing
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, line, -1, NULL, 0);
        if (wideLen <= 0) continue;
        
        wchar_t* wideLine = (wchar_t*)SAFE_MALLOC(wideLen * sizeof(wchar_t));
        if (!wideLine) continue;
        
        if (MultiByteToWideChar(CP_UTF8, 0, line, -1, wideLine, wideLen) <= 0) {
            SAFE_FREE(wideLine);
            continue;
        }
        
        // Handle version header
        if (!versionValidated && wcsncmp(wideLine, L"CACHE_VERSION=", 14) == 0) {
            const wchar_t* version = wideLine + 14;
            if (wcscmp(version, CACHE_VERSION) != 0) {
                swprintf(debugMsg, 1024, L"YouTubeCacher: LoadCacheFromFile - WARNING: Version mismatch. File: '%ls', Expected: '%ls'", version, CACHE_VERSION);
                DebugOutput(debugMsg);
            }
            versionValidated = TRUE;
            SAFE_FREE(wideLine);
            continue;
        }
        
        // Parse cache entry: VIDEO_ID|TITLE|DURATION|MAIN_FILE|SUBTITLE_COUNT|...
        wchar_t* context = NULL;
        wchar_t* videoId = wcstok(wideLine, L"|", &context);
        if (!videoId || wcslen(videoId) == 0) {
            SAFE_FREE(wideLine);
            invalidEntries++;
            continue;
        }
        
        // Quick validation and parsing
        CacheEntry* entry = (CacheEntry*)SAFE_MALLOC(sizeof(CacheEntry));
        if (!entry) {
            SAFE_FREE(wideLine);
            continue;
        }
        
        memset(entry, 0, sizeof(CacheEntry));
        entry->videoId = SAFE_WCSDUP(videoId);
        
        // Parse title (base64 encoded)
        wchar_t* titleToken = wcstok(NULL, L"|", &context);
        if (titleToken && wcslen(titleToken) > 0) {
            entry->title = Base64DecodeWide(titleToken);
            if (!entry->title) {
                entry->title = SAFE_WCSDUP(L"Unknown Title");
            }
        } else {
            entry->title = SAFE_WCSDUP(L"Unknown Title");
        }
        
        // Parse duration
        wchar_t* durationToken = wcstok(NULL, L"|", &context);
        if (durationToken && wcslen(durationToken) > 0) {
            entry->duration = SAFE_WCSDUP(durationToken);
        } else {
            entry->duration = SAFE_WCSDUP(L"Unknown");
        }
        
        // Parse main video file
        wchar_t* fileToken = wcstok(NULL, L"|", &context);
        if (fileToken && wcslen(fileToken) > 0) {
            entry->mainVideoFile = SAFE_WCSDUP(fileToken);
        } else {
            // Missing main file - invalid entry
            FreeCacheEntry(entry);
            SAFE_FREE(wideLine);
            invalidEntries++;
            continue;
        }
        
        // Parse subtitle count and files (simplified for performance)
        wchar_t* subtitleCountToken = wcstok(NULL, L"|", &context);
        if (subtitleCountToken) {
            entry->subtitleCount = _wtoi(subtitleCountToken);
            if (entry->subtitleCount > 0 && entry->subtitleCount <= 100) { // Reasonable limit
                entry->subtitleFiles = (wchar_t**)SAFE_MALLOC(entry->subtitleCount * sizeof(wchar_t*));
                if (entry->subtitleFiles) {
                    memset(entry->subtitleFiles, 0, entry->subtitleCount * sizeof(wchar_t*));
                    for (int j = 0; j < entry->subtitleCount; j++) {
                        wchar_t* subtitleToken = wcstok(NULL, L"|", &context);
                        if (subtitleToken && wcslen(subtitleToken) > 0) {
                            entry->subtitleFiles[j] = SAFE_WCSDUP(subtitleToken);
                        }
                    }
                }
            }
        }
        
        // Add entry to cache
        entry->next = manager->entries;
        manager->entries = entry;
        manager->totalEntries++;
        validEntries++;
        
        SAFE_FREE(wideLine);
    }
    
    LeaveCriticalSection(&manager->lock);
    
    // Cleanup
    SAFE_FREE(lines);
    SAFE_FREE(fileBuffer);
    
    swprintf(debugMsg, 1024, L"YouTubeCacher: LoadCacheFromFile - COMPLETE: Loaded %d valid entries, %d invalid entries", validEntries, invalidEntries);
    DebugOutput(debugMsg);
    
    return TRUE;
}

// Save cache to file
BOOL SaveCacheToFile(CacheManager* manager) {
    DebugOutput(L"YouTubeCacher: SaveCacheToFile - ENTRY");
    
    if (!manager) {
        DebugOutput(L"YouTubeCacher: SaveCacheToFile - ERROR: NULL manager");
        return FALSE;
    }
    
    wchar_t debugMsg[1024];
    swprintf(debugMsg, 1024, L"YouTubeCacher: SaveCacheToFile - Attempting to save to: %ls", manager->cacheFilePath);
    DebugOutput(debugMsg);
    
    FILE* file = _wfopen(manager->cacheFilePath, L"w,ccs=UTF-8");
    if (!file) {
        DWORD error = GetLastError();
        
        // Use enhanced error handling with better context information
        ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_ERROR);
        if (ctx) {
            AddContextVariable(ctx, L"FilePath", manager->cacheFilePath);
            AddContextVariable(ctx, L"Operation", L"Open cache file for writing");
            wchar_t errorCodeStr[32];
            swprintf(errorCodeStr, 32, L"%lu", error);
            AddContextVariable(ctx, L"SystemError", errorCodeStr);
            
            if (error == ERROR_ACCESS_DENIED) {
                SetUserFriendlyMessage(ctx, L"Cannot save cache file due to insufficient permissions.\r\nPlease check folder permissions or run as administrator.");
            } else if (error == ERROR_DISK_FULL) {
                SetUserFriendlyMessage(ctx, L"Cannot save cache file because the disk is full.\r\nPlease free up disk space and try again.");
            } else if (error == ERROR_PATH_NOT_FOUND) {
                SetUserFriendlyMessage(ctx, L"Cannot save cache file because the folder path does not exist.\r\nPlease check that the download folder is accessible.");
            } else {
                SetUserFriendlyMessage(ctx, L"Unable to save cache file due to a system error.\r\nThe storage device may have issues or be write-protected.");
            }
            FreeErrorContext(ctx);
        }
        swprintf(debugMsg, 1024, L"YouTubeCacher: SaveCacheToFile - ERROR: Failed to open file for writing (error %lu): %ls\r\n", 
                error, manager->cacheFilePath);
        DebugOutput(debugMsg);
        return FALSE;
    }
    
    DebugOutput(L"YouTubeCacher: SaveCacheToFile - File opened for writing");
    
    // Write version header
    fwprintf(file, L"CACHE_VERSION=%ls\n", CACHE_VERSION);
    
    EnterCriticalSection(&manager->lock);
    
    swprintf(debugMsg, 1024, L"YouTubeCacher: SaveCacheToFile - Writing %d entries", manager->totalEntries);
    DebugOutput(debugMsg);
    
    // Write each cache entry
    CacheEntry* current = manager->entries;
    int entryCount = 0;
    while (current) {
        entryCount++;
        if (ValidateCacheEntry(current)) {
            swprintf(debugMsg, 1024, L"YouTubeCacher: SaveCacheToFile - Writing entry %d: %ls", 
                    entryCount, current->videoId ? current->videoId : L"NULL");
            DebugOutput(debugMsg);
            // Encode title as base64 to handle all Unicode characters safely
            wchar_t* encodedTitle = current->title ? Base64EncodeWide(current->title) : NULL;
            
            fwprintf(file, L"%ls|%ls|%ls|%ls|%d",
                    current->videoId ? current->videoId : L"",
                    encodedTitle ? encodedTitle : L"",
                    current->duration ? current->duration : L"",
                    current->mainVideoFile ? current->mainVideoFile : L"",
                    current->subtitleCount);
            
            if (encodedTitle) SAFE_FREE(encodedTitle);
            
            // Write subtitle files
            for (int i = 0; i < current->subtitleCount; i++) {
                if (current->subtitleFiles && current->subtitleFiles[i]) {
                    fwprintf(file, L"|%ls", current->subtitleFiles[i]);
                } else {
                    fwprintf(file, L"|");
                }
            }
            
            fwprintf(file, L"\n");
        } else {
            swprintf(debugMsg, 1024, L"YouTubeCacher: SaveCacheToFile - Skipping invalid entry %d: %ls", 
                    entryCount, current->videoId ? current->videoId : L"NULL");
            DebugOutput(debugMsg);
        }
        current = current->next;
    }
    
    LeaveCriticalSection(&manager->lock);
    
    // Explicitly flush to ensure data is written to disk
    fflush(file);
    fclose(file);
    
    swprintf(debugMsg, 1024, L"YouTubeCacher: SaveCacheToFile - COMPLETE: Wrote %d entries to file", entryCount);
    DebugOutput(debugMsg);
    
    return TRUE;
}

// Add a new cache entry
BOOL AddCacheEntry(CacheManager* manager, const wchar_t* videoId, const wchar_t* title, 
                   const wchar_t* duration, const wchar_t* mainVideoFile, 
                   wchar_t** subtitleFiles, int subtitleCount) {
    if (!manager || !videoId || !mainVideoFile) return FALSE;
    
    // Debug logging
    DebugOutput(L"YouTubeCacher: AddCacheEntry - Starting");
    if (title) {
        wchar_t debugMsg[1024];
        swprintf(debugMsg, 1024, L"YouTubeCacher: AddCacheEntry - Title: %ls (length: %zu)", title, wcslen(title));
        DebugOutput(debugMsg);
    }
    
    EnterCriticalSection(&manager->lock);
    
    // Check if entry already exists
    CacheEntry* existing = FindCacheEntry(manager, videoId);
    if (existing) {
        LeaveCriticalSection(&manager->lock);
        return FALSE; // Already exists
    }
    
    // Create new entry
    CacheEntry* entry = (CacheEntry*)SAFE_MALLOC(sizeof(CacheEntry));
    if (!entry) {
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }
    
    memset(entry, 0, sizeof(CacheEntry));
    
    entry->videoId = SAFE_WCSDUP(videoId);
    entry->title = title ? SAFE_WCSDUP(title) : NULL;
    entry->duration = duration ? SAFE_WCSDUP(duration) : NULL;
    entry->mainVideoFile = SAFE_WCSDUP(mainVideoFile);
    entry->subtitleCount = subtitleCount;
    
    // Copy subtitle files
    if (subtitleCount > 0 && subtitleFiles) {
        entry->subtitleFiles = (wchar_t**)SAFE_MALLOC(subtitleCount * sizeof(wchar_t*));
        if (entry->subtitleFiles) {
            for (int i = 0; i < subtitleCount; i++) {
                entry->subtitleFiles[i] = subtitleFiles[i] ? SAFE_WCSDUP(subtitleFiles[i]) : NULL;
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
    
    DebugOutput(L"YouTubeCacher: AddCacheEntry - Entry added to memory, saving to file");
    
    // Save to file
    BOOL saveResult = SaveCacheToFile(manager);
    
    if (saveResult) {
        DebugOutput(L"YouTubeCacher: AddCacheEntry - Successfully saved to file");
    } else {
        DebugOutput(L"YouTubeCacher: AddCacheEntry - ERROR: Failed to save to file");
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
    DeleteResult* result = (DeleteResult*)SAFE_MALLOC(sizeof(DeleteResult));
    if (!result) {
        LeaveCriticalSection(&manager->lock);
        return NULL;
    }
    
    memset(result, 0, sizeof(DeleteResult));
    
    // Calculate total files to delete
    result->totalFiles = (entry->mainVideoFile ? 1 : 0) + entry->subtitleCount;
    
    // Allocate error array (worst case: all files fail)
    if (result->totalFiles > 0) {
        result->errors = (FileDeleteError*)SAFE_MALLOC(result->totalFiles * sizeof(FileDeleteError));
        if (!result->errors) {
            SAFE_FREE(result);
            LeaveCriticalSection(&manager->lock);
            return NULL;
        }
        memset(result->errors, 0, result->totalFiles * sizeof(FileDeleteError));
    }
    
    // Delete main video file with enhanced error handling
    if (entry->mainVideoFile) {
        if (!DeleteFileW(entry->mainVideoFile)) {
            DWORD error = GetLastError();
            result->errors[result->errorCount].fileName = SAFE_WCSDUP(entry->mainVideoFile);
            result->errors[result->errorCount].errorCode = error;
            result->errorCount++;
            
            // Use enhanced error context with more detailed information
            ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_WARNING);
            if (ctx) {
                AddContextVariable(ctx, L"FilePath", entry->mainVideoFile);
                AddContextVariable(ctx, L"Operation", L"Delete main video file");
                AddContextVariable(ctx, L"FileType", L"Video");
                wchar_t errorCodeStr[32];
                swprintf(errorCodeStr, 32, L"%lu", error);
                AddContextVariable(ctx, L"SystemError", errorCodeStr);
                
                if (error == ERROR_ACCESS_DENIED) {
                    SetUserFriendlyMessage(ctx, L"Cannot delete video file due to insufficient permissions.\r\nThe file may be read-only or you may need administrator privileges.");
                } else if (error == ERROR_SHARING_VIOLATION) {
                    SetUserFriendlyMessage(ctx, L"Cannot delete video file because it is currently in use.\r\nPlease close any media players or programs using the file and try again.");
                } else if (error == ERROR_FILE_NOT_FOUND) {
                    SetUserFriendlyMessage(ctx, L"Video file was already deleted or moved.\r\nThe cache entry will be updated to reflect this change.");
                } else {
                    SetUserFriendlyMessage(ctx, L"Unable to delete video file due to a system error.\r\nThe file may be corrupted or the storage device may have issues.");
                }
                FreeErrorContext(ctx);
            }
            
            // Log deletion failure with enhanced context
            wchar_t logMsg[1024];
            swprintf(logMsg, 1024, L"Failed to delete video file: %ls (Error: %lu)\r\n", 
                    entry->mainVideoFile, error);
            WriteToLogfile(logMsg);
        } else {
            result->successfulDeletes++;
            
            // Log successful deletion
            wchar_t logMsg[1024];
            swprintf(logMsg, 1024, L"Deleted video file: %ls\r\n", entry->mainVideoFile);
            WriteToLogfile(logMsg);
        }
    }
    
    // Delete subtitle files with enhanced error handling
    for (int i = 0; i < entry->subtitleCount; i++) {
        if (entry->subtitleFiles && entry->subtitleFiles[i]) {
            if (!DeleteFileW(entry->subtitleFiles[i])) {
                DWORD error = GetLastError();
                result->errors[result->errorCount].fileName = SAFE_WCSDUP(entry->subtitleFiles[i]);
                result->errors[result->errorCount].errorCode = error;
                result->errorCount++;
                
                // Use enhanced error context with subtitle-specific information
                ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_WARNING);
                if (ctx) {
                    AddContextVariable(ctx, L"FilePath", entry->subtitleFiles[i]);
                    AddContextVariable(ctx, L"Operation", L"Delete subtitle file");
                    AddContextVariable(ctx, L"FileType", L"Subtitle");
                    wchar_t subtitleIndex[16];
                    swprintf(subtitleIndex, 16, L"%d", i + 1);
                    AddContextVariable(ctx, L"SubtitleIndex", subtitleIndex);
                    wchar_t errorCodeStr[32];
                    swprintf(errorCodeStr, 32, L"%lu", error);
                    AddContextVariable(ctx, L"SystemError", errorCodeStr);
                    
                    if (error == ERROR_ACCESS_DENIED) {
                        SetUserFriendlyMessage(ctx, L"Cannot delete subtitle file due to insufficient permissions.\r\nThe file may be read-only or you may need administrator privileges.");
                    } else if (error == ERROR_SHARING_VIOLATION) {
                        SetUserFriendlyMessage(ctx, L"Cannot delete subtitle file because it is currently in use.\r\nPlease close any text editors or programs using the file and try again.");
                    } else if (error == ERROR_FILE_NOT_FOUND) {
                        SetUserFriendlyMessage(ctx, L"Subtitle file was already deleted or moved.\r\nThe cache entry will be updated to reflect this change.");
                    } else {
                        SetUserFriendlyMessage(ctx, L"Unable to delete subtitle file due to a system error.\r\nThe file may be corrupted or the storage device may have issues.");
                    }
                    FreeErrorContext(ctx);
                }
                
                // Log deletion failure with enhanced context
                wchar_t logMsg[1024];
                swprintf(logMsg, 1024, L"Failed to delete subtitle file: %ls (Error: %lu)\r\n", 
                        entry->subtitleFiles[i], error);
                WriteToLogfile(logMsg);
            } else {
                result->successfulDeletes++;
                
                // Log successful deletion
                wchar_t logMsg[1024];
                swprintf(logMsg, 1024, L"Deleted subtitle file: %ls\r\n", entry->subtitleFiles[i]);
                WriteToLogfile(logMsg);
            }
        }
    }
    
    LeaveCriticalSection(&manager->lock);
    
    // Remove from cache if all files were deleted successfully
    if (result->errorCount == 0) {
        // Log cache entry removal
        wchar_t logMsg[512];
        if (entry->title) {
            swprintf(logMsg, 512, L"Removed cache entry for video: %ls (ID: %ls)", 
                    entry->title, videoId);
        } else {
            swprintf(logMsg, 512, L"Removed cache entry for video ID: %ls", videoId);
        }
        WriteToLogfile(logMsg);
        
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
                SAFE_FREE(result->errors[i].fileName);
            }
        }
        SAFE_FREE(result->errors);
    }
    
    SAFE_FREE(result);
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
    
    wchar_t* details = (wchar_t*)SAFE_MALLOC(bufferSize * sizeof(wchar_t));
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
    
    // Check if video file still exists with enhanced error handling
    DWORD attributes = GetFileAttributesW(entry->mainVideoFile);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        
        // Report file access failure for playback
        ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_NOT_FOUND, YTC_SEVERITY_ERROR);
        if (ctx) {
            AddContextVariable(ctx, L"FilePath", entry->mainVideoFile);
            AddContextVariable(ctx, L"Operation", L"Check file for playback");
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
                SetUserFriendlyMessage(ctx, L"The video file no longer exists at the expected location.\r\nIt may have been moved, deleted, or the storage device may be disconnected.");
            } else if (error == ERROR_ACCESS_DENIED) {
                SetUserFriendlyMessage(ctx, L"Cannot access the video file due to insufficient permissions.\r\nPlease check file permissions or run as administrator.");
            } else {
                SetUserFriendlyMessage(ctx, L"Unable to access the video file due to a system error.\r\nThe storage device may have issues or be disconnected.");
            }
            FreeErrorContext(ctx);
        }
        
        LeaveCriticalSection(&manager->lock);
        return FALSE;
    }
    
    // Build command line to launch player
    size_t cmdLineLen = wcslen(playerPath) + wcslen(entry->mainVideoFile) + 10;
    wchar_t* cmdLine = (wchar_t*)SAFE_MALLOC(cmdLineLen * sizeof(wchar_t));
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
    
    SAFE_FREE(cmdLine);
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
            item.lParam = (LPARAM)SAFE_WCSDUP(current->videoId); // Store video ID
            
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
            wchar_t* videoId = (wchar_t*)SAFE_MALLOC(12 * sizeof(wchar_t));
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
            wchar_t* videoId = (wchar_t*)SAFE_MALLOC(12 * sizeof(wchar_t));
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
            wchar_t* videoId = (wchar_t*)SAFE_MALLOC(12 * sizeof(wchar_t));
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

// Validate a cache entry using enhanced error handling
BOOL ValidateCacheEntry(const CacheEntry* entry) {
    if (!entry || !entry->videoId || !entry->mainVideoFile) {
        OutputDebugStringW(L"YouTubeCacher: ValidateCacheEntry - NULL entry or missing required fields\n");
        return FALSE;
    }
    
    // Use the new SafeFileExists function for better error handling
    BOOL fileExists = SafeFileExists(entry->mainVideoFile);
    
    if (!fileExists) {
        // Create minimal error context for validation failures (don't show dialog as this is called frequently)
        ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_NOT_FOUND, YTC_SEVERITY_INFO);
        if (ctx) {
            AddContextVariable(ctx, L"FilePath", entry->mainVideoFile);
            AddContextVariable(ctx, L"Operation", L"Validate cache entry");
            AddContextVariable(ctx, L"VideoId", entry->videoId ? entry->videoId : L"Unknown");
            if (entry->title) {
                AddContextVariable(ctx, L"VideoTitle", entry->title);
            }
            SetUserFriendlyMessage(ctx, L"Cache entry references a file that no longer exists.\r\nThe file may have been moved or deleted outside of the application.");
            
            // Don't show dialog for validation - just log the context for debugging
            FreeErrorContext(ctx);
        }
        
        wchar_t debugMsg[1024];
        swprintf(debugMsg, 1024, L"YouTubeCacher: ValidateCacheEntry - File validation failed: %ls\r\n", 
                entry->mainVideoFile);
        OutputDebugStringW(debugMsg);
    }
    
    return fileExists;
}

// Get video file information with enhanced error handling
BOOL GetVideoFileInfo(const wchar_t* filePath, DWORD* fileSize, FILETIME* modTime) {
    VALIDATE_POINTER_PARAM(filePath, L"filePath", cleanup);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(filePath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        
        // Use enhanced error context with more detailed information
        ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_WARNING);
        if (ctx) {
            AddContextVariable(ctx, L"FilePath", filePath);
            AddContextVariable(ctx, L"Operation", L"Get file information");
            AddContextVariable(ctx, L"Function", L"FindFirstFileW");
            wchar_t errorCodeStr[32];
            swprintf(errorCodeStr, 32, L"%lu", error);
            AddContextVariable(ctx, L"SystemError", errorCodeStr);
            
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
                SetUserFriendlyMessage(ctx, L"Cannot get file information because the file does not exist.\r\nThe file may have been moved or deleted.");
            } else if (error == ERROR_ACCESS_DENIED) {
                SetUserFriendlyMessage(ctx, L"Cannot get file information due to insufficient permissions.\r\nPlease check file permissions.");
            } else {
                SetUserFriendlyMessage(ctx, L"Unable to get file information due to a system error.\r\nThe storage device may have issues.");
            }
            FreeErrorContext(ctx);
        }
        
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

cleanup:
    return FALSE;
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
    
    wchar_t** tempFiles = (wchar_t**)SAFE_MALLOC(numExts * sizeof(wchar_t*));
    if (!tempFiles) return FALSE;
    
    int foundCount = 0;
    
    for (int i = 0; i < numExts; i++) {
        wchar_t subtitlePath[MAX_EXTENDED_PATH];
        swprintf(subtitlePath, MAX_EXTENDED_PATH, L"%ls%ls", baseName, subtitleExts[i]);
        
        DWORD attributes = GetFileAttributesW(subtitlePath);
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            tempFiles[foundCount] = SAFE_WCSDUP(subtitlePath);
            foundCount++;
        } else if (attributes == INVALID_FILE_ATTRIBUTES) {
            DWORD error = GetLastError();
            // Only report non-file-not-found errors for subtitle search
            if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
                ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_INFO);
                if (ctx) {
                    AddContextVariable(ctx, L"FilePath", subtitlePath);
                    AddContextVariable(ctx, L"Operation", L"Search for subtitle files");
                    SetUserFriendlyMessage(ctx, L"Error occurred while searching for subtitle files.\r\nSome subtitle files may not be detected.");
                    FreeErrorContext(ctx);
                }
            }
        }
    }
    
    if (foundCount > 0) {
        *subtitleFiles = (wchar_t**)SAFE_REALLOC(tempFiles, foundCount * sizeof(wchar_t*));
        *count = foundCount;
        return TRUE;
    } else {
        SAFE_FREE(tempFiles);
        return FALSE;
    }
}

// Format file size for display
wchar_t* FormatFileSize(DWORD sizeBytes) {
    wchar_t* result = (wchar_t*)SAFE_MALLOC(32 * sizeof(wchar_t));
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
    
    wchar_t* result = (wchar_t*)SAFE_MALLOC(512 * sizeof(wchar_t));
    if (!result) return NULL;
    
    wchar_t* sizeStr = FormatFileSize(entry->fileSize);
    
    swprintf(result, 512, L"%ls - %ls (%ls)",
            entry->title ? entry->title : L"Unknown Title",
            entry->duration ? entry->duration : L"Unknown Duration",
            sizeStr ? sizeStr : L"Unknown Size");
    
    if (sizeStr) SAFE_FREE(sizeStr);
    
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
    } else {
        // Use enhanced file operation error handling
        DWORD error = GetLastError();
        ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_WARNING);
        if (ctx) {
            AddContextVariable(ctx, L"FilePath", fullPath);
            AddContextVariable(ctx, L"Operation", L"Create dummy video file");
            wchar_t errorCodeStr[32];
            swprintf(errorCodeStr, 32, L"%lu", error);
            AddContextVariable(ctx, L"SystemError", errorCodeStr);
            
            if (error == ERROR_ACCESS_DENIED) {
                SetUserFriendlyMessage(ctx, L"Cannot create dummy video file due to insufficient permissions.\r\nPlease check folder permissions or run as administrator.");
            } else if (error == ERROR_DISK_FULL) {
                SetUserFriendlyMessage(ctx, L"Cannot create dummy video file because the disk is full.\r\nPlease free up disk space and try again.");
            } else {
                SetUserFriendlyMessage(ctx, L"Unable to create dummy video file due to a system error.\r\nThe storage device may have issues or be write-protected.");
            }
            FreeErrorContext(ctx);
        }
    }
    
    // Create dummy subtitle files
    wchar_t** subtitleFiles = (wchar_t**)SAFE_MALLOC(2 * sizeof(wchar_t*));
    if (subtitleFiles) {
        // Create .srt file
        wchar_t srtPath[MAX_EXTENDED_PATH];
        swprintf(srtPath, MAX_EXTENDED_PATH, L"%ls\\debug_video_%d [%ls].en.srt", downloadPath, dummyCounter, videoId);
        subtitleFiles[0] = SAFE_WCSDUP(srtPath);
        
        HANDLE hSrt = CreateFileW(srtPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hSrt != INVALID_HANDLE_VALUE) {
            const char srtData[] = "1\n00:00:00,000 --> 00:00:05,000\nThis is a dummy subtitle file.\n\n";
            DWORD bytesWritten;
            WriteFile(hSrt, srtData, sizeof(srtData), &bytesWritten, NULL);
            CloseHandle(hSrt);
        } else {
            DWORD error = GetLastError();
            ErrorContext* ctx = CREATE_ERROR_CONTEXT(YTC_ERROR_FILE_ACCESS, YTC_SEVERITY_INFO);
            if (ctx) {
                AddContextVariable(ctx, L"FilePath", srtPath);
                AddContextVariable(ctx, L"Operation", L"Create dummy SRT subtitle file");
                wchar_t errorCodeStr[32];
                swprintf(errorCodeStr, 32, L"%lu", error);
                AddContextVariable(ctx, L"SystemError", errorCodeStr);
                SetUserFriendlyMessage(ctx, L"Could not create dummy subtitle file.\r\nThis may affect testing but does not impact normal operation.");
                FreeErrorContext(ctx);
            }
        }
        
        // Create .vtt file
        wchar_t vttPath[MAX_EXTENDED_PATH];
        swprintf(vttPath, MAX_EXTENDED_PATH, L"%ls\\debug_video_%d [%ls].en.vtt", downloadPath, dummyCounter, videoId);
        subtitleFiles[1] = SAFE_WCSDUP(vttPath);
        
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
        if (subtitleFiles[0]) SAFE_FREE(subtitleFiles[0]);
        if (subtitleFiles[1]) SAFE_FREE(subtitleFiles[1]);
        SAFE_FREE(subtitleFiles);
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
                    videoId = (wchar_t*)SAFE_MALLOC(12 * sizeof(wchar_t));
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
                        if (subtitleFiles[i]) SAFE_FREE(subtitleFiles[i]);
                    }
                    SAFE_FREE(subtitleFiles);
                }
                
                SAFE_FREE(videoId);
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
    if (sizeStr) SAFE_FREE(sizeStr);
    
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
    wchar_t** videoIds = (wchar_t**)SAFE_MALLOC(selectedCount * sizeof(wchar_t*));
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
                videoIds[currentIndex] = SAFE_WCSDUP(videoId);
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
            SAFE_FREE(videoIds[i]);
        }
    }
    
    SAFE_FREE(videoIds);
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
                SAFE_FREE(videoId);
            }
        }
    }
}