#ifndef PARSER_H
#define PARSER_H

#include <windows.h>
#include "YouTubeCacher.h"

// Enhanced download state tracking
typedef enum {
    DOWNLOAD_STATE_INITIALIZING,
    DOWNLOAD_STATE_CHECKING_URL,
    DOWNLOAD_STATE_EXTRACTING_INFO,
    DOWNLOAD_STATE_PREPARING_DOWNLOAD,
    DOWNLOAD_STATE_DOWNLOADING,
    DOWNLOAD_STATE_POST_PROCESSING,
    DOWNLOAD_STATE_FINALIZING,
    DOWNLOAD_STATE_COMPLETED,
    DOWNLOAD_STATE_FAILED,
    DOWNLOAD_STATE_CANCELLED
} DownloadState;

// File tracking information
typedef struct {
    wchar_t* filePath;
    wchar_t* extension;
    BOOL isMainVideo;
    BOOL isSubtitle;
    BOOL isMetadata;
    BOOL isThumbnail;
    DWORD fileSize;
    FILETIME creationTime;
} TrackedFile;

// Enhanced progress information with state tracking
typedef struct {
    DownloadState currentState;
    wchar_t* stateDescription;
    int progressPercentage;
    wchar_t* statusMessage;
    wchar_t* currentOperation;
    
    // File tracking
    TrackedFile** trackedFiles;
    int fileCount;
    int maxFiles;
    
    // Video information
    wchar_t* videoId;
    wchar_t* videoTitle;
    wchar_t* videoDuration;
    wchar_t* videoFormat;
    wchar_t* finalVideoFile;
    
    // UI communication
    HWND parentWindow;
    
    // Download statistics
    long long downloadedBytes;
    long long totalBytes;
    double downloadSpeed;
    wchar_t* eta;
    
    // Error information
    BOOL hasError;
    wchar_t* errorMessage;
    wchar_t* errorDetails;
    
    // Pre-download messages
    wchar_t** preDownloadMessages;
    int messageCount;
    int maxMessages;
} EnhancedProgressInfo;

// Output line classification
typedef enum {
    LINE_TYPE_UNKNOWN,
    LINE_TYPE_INFO_EXTRACTION,
    LINE_TYPE_FORMAT_SELECTION,
    LINE_TYPE_DOWNLOAD_PROGRESS,
    LINE_TYPE_POST_PROCESSING,
    LINE_TYPE_FILE_DESTINATION,
    LINE_TYPE_ERROR,
    LINE_TYPE_WARNING,
    LINE_TYPE_DEBUG,
    LINE_TYPE_COMPLETION
} OutputLineType;

// Parsed output line information
typedef struct {
    OutputLineType lineType;
    wchar_t* originalLine;
    wchar_t* extractedInfo;
    int progressPercentage;
    wchar_t* fileName;
    wchar_t* fileExtension;
    BOOL isMainVideoFile;
} ParsedOutputLine;

// Function prototypes
BOOL InitializeEnhancedProgressInfo(EnhancedProgressInfo* progress);
void FreeEnhancedProgressInfo(EnhancedProgressInfo* progress);
BOOL ProcessYtDlpOutputLine(const wchar_t* line, EnhancedProgressInfo* progress);
OutputLineType ClassifyOutputLine(const wchar_t* line);
BOOL ParseProgressLine(const wchar_t* line, EnhancedProgressInfo* progress);
BOOL ParseFileDestinationLine(const wchar_t* line, EnhancedProgressInfo* progress);
BOOL ParseInfoExtractionLine(const wchar_t* line, EnhancedProgressInfo* progress);
BOOL ParseJsonMetadataLine(const wchar_t* line, EnhancedProgressInfo* progress);
wchar_t* UnescapeJsonString(const wchar_t* escaped);
BOOL ParsePostProcessingLine(const wchar_t* line, EnhancedProgressInfo* progress);
BOOL AddTrackedFile(EnhancedProgressInfo* progress, const wchar_t* filePath, BOOL isMainVideo);
BOOL AddPreDownloadMessage(EnhancedProgressInfo* progress, const wchar_t* message);
void UpdateDownloadState(EnhancedProgressInfo* progress, DownloadState newState, const wchar_t* description);
wchar_t* DetectFinalVideoFile(EnhancedProgressInfo* progress);
BOOL IsVideoFileExtension(const wchar_t* extension);
BOOL IsSubtitleFileExtension(const wchar_t* extension);
wchar_t* ExtractFileNameFromPath(const wchar_t* fullPath);
wchar_t* ExtractFileExtension(const wchar_t* fileName);
void LogProgressState(const EnhancedProgressInfo* progress);

// Enhanced subprocess context with better output processing
typedef struct {
    SubprocessContext* baseContext;
    EnhancedProgressInfo* enhancedProgress;
    CRITICAL_SECTION progressLock;
    BOOL useEnhancedProcessing;
} EnhancedSubprocessContext;

// Enhanced subprocess functions
EnhancedSubprocessContext* CreateEnhancedSubprocessContext(const YtDlpConfig* config, 
                                                          const YtDlpRequest* request,
                                                          ProgressCallback progressCallback, 
                                                          void* callbackUserData, 
                                                          HWND parentWindow);
void FreeEnhancedSubprocessContext(EnhancedSubprocessContext* context);
BOOL StartEnhancedSubprocessExecution(EnhancedSubprocessContext* context);
DWORD WINAPI EnhancedSubprocessWorkerThread(LPVOID lpParam);
void EnhancedProgressCallback(const EnhancedProgressInfo* progress, void* userData);

#endif // PARSER_H