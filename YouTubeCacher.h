#ifndef YOUTUBECACHER_H
#define YOUTUBECACHER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <commdlg.h>
#include <shlobj.h>
#include <commctrl.h>
#include <knownfolders.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlwapi.h>
#include <stdint.h>
#include "resource.h"
#include "cache.h"
#include "base64.h"
#include "memory.h"
#include "error.h"

// Application constants
#define APP_NAME            L"YouTube Cacher"
#define APP_VERSION         L"0.0.1"
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

// Window sizing functions are now in ui.h

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
    ERROR_TYPE_MEMORY_ALLOCATION,
    ERROR_TYPE_THREAD_CREATION,
    ERROR_TYPE_YTDLP_NOT_FOUND,
    ERROR_TYPE_YTDLP_EXECUTION,
    ERROR_TYPE_INVALID_PARAMETERS,
    ERROR_TYPE_UNKNOWN
} ErrorType;

// Detailed error information structure
typedef struct {
    ErrorType errorType;
    DWORD errorCode;           // Windows error code or custom error code
    wchar_t* operation;        // What operation was being performed
    wchar_t* details;          // Detailed error description
    wchar_t* diagnostics;      // Technical diagnostic information
    wchar_t* solutions;        // Suggested solutions
    wchar_t* context;          // Additional context (URL, file path, etc.)
} DetailedErrorInfo;

// Operation result structure
typedef struct {
    BOOL success;
    DetailedErrorInfo* errorInfo;  // NULL if success is TRUE
} OperationResult;

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

// Forward declarations and basic structures needed by modules
// Video metadata structure
typedef struct {
    wchar_t* title;
    wchar_t* duration;
    wchar_t* id;
    BOOL success;
} VideoMetadata;

// Cached video metadata structure
typedef struct {
    wchar_t* url;
    VideoMetadata metadata;
    BOOL isValid;
} CachedVideoMetadata;

// Forward declarations for structures used in headers
// SubprocessContext is defined later in this file

// Include module headers after basic type definitions
#include "appstate.h"
#include "settings.h"
#include "threading.h"
#include "threadsafe.h"

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
    
    // Accelerator key definitions (optional, NULL to use defaults)
    const wchar_t* detailsButtonText;  // e.g., "&Details >>" for Alt+D
    const wchar_t* copyButtonText;     // e.g., "&Copy" for Alt+C
    const wchar_t* okButtonText;       // e.g., "&OK" for Alt+O
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

// Thread-safe subprocess execution structures are now defined in threading.h

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

// Legacy context adapter functions for thread-safe subprocess handling
BOOL StartThreadSafeSubprocessFromLegacyContext(SubprocessContext* legacyContext);
BOOL IsLegacySubprocessRunning(const SubprocessContext* legacyContext);
BOOL WaitForLegacySubprocessCompletion(SubprocessContext* legacyContext, DWORD timeoutMs);
BOOL CancelLegacySubprocessExecution(SubprocessContext* legacyContext);
void CleanupLegacySubprocessContext(SubprocessContext* legacyContext);

// Structure for non-blocking download context
typedef struct NonBlockingDownloadContext {
    YtDlpConfig config;
    YtDlpRequest* request;
    HWND parentWindow;
    wchar_t tempDir[MAX_EXTENDED_PATH];
    wchar_t url[MAX_URL_LENGTH];
    SubprocessContext* context;
} NonBlockingDownloadContext;

// Structure for unified download context
typedef struct {
    HWND hDialog;
    wchar_t url[MAX_URL_LENGTH];
    YtDlpConfig config;
    YtDlpRequest* request;
    wchar_t tempDir[MAX_EXTENDED_PATH];
} UnifiedDownloadContext;

// Main application function prototypes (not moved to modules)
void DebugOutput(const wchar_t* message);
void WriteSessionStartToLogfile(void);
void WriteSessionEndToLogfile(const wchar_t* reason);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL RegisterMainWindowClass(HINSTANCE hInstance);
INT_PTR CALLBACK EnhancedErrorDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Download completion handling (used across modules)
void HandleDownloadCompletion(HWND hDlg, YtDlpResult* result, NonBlockingDownloadContext* downloadContext);

// YtDlp function prototypes moved to ytdlp.h
// Process management function prototypes moved to ytdlp.h

// Process status structure for detailed monitoring
typedef struct {
    DWORD processId;
    wchar_t processName[MAX_PATH];
    DWORD cpuTime;
    BOOL isResponding;
    DWORD memoryUsage;
} ProcessStatus;

// YtDlp function prototypes moved to ytdlp.h

// Registry constants for custom arguments
#define REG_CUSTOM_ARGS     L"CustomYtDlpArgs"
#define REG_ENABLE_DEBUG    L"EnableDebug"
#define REG_ENABLE_LOGFILE  L"EnableLogfile"
#define REG_ENABLE_AUTOPASTE L"EnableAutopaste"

// Enhanced error dialog function prototypes moved to ui.h

// Enhanced error dialog functions moved to ui.h

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

// Video information retrieval function prototypes moved to ytdlp.h

// Module headers are now included above after basic type definitions

// GetInfoContext structure for non-blocking info retrieval
typedef struct {
    HWND hDialog;
    wchar_t url[MAX_URL_LENGTH];
    CachedVideoMetadata* cachedMetadata;
} GetInfoContext;

// Video metadata and download function prototypes moved to ytdlp.h

// Error logging functions moved to ui.h

// Configuration management function prototypes moved to ytdlp.h



// Tab order management structures
typedef struct {
    int controlId;
    int tabOrder;
    BOOL isTabStop;
} TabOrderEntry;

typedef struct {
    TabOrderEntry* entries;
    int count;
} TabOrderConfig;

// UI Component system structures
typedef struct UIComponent UIComponent;

// Component function pointer types
typedef void (*ComponentInitFunc)(UIComponent* component, HWND parent, int x, int y);
typedef void (*ComponentDestroyFunc)(UIComponent* component);
typedef BOOL (*ComponentValidateFunc)(UIComponent* component, wchar_t* errorMsg, size_t errorMsgSize);
typedef void (*ComponentGetValueFunc)(UIComponent* component, void* value);
typedef void (*ComponentSetValueFunc)(UIComponent* component, const void* value);

// Base component structure
struct UIComponent {
    HWND hwndContainer;
    HWND* childControls;
    int childCount;
    
    ComponentInitFunc init;
    ComponentDestroyFunc destroy;
    ComponentValidateFunc validate;
    ComponentGetValueFunc getValue;
    ComponentSetValueFunc setValue;
    
    void* userData;
};

// Component registry structure
typedef struct {
    UIComponent** components;
    int count;
    int capacity;
} ComponentRegistry;

// File browser component structure
typedef struct {
    UIComponent base;
    HWND hwndLabel;
    HWND hwndEdit;
    HWND hwndButton;
    wchar_t* label;
    wchar_t* filter;  // e.g., L"Executables\0*.exe\0All Files\0*.*\0"
    wchar_t* currentPath;
    int controlId;    // Base control ID for this component
} FileBrowserComponent;

// Folder browser component structure
typedef struct {
    UIComponent base;
    HWND hwndLabel;
    HWND hwndEdit;
    HWND hwndButton;
    wchar_t* label;
    wchar_t* currentPath;
    int controlId;    // Base control ID for this component
} FolderBrowserComponent;

// Validation types for labeled text input
typedef enum {
    VALIDATION_NONE,
    VALIDATION_REQUIRED,
    VALIDATION_NUMERIC,
    VALIDATION_PATH,
    VALIDATION_URL,
    VALIDATION_CUSTOM
} ValidationType;

// Custom validation function type
typedef BOOL (*CustomValidationFunc)(const wchar_t* value, wchar_t* errorMsg, size_t errorMsgSize);

// Labeled text input component structure
typedef struct {
    UIComponent base;
    HWND hwndLabel;
    HWND hwndEdit;
    HWND hwndError;
    wchar_t* label;
    ValidationType validationType;
    CustomValidationFunc customValidator;
    BOOL isRequired;
    int controlId;    // Base control ID for this component
} LabeledTextInput;

// Button layout types
typedef enum {
    BUTTON_LAYOUT_OK,
    BUTTON_LAYOUT_OK_CANCEL,
    BUTTON_LAYOUT_YES_NO,
    BUTTON_LAYOUT_YES_NO_CANCEL,
    BUTTON_LAYOUT_CUSTOM
} ButtonLayout;

// Button configuration structure
typedef struct {
    const wchar_t* text;
    int controlId;
    BOOL isDefault;
} ButtonConfig;

// Button row component structure
typedef struct {
    UIComponent base;
    HWND* buttons;
    int buttonCount;
    ButtonLayout layout;
    int controlId;    // Base control ID for this component
} ButtonRowComponent;

// Validation framework structures
typedef struct {
    UIComponent* component;
    BOOL isValid;
    wchar_t errorMessage[256];
} ComponentValidationResult;

typedef struct {
    ComponentValidationResult* results;
    int count;
    BOOL allValid;
} ComponentValidationSummary;

// Font management structures
typedef struct {
    HFONT hFont;
    int pointSize;  // Logical point size
    int dpi;        // DPI this font was created for
    LOGFONTW logFont;
} ScalableFont;

typedef struct {
    ScalableFont** fonts;
    int count;
    int capacity;
} FontManager;

// Icon management structures
typedef struct {
    int size;
    HICON hIcon;
} IconSize;

typedef struct {
    int resourceId;
    IconSize* sizes;
    int sizeCount;
    int sizeCapacity;
} ScalableIcon;

typedef struct {
    ScalableIcon** icons;
    int count;
    int capacity;
} IconManager;

// DPI management structures
typedef struct {
    HWND hwnd;
    int currentDpi;
    int baseDpi;  // Always 96
    double scaleFactor;
    RECT logicalRect;  // Window rect in logical coordinates
    FontManager* fontManager;
    IconManager* iconManager;
} DPIContext;

// Global DPI manager
typedef struct {
    DPIContext* mainWindow;
    DPIContext** dialogs;
    int dialogCount;
    int dialogCapacity;
    CRITICAL_SECTION lock;  // Thread safety
} DPIManager;

// Global DPI manager instance
extern DPIManager* g_dpiManager;

// Include other headers after all type definitions to avoid circular dependencies
#include "uri.h"
#include "parser.h"
#include "log.h"
#include "ytdlp.h"
#include "ui.h"
#include "accessibility.h"
#include "keyboard.h"
#include "components.h"
#include "dpi.h"

#endif // YOUTUBECACHER_H