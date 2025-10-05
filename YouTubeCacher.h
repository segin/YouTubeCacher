#ifndef YOUTUBECACHER_H
#define YOUTUBECACHER_H

#include <windows.h>
#include "resource.h"

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

// Window sizing constants
#define MIN_WINDOW_WIDTH    500
#define MIN_WINDOW_HEIGHT   350
#define DEFAULT_WIDTH       550
#define DEFAULT_HEIGHT      420

// Layout constants
#define BUTTON_PADDING      80
#define BUTTON_WIDTH        70
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

// Function prototypes
void CheckClipboardForYouTubeURL(HWND hDlg);
void ResizeControls(HWND hDlg);
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

// Error analysis functions
ErrorAnalysis* AnalyzeYtDlpError(const YtDlpResult* result);
void FreeErrorAnalysis(ErrorAnalysis* analysis);

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

// Global variables (extern declarations)
extern wchar_t cmdLineURL[MAX_URL_LENGTH];
extern HBRUSH hBrushWhite;
extern HBRUSH hBrushLightGreen;
extern HBRUSH hBrushLightBlue;
extern HBRUSH hBrushLightTeal;
extern HBRUSH hCurrentBrush;
extern BOOL bProgrammaticChange;
extern BOOL bManualPaste;

#endif // YOUTUBECACHER_H