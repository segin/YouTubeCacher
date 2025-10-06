#ifndef CACHE_H
#define CACHE_H

#include <windows.h>

// Long path support constants (must match YouTubeCacher.h)
#define MAX_LONG_PATH       32767  // Windows 10 long path limit
#define MAX_EXTENDED_PATH   (MAX_LONG_PATH + 1)

// Cache entry structure
typedef struct CacheEntry {
    wchar_t* videoId;           // YouTube video ID
    wchar_t* title;             // Video title
    wchar_t* duration;          // Video duration
    wchar_t* mainVideoFile;     // Main video file path
    wchar_t** subtitleFiles;    // Array of subtitle file paths
    int subtitleCount;          // Number of subtitle files
    FILETIME downloadTime;      // When the video was downloaded
    DWORD fileSize;             // Total size of all files
    struct CacheEntry* next;    // Linked list pointer
} CacheEntry;

// Cache management structure
typedef struct {
    CacheEntry* entries;        // Linked list of cache entries
    int totalEntries;           // Total number of cached videos
    wchar_t cacheFilePath[MAX_EXTENDED_PATH]; // Path to cache index file
    CRITICAL_SECTION lock;      // Thread safety
} CacheManager;

// Cache file constants
#define CACHE_FILE_NAME         L"cache_index.txt"
#define CACHE_VERSION           L"1.0"
#define MAX_CACHE_LINE_LENGTH   2048

// File deletion error information
typedef struct {
    wchar_t* fileName;
    DWORD errorCode;
} FileDeleteError;

typedef struct {
    FileDeleteError* errors;
    int errorCount;
    int totalFiles;
    int successfulDeletes;
} DeleteResult;

// Function prototypes
BOOL InitializeCacheManager(CacheManager* manager, const wchar_t* downloadPath);
void CleanupCacheManager(CacheManager* manager);
BOOL LoadCacheFromFile(CacheManager* manager);
BOOL SaveCacheToFile(CacheManager* manager);
BOOL AddCacheEntry(CacheManager* manager, const wchar_t* videoId, const wchar_t* title, 
                   const wchar_t* duration, const wchar_t* mainVideoFile, 
                   wchar_t** subtitleFiles, int subtitleCount);
BOOL RemoveCacheEntry(CacheManager* manager, const wchar_t* videoId);
CacheEntry* FindCacheEntry(CacheManager* manager, const wchar_t* videoId);
BOOL DeleteCacheEntryFiles(CacheManager* manager, const wchar_t* videoId);
DeleteResult* DeleteCacheEntryFilesDetailed(CacheManager* manager, const wchar_t* videoId);
void FreeDeleteResult(DeleteResult* result);
wchar_t* FormatDeleteErrorDetails(const DeleteResult* result);
BOOL PlayCacheEntry(CacheManager* manager, const wchar_t* videoId, const wchar_t* playerPath);
void RefreshCacheList(HWND hListView, CacheManager* manager);
wchar_t* ExtractVideoIdFromUrl(const wchar_t* url);
BOOL ScanDownloadFolderForVideos(CacheManager* manager, const wchar_t* downloadPath);
BOOL AddDummyVideo(CacheManager* manager, const wchar_t* downloadPath);
void FreeCacheEntry(CacheEntry* entry);
BOOL ValidateCacheEntry(const CacheEntry* entry);

// Utility functions
BOOL GetVideoFileInfo(const wchar_t* filePath, DWORD* fileSize, FILETIME* modTime);
BOOL FindSubtitleFiles(const wchar_t* videoFilePath, wchar_t*** subtitleFiles, int* count);
wchar_t* FormatFileSize(DWORD sizeBytes);
wchar_t* FormatCacheEntryDisplay(const CacheEntry* entry);

// UI helper functions for ListView management
void UpdateCacheListStatus(HWND hDlg, CacheManager* manager);
wchar_t* GetSelectedVideoId(HWND hListView);
wchar_t** GetSelectedVideoIds(HWND hListView, int* count);
void FreeSelectedVideoIds(wchar_t** videoIds, int count);
void InitializeCacheListView(HWND hListView);
void ResizeCacheListViewColumns(HWND hListView, int totalWidth);
void CleanupListViewItemData(HWND hListView);

#endif // CACHE_H