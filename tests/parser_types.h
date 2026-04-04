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

    // Playlist progress tracking
    BOOL isPlaylist;
    int playlistCurrentVideo;
    int playlistTotalVideos;
    wchar_t* currentVideoTitle;
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
