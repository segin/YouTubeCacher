#include "YouTubeCacher.h"

// External debug output function from main.c
extern void DebugOutput(const wchar_t* message);

// Custom window messages (should match main.c)
#define WM_UNIFIED_DOWNLOAD_UPDATE (WM_USER + 113)

// Initialize enhanced progress information
BOOL InitializeEnhancedProgressInfo(EnhancedProgressInfo* progress) {
    if (!progress) return FALSE;
    
    memset(progress, 0, sizeof(EnhancedProgressInfo));
    
    progress->currentState = DOWNLOAD_STATE_INITIALIZING;
    progress->stateDescription = _wcsdup(L"Initializing download process");
    progress->progressPercentage = 0;
    progress->statusMessage = _wcsdup(L"Starting...");
    
    // Initialize file tracking arrays
    progress->maxFiles = 10;
    progress->trackedFiles = (TrackedFile**)malloc(progress->maxFiles * sizeof(TrackedFile*));
    if (!progress->trackedFiles) {
        FreeEnhancedProgressInfo(progress);
        return FALSE;
    }
    
    // Initialize message tracking arrays
    progress->maxMessages = 50;
    progress->preDownloadMessages = (wchar_t**)malloc(progress->maxMessages * sizeof(wchar_t*));
    if (!progress->preDownloadMessages) {
        FreeEnhancedProgressInfo(progress);
        return FALSE;
    }
    
    return TRUE;
}

// Free enhanced progress information
void FreeEnhancedProgressInfo(EnhancedProgressInfo* progress) {
    if (!progress) return;
    
    if (progress->stateDescription) free(progress->stateDescription);
    if (progress->statusMessage) free(progress->statusMessage);
    if (progress->currentOperation) free(progress->currentOperation);
    if (progress->videoId) free(progress->videoId);
    if (progress->videoTitle) free(progress->videoTitle);
    if (progress->videoDuration) free(progress->videoDuration);
    if (progress->videoFormat) free(progress->videoFormat);
    if (progress->finalVideoFile) free(progress->finalVideoFile);
    if (progress->eta) free(progress->eta);
    if (progress->errorMessage) free(progress->errorMessage);
    if (progress->errorDetails) free(progress->errorDetails);
    
    // Free tracked files
    if (progress->trackedFiles) {
        for (int i = 0; i < progress->fileCount; i++) {
            if (progress->trackedFiles[i]) {
                if (progress->trackedFiles[i]->filePath) free(progress->trackedFiles[i]->filePath);
                if (progress->trackedFiles[i]->extension) free(progress->trackedFiles[i]->extension);
                free(progress->trackedFiles[i]);
            }
        }
        free(progress->trackedFiles);
    }
    
    // Free pre-download messages
    if (progress->preDownloadMessages) {
        for (int i = 0; i < progress->messageCount; i++) {
            if (progress->preDownloadMessages[i]) {
                free(progress->preDownloadMessages[i]);
            }
        }
        free(progress->preDownloadMessages);
    }
    
    memset(progress, 0, sizeof(EnhancedProgressInfo));
}

// Process a single line of yt-dlp output
BOOL ProcessYtDlpOutputLine(const wchar_t* line, EnhancedProgressInfo* progress) {
    if (!line || !progress) return FALSE;
    
    // Log the raw line for debugging
    wchar_t debugMsg[1024];
    swprintf(debugMsg, 1024, L"YouTubeCacher: ProcessYtDlpOutputLine: %ls", line);
    DebugOutput(debugMsg);
    
    // Classify the line type
    OutputLineType lineType = ClassifyOutputLine(line);
    
    switch (lineType) {
        case LINE_TYPE_INFO_EXTRACTION:
            return ParseInfoExtractionLine(line, progress);
            
        case LINE_TYPE_FORMAT_SELECTION:
            UpdateDownloadState(progress, DOWNLOAD_STATE_PREPARING_DOWNLOAD, L"Selecting video format");
            AddPreDownloadMessage(progress, line);
            return TRUE;
            
        case LINE_TYPE_DOWNLOAD_PROGRESS:
            if (progress->currentState < DOWNLOAD_STATE_DOWNLOADING) {
                UpdateDownloadState(progress, DOWNLOAD_STATE_DOWNLOADING, L"Downloading video");
            }
            return ParseProgressLine(line, progress);
            
        case LINE_TYPE_POST_PROCESSING:
            UpdateDownloadState(progress, DOWNLOAD_STATE_POST_PROCESSING, L"Post-processing video");
            return ParsePostProcessingLine(line, progress);
            
        case LINE_TYPE_FILE_DESTINATION:
            return ParseFileDestinationLine(line, progress);
            
        case LINE_TYPE_ERROR:
            progress->hasError = TRUE;
            if (progress->errorMessage) free(progress->errorMessage);
            progress->errorMessage = _wcsdup(line);
            UpdateDownloadState(progress, DOWNLOAD_STATE_FAILED, L"Download failed");
            return TRUE;
            
        case LINE_TYPE_WARNING:
            AddPreDownloadMessage(progress, line);
            return TRUE;
            
        case LINE_TYPE_COMPLETION:
            UpdateDownloadState(progress, DOWNLOAD_STATE_COMPLETED, L"Download completed");
            progress->progressPercentage = 100;
            if (progress->statusMessage) free(progress->statusMessage);
            progress->statusMessage = _wcsdup(L"Download completed successfully");
            return TRUE;
            
        case LINE_TYPE_DEBUG:
        case LINE_TYPE_UNKNOWN:
        default:
            // Store as pre-download message if we haven't started downloading yet
            if (progress->currentState < DOWNLOAD_STATE_DOWNLOADING) {
                AddPreDownloadMessage(progress, line);
            }
            return TRUE;
    }
}

// Classify output line type based on content patterns
OutputLineType ClassifyOutputLine(const wchar_t* line) {
    if (!line) return LINE_TYPE_UNKNOWN;
    
    // Convert to lowercase for pattern matching
    wchar_t* lowerLine = _wcsdup(line);
    if (!lowerLine) return LINE_TYPE_UNKNOWN;
    
    for (wchar_t* p = lowerLine; *p; p++) {
        *p = towlower(*p);
    }
    
    OutputLineType result = LINE_TYPE_UNKNOWN;
    
    // Check for various patterns
    if (wcsstr(lowerLine, L"[info]") && wcsstr(lowerLine, L"extracting")) {
        result = LINE_TYPE_INFO_EXTRACTION;
    }
    else if (wcsstr(lowerLine, L"[info]") && (wcsstr(lowerLine, L"format") || wcsstr(lowerLine, L"quality"))) {
        result = LINE_TYPE_FORMAT_SELECTION;
    }
    else if (wcsstr(lowerLine, L"[download]") && (wcsstr(lowerLine, L"%") || wcsstr(lowerLine, L"downloading"))) {
        result = LINE_TYPE_DOWNLOAD_PROGRESS;
    }
    // Check for pipe-delimited progress format: numbers|numbers|numbers|numbers
    else if (wcschr(line, L'|')) {
        // Simple heuristic: if line has pipes and starts with digits, likely progress
        const wchar_t* p = line;
        while (*p && iswspace(*p)) p++; // Skip leading whitespace
        if (*p && iswdigit(*p)) {
            // Count pipes to see if it matches expected format (3 pipes = 4 fields)
            int pipeCount = 0;
            for (const wchar_t* q = line; *q; q++) {
                if (*q == L'|') pipeCount++;
            }
            if (pipeCount >= 3) { // At least 3 pipes for downloaded|total|speed|eta format
                result = LINE_TYPE_DOWNLOAD_PROGRESS;
            }
        }
    }
    else if (wcsstr(lowerLine, L"[ffmpeg]") || wcsstr(lowerLine, L"post-process") || wcsstr(lowerLine, L"converting")) {
        result = LINE_TYPE_POST_PROCESSING;
    }
    else if (wcsstr(lowerLine, L"destination:") || wcsstr(lowerLine, L"saving to:") || 
             (wcsstr(lowerLine, L"[download]") && wcsstr(lowerLine, L"has already been downloaded"))) {
        result = LINE_TYPE_FILE_DESTINATION;
    }
    else if (wcsstr(lowerLine, L"error") || wcsstr(lowerLine, L"failed") || wcsstr(lowerLine, L"exception")) {
        result = LINE_TYPE_ERROR;
    }
    else if (wcsstr(lowerLine, L"warning") || wcsstr(lowerLine, L"warn")) {
        result = LINE_TYPE_WARNING;
    }
    else if (wcsstr(lowerLine, L"100%") || wcsstr(lowerLine, L"download completed") || 
             wcsstr(lowerLine, L"finished downloading")) {
        result = LINE_TYPE_COMPLETION;
    }
    else if (wcsstr(lowerLine, L"[debug]")) {
        result = LINE_TYPE_DEBUG;
    }
    
    free(lowerLine);
    return result;
}

// Parse progress line (download percentage, speed, ETA)
BOOL ParseProgressLine(const wchar_t* line, EnhancedProgressInfo* progress) {
    if (!line || !progress) return FALSE;
    
    // Check for pipe-delimited progress format first: downloaded|total|speed|eta
    const wchar_t* firstPipe = wcschr(line, L'|');
    if (firstPipe) {
        // Handle both "download:123|456|789|10" and raw "123|456|789|10" formats
        const wchar_t* dataStart = line;
        if (wcsstr(line, L"download:") == line) {
            dataStart = line + 9; // Skip "download:" prefix
        }
        
        // Parse pipe-delimited format: downloaded_bytes|total_bytes|speed_bytes_per_sec|eta_seconds
        wchar_t* lineCopy = _wcsdup(dataStart);
        if (!lineCopy) return FALSE;
        
        wchar_t* context = NULL;
        wchar_t* token = wcstok(lineCopy, L"|", &context);
        int tokenIndex = 0;
        
        long long downloadedBytes = 0;
        long long totalBytes = 0;
        
        while (token && tokenIndex < 4) {
            switch (tokenIndex) {
                case 0: { // Downloaded bytes
                    if (wcslen(token) > 0 && wcscmp(token, L"N/A") != 0 && wcscmp(token, L"NA") != 0) {
                        downloadedBytes = wcstoll(token, NULL, 10);
                    }
                    break;
                }
                case 1: { // Total bytes
                    if (wcslen(token) > 0 && wcscmp(token, L"N/A") != 0 && wcscmp(token, L"NA") != 0) {
                        totalBytes = wcstoll(token, NULL, 10);
                    }
                    break;
                }
                case 2: { // Speed (we'll parse this for status message)
                    // Speed parsing can be added here if needed
                    break;
                }
                case 3: { // ETA (we'll parse this for status message)
                    // ETA parsing can be added here if needed
                    break;
                }
            }
            token = wcstok(NULL, L"|", &context);
            tokenIndex++;
        }
        
        free(lineCopy);
        
        // Calculate percentage from raw data
        if (totalBytes > 0 && downloadedBytes >= 0) {
            progress->progressPercentage = (int)((downloadedBytes * 100) / totalBytes);
            if (progress->progressPercentage > 100) progress->progressPercentage = 100;
            
            // Update status message
            if (progress->statusMessage) free(progress->statusMessage);
            wchar_t statusMsg[256];
            swprintf(statusMsg, 256, L"Downloading (%d%%)", progress->progressPercentage);
            progress->statusMessage = _wcsdup(statusMsg);
            
            return TRUE;
        } else if (downloadedBytes > 0) {
            // No total size available - use indeterminate progress (-1)
            progress->progressPercentage = -1;
            
            // Update status message with downloaded amount
            if (progress->statusMessage) free(progress->statusMessage);
            wchar_t statusMsg[256];
            if (downloadedBytes >= 1024 * 1024) {
                swprintf(statusMsg, 256, L"Downloaded %.1f MB", downloadedBytes / (1024.0 * 1024.0));
            } else if (downloadedBytes >= 1024) {
                swprintf(statusMsg, 256, L"Downloaded %.1f KB", downloadedBytes / 1024.0);
            } else {
                swprintf(statusMsg, 256, L"Downloaded %lld bytes", downloadedBytes);
            }
            progress->statusMessage = _wcsdup(statusMsg);
            
            return TRUE;
        }
    }
    
    // Fallback to traditional percentage pattern parsing
    // Look for percentage pattern
    const wchar_t* percentPos = wcsstr(line, L"%");
    if (percentPos) {
        // Go backwards to find the start of the number
        const wchar_t* start = percentPos - 1;
        while (start > line && (iswdigit(*start) || *start == L'.' || *start == L' ')) {
            start--;
        }
        start++; // Move to first digit
        
        if (start < percentPos) {
            double percent = wcstod(start, NULL);
            progress->progressPercentage = (int)percent;
            if (progress->progressPercentage > 100) progress->progressPercentage = 100;
        }
    }
    
    // Look for speed information
    const wchar_t* atPos = wcsstr(line, L" at ");
    if (atPos) {
        const wchar_t* speedStart = atPos + 4; // Skip " at "
        const wchar_t* speedEnd = wcsstr(speedStart, L" ETA");
        if (!speedEnd) speedEnd = speedStart + wcslen(speedStart);
        
        if (speedEnd > speedStart) {
            size_t speedLen = speedEnd - speedStart;
            wchar_t* speedStr = (wchar_t*)malloc((speedLen + 1) * sizeof(wchar_t));
            if (speedStr) {
                wcsncpy(speedStr, speedStart, speedLen);
                speedStr[speedLen] = L'\0';
                
                // Update status message with speed
                if (progress->statusMessage) free(progress->statusMessage);
                wchar_t statusMsg[256];
                swprintf(statusMsg, 256, L"Downloading (%d%%) at %ls", progress->progressPercentage, speedStr);
                progress->statusMessage = _wcsdup(statusMsg);
                
                free(speedStr);
            }
        }
    }
    
    // Look for ETA information
    const wchar_t* etaPos = wcsstr(line, L" ETA ");
    if (etaPos) {
        const wchar_t* etaStart = etaPos + 5; // Skip " ETA "
        const wchar_t* etaEnd = etaStart;
        while (*etaEnd && *etaEnd != L' ' && *etaEnd != L'\n' && *etaEnd != L'\r') {
            etaEnd++;
        }
        
        if (etaEnd > etaStart) {
            size_t etaLen = etaEnd - etaStart;
            if (progress->eta) free(progress->eta);
            progress->eta = (wchar_t*)malloc((etaLen + 1) * sizeof(wchar_t));
            if (progress->eta) {
                wcsncpy(progress->eta, etaStart, etaLen);
                progress->eta[etaLen] = L'\0';
            }
        }
    }
    
    return TRUE;
}

// Parse file destination line to track output files
BOOL ParseFileDestinationLine(const wchar_t* line, EnhancedProgressInfo* progress) {
    if (!line || !progress) return FALSE;
    
    wchar_t* filePath = NULL;
    
    // Look for various destination patterns
    if (wcsstr(line, L"Destination: ")) {
        const wchar_t* start = wcsstr(line, L"Destination: ") + 13;
        filePath = _wcsdup(start);
    }
    else if (wcsstr(line, L"[download] ") && wcsstr(line, L" has already been downloaded")) {
        // Extract filename from "has already been downloaded" message
        const wchar_t* start = wcsstr(line, L"[download] ") + 11;
        const wchar_t* end = wcsstr(start, L" has already been downloaded");
        if (end && end > start) {
            size_t len = end - start;
            filePath = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
            if (filePath) {
                wcsncpy(filePath, start, len);
                filePath[len] = L'\0';
            }
        }
    }
    else if (wcsstr(line, L"[download] Downloading video ")) {
        // Extract from downloading message
        const wchar_t* start = wcsstr(line, L"[download] Downloading video ") + 29;
        const wchar_t* end = wcsstr(start, L" of ");
        if (!end) end = start + wcslen(start);
        if (end > start) {
            size_t len = end - start;
            filePath = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
            if (filePath) {
                wcsncpy(filePath, start, len);
                filePath[len] = L'\0';
            }
        }
    }
    
    if (filePath) {
        // Clean up the file path (remove quotes, trim whitespace)
        wchar_t* cleanPath = filePath;
        while (*cleanPath == L' ' || *cleanPath == L'"') cleanPath++;
        
        wchar_t* end = cleanPath + wcslen(cleanPath) - 1;
        while (end > cleanPath && (*end == L' ' || *end == L'"' || *end == L'\n' || *end == L'\r')) {
            *end = L'\0';
            end--;
        }
        
        if (wcslen(cleanPath) > 0) {
            wchar_t* extension = ExtractFileExtension(cleanPath);
            BOOL isMainVideo = IsVideoFileExtension(extension);
            
            AddTrackedFile(progress, cleanPath, isMainVideo);
            
            // If this is a video file, it might be our final output
            if (isMainVideo) {
                if (progress->finalVideoFile) free(progress->finalVideoFile);
                progress->finalVideoFile = _wcsdup(cleanPath);
                
                // Extract video format from extension
                if (progress->videoFormat) free(progress->videoFormat);
                progress->videoFormat = _wcsdup(extension ? extension : L"unknown");
            }
            
            if (extension) free(extension);
        }
        
        free(filePath);
        return TRUE;
    }
    
    return FALSE;
}

// Parse info extraction line to get video metadata
BOOL ParseInfoExtractionLine(const wchar_t* line, EnhancedProgressInfo* progress) {
    if (!line || !progress) return FALSE;
    
    UpdateDownloadState(progress, DOWNLOAD_STATE_EXTRACTING_INFO, L"Extracting video information");
    
    // Look for video ID in the line
    if (wcsstr(line, L"[info] ") && wcslen(line) > 20) {
        const wchar_t* infoStart = wcsstr(line, L"[info] ") + 7;
        
        // Try to extract video ID (11 characters, alphanumeric)
        if (!progress->videoId) {
            wchar_t tempId[12];
            int idChars = 0;
            const wchar_t* p = infoStart;
            
            while (*p && idChars < 11) {
                if (iswalnum(*p) || *p == L'-' || *p == L'_') {
                    tempId[idChars++] = *p;
                } else if (idChars > 0) {
                    break;
                }
                p++;
            }
            
            if (idChars == 11) {
                tempId[11] = L'\0';
                progress->videoId = _wcsdup(tempId);
            }
        }
    }
    
    AddPreDownloadMessage(progress, line);
    return TRUE;
}

// Parse post-processing line
BOOL ParsePostProcessingLine(const wchar_t* line, EnhancedProgressInfo* progress) {
    if (!line || !progress) return FALSE;
    
    if (progress->currentOperation) free(progress->currentOperation);
    
    if (wcsstr(line, L"[ffmpeg]")) {
        progress->currentOperation = _wcsdup(L"Converting video format");
    }
    else if (wcsstr(line, L"Merging formats")) {
        progress->currentOperation = _wcsdup(L"Merging video and audio");
    }
    else if (wcsstr(line, L"Correcting container")) {
        progress->currentOperation = _wcsdup(L"Correcting video container");
    }
    else {
        progress->currentOperation = _wcsdup(L"Post-processing video");
    }
    
    // Look for output file in post-processing messages
    if (wcsstr(line, L"Destination: ")) {
        ParseFileDestinationLine(line, progress);
    }
    
    return TRUE;
}

// Add a tracked file to the progress info
BOOL AddTrackedFile(EnhancedProgressInfo* progress, const wchar_t* filePath, BOOL isMainVideo) {
    if (!progress || !filePath) return FALSE;
    
    // Expand array if needed
    if (progress->fileCount >= progress->maxFiles) {
        progress->maxFiles *= 2;
        TrackedFile** newArray = (TrackedFile**)realloc(progress->trackedFiles, 
                                                        progress->maxFiles * sizeof(TrackedFile*));
        if (!newArray) return FALSE;
        progress->trackedFiles = newArray;
    }
    
    // Create new tracked file
    TrackedFile* file = (TrackedFile*)malloc(sizeof(TrackedFile));
    if (!file) return FALSE;
    
    memset(file, 0, sizeof(TrackedFile));
    file->filePath = _wcsdup(filePath);
    file->extension = ExtractFileExtension(filePath);
    file->isMainVideo = isMainVideo;
    file->isSubtitle = IsSubtitleFileExtension(file->extension);
    file->isMetadata = (wcsstr(filePath, L".info.json") != NULL);
    file->isThumbnail = (wcsstr(filePath, L".jpg") != NULL || wcsstr(filePath, L".png") != NULL);
    
    // Get file creation time if file exists
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, &file->creationTime, NULL, NULL);
        
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(hFile, &fileSize)) {
            file->fileSize = fileSize.LowPart; // Simplified for files < 4GB
        }
        
        CloseHandle(hFile);
    }
    
    progress->trackedFiles[progress->fileCount++] = file;
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: Added tracked file: %ls (main video: %s)", 
            filePath, isMainVideo ? L"yes" : L"no");
    DebugOutput(debugMsg);
    
    return TRUE;
}

// Add a pre-download message
BOOL AddPreDownloadMessage(EnhancedProgressInfo* progress, const wchar_t* message) {
    if (!progress || !message) return FALSE;
    
    // Expand array if needed
    if (progress->messageCount >= progress->maxMessages) {
        progress->maxMessages *= 2;
        wchar_t** newArray = (wchar_t**)realloc(progress->preDownloadMessages, 
                                               progress->maxMessages * sizeof(wchar_t*));
        if (!newArray) return FALSE;
        progress->preDownloadMessages = newArray;
    }
    
    progress->preDownloadMessages[progress->messageCount++] = _wcsdup(message);
    return TRUE;
}

// Update download state
void UpdateDownloadState(EnhancedProgressInfo* progress, DownloadState newState, const wchar_t* description) {
    if (!progress) return;
    
    if (progress->currentState != newState) {
        progress->currentState = newState;
        
        if (progress->stateDescription) free(progress->stateDescription);
        progress->stateDescription = description ? _wcsdup(description) : NULL;
        
        wchar_t debugMsg[256];
        swprintf(debugMsg, 256, L"YouTubeCacher: Download state changed to: %d (%ls)", 
                newState, description ? description : L"no description");
        DebugOutput(debugMsg);
    }
}

// Detect the final video file from tracked files
wchar_t* DetectFinalVideoFile(EnhancedProgressInfo* progress) {
    if (!progress || !progress->trackedFiles) return NULL;
    
    // First, look for the most recent main video file
    TrackedFile* latestVideo = NULL;
    FILETIME latestTime = {0};
    
    for (int i = 0; i < progress->fileCount; i++) {
        TrackedFile* file = progress->trackedFiles[i];
        if (file && file->isMainVideo) {
            if (!latestVideo || CompareFileTime(&file->creationTime, &latestTime) > 0) {
                latestVideo = file;
                latestTime = file->creationTime;
            }
        }
    }
    
    if (latestVideo) {
        return _wcsdup(latestVideo->filePath);
    }
    
    // Fallback: return the finalVideoFile if set
    if (progress->finalVideoFile) {
        return _wcsdup(progress->finalVideoFile);
    }
    
    return NULL;
}

// Check if extension is a video file
BOOL IsVideoFileExtension(const wchar_t* extension) {
    if (!extension) return FALSE;
    
    const wchar_t* videoExts[] = {
        L".mp4", L".mkv", L".webm", L".avi", L".mov", L".flv", 
        L".m4v", L".3gp", L".wmv", L".mpg", L".mpeg", L".ts", L".m2ts"
    };
    
    int numExts = sizeof(videoExts) / sizeof(videoExts[0]);
    for (int i = 0; i < numExts; i++) {
        if (wcsicmp(extension, videoExts[i]) == 0) {
            return TRUE;
        }
    }
    
    return FALSE;
}

// Check if extension is a subtitle file
BOOL IsSubtitleFileExtension(const wchar_t* extension) {
    if (!extension) return FALSE;
    
    const wchar_t* subExts[] = {
        L".srt", L".vtt", L".ass", L".ssa", L".sub", L".sbv", L".ttml"
    };
    
    int numExts = sizeof(subExts) / sizeof(subExts[0]);
    for (int i = 0; i < numExts; i++) {
        if (wcsicmp(extension, subExts[i]) == 0) {
            return TRUE;
        }
    }
    
    return FALSE;
}

// Extract filename from full path
wchar_t* ExtractFileNameFromPath(const wchar_t* fullPath) {
    if (!fullPath) return NULL;
    
    const wchar_t* lastSlash = wcsrchr(fullPath, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(fullPath, L'/');
    
    if (lastSlash) {
        return _wcsdup(lastSlash + 1);
    } else {
        return _wcsdup(fullPath);
    }
}

// Extract file extension from filename
wchar_t* ExtractFileExtension(const wchar_t* fileName) {
    if (!fileName) return NULL;
    
    const wchar_t* lastDot = wcsrchr(fileName, L'.');
    if (lastDot && wcslen(lastDot) > 1) {
        return _wcsdup(lastDot);
    }
    
    return NULL;
}

// Log current progress state for debugging
void LogProgressState(const EnhancedProgressInfo* progress) {
    if (!progress) return;
    
    wchar_t debugMsg[1024];
    swprintf(debugMsg, 1024, 
        L"YouTubeCacher: Progress State - State: %d, Progress: %d%%, Files: %d, Messages: %d\n",
        progress->currentState, progress->progressPercentage, 
        progress->fileCount, progress->messageCount);
    DebugOutput(debugMsg);
    
    if (progress->finalVideoFile) {
        swprintf(debugMsg, 1024, L"YouTubeCacher: Final video file: %ls", progress->finalVideoFile);
        DebugOutput(debugMsg);
    }
}

// Enhanced subprocess context functions

// Create enhanced subprocess context
EnhancedSubprocessContext* CreateEnhancedSubprocessContext(const YtDlpConfig* config, 
                                                          const YtDlpRequest* request,
                                                          ProgressCallback progressCallback, 
                                                          void* callbackUserData, 
                                                          HWND parentWindow) {
    if (!config || !request) return NULL;
    
    EnhancedSubprocessContext* context = (EnhancedSubprocessContext*)malloc(sizeof(EnhancedSubprocessContext));
    if (!context) return NULL;
    
    memset(context, 0, sizeof(EnhancedSubprocessContext));
    
    // Create base subprocess context
    context->baseContext = CreateSubprocessContext(config, request, progressCallback, callbackUserData, parentWindow);
    if (!context->baseContext) {
        free(context);
        return NULL;
    }
    
    // Create enhanced progress info
    context->enhancedProgress = (EnhancedProgressInfo*)malloc(sizeof(EnhancedProgressInfo));
    if (!context->enhancedProgress) {
        FreeSubprocessContext(context->baseContext);
        free(context);
        return NULL;
    }
    
    if (!InitializeEnhancedProgressInfo(context->enhancedProgress)) {
        free(context->enhancedProgress);
        FreeSubprocessContext(context->baseContext);
        free(context);
        return NULL;
    }
    
    // Initialize critical section for thread safety
    InitializeCriticalSection(&context->progressLock);
    context->useEnhancedProcessing = TRUE;
    
    return context;
}

// Free enhanced subprocess context
void FreeEnhancedSubprocessContext(EnhancedSubprocessContext* context) {
    if (!context) return;
    
    if (context->enhancedProgress) {
        FreeEnhancedProgressInfo(context->enhancedProgress);
        free(context->enhancedProgress);
    }
    
    if (context->baseContext) {
        FreeSubprocessContext(context->baseContext);
    }
    
    DeleteCriticalSection(&context->progressLock);
    free(context);
}

// Start enhanced subprocess execution
BOOL StartEnhancedSubprocessExecution(EnhancedSubprocessContext* context) {
    if (!context || !context->baseContext) return FALSE;
    
    // Create enhanced worker thread instead of base worker thread
    HANDLE hThread = CreateThread(NULL, 0, EnhancedSubprocessWorkerThread, context, 0, NULL);
    if (!hThread) return FALSE;
    
    // Store thread handle in base context for cleanup
    context->baseContext->threadContext.hThread = hThread;
    return TRUE;
}

// Enhanced subprocess worker thread
DWORD WINAPI EnhancedSubprocessWorkerThread(LPVOID lpParam) {
    DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread started");
    
    EnhancedSubprocessContext* enhancedContext = (EnhancedSubprocessContext*)lpParam;
    if (!enhancedContext || !enhancedContext->baseContext) {
        DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - invalid context");
        return 1;
    }
    
    SubprocessContext* context = enhancedContext->baseContext;
    EnhancedProgressInfo* progress = enhancedContext->enhancedProgress;
    
    DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - context valid");
    
    // Mark thread as running
    EnterCriticalSection(&context->threadContext.criticalSection);
    context->threadContext.isRunning = TRUE;
    LeaveCriticalSection(&context->threadContext.criticalSection);
    
    // Initialize result structure
    context->result = (YtDlpResult*)malloc(sizeof(YtDlpResult));
    if (!context->result) {
        DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - Failed to allocate result structure");
        context->completed = TRUE;
        return 1;
    }
    memset(context->result, 0, sizeof(YtDlpResult));
    
    // Report initial progress
    UpdateDownloadState(progress, DOWNLOAD_STATE_INITIALIZING, L"Initializing yt-dlp process");
    if (context->progressCallback) {
        context->progressCallback(0, L"Initializing yt-dlp process...", context->callbackUserData);
    }
    
    // Build command line arguments
    wchar_t arguments[2048];
    if (!GetYtDlpArgsForOperation(context->request->operation, context->request->url, 
                                 context->request->outputPath, context->config, arguments, 2048)) {
        DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - FAILED to build yt-dlp arguments");
        context->result->success = FALSE;
        context->result->exitCode = 1;
        context->result->errorMessage = _wcsdup(L"Failed to build yt-dlp arguments");
        context->completed = TRUE;
        return 1;
    }
    
    // Check for cancellation before starting process
    if (IsCancellationRequested(&context->threadContext)) {
        DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - Operation was cancelled");
        context->result->success = FALSE;
        context->result->errorMessage = _wcsdup(L"Operation cancelled by user");
        context->completed = TRUE;
        return 1;
    }
    
    // Create pipes for output capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&context->hOutputRead, &context->hOutputWrite, &sa, 0)) {
        DWORD error = GetLastError();
        DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - FAILED to create output pipe");
        context->result->success = FALSE;
        context->result->exitCode = error;
        context->result->errorMessage = _wcsdup(L"Failed to create output pipe");
        context->completed = TRUE;
        return 1;
    }
    
    SetHandleInformation(context->hOutputRead, HANDLE_FLAG_INHERIT, 0);
    
    // Setup process startup info
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = context->hOutputWrite;
    si.hStdError = context->hOutputWrite;
    si.hStdInput = NULL;
    
    PROCESS_INFORMATION pi = {0};
    
    // Build command line
    size_t cmdLineLen = wcslen(context->config->ytDlpPath) + wcslen(arguments) + 10;
    wchar_t* cmdLine = (wchar_t*)malloc(cmdLineLen * sizeof(wchar_t));
    if (!cmdLine) {
        DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - FAILED to allocate command line memory");
        CloseHandle(context->hOutputRead);
        CloseHandle(context->hOutputWrite);
        context->result->success = FALSE;
        context->result->exitCode = 1;
        context->result->errorMessage = _wcsdup(L"Memory allocation failed");
        context->completed = TRUE;
        return 1;
    }
    
    swprintf(cmdLine, cmdLineLen, L"\"%ls\" %ls", context->config->ytDlpPath, arguments);
    
    // Update state to checking URL
    UpdateDownloadState(progress, DOWNLOAD_STATE_CHECKING_URL, L"Starting yt-dlp process");
    if (context->progressCallback) {
        context->progressCallback(10, L"Starting yt-dlp process...", context->callbackUserData);
    }
    
    // Create the yt-dlp process
    BOOL processCreated = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    
    if (!processCreated) {
        DWORD error = GetLastError();
        DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - FAILED to create process");
        free(cmdLine);
        CloseHandle(context->hOutputRead);
        CloseHandle(context->hOutputWrite);
        context->result->success = FALSE;
        context->result->exitCode = error;
        context->result->errorMessage = _wcsdup(L"Failed to start yt-dlp process");
        context->completed = TRUE;
        return 1;
    }
    
    // Store process handle for potential cancellation
    context->hProcess = pi.hProcess;
    CloseHandle(context->hOutputWrite);
    free(cmdLine);
    
    // Initialize output buffer
    context->outputBufferSize = 8192;
    context->accumulatedOutput = (wchar_t*)malloc(context->outputBufferSize * sizeof(wchar_t));
    if (context->accumulatedOutput) {
        context->accumulatedOutput[0] = L'\0';
    }
    
    // Enhanced output reading loop with line-by-line processing
    char buffer[4096];
    DWORD bytesRead;
    BOOL processRunning = TRUE;
    int loopCount = 0;
    
    // Line accumulator for UTF-8 processing
    static char lineAccumulator[8192] = {0};
    static size_t fillCounter = 0;
    
    while (processRunning || fillCounter > 0) {
        loopCount++;
        
        // Check for cancellation
        if (IsCancellationRequested(&context->threadContext)) {
            DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread - Cancellation requested");
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        
        // Check if process is still running
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 100); // 100ms timeout
        if (waitResult == WAIT_OBJECT_0) {
            processRunning = FALSE;
        }
        
        // Read available output
        DWORD bytesAvailable = 0;
        if (PeekNamedPipe(context->hOutputRead, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
            if (ReadFile(context->hOutputRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                
                // Safely add new bytes to accumulator with bounds checking
                size_t spaceAvailable = sizeof(lineAccumulator) - fillCounter - 1;
                size_t bytesToCopy = (bytesRead < spaceAvailable) ? bytesRead : spaceAvailable;
                
                if (bytesToCopy > 0) {
                    memcpy(lineAccumulator + fillCounter, buffer, bytesToCopy);
                    fillCounter += bytesToCopy;
                    lineAccumulator[fillCounter] = '\0';
                }
                
                // Process complete lines with enhanced processing
                char* start = lineAccumulator;
                char* newline;
                while ((newline = strchr(start, '\n')) != NULL) {
                    *newline = '\0';
                    
                    // Remove \r if present
                    size_t lineLen = newline - start;
                    if (lineLen > 0 && start[lineLen - 1] == '\r') {
                        start[lineLen - 1] = '\0';
                        lineLen--;
                    }
                    
                    // Convert this complete UTF-8 line to wide chars
                    if (lineLen > 0) {
                        wchar_t wideLineBuffer[2048];
                        int converted = MultiByteToWideChar(CP_UTF8, 0, start, (int)lineLen, wideLineBuffer, 2047);
                        if (converted > 0) {
                            wideLineBuffer[converted] = L'\0';
                            
                            // Process the line with enhanced processing
                            EnterCriticalSection(&enhancedContext->progressLock);
                            ProcessYtDlpOutputLine(wideLineBuffer, progress);
                            LeaveCriticalSection(&enhancedContext->progressLock);
                            
                            // Add to accumulated output
                            if (context->accumulatedOutput) {
                                size_t currentLen = wcslen(context->accumulatedOutput);
                                size_t newLen = currentLen + converted + 2;
                                if (newLen >= context->outputBufferSize) {
                                    context->outputBufferSize = newLen * 2;
                                    wchar_t* newBuffer = (wchar_t*)realloc(context->accumulatedOutput, 
                                                                          context->outputBufferSize * sizeof(wchar_t));
                                    if (newBuffer) {
                                        context->accumulatedOutput = newBuffer;
                                    }
                                }
                                
                                if (context->accumulatedOutput) {
                                    wcscat(context->accumulatedOutput, wideLineBuffer);
                                    wcscat(context->accumulatedOutput, L"\n");
                                }
                            }
                            
                            // Update progress callback with enhanced information
                            if (context->progressCallback) {
                                EnterCriticalSection(&enhancedContext->progressLock);
                                const wchar_t* statusMsg = progress->statusMessage ? progress->statusMessage : L"Processing...";
                                context->progressCallback(progress->progressPercentage, statusMsg, context->callbackUserData);
                                LeaveCriticalSection(&enhancedContext->progressLock);
                            }
                        }
                    }
                    
                    start = newline + 1;
                }
                
                // Move remaining incomplete data to start of buffer
                size_t remaining = fillCounter - (start - lineAccumulator);
                if (remaining > 0 && start != lineAccumulator) {
                    memmove(lineAccumulator, start, remaining);
                }
                fillCounter = remaining;
                lineAccumulator[fillCounter] = '\0';
            }
        }
        
        // If process has exited and no more data, break
        if (!processRunning && fillCounter == 0) {
            break;
        }
        
        // Small delay to prevent excessive CPU usage
        if (bytesAvailable == 0) {
            Sleep(50);
        }
    }
    
    // Process any remaining data in the accumulator
    if (fillCounter > 0) {
        wchar_t wideLineBuffer[2048];
        int converted = MultiByteToWideChar(CP_UTF8, 0, lineAccumulator, (int)fillCounter, wideLineBuffer, 2047);
        if (converted > 0) {
            wideLineBuffer[converted] = L'\0';
            
            EnterCriticalSection(&enhancedContext->progressLock);
            ProcessYtDlpOutputLine(wideLineBuffer, progress);
            LeaveCriticalSection(&enhancedContext->progressLock);
            
            if (context->accumulatedOutput) {
                wcscat(context->accumulatedOutput, wideLineBuffer);
            }
        }
        fillCounter = 0;
    }
    
    // Wait for process completion and get exit code
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    // Finalize enhanced progress
    EnterCriticalSection(&enhancedContext->progressLock);
    if (exitCode == 0) {
        UpdateDownloadState(progress, DOWNLOAD_STATE_COMPLETED, L"Download completed successfully");
        progress->progressPercentage = 100;
        
        // Detect final video file
        wchar_t* finalFile = DetectFinalVideoFile(progress);
        if (finalFile) {
            if (progress->finalVideoFile) free(progress->finalVideoFile);
            progress->finalVideoFile = finalFile;
        }
    } else {
        UpdateDownloadState(progress, DOWNLOAD_STATE_FAILED, L"Download failed");
        progress->hasError = TRUE;
    }
    LeaveCriticalSection(&enhancedContext->progressLock);
    
    // Set result
    context->result->success = (exitCode == 0);
    context->result->exitCode = exitCode;
    context->result->output = context->accumulatedOutput;
    
    if (!context->result->success) {
        EnterCriticalSection(&enhancedContext->progressLock);
        if (progress->errorMessage) {
            context->result->errorMessage = _wcsdup(progress->errorMessage);
        } else {
            context->result->errorMessage = _wcsdup(L"yt-dlp process failed");
        }
        LeaveCriticalSection(&enhancedContext->progressLock);
    }
    
    // Cleanup
    CloseHandle(context->hOutputRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Mark as completed
    context->completed = TRUE;
    context->completionTime = GetTickCount();
    
    DebugOutput(L"YouTubeCacher: EnhancedSubprocessWorkerThread completed");
    return 0;
}

// Enhanced progress callback
void EnhancedProgressCallback(const EnhancedProgressInfo* progress, void* userData) {
    if (!progress || !userData) return;
    
    HWND hDlg = (HWND)userData;
    
    // Send enhanced progress information to the main window
    PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, progress->progressPercentage);
    
    if (progress->statusMessage) {
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)_wcsdup(progress->statusMessage));
    }
    
    // Log detailed progress state
    LogProgressState(progress);
}