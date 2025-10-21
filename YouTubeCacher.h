#ifndef YOUTUBECACHER_H
#define YOUTUBECACHER_H

#include <windows.h>
#include "resource.h"
#include "cache.h"

// Application constants
#define APP_NAME            L"YouTube Cacher"
#define APP_VERSION         L"1.0"
#define MAX_URL_LENGTH      1024
#define MAX_BUFFER_SIZE     1024

// Registry constants
#define REGISTRY_KEY        L"Software\\Talamar Developments\\YouTube Cacher"
#define REG_YTDLP_PATH      L"YtDlpPath"
#define REG_DOWNLOAD_PATH   L"DownloadPath"
#define REG_PLAYER_PATH     L"PlayerPath"

// Long path support constants
#define MAX_LONG_PATH       32767  // Windows 10 long path limit
#define MAX_EXTENDED_PATH   (MAX_LONG_PATH + 1)

// Window sizing constants - these will be calculated dynamically
#define BASE_MIN_WINDOW_WIDTH    500
#define BASE_MIN_WINDOW_HEIGHT   380
#define BASE_DEFAULT_WIDTH       550
#define BASE_DEFAULT_HEIGHT      450

// Function to calculate minimum window dimensions based on DPI and content
void CalculateMinimumWindowSize(int* minWidth, int* minHeight, double dpiScaleX, double dpiScaleY);

// Function to calculate optimal default window dimensions based on DPI and content
void CalculateDefaultWindowSize(int* defaultWidth, int* defaultHeight, double dpiScaleX, double dpiScaleY);

// Layout constants
#define BUTTON_PADDING      80
#define BUTTON_WIDTH        78
#define BUTTON_HEIGHT_SMALL 24
#define BUTTON_HEIGHT_LARGE 30
#define TEXT_FIELD_HEIGHT   20
#define LABEL_HEIGHT        14

// Color definitions for text field backgrounds
#define COLOR_WHITE         RGB(255, 255, 255)
#define COLOR_LIGHT_GREEN   RGB(220, 255, 220)
#define COLOR_LIGHT_BLUE    RGB(220, 220, 255)
#define COLOR_LIGHT_TEAL    RGB(220, 255, 255)

// YtDlp operation types
typedef enum {
    YTDLP_OP_GET_INFO,
    YTDLP_OP_GET_TITLE,
    YTDLP_OP_GET_DURATION,
    YTDLP_OP_GET_TITLE_DURATION,
    YTDLP_OP_DOWNLOAD,
    YTDLP_OP_VALIDATE
} YtDlpOperation;

// Validation result types
typedef enum {
    VALIDATION_OK,
    VALIDATION_NOT_FOUND,
    VALIDATION_NOT_EXECUTABLE,
    VALIDATION_MISSING_DEPENDENCIES,
    VALIDATION_VERSION_INCOMPATIBLE,
    VALIDATION_PERMISSION_DENIED
} ValidationResult;

// Temporary directory strategies
typedef enum {
    TEMP_DIR_SYSTEM,
    TEMP_DIR_DOWNLOAD,
    TEMP_DIR_CUSTOM,
    TEMP_DIR_APPDATA
} TempDirStrategy;

// Error types for analysis
typedef enum {
    ERROR_TYPE_TEMP_DIR,
    ERROR_TYPE_NETWORK,
    ERROR_TYPE_PERMISSIONS,
    ERROR_TYPE_DEPENDENCIES,
    ERROR_TYPE_URL_INVALID,
    ERROR_TYPE_DISK_SPACE,
    ERROR_TYPE_UNKNOWN
} ErrorType;

// YtDlp configuration structure
typedef struct {
    wchar_t ytDlpPath[MAX_EXTENDED_PATH];
    wchar_t defaultTempDir[MAX_EXTENDED_PATH];
    wchar_t defaultArgs[1024];
    DWORD timeoutSeconds;
    BOOL enableVerboseLogging;
    BOOL autoRetryOnFailure;
    TempDirStrategy tempDirStrategy;
} YtDlpConfig;

// YtDlp request structure
typedef struct {
    YtDlpOperation operation;
    wchar_t* url;
    wchar_t* outputPath;
    wchar_t* tempDir;
    BOOL useCustomArgs;
    wchar_t* customArgs;
} YtDlpRequest;

// YtDlp result structure
typedef struct {
    BOOL success;
    DWORD exitCode;
    wchar_t* output;
    wchar_t* errorMessage;
    wchar_t* diagnostics;
} YtDlpResult;

// Validation information structure
typedef struct {
    ValidationResult result;
    wchar_t* version;
    wchar_t* errorDetails;
    wchar_t* suggestions;
} ValidationInfo;

// Process handle structure for robust process management
typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hStdOut;
    HANDLE hStdErr;
    DWORD processId;
    BOOL isRunning;
} ProcessHandle;

// Process options structure
typedef struct {
    DWORD timeoutMs;
    BOOL captureOutput;
    BOOL hideWindow;
    wchar_t* workingDirectory;
    wchar_t* environment;
} ProcessOptions;

// Error analysis structure
typedef struct {
    ErrorType type;
    wchar_t* description;
    wchar_t* solution;
    wchar_t* technicalDetails;
} ErrorAnalysis;

// Progress dialog structure
typedef struct {
    HWND hDialog;
    HWND hProgressBar;
    HWND hStatusText;
    HWND hCancelButton;
    BOOL cancelled;
} ProgressDialog;

// Operation context structure
typedef struct {
    YtDlpConfig config;
    YtDlpRequest request;
    ProcessHandle process;
    ProgressDialog progress;
    wchar_t tempDir[MAX_EXTENDED_PATH];
    BOOL operationActive;
} YtDlpContext;

// Dialog types for enhanced dialogs
typedef enum {
    DIALOG_TYPE_ERROR,
    DIALOG_TYPE_SUCCESS,
    DIALOG_TYPE_WARNING,
    DIALOG_TYPE_INFO
} DialogType;

// Unified dialog types
typedef enum {
    UNIFIED_DIALOG_INFO,
    UNIFIED_DIALOG_WARNING, 
    UNIFIED_DIALOG_ERROR,
    UNIFIED_DIALOG_SUCCESS
} UnifiedDialogType;

// Unified dialog configuration structure
typedef struct {
    UnifiedDialogType dialogType;   // INFO, WARNING, ERROR, SUCCESS
    const wchar_t* title;
    const wchar_t* message;
    const wchar_t* details;         // Always shown in first tab
    
    // Optional additional tabs (NULL to hide)
    const wchar_t* tab1_name;       // Custom name for tab 1 (details)
    const wchar_t* tab2_content;    // Content for tab 2
    const wchar_t* tab2_name;       // Custom name for tab 2
    const wchar_t* tab3_content;    // Content for tab 3
    const wchar_t* tab3_name;       // Custom name for tab 3
    
    BOOL showDetailsButton;         // Whether to show expandable details
    BOOL showCopyButton;           // Whether to show copy button
} UnifiedDialogConfig;

// Enhanced dialog structure (supports both error and success dialogs)
typedef struct {
    wchar_t* title;
    wchar_t* message;
    wchar_t* details;
    wchar_t* diagnostics;
    wchar_t* solutions;
    ErrorType errorType;
    DialogType dialogType;
    BOOL isExpanded;
    HWND hDialog;
    HWND hTabControl;
} EnhancedErrorDialog;

// Thread-safe subprocess execution structures
typedef struct {
    HANDLE hThread;
    DWORD threadId;
    BOOL isRunning;
    BOOL cancelRequested;
    CRITICAL_SECTION criticalSection;
} ThreadContext;

// Progress callback function type
typedef void (*ProgressCallback)(int percentage, const wchar_t* status, void* userData);

// Subprocess execution context for multithreading
typedef struct {
    // Input parameters (set by caller)
    YtDlpConfig* config;
    YtDlpRequest* request;
    ProgressCallback progressCallback;
    void* callbackUserData;
    HWND parentWindow;
    
    // Thread management
    ThreadContext threadContext;
    
    // Output results (set by worker thread)
    YtDlpResult* result;
    BOOL completed;
    DWORD completionTime;
    
    // Process monitoring
    HANDLE hProcess;
    HANDLE hOutputRead;
    HANDLE hOutputWrite;
    wchar_t* accumulatedOutput;
    size_t outputBufferSize;
} SubprocessContext;

// Structure for non-blocking download context
typedef struct NonBlockingDownloadContext {
    YtDlpConfig config;
    YtDlpRequest* request;
    HWND parentWindow;
    wchar_t tempDir[MAX_EXTENDED_PATH];
    wchar_t url[MAX_URL_LENGTH];
    SubprocessContext* context;
} NonBlockingDownloadContext;

// Function prototypes
void DebugOutput(const wchar_t* message);
void WriteSessionStartToLogfile(void);
void WriteSessionEndToLogfile(const wchar_t* reason);
void FormatDuration(wchar_t* duration, size_t bufferSize);
void InstallYtDlpWithWinget(HWND hParent);
void CheckClipboardForYouTubeURL(HWND hDlg);
void ResizeControls(HWND hDlg);
void ApplyModernThemeToDialog(HWND hDlg);
void ApplyDelayedTheming(HWND hDlg);
void ForceVisualStylesActivation(void);
HWND CreateThemedDialog(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL RegisterMainWindowClass(HINSTANCE hInstance);
void GetDefaultDownloadPath(wchar_t* path, size_t pathSize);
void GetDefaultYtDlpPath(wchar_t* path, size_t pathSize);
BOOL CreateDownloadDirectoryIfNeeded(const wchar_t* path);
BOOL ValidateYtDlpExecutable(const wchar_t* path);
LRESULT CALLBACK TextFieldSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL LoadSettingFromRegistry(const wchar_t* valueName, wchar_t* buffer, DWORD bufferSize);
BOOL SaveSettingToRegistry(const wchar_t* valueName, const wchar_t* value);
void LoadSettings(HWND hDlg);
void SaveSettings(HWND hDlg);
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ProgressDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK EnhancedErrorDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// YtDlp management function prototypes
BOOL InitializeYtDlpConfig(YtDlpConfig* config);
void CleanupYtDlpConfig(YtDlpConfig* config);
ValidationInfo* ValidateYtDlpInstallation(const YtDlpConfig* config);
void FreeValidationInfo(ValidationInfo* info);

// Enhanced validation functions
BOOL ValidateYtDlpComprehensive(const wchar_t* path, ValidationInfo* info);
BOOL TestYtDlpFunctionality(const wchar_t* path);
BOOL DetectPythonRuntime(wchar_t* pythonPath, size_t pathSize);
BOOL GetYtDlpVersion(const wchar_t* path, wchar_t* version, size_t versionSize);
BOOL CheckYtDlpCompatibility(const wchar_t* version);
BOOL ParseValidationError(DWORD errorCode, const wchar_t* output, ValidationInfo* info);
BOOL TestYtDlpWithUrl(const wchar_t* path, ValidationInfo* info);

// YtDlp request/result management
YtDlpRequest* CreateYtDlpRequest(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath);
void FreeYtDlpRequest(YtDlpRequest* request);
YtDlpResult* ExecuteYtDlpRequest(const YtDlpConfig* config, const YtDlpRequest* request);
void FreeYtDlpResult(YtDlpResult* result);

// Process management functions
ProcessHandle* CreateYtDlpProcess(const wchar_t* commandLine, const ProcessOptions* options);
BOOL WaitForProcessCompletion(ProcessHandle* handle, DWORD timeoutMs);
BOOL TerminateProcessSafely(ProcessHandle* handle);
void CleanupProcessHandle(ProcessHandle* handle);
wchar_t* ReadProcessOutput(ProcessHandle* handle);

// Process status structure for detailed monitoring
typedef struct {
    DWORD processId;
    wchar_t processName[MAX_PATH];
    DWORD cpuTime;
    BOOL isResponding;
    DWORD memoryUsage;
} ProcessStatus;

// Process timeout and cancellation support functions
BOOL ExecuteYtDlpWithTimeout(const wchar_t* commandLine, const ProcessOptions* options, 
                            DWORD timeoutMs, BOOL* cancelFlag, wchar_t** output, DWORD* exitCode);
BOOL IsProcessHung(ProcessHandle* handle, DWORD responseTimeoutMs);
BOOL KillAllYtDlpProcesses(void);

// Enhanced process monitoring functions
BOOL GetProcessStatus(ProcessHandle* handle, ProcessStatus* status);
typedef void (*ProcessStatusCallback)(const ProcessStatus* status, void* userData);
BOOL ExecuteYtDlpWithAdvancedMonitoring(const wchar_t* commandLine, const ProcessOptions* options,
                                       DWORD timeoutMs, BOOL* cancelFlag, 
                                       ProcessStatusCallback statusCallback, void* callbackUserData,
                                       DWORD statusUpdateIntervalMs, wchar_t** output, DWORD* exitCode);

// Cancellation flag utility functions
BOOL* CreateCancellationFlag(void);
void SignalCancellation(BOOL* cancelFlag);
BOOL IsCancelled(const BOOL* cancelFlag);
void FreeCancellationFlag(BOOL* cancelFlag);

// Error analysis functions
ErrorAnalysis* AnalyzeYtDlpError(const YtDlpResult* result);
void FreeErrorAnalysis(ErrorAnalysis* analysis);
BOOL GenerateDiagnosticReport(const YtDlpRequest* request, const YtDlpResult* result, 
                             const YtDlpConfig* config, wchar_t* report, size_t reportSize);

// Temporary directory management
BOOL CreateYtDlpTempDir(wchar_t* tempPath, size_t pathSize, TempDirStrategy strategy);
BOOL ValidateTempDirAccess(const wchar_t* tempPath);
BOOL CleanupYtDlpTempDir(const wchar_t* tempPath);
BOOL CreateYtDlpTempDirWithFallback(wchar_t* tempPath, size_t pathSize);
BOOL CreateTempDirectory(const YtDlpConfig* config, wchar_t* tempDir, size_t tempDirSize);
BOOL CleanupTempDirectory(const wchar_t* tempDir);

// YtDlp command line construction with temp directory integration
BOOL BuildYtDlpCommandLine(const YtDlpRequest* request, const YtDlpConfig* config, 
                          wchar_t* commandLine, size_t commandLineSize);
BOOL EnsureTempDirForOperation(YtDlpRequest* request, const YtDlpConfig* config);
BOOL AddTempDirToArgs(const wchar_t* tempDir, wchar_t* args, size_t argsSize);
BOOL HandleTempDirFailure(HWND hParent, const YtDlpConfig* config);

// Progress dialog functions
ProgressDialog* CreateProgressDialog(HWND parent, const wchar_t* title);
void UpdateProgressDialog(ProgressDialog* dialog, int progress, const wchar_t* status);
BOOL IsProgressDialogCancelled(const ProgressDialog* dialog);
void DestroyProgressDialog(ProgressDialog* dialog);

// Context management functions
YtDlpContext* CreateYtDlpContext(void);
BOOL InitializeYtDlpContext(YtDlpContext* context, const YtDlpConfig* config);
void CleanupYtDlpContext(YtDlpContext* context);
void FreeYtDlpContext(YtDlpContext* context);

// Command line argument management function prototypes
BOOL GetYtDlpArgsForOperation(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath, 
                             const YtDlpConfig* config, wchar_t* args, size_t argsSize);
BOOL GetOptimizedArgsForInfo(wchar_t* args, size_t argsSize);
BOOL GetOptimizedArgsForDownload(const wchar_t* outputPath, wchar_t* args, size_t argsSize);
BOOL GetFallbackArgs(YtDlpOperation operation, wchar_t* args, size_t argsSize);
BOOL ValidateYtDlpArguments(const wchar_t* args);
BOOL SanitizeYtDlpArguments(wchar_t* args, size_t argsSize);

// Registry constants for custom arguments
#define REG_CUSTOM_ARGS     L"CustomYtDlpArgs"
#define REG_ENABLE_DEBUG    L"EnableDebug"
#define REG_ENABLE_LOGFILE  L"EnableLogfile"
#define REG_ENABLE_AUTOPASTE L"EnableAutopaste"

// Enhanced error dialog functions
EnhancedErrorDialog* CreateEnhancedErrorDialog(const wchar_t* title, const wchar_t* message, 
                                              const wchar_t* details, const wchar_t* diagnostics, 
                                              const wchar_t* solutions, ErrorType errorType);
INT_PTR ShowEnhancedErrorDialog(HWND parent, EnhancedErrorDialog* errorDialog);
void FreeEnhancedErrorDialog(EnhancedErrorDialog* errorDialog);

// Unified dialog function - single entry point for all dialog types
INT_PTR ShowUnifiedDialog(HWND parent, const UnifiedDialogConfig* config);
BOOL CopyErrorInfoToClipboard(const EnhancedErrorDialog* errorDialog);
void ResizeErrorDialog(HWND hDlg, BOOL expanded);
void InitializeErrorDialogTabs(HWND hTabControl);
void InitializeSuccessDialogTabs(HWND hTabControl);
void ShowErrorDialogTab(HWND hDlg, int tabIndex);

// Convenience functions for common error scenarios
INT_PTR ShowYtDlpError(HWND parent, const YtDlpResult* result, const YtDlpRequest* request);
INT_PTR ShowValidationError(HWND parent, const ValidationInfo* validationInfo);
INT_PTR ShowProcessError(HWND parent, DWORD errorCode, const wchar_t* operation);
INT_PTR ShowTempDirError(HWND parent, const wchar_t* tempDir, DWORD errorCode);

// Additional convenience functions for common scenarios
INT_PTR ShowMemoryError(HWND parent, const wchar_t* operation);
INT_PTR ShowConfigurationError(HWND parent, const wchar_t* details);
INT_PTR ShowUIError(HWND parent, const wchar_t* operation);
INT_PTR ShowSuccessMessage(HWND parent, const wchar_t* title, const wchar_t* message);
INT_PTR ShowWarningMessage(HWND parent, const wchar_t* title, const wchar_t* message);
INT_PTR ShowInfoMessage(HWND parent, const wchar_t* title, const wchar_t* message);

// Video metadata structure
typedef struct {
    wchar_t* title;
    wchar_t* duration;
    wchar_t* id;
    BOOL success;
} VideoMetadata;

// Progress information structure
typedef struct {
    int percentage;
    wchar_t* status;
    wchar_t* speed;
    wchar_t* eta;
    long long downloadedBytes;
    long long totalBytes;
    BOOL isComplete;
} ProgressInfo;

// Heap-allocated progress data for PostMessage
typedef struct {
    int percentage;
    wchar_t status[64];
    wchar_t speed[32];
    wchar_t eta[16];
    BOOL isComplete;
} HeapProgressData;

// Video information retrieval functions
BOOL GetVideoTitleAndDuration(HWND hDlg, const wchar_t* url, wchar_t* title, size_t titleSize, wchar_t* duration, size_t durationSize);
BOOL GetVideoTitleAndDurationSync(const wchar_t* url, wchar_t* title, size_t titleSize, wchar_t* duration, size_t durationSize);
DWORD WINAPI GetVideoInfoThread(LPVOID lpParam);
void UpdateVideoInfoUI(HWND hDlg, const wchar_t* title, const wchar_t* duration);

// UI control functions
void SetDownloadUIState(HWND hDlg, BOOL isDownloading);

// Cached video metadata structure
typedef struct {
    wchar_t* url;
    VideoMetadata metadata;
    BOOL isValid;
} CachedVideoMetadata;

// Video metadata functions
BOOL GetVideoMetadata(const wchar_t* url, VideoMetadata* metadata);
BOOL ParseVideoMetadataFromJson(const wchar_t* jsonOutput, VideoMetadata* metadata);
void FreeVideoMetadata(VideoMetadata* metadata);

// Cached metadata functions
void InitializeCachedMetadata(CachedVideoMetadata* cached);
void FreeCachedMetadata(CachedVideoMetadata* cached);
BOOL IsCachedMetadataValid(const CachedVideoMetadata* cached, const wchar_t* url);
void StoreCachedMetadata(CachedVideoMetadata* cached, const wchar_t* url, const VideoMetadata* metadata);
BOOL GetCachedMetadata(const CachedVideoMetadata* cached, VideoMetadata* metadata);

// Non-blocking Get Info functions
typedef struct {
    HWND hDialog;
    wchar_t url[MAX_URL_LENGTH];
    CachedVideoMetadata* cachedMetadata;
} GetInfoContext;



DWORD WINAPI GetInfoWorkerThread(LPVOID lpParam);
BOOL StartNonBlockingGetInfo(HWND hDlg, const wchar_t* url, CachedVideoMetadata* cachedMetadata);

// Unified download functions
DWORD WINAPI UnifiedDownloadWorkerThread(LPVOID lpParam);
void UnifiedDownloadProgressCallback(int percentage, const wchar_t* status, void* userData);
BOOL StartUnifiedDownload(HWND hDlg, const wchar_t* url);

// Progress parsing functions
BOOL ParseProgressOutput(const wchar_t* line, ProgressInfo* progress);
void FreeProgressInfo(ProgressInfo* progress);

// Main window progress bar functions
void UpdateMainProgressBar(HWND hDlg, int percentage, const wchar_t* status);
void ShowMainProgressBar(HWND hDlg, BOOL show);
void SetProgressBarMarquee(HWND hDlg, BOOL enable);
void MainWindowProgressCallback(int percentage, const wchar_t* status, void* userData);

// Multithreaded subprocess execution functions
SubprocessContext* CreateSubprocessContext(const YtDlpConfig* config, const YtDlpRequest* request, 
                                          ProgressCallback progressCallback, void* callbackUserData, HWND parentWindow);
BOOL StartSubprocessExecution(SubprocessContext* context);
BOOL IsSubprocessRunning(const SubprocessContext* context);
BOOL CancelSubprocessExecution(SubprocessContext* context);
BOOL WaitForSubprocessCompletion(SubprocessContext* context, DWORD timeoutMs);
YtDlpResult* GetSubprocessResult(SubprocessContext* context);
void FreeSubprocessContext(SubprocessContext* context);

// Convenience function for simple multithreaded execution with progress dialog
YtDlpResult* ExecuteYtDlpRequestMultithreaded(const YtDlpConfig* config, const YtDlpRequest* request, 
                                             HWND parentWindow, const wchar_t* operationTitle);

// Non-blocking download functions
BOOL StartNonBlockingDownload(YtDlpConfig* config, YtDlpRequest* request, HWND parentWindow);
void HandleDownloadCompletion(HWND hDlg, YtDlpResult* result, NonBlockingDownloadContext* downloadContext);

// Thread-safe helper functions
BOOL InitializeThreadContext(ThreadContext* threadContext);
void CleanupThreadContext(ThreadContext* threadContext);
BOOL SetCancellationFlag(ThreadContext* threadContext);
BOOL IsCancellationRequested(const ThreadContext* threadContext);

// Worker thread function
DWORD WINAPI SubprocessWorkerThread(LPVOID lpParam);

// Error logging functions
BOOL InitializeErrorLogging(void);
void LogError(const wchar_t* category, const wchar_t* message, const wchar_t* details);
void LogWarning(const wchar_t* category, const wchar_t* message);
void LogInfo(const wchar_t* category, const wchar_t* message);
void CleanupErrorLogging(void);

// Startup validation and configuration management functions
BOOL InitializeYtDlpSystem(HWND hMainWindow);
BOOL LoadYtDlpConfig(YtDlpConfig* config);
BOOL SaveYtDlpConfig(const YtDlpConfig* config);
BOOL ValidateYtDlpConfiguration(const YtDlpConfig* config, ValidationInfo* validationInfo);
BOOL MigrateYtDlpConfiguration(YtDlpConfig* config);
BOOL SetupDefaultYtDlpConfiguration(YtDlpConfig* config);
void NotifyConfigurationIssues(HWND hParent, const ValidationInfo* validationInfo);

// Global variables (extern declarations)
extern wchar_t cmdLineURL[MAX_URL_LENGTH];
extern HBRUSH hBrushWhite;
extern HBRUSH hBrushLightGreen;
extern HBRUSH hBrushLightBlue;
extern HBRUSH hBrushLightTeal;
extern HBRUSH hCurrentBrush;
extern BOOL bProgrammaticChange;
extern BOOL bManualPaste;
extern CacheManager g_cacheManager;

#endif // YOUTUBECACHER_H