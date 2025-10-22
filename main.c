#include "YouTubeCacher.h"

// MinGW32 provides _wcsdup and _wcsicmp, no custom implementation needed

// Subclass procedure for text field to detect paste operations
LRESULT CALLBACK TextFieldSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PASTE:
            // User is manually pasting - set flag
            SetManualPasteFlag(TRUE);
            break;
            
        case WM_KEYDOWN:
            // Check for Ctrl+V
            if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                SetManualPasteFlag(TRUE);
            }
            break;
    }
    
    // Call original window procedure
    return CallWindowProc(GetOriginalTextFieldProc(), hwnd, uMsg, wParam, lParam);
}



// WriteToLogfile moved to log.c

// WriteSessionStartToLogfile moved to log.c

// WriteSessionEndToLogfile moved to log.c

// DebugOutput moved to log.c



// Function to update debug control visibility
void UpdateDebugControlVisibility(HWND hDlg) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    int showState = enableDebug ? SW_SHOW : SW_HIDE;
    
    // Show/hide Add button
    ShowWindow(GetDlgItem(hDlg, IDC_BUTTON1), showState);
    
    // Show/hide color buttons
    ShowWindow(GetDlgItem(hDlg, IDC_COLOR_GREEN), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_COLOR_TEAL), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_COLOR_BLUE), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_COLOR_WHITE), showState);
}

// InstallYtDlpWithWinget moved to ytdlp.c



// ValidateYtDlpExecutable moved to ytdlp.c







// Stub implementations for YtDlp Manager system functions
// These are placeholders for the actual implementations from tasks 1-7

// InitializeYtDlpConfig moved to ytdlp.c

// CleanupYtDlpConfig moved to ytdlp.c

// ValidateYtDlpComprehensive moved to ytdlp.c

// FreeValidationInfo moved to ytdlp.c

// CreateYtDlpRequest and FreeYtDlpRequest moved to ytdlp.c

// CreateTempDirectory moved to ytdlp.c

// CreateYtDlpTempDirWithFallback moved to ytdlp.c

// CleanupTempDirectory moved to ytdlp.c

// CreateProgressDialog moved to ytdlp.c

// UpdateProgressDialog moved to ytdlp.c

// IsProgressDialogCancelled moved to ytdlp.c

// DestroyProgressDialog moved to ytdlp.c

// ExecuteYtDlpRequest moved to ytdlp.c

// Multithreaded subprocess execution implementation

// InitializeThreadContext moved to threading.c

// CleanupThreadContext moved to threading.c

// SetCancellationFlag moved to threading.c

// IsCancellationRequested moved to threading.c

// SubprocessWorkerThread moved to ytdlp.c

// CreateSubprocessContext moved to ytdlp.c

// StartSubprocessExecution moved to ytdlp.c

// IsSubprocessRunning moved to ytdlp.c

// CancelSubprocessExecution moved to ytdlp.c

// WaitForSubprocessCompletion moved to ytdlp.c

// Get subprocess result (transfers ownership to caller)
YtDlpResult* GetSubprocessResult(SubprocessContext* context) {
    OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - ENTRY\n");
    
    if (!context) {
        OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - NULL context, returning NULL\n");
        return NULL;
    }
    
    if (!context->completed) {
        OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - Context not completed, returning NULL\n");
        return NULL;
    }
    
    OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - Context is completed\n");
    
    if (!context->result) {
        OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - Context result is NULL, returning NULL\n");
        return NULL;
    }
    
    wchar_t debugMsg[256];
    swprintf(debugMsg, 256, L"YouTubeCacher: GetSubprocessResult - Transferring result: success=%d, exitCode=%d\n", 
            context->result->success, context->result->exitCode);
    OutputDebugStringW(debugMsg);
    
    YtDlpResult* result = context->result;
    context->result = NULL; // Transfer ownership
    
    OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - Result transferred successfully\n");
    return result;
}

// FreeSubprocessContext moved to ytdlp.c

// SubprocessProgressCallback moved to threading.c

// ExecuteYtDlpRequestMultithreaded moved to ytdlp.c



// Custom window messages
#define WM_DOWNLOAD_COMPLETE (WM_USER + 102)
#define WM_UNIFIED_DOWNLOAD_UPDATE (WM_USER + 113)

// UnifiedDownloadContext moved to YouTubeCacher.h

// UnifiedDownloadWorkerThread moved to ytdlp.c

// UnifiedDownloadProgressCallback moved to threading.c

// StartUnifiedDownload moved to ytdlp.c

// NonBlockingDownloadThread moved to ytdlp.c

// StartNonBlockingDownload moved to ytdlp.c

// HandleDownloadCompletion moved to threading.c

// Structure for passing data to the video info thread
typedef struct {
    HWND hDlg;
    wchar_t url[MAX_URL_LENGTH];
    wchar_t title[512];
    wchar_t duration[64];
    BOOL success;
    HANDLE hThread;
    DWORD threadId;
} VideoInfoThreadData;

// GetVideoInfoThread moved to ytdlp.c

// Synchronous version of the video info function (renamed from original)
// Structure for concurrent video info retrieval
typedef struct {
    YtDlpConfig* config;
    YtDlpRequest* request;
    YtDlpResult* result;
    HANDLE hThread;
    DWORD threadId;
    BOOL completed;
} VideoInfoThread;

// VideoInfoWorkerThread moved to ytdlp.c

// GetVideoTitleAndDurationSync moved to ytdlp.c

// GetVideoTitleAndDuration moved to ytdlp.c

// Update the video info UI fields with title and duration
void UpdateVideoInfoUI(HWND hDlg, const wchar_t* title, const wchar_t* duration) {
    if (!hDlg) return;
    
    // Update video title field
    if (title && wcslen(title) > 0) {
        // Debug: Log the title string being set
        wchar_t debugMsg[1024];
        swprintf(debugMsg, 1024, L"YouTubeCacher: Setting title in UI: %ls (length: %zu)\n", title, wcslen(title));
        OutputDebugStringW(debugMsg);
        
        // Debug: Log individual characters
        OutputDebugStringW(L"YouTubeCacher: Title character codes: ");
        for (size_t i = 0; i < min(wcslen(title), 20); i++) {
            swprintf(debugMsg, 1024, L"U+%04X ", (unsigned int)title[i]);
            OutputDebugStringW(debugMsg);
        }
        OutputDebugStringW(L"\n");
        
        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, title);
    } else {
        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, L"Title not available");
    }
    
    // Update video duration field
    if (duration && wcslen(duration) > 0) {
        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, duration);
    } else {
        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, L"Unknown");
    }
    
    // Force redraw of the updated fields
    InvalidateRect(GetDlgItem(hDlg, IDC_VIDEO_TITLE), NULL, TRUE);
    InvalidateRect(GetDlgItem(hDlg, IDC_VIDEO_DURATION), NULL, TRUE);
}

// Enable/disable UI controls during download operations
void SetDownloadUIState(HWND hDlg, BOOL isDownloading) {
    if (!hDlg) return;
    
    // Disable/enable URL input field
    EnableWindow(GetDlgItem(hDlg, IDC_TEXT_FIELD), !isDownloading);
    
    // Disable/enable Get Info button
    EnableWindow(GetDlgItem(hDlg, IDC_GETINFO_BTN), !isDownloading);
    
    // Disable/enable Download button
    EnableWindow(GetDlgItem(hDlg, IDC_DOWNLOAD_BTN), !isDownloading);
    
    // Update global state
    SetDownloadingState(isDownloading);
}

// Update the main window's progress bar instead of using separate dialogs
void UpdateMainProgressBar(HWND hDlg, int percentage, const wchar_t* status) {
    if (!hDlg) return;
    
    // Update the progress bar
    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
    if (hProgressBar) {
        // Check if we're in marquee mode
        LONG style = GetWindowLongW(hProgressBar, GWL_STYLE);
        BOOL isMarquee = (style & PBS_MARQUEE) != 0;
        
        // If we have a valid percentage (> 0) and we're in marquee mode, switch to normal mode
        if (percentage > 0 && isMarquee) {
            SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
            SetWindowLongW(hProgressBar, GWL_STYLE, style & ~PBS_MARQUEE);
            SendMessageW(hProgressBar, PBM_SETPOS, percentage, 0);
        }
        // If we have a valid percentage and we're not in marquee mode, just update position
        else if (percentage > 0 && !isMarquee) {
            SendMessageW(hProgressBar, PBM_SETPOS, percentage, 0);
        }
        // If percentage is 0 or negative, don't interfere with marquee mode
        // (Let the caller explicitly manage marquee mode)
        else if (!isMarquee) {
            SendMessageW(hProgressBar, PBM_SETPOS, percentage, 0);
        }
        
        // Enable/show the progress bar during operations
        EnableWindow(hProgressBar, TRUE);
        ShowWindow(hProgressBar, SW_SHOW);
    }
    
    // Update the progress text if we have it
    HWND hProgressText = GetDlgItem(hDlg, IDC_PROGRESS_TEXT);
    if (hProgressText && status) {
        SetWindowTextW(hProgressText, status);
    }
    
    // Force UI update
    UpdateWindow(hDlg);
}

// Show or hide the main window's progress bar
void ShowMainProgressBar(HWND hDlg, BOOL show) {
    if (!hDlg) return;
    
    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
    if (hProgressBar) {
        ShowWindow(hProgressBar, show ? SW_SHOW : SW_HIDE);
        EnableWindow(hProgressBar, show);
        
        if (!show) {
            // Stop marquee and reset progress bar when hiding
            LONG style = GetWindowLongW(hProgressBar, GWL_STYLE);
            if (style & PBS_MARQUEE) {
                SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
                SetWindowLongW(hProgressBar, GWL_STYLE, style & ~PBS_MARQUEE);
            }
            SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);
        }
    }
    
    // Clear progress text when hiding
    HWND hProgressText = GetDlgItem(hDlg, IDC_PROGRESS_TEXT);
    if (hProgressText) {
        SetWindowTextW(hProgressText, show ? L"" : L"");
    }
}

// Set progress bar to marquee mode for indefinite operations (only if not already marqueeing)
void SetProgressBarMarquee(HWND hDlg, BOOL enable) {
    if (!hDlg) return;
    
    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
    if (!hProgressBar) return;
    
    LONG style = GetWindowLongW(hProgressBar, GWL_STYLE);
    BOOL isCurrentlyMarquee = (style & PBS_MARQUEE) != 0;
    
    if (enable && !isCurrentlyMarquee) {
        // Enable marquee mode
        SetWindowLongW(hProgressBar, GWL_STYLE, style | PBS_MARQUEE);
        SendMessageW(hProgressBar, PBM_SETMARQUEE, TRUE, 50); // 50ms animation speed
        DebugOutput(L"YouTubeCacher: Progress bar set to marquee mode");
    } else if (!enable && isCurrentlyMarquee) {
        // Disable marquee mode
        SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
        SetWindowLongW(hProgressBar, GWL_STYLE, style & ~PBS_MARQUEE);
        SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);
        DebugOutput(L"YouTubeCacher: Progress bar marquee mode disabled");
    }
    // If already in the requested state, do nothing (don't reset)
}

// MainWindowProgressCallback moved to threading.c

// FreeYtDlpResult, AnalyzeYtDlpError, and FreeErrorAnalysis moved to ytdlp.c

// ValidateYtDlpArguments and SanitizeYtDlpArguments moved to ytdlp.c

// GetYtDlpArgsForOperation moved to ytdlp.c

// Video metadata functions moved to ytdlp.c

// GetVideoMetadata moved to ytdlp.c

// Cached metadata functions moved to ytdlp.c

// Non-blocking Get Info functions moved to ytdlp.c



// Progress parsing functions moved to ytdlp.c

// Startup validation and configuration management functions

BOOL InitializeYtDlpSystem(HWND hMainWindow) {
    // Load configuration from registry
    YtDlpConfig config;
    if (!LoadYtDlpConfig(&config)) {
        // Failed to load configuration, set up defaults
        if (!SetupDefaultYtDlpConfiguration(&config)) {
            ShowConfigurationError(hMainWindow, L"Failed to initialize yt-dlp configuration with default values.");
            return FALSE;
        }
        
        // Save the default configuration
        SaveYtDlpConfig(&config);
    }
    
    // Validate the loaded/default configuration
    ValidationInfo validationInfo;
    if (!ValidateYtDlpConfiguration(&config, &validationInfo)) {
        // Configuration validation failed - notify user
        NotifyConfigurationIssues(hMainWindow, &validationInfo);
        FreeValidationInfo(&validationInfo);
        return FALSE;
    }
    
    FreeValidationInfo(&validationInfo);
    return TRUE;
}

// LoadYtDlpConfig and SaveYtDlpConfig moved to ytdlp.c

BOOL TestYtDlpFunctionality(const wchar_t* path) {
    if (!path || wcslen(path) == 0) return FALSE;
    
    // Build command line to test yt-dlp version
    size_t cmdLineLen = wcslen(path) + 20;
    wchar_t* cmdLine = (wchar_t*)malloc(cmdLineLen * sizeof(wchar_t));
    if (!cmdLine) return FALSE;
    
    swprintf(cmdLine, cmdLineLen, L"\"%ls\" --version", path);
    
    // Create pipes for output capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        free(cmdLine);
        return FALSE;
    }
    
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    
    // Set up process creation
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;
    
    PROCESS_INFORMATION pi = {0};
    
    // Create process
    BOOL processCreated = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    
    if (!processCreated) {
        free(cmdLine);
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return FALSE;
    }
    
    CloseHandle(hWrite);
    
    // Wait for process completion with timeout
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 10000); // 10 second timeout
    
    BOOL success = FALSE;
    if (waitResult == WAIT_OBJECT_0) {
        // Process completed, check exit code
        DWORD exitCode;
        if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode == 0) {
            success = TRUE;
        }
    } else {
        // Process timed out or failed, terminate it
        TerminateProcess(pi.hProcess, 1);
    }
    
    // Cleanup
    free(cmdLine);
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return success;
}

BOOL ValidateYtDlpConfiguration(const YtDlpConfig* config, ValidationInfo* validationInfo) {
    if (!config || !validationInfo) return FALSE;
    
    // Initialize validation info
    memset(validationInfo, 0, sizeof(ValidationInfo));
    
    // Check if yt-dlp path is set and valid
    if (wcslen(config->ytDlpPath) == 0) {
        validationInfo->result = VALIDATION_NOT_FOUND;
        validationInfo->errorDetails = _wcsdup(L"yt-dlp path is not configured");
        validationInfo->suggestions = _wcsdup(L"Please configure the yt-dlp path in File > Settings");
        return FALSE;
    }
    
    // Validate the yt-dlp executable
    if (!ValidateYtDlpExecutable(config->ytDlpPath)) {
        validationInfo->result = VALIDATION_NOT_EXECUTABLE;
        validationInfo->errorDetails = _wcsdup(L"yt-dlp executable not found or not accessible");
        validationInfo->suggestions = _wcsdup(L"Please check the yt-dlp path in File > Settings and ensure the file exists and is executable");
        return FALSE;
    }
    
    // Skip functionality test during startup - it will be tested when actually used
    
    // Validate temporary directory access
    if (wcslen(config->defaultTempDir) > 0) {
        DWORD attributes = GetFileAttributesW(config->defaultTempDir);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            // Try to create the directory
            if (!CreateDirectoryW(config->defaultTempDir, NULL)) {
                validationInfo->result = VALIDATION_PERMISSION_DENIED;
                validationInfo->errorDetails = _wcsdup(L"Default temporary directory is not accessible");
                validationInfo->suggestions = _wcsdup(L"Please check permissions for the temporary directory or choose a different location");
                return FALSE;
            }
        }
    }
    
    // Validate custom arguments if present
    if (wcslen(config->defaultArgs) > 0) {
        if (!ValidateYtDlpArguments(config->defaultArgs)) {
            validationInfo->result = VALIDATION_PERMISSION_DENIED;
            validationInfo->errorDetails = _wcsdup(L"Custom yt-dlp arguments contain potentially dangerous options");
            validationInfo->suggestions = _wcsdup(L"Please remove --exec, --batch-file, or other potentially harmful arguments from custom arguments");
            return FALSE;
        }
    }
    
    // All validation passed
    validationInfo->result = VALIDATION_OK;
    validationInfo->version = _wcsdup(L"Configuration validated successfully");
    return TRUE;
}

BOOL MigrateYtDlpConfiguration(YtDlpConfig* config) {
    if (!config) return FALSE;
    
    BOOL migrationPerformed = FALSE;
    
    // Check if this is an old configuration that needs migration
    // For now, we'll just ensure all fields have reasonable defaults
    
    // Migrate timeout if it's unreasonable
    if (config->timeoutSeconds < 30 || config->timeoutSeconds > 3600) {
        config->timeoutSeconds = 300; // 5 minutes default
        migrationPerformed = TRUE;
    }
    
    // Migrate temp directory strategy if invalid
    if (config->tempDirStrategy > TEMP_DIR_APPDATA) {
        config->tempDirStrategy = TEMP_DIR_SYSTEM;
        migrationPerformed = TRUE;
    }
    
    // Ensure temp directory is set
    if (wcslen(config->defaultTempDir) == 0) {
        DWORD result = GetTempPathW(MAX_EXTENDED_PATH, config->defaultTempDir);
        if (result == 0 || result >= MAX_EXTENDED_PATH) {
            wcscpy(config->defaultTempDir, L"C:\\Temp\\");
        }
        migrationPerformed = TRUE;
    }
    
    // If migration was performed, save the updated configuration
    if (migrationPerformed) {
        SaveYtDlpConfig(config);
    }
    
    return TRUE;
}

BOOL SetupDefaultYtDlpConfiguration(YtDlpConfig* config) {
    if (!config) return FALSE;
    
    // Initialize all fields to default values
    memset(config, 0, sizeof(YtDlpConfig));
    
    // Set default yt-dlp path
    GetDefaultYtDlpPath(config->ytDlpPath, MAX_EXTENDED_PATH);
    
    // Set default temporary directory
    DWORD result = GetTempPathW(MAX_EXTENDED_PATH, config->defaultTempDir);
    if (result == 0 || result >= MAX_EXTENDED_PATH) {
        wcscpy(config->defaultTempDir, L"C:\\Temp\\");
    }
    
    // Set default arguments (empty)
    config->defaultArgs[0] = L'\0';
    
    // Set default timeout (5 minutes)
    config->timeoutSeconds = 300;
    
    // Set default boolean options
    config->enableVerboseLogging = FALSE;
    config->autoRetryOnFailure = FALSE;
    
    // Set default temp directory strategy
    config->tempDirStrategy = TEMP_DIR_SYSTEM;
    
    return TRUE;
}

void NotifyConfigurationIssues(HWND hParent, const ValidationInfo* validationInfo) {
    if (!validationInfo) return;
    
    wchar_t title[256];
    wchar_t message[1024];
    
    switch (validationInfo->result) {
        case VALIDATION_NOT_FOUND:
            wcscpy(title, L"yt-dlp Not Found");
            swprintf(message, 1024, L"yt-dlp could not be found.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please check your configuration");
            break;
            
        case VALIDATION_NOT_EXECUTABLE:
            wcscpy(title, L"yt-dlp Not Executable");
            swprintf(message, 1024, L"yt-dlp executable is not valid or accessible.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please check your configuration");
            break;
            
        case VALIDATION_MISSING_DEPENDENCIES:
            wcscpy(title, L"yt-dlp Dependencies Missing");
            swprintf(message, 1024, L"yt-dlp is installed but missing required dependencies.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please install Python and yt-dlp dependencies");
            break;
            
        case VALIDATION_VERSION_INCOMPATIBLE:
            wcscpy(title, L"yt-dlp Version Incompatible");
            swprintf(message, 1024, L"yt-dlp version is not compatible.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please update yt-dlp");
            break;
            
        case VALIDATION_PERMISSION_DENIED:
            wcscpy(title, L"Configuration Permission Error");
            swprintf(message, 1024, L"Configuration has permission or security issues.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please check permissions");
            break;
            
        default:
            wcscpy(title, L"Configuration Error");
            swprintf(message, 1024, L"An unknown configuration error occurred.\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Please check your yt-dlp configuration");
            break;
    }
    
    MessageBoxW(hParent, message, title, MB_OK | MB_ICONWARNING);
}





void CheckClipboardForYouTubeURL(HWND hDlg) {
    // Check if autopaste is enabled
    if (!GetAutopasteState()) {
        return; // Autopaste is disabled
    }
    
    // Only check clipboard if text field is empty
    wchar_t currentText[MAX_BUFFER_SIZE];
    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, currentText, MAX_BUFFER_SIZE);
    
    if (wcslen(currentText) == 0) {
        if (OpenClipboard(hDlg)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData != NULL) {
                wchar_t* clipText = (wchar_t*)GlobalLock(hData);
                if (clipText != NULL && IsYouTubeURL(clipText)) {
                    SetProgrammaticChangeFlag(TRUE);
                    SetDlgItemTextW(hDlg, IDC_TEXT_FIELD, clipText);
                    SetCurrentBrush(GetBrush(BRUSH_LIGHT_GREEN));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    SetProgrammaticChangeFlag(FALSE);
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
    }
}

// Function to calculate minimum window dimensions based on DPI and content requirements
void CalculateMinimumWindowSize(int* minWidth, int* minHeight, double dpiScaleX, double dpiScaleY) {
    if (!minWidth || !minHeight) return;
    
    // Base measurements in logical units (96 DPI)
    const int BASE_MARGIN = 10;
    const int BASE_WINDOW_MARGIN = 10;
    const int BASE_BUTTON_WIDTH = 78;
    const int BASE_TEXT_HEIGHT = 22;
    const int BASE_LABEL_HEIGHT = 16;
    const int BASE_PROGRESS_HEIGHT = 16;
    const int BASE_GROUP_TITLE_HEIGHT = 18;
    const int BASE_LIST_MIN_HEIGHT = 100;  // Minimum height for the offline videos list
    const int BASE_SIDE_BUTTON_HEIGHT = 32;
    
    // Scale measurements to current DPI
    int margin = (int)(BASE_MARGIN * dpiScaleX);
    int windowMargin = (int)(BASE_WINDOW_MARGIN * dpiScaleX);
    int buttonWidth = (int)(BASE_BUTTON_WIDTH * dpiScaleX);
    int textHeight = (int)(BASE_TEXT_HEIGHT * dpiScaleY);
    int labelHeight = (int)(BASE_LABEL_HEIGHT * dpiScaleY);
    int progressHeight = (int)(BASE_PROGRESS_HEIGHT * dpiScaleY);
    int groupTitleHeight = (int)(BASE_GROUP_TITLE_HEIGHT * dpiScaleY);
    int listMinHeight = (int)(BASE_LIST_MIN_HEIGHT * dpiScaleY);
    int sideButtonHeight = (int)(BASE_SIDE_BUTTON_HEIGHT * dpiScaleY);
    
    // Calculate minimum width requirements
    // Logic: Window margins (20) + text field min width (200) + gap (10) + button width (78) + margin (10) = 318
    int minTextFieldWidth = (int)(200 * dpiScaleX);  // Minimum usable text field width
    int minContentWidth = minTextFieldWidth + margin + buttonWidth + margin;  // Content within group
    int totalMinWidth = (2 * windowMargin) + minContentWidth + (2 * margin);  // Add group margins
    
    // Ensure minimum for UI elements like labels
    int minUIWidth = (int)(400 * dpiScaleX);  // Absolute minimum for UI readability
    *minWidth = (totalMinWidth > minUIWidth) ? totalMinWidth : minUIWidth;
    
    // Calculate minimum height requirements with detailed breakdown
    
    // DOWNLOAD GROUP HEIGHT CALCULATION (130px at 96 DPI) - TWO-LINE VIDEO INFO:
    // - Group title area: 18px (group box border + "Download video" text)
    // - Top margin: 10px (breathing room after title)
    // - URL input row: 22px (standard edit control height)
    // - Reduced margin: 8px (3/4 × 10px for spacing before progress bar)
    // - Progress bar: 16px (standard progress control height, restored to original position)
    // - Reduced margin: 8px (space before video info section)
    // - Video title line: 16px (title text, truncates before "Get Info" button)
    // - Small margin: 6px (1/2 × 10px between title and duration lines)
    // - Duration + status line: 16px (duration + download progress on same line)
    // - Bottom margin: 10px (separation from offline videos group)
    int downloadGroupHeight = groupTitleHeight +           // 18px: group title space
                             margin +                      // 10px: top margin
                             textHeight +                  // 22px: URL field
                             (margin * 3/4) +             //  8px: reduced margin
                             progressHeight +              // 16px: progress bar
                             (margin * 3/4) +             //  8px: reduced margin
                             labelHeight +                 // 16px: video title line
                             (margin / 2) +               //  6px: small margin
                             labelHeight +                 // 16px: duration + status line
                             margin;                       // 10px: bottom margin
                                                          // Total: 130px
    
    // OFFLINE VIDEOS GROUP MINIMUM HEIGHT CALCULATION (159px at 96 DPI):
    // - Group title area: 18px (group box border + "Offline videos" text)
    // - Small margin: 5px (1/2 × 10px for tight spacing after title)
    // - Status labels row: 16px (shows "Status: Ready" and "Items: 0")
    // - Standard margin: 10px (separation before main list content)
    // - Minimum list height: 100px (enough to display 4-5 video entries comfortably)
    // - Bottom margin: 10px (space from window bottom edge)
    int offlineGroupMinHeight = groupTitleHeight +         // 18px: group title space
                               (margin / 2) +             //  5px: small top margin
                               labelHeight +               // 16px: status labels
                               margin +                    // 10px: standard margin
                               listMinHeight +             // 100px: minimum list space
                               margin;                     // 10px: bottom margin
                                                          // Total: 159px
    
    // SIDE BUTTON SPACE VALIDATION:
    // Ensure offline group can accommodate side buttons (Play/Delete)
    // - Two buttons: 2 × 32px = 64px
    // - Spacing between: 5px (1/2 × 10px)
    // - Total button area needed: 69px
    // Since our list minimum (100px) > button area (69px), we have adequate space
    int minSideButtonSpace = (2 * sideButtonHeight) + (margin / 2);  // 64 + 5 = 69px
    if (offlineGroupMinHeight < (groupTitleHeight + (margin / 2) + labelHeight + margin + minSideButtonSpace + margin)) {
        // This should never trigger with our current calculations, but provides safety
        offlineGroupMinHeight = groupTitleHeight + (margin / 2) + labelHeight + margin + minSideButtonSpace + margin;
    }
    
    // TOTAL WINDOW HEIGHT CALCULATION:
    // - Top window margin: 10px (space from window top edge)
    // - Download group: 136px (calculated above)
    // - Inter-group margin: 10px (visual separation between main sections)
    // - Offline group: 159px (calculated above)
    // - Bottom window margin: 10px (space from window bottom edge)
    // - Content subtotal: 325px
    // - Window chrome: ~60px (title bar, menu bar, window borders - OS dependent)
    // - Total minimum: ~385px
    *minHeight = windowMargin +                    // 10px: top margin
                downloadGroupHeight +              // 136px: download section
                margin +                           // 10px: inter-group spacing
                offlineGroupMinHeight +            // 159px: offline section
                windowMargin;                      // 10px: bottom margin
                                                  // Subtotal: 325px
    
    // Add window chrome space (title bar, menu, borders)
    // This varies by Windows version and theme, but ~60px is typical
    *minHeight += (int)(60 * dpiScaleY);          // 60px: window chrome
                                                  // Final total: ~385px at 96 DPI
}

// Function to calculate optimal default window dimensions based on DPI and content requirements
void CalculateDefaultWindowSize(int* defaultWidth, int* defaultHeight, double dpiScaleX, double dpiScaleY) {
    if (!defaultWidth || !defaultHeight) return;
    
    // Start with minimum size as baseline
    CalculateMinimumWindowSize(defaultWidth, defaultHeight, dpiScaleX, dpiScaleY);
    
    // Add comfortable extra space for better user experience
    
    // Width reasoning:
    // - Keep minimum width as default - user can resize window if needed for long titles
    // - No extra horizontal space required since video info wraps appropriately
    // - Focus on vertical space for better video list viewing instead
    int extraWidth = (int)(50 * dpiScaleX);   // Add minimal 50px for slightly more comfortable text field
    *defaultWidth += extraWidth;
    
    // Height reasoning:
    // - Minimum gives us ~100px list height (shows ~3-4 items)
    // - For default, target ~200px list height (shows ~8-10 items comfortably)
    // - Users typically have multiple downloaded videos, need to see more at once
    int extraHeight = (int)(120 * dpiScaleY);  // Add 120px for more comfortable list viewing
    *defaultHeight += extraHeight;
    
    // Ensure we don't exceed reasonable screen space (80% of typical small screen)
    int maxReasonableWidth = (int)(1090 * dpiScaleX);   // 80% of 1366px width
    int maxReasonableHeight = (int)(614 * dpiScaleY);   // 80% of 768px height
    
    if (*defaultWidth > maxReasonableWidth) {
        *defaultWidth = maxReasonableWidth;
    }
    if (*defaultHeight > maxReasonableHeight) {
        *defaultHeight = maxReasonableHeight;
    }
}

// Apply modern Windows theming to dialog and its controls
void ApplyModernThemeToDialog(HWND hDlg) {
    if (!hDlg) return;
    
    // Load UxTheme library for theming functions
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!hUxTheme) return;
    
    // Define constants that might not be available in older headers
    #ifndef ETDT_ENABLETAB
    #define ETDT_ENABLETAB 0x00000006
    #endif
    #ifndef ETDT_USETABTEXTURE
    #define ETDT_USETABTEXTURE 0x00000004
    #endif
    
    // Function pointers for theming APIs
    typedef BOOL (WINAPI *EnableThemeDialogTextureFunc)(HWND, DWORD);
    typedef HRESULT (WINAPI *SetWindowThemeFunc)(HWND, LPCWSTR, LPCWSTR);
    typedef BOOL (WINAPI *IsThemeActiveFunc)(void);
    typedef BOOL (WINAPI *IsAppThemedFunc)(void);
    
    EnableThemeDialogTextureFunc EnableThemeDialogTexture = 
        (EnableThemeDialogTextureFunc)(void*)GetProcAddress(hUxTheme, "EnableThemeDialogTexture");
    SetWindowThemeFunc SetWindowTheme = 
        (SetWindowThemeFunc)(void*)GetProcAddress(hUxTheme, "SetWindowTheme");
    IsThemeActiveFunc IsThemeActive = 
        (IsThemeActiveFunc)(void*)GetProcAddress(hUxTheme, "IsThemeActive");
    IsAppThemedFunc IsAppThemed = 
        (IsAppThemedFunc)(void*)GetProcAddress(hUxTheme, "IsAppThemed");
    
    // Only apply theming if themes are active and app is themed
    if (IsThemeActive && IsThemeActive() && IsAppThemed && IsAppThemed()) {
        // Enable dialog texture with both tab texture and tab support
        if (EnableThemeDialogTexture) {
            EnableThemeDialogTexture(hDlg, ETDT_ENABLETAB | ETDT_USETABTEXTURE);
        }
        
        // Apply modern theme to specific control types
        if (SetWindowTheme) {
            // Theme the dialog itself first
            SetWindowTheme(hDlg, L"Explorer", NULL);
            
            // Find and theme all child controls recursively
            HWND hChild = GetWindow(hDlg, GW_CHILD);
            while (hChild) {
                wchar_t className[256];
                if (GetClassNameW(hChild, className, 256)) {
                    BOOL themed = FALSE;
                    
                    // Apply specific themes based on control type
                    if (wcscmp(className, L"Button") == 0) {
                        // Check if it's a group box (different styling)
                        LONG style = GetWindowLong(hChild, GWL_STYLE);
                        if ((style & BS_GROUPBOX) == BS_GROUPBOX) {
                            SetWindowTheme(hChild, L"Explorer", NULL);
                        } else {
                            SetWindowTheme(hChild, L"Explorer", NULL);
                        }
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"Edit") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ListBox") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ComboBox") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"msctls_progress32") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"SysTabControl32") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"Static") == 0) {
                        // For group boxes and other static controls
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ScrollBar") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    
                    // Force redraw if we applied theming
                    if (themed) {
                        InvalidateRect(hChild, NULL, TRUE);
                        UpdateWindow(hChild);
                    }
                }
                hChild = GetWindow(hChild, GW_HWNDNEXT);
            }
        }
        
        // Force redraw of the entire dialog
        InvalidateRect(hDlg, NULL, TRUE);
        UpdateWindow(hDlg);
    }
    
    FreeLibrary(hUxTheme);
}

// Apply theming with a slight delay to ensure all controls are ready
void ApplyDelayedTheming(HWND hDlg) {
    if (!hDlg) return;
    
    // Use a timer to apply theming after the dialog is fully initialized
    SetTimer(hDlg, 9999, 100, NULL); // 100ms delay
}

// Force visual styles activation using multiple methods
void ForceVisualStylesActivation(void) {
    // Method 1: Try to activate visual styles through UxTheme
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme) {
        typedef BOOL (WINAPI *SetThemeAppPropertiesFunc)(DWORD);
        typedef HRESULT (WINAPI *EnableThemingFunc)(BOOL);
        
        SetThemeAppPropertiesFunc SetThemeAppProperties = 
            (SetThemeAppPropertiesFunc)(void*)GetProcAddress(hUxTheme, "SetThemeAppProperties");
        EnableThemingFunc EnableTheming = 
            (EnableThemingFunc)(void*)GetProcAddress(hUxTheme, "EnableTheming");
        
        if (SetThemeAppProperties) {
            // Enable all theming properties
            SetThemeAppProperties(0x7); // STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT
        }
        
        if (EnableTheming) {
            EnableTheming(TRUE);
        }
        
        FreeLibrary(hUxTheme);
    }
    
    // Method 2: Re-initialize Common Controls with explicit visual styles
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | 
                 ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
}

// Create a dialog with explicit theming support
HWND CreateThemedDialog(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc) {
    // Ensure visual styles are active before creating dialog
    ForceVisualStylesActivation();
    
    // Create the dialog
    HWND hDlg = CreateDialogW(hInstance, lpTemplate, hWndParent, lpDialogFunc);
    
    if (hDlg) {
        // Apply theming immediately after creation
        ApplyModernThemeToDialog(hDlg);
        
        // Show the dialog
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);
        
        // Apply theming again after showing (sometimes needed)
        ApplyDelayedTheming(hDlg);
    }
    
    return hDlg;
}

void ResizeControls(HWND hDlg) {
    RECT rect;
    GetClientRect(hDlg, &rect);
    
    // Get DPI for this window to scale all measurements appropriately
    HDC hdc = GetDC(hDlg);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hDlg, hdc);
    
    // Calculate DPI scaling factors (96 DPI = 100% scaling)
    double scaleX = (double)dpiX / 96.0;
    double scaleY = (double)dpiY / 96.0;
    
    // Base measurements in logical units (96 DPI)
    const int BASE_MARGIN = 10;           // Standard margin between elements
    const int BASE_WINDOW_MARGIN = 10;    // Margin from window edges
    const int BASE_BUTTON_WIDTH = 78;     // Standard button width
    const int BASE_BUTTON_HEIGHT = 26;    // Standard button height

    const int BASE_TEXT_HEIGHT = 22;      // Text field height
    const int BASE_LABEL_HEIGHT = 16;     // Label height
    const int BASE_PROGRESS_HEIGHT = 16;  // Progress bar height
    const int BASE_GROUP_TITLE_HEIGHT = 18; // Group box title area height
    
    // Scale all measurements to current DPI
    int margin = (int)(BASE_MARGIN * scaleX);
    int windowMargin = (int)(BASE_WINDOW_MARGIN * scaleX);
    int buttonWidth = (int)(BASE_BUTTON_WIDTH * scaleX);
    int buttonHeight = (int)(BASE_BUTTON_HEIGHT * scaleY);

    int textHeight = (int)(BASE_TEXT_HEIGHT * scaleY);
    int labelHeight = (int)(BASE_LABEL_HEIGHT * scaleY);
    int progressHeight = (int)(BASE_PROGRESS_HEIGHT * scaleY);
    int groupTitleHeight = (int)(BASE_GROUP_TITLE_HEIGHT * scaleY);
    
    // Calculate available space
    int clientWidth = rect.right - rect.left;
    int clientHeight = rect.bottom - rect.top;
    
    // Calculate Download video group dimensions
    // Two-line video info layout: URL field + progress bar + title line + duration line
    // Vertical layout: Group title (18) + margin (10) + URL row (22) + margin (8) + 
    //                  progress bar (16) + margin (8) + title line (16) + small margin (6) +
    //                  duration line (16) + bottom margin (10) = 130 total
    int downloadGroupHeight = groupTitleHeight + margin + textHeight + (margin * 3/4) + 
                             progressHeight + (margin * 3/4) + labelHeight + (margin / 2) + 
                             labelHeight + margin;
    
    // Position Download video group box
    int downloadGroupX = windowMargin;
    int downloadGroupY = windowMargin;
    int downloadGroupWidth = clientWidth - (2 * windowMargin);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_GROUP), NULL, 
                downloadGroupX, downloadGroupY, downloadGroupWidth, downloadGroupHeight, SWP_NOZORDER);
    
    // Calculate button positions (right-aligned within group) - restore original logic
    int buttonX = downloadGroupX + downloadGroupWidth - buttonWidth - margin;
    int availableTextWidth = buttonX - downloadGroupX - (3 * margin); // Space for text controls
    
    // Position controls within Download video group
    int currentY = downloadGroupY + groupTitleHeight + margin;
    
    // URL row - restore original position
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL1), NULL, 
                downloadGroupX + margin, currentY + 2, (int)(30 * scaleX), labelHeight, SWP_NOZORDER);
    
    int urlFieldX = downloadGroupX + margin + (int)(35 * scaleX);
    int urlFieldWidth = availableTextWidth - (int)(35 * scaleX);
    SetWindowPos(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, 
                urlFieldX, currentY, urlFieldWidth, textHeight, SWP_NOZORDER);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_BTN), NULL, 
                buttonX, currentY - 1, buttonWidth, buttonHeight, SWP_NOZORDER);
    
    currentY += textHeight + (margin * 3/4);
    
    // Progress bar row - restore to original position (right after URL field)
    SetWindowPos(GetDlgItem(hDlg, IDC_PROGRESS_BAR), NULL, 
                urlFieldX, currentY, urlFieldWidth, progressHeight, SWP_NOZORDER);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_GETINFO_BTN), NULL, 
                buttonX, currentY - 1, buttonWidth, buttonHeight, SWP_NOZORDER);
    
    currentY += progressHeight + (margin * 3/4);
    
    // CORRECTED: Two-line video info layout below progress bar
    
    // LINE 1: Video title (truncates to account for "Get Info" button)
    // Mathematical reasoning: availableTextWidth already accounts for button space
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_TITLE_LABEL), NULL, 
                downloadGroupX + margin, currentY, (int)(35 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Title text width = availableTextWidth - label width (35px)
    // This ensures title truncates BEFORE hitting the "Get Info" button
    int titleTextWidth = availableTextWidth - (int)(35 * scaleX);
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_TITLE), NULL, 
                urlFieldX, currentY, titleTextWidth, labelHeight, SWP_NOZORDER);
    
    // Move to next line for duration + download status
    currentY += labelHeight + (margin / 2);  // 6px spacing between lines
    
    // LINE 2: Duration + Download Status (on same line, below title)
    // Duration label: "Duration:" (50px width)
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_DURATION_LABEL), NULL, 
                downloadGroupX + margin, currentY, (int)(50 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Duration value: "5:23" format (60px width - enough for "99:59:59")
    int durationValueX = downloadGroupX + margin + (int)(55 * scaleX);  // After label + 5px gap
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_DURATION), NULL, 
                durationValueX, currentY, (int)(60 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Download status: positioned after duration with 10px gap, uses remaining width
    int statusX = durationValueX + (int)(60 * scaleX) + (int)(10 * scaleX);  // After duration + gap
    int statusWidth = (downloadGroupX + downloadGroupWidth - margin) - statusX;
    SetWindowPos(GetDlgItem(hDlg, IDC_PROGRESS_TEXT), NULL, 
                statusX, currentY, statusWidth, labelHeight, SWP_NOZORDER);
    
    // Position color indicator buttons (hidden by default, positioned for future use)
    // These will be positioned relative to the Add button later in the layout code
    
    // Calculate Offline videos group position and size
    int offlineGroupY = downloadGroupY + downloadGroupHeight + margin;
    int offlineGroupHeight = clientHeight - offlineGroupY - windowMargin;
    
    // Ensure minimum height for offline group
    if (offlineGroupHeight < (int)(100 * scaleY)) {
        offlineGroupHeight = (int)(100 * scaleY);
    }
    
    SetWindowPos(GetDlgItem(hDlg, IDC_OFFLINE_GROUP), NULL, 
                downloadGroupX, offlineGroupY, downloadGroupWidth, offlineGroupHeight, SWP_NOZORDER);
    
    // Position controls within Offline videos group
    int offlineContentY = offlineGroupY + groupTitleHeight + (margin / 2);
    
    // Status labels
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL2), NULL, 
                downloadGroupX + margin, offlineContentY, (int)(150 * scaleX), labelHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL3), NULL, 
                downloadGroupX + margin + (int)(160 * scaleX), offlineContentY, (int)(100 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Calculate listbox and side buttons
    int listY = offlineContentY + labelHeight + margin;
    int listHeight = offlineGroupY + offlineGroupHeight - listY - margin;
    int sideButtonX = downloadGroupX + downloadGroupWidth - buttonWidth - margin;
    int listWidth = sideButtonX - downloadGroupX - (2 * margin);
    
    // Ensure minimum dimensions
    if (listWidth < (int)(200 * scaleX)) {
        listWidth = (int)(200 * scaleX);
    }
    if (listHeight < (int)(50 * scaleY)) {
        listHeight = (int)(50 * scaleY);
    }
    
    SetWindowPos(GetDlgItem(hDlg, IDC_LIST), NULL, 
                downloadGroupX + margin, listY, listWidth, listHeight, SWP_NOZORDER);
    
    // Resize ListView columns
    ResizeCacheListViewColumns(GetDlgItem(hDlg, IDC_LIST), listWidth);
    
    // Side buttons (Play, Delete, and Add)
    int sideButtonHeight = (int)(32 * scaleY);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON2), NULL, 
                sideButtonX, listY, buttonWidth, sideButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON3), NULL, 
                sideButtonX, listY + sideButtonHeight + (margin / 2), buttonWidth, sideButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON1), NULL, 
                sideButtonX, listY + (sideButtonHeight + (margin / 2)) * 2, buttonWidth, sideButtonHeight, SWP_NOZORDER);
    
    // Position colored buttons in 2x2 grid below Add button
    int addButtonY = listY + (sideButtonHeight + (margin / 2)) * 2;
    int colorButtonStartY = addButtonY + sideButtonHeight + (margin / 2);
    int colorButtonWidth = (int)(36 * scaleX);
    int colorButtonHeight = (int)(20 * scaleY);
    int colorButtonSpacing = (int)(6 * scaleX); // Small gap between buttons
    
    // Top row: Green and Teal
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_GREEN), NULL, 
                sideButtonX, colorButtonStartY, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_TEAL), NULL, 
                sideButtonX + colorButtonWidth + colorButtonSpacing, colorButtonStartY, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    
    // Bottom row: Blue and White
    int colorButtonRow2Y = colorButtonStartY + colorButtonHeight + (int)(4 * scaleY);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_BLUE), NULL, 
                sideButtonX, colorButtonRow2Y, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_WHITE), NULL, 
                sideButtonX + colorButtonWidth + colorButtonSpacing, colorButtonRow2Y, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
}

// Settings dialog procedure
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    switch (message) {
        case WM_INITDIALOG: {
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            // Load settings from registry (with defaults if not found)
            LoadSettings(hDlg);
            
            // Apply DPI-aware positioning (similar to error dialog)
            HWND hParent = GetParent(hDlg);
            if (hParent) {
                RECT parentRect, dialogRect;
                GetWindowRect(hDlg, &dialogRect);
                GetWindowRect(hParent, &parentRect);
                
                int dialogWidth = dialogRect.right - dialogRect.left;
                int dialogHeight = dialogRect.bottom - dialogRect.top;
                
                // Get monitor-specific work area for HiDPI awareness
                HMONITOR hMonitor = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                GetMonitorInfoW(hMonitor, &mi);
                RECT screenRect = mi.rcWork;
                
                // Center on parent window
                int x = parentRect.left + (parentRect.right - parentRect.left - dialogWidth) / 2;
                int y = parentRect.top + (parentRect.bottom - parentRect.top - dialogHeight) / 2;
                
                // Adjust if any edge would be off screen
                if (x < screenRect.left) x = screenRect.left;
                if (y < screenRect.top) y = screenRect.top;
                if (x + dialogWidth > screenRect.right) x = screenRect.right - dialogWidth;
                if (y + dialogHeight > screenRect.bottom) y = screenRect.bottom - dialogHeight;
                
                SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            
            return TRUE;
        }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_YTDLP_BROWSE: {
                    OPENFILENAMEW ofn = {0};
                    wchar_t szFile[MAX_EXTENDED_PATH] = {0};
                    
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hDlg;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = MAX_EXTENDED_PATH;
                    ofn.lpstrFilter = L"Executable Files\0*.exe;*.cmd;*.bat;*.py;*.ps1\0All Files\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrTitle = L"Select yt-dlp executable";
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    
                    if (GetOpenFileNameW(&ofn)) {
                        SetDlgItemTextW(hDlg, IDC_YTDLP_PATH, szFile);
                    }
                    return TRUE;
                }
                
                case IDC_FOLDER_BROWSE: {
                    BROWSEINFOW bi = {0};
                    wchar_t szPath[MAX_EXTENDED_PATH];
                    LPITEMIDLIST pidl;
                    
                    bi.hwndOwner = hDlg;
                    bi.lpszTitle = L"Select Download Folder";
                    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                    
                    pidl = SHBrowseForFolderW(&bi);
                    if (pidl != NULL) {
                        if (SHGetPathFromIDListW(pidl, szPath)) {
                            SetDlgItemTextW(hDlg, IDC_FOLDER_PATH, szPath);
                        }
                        CoTaskMemFree(pidl);
                    }
                    return TRUE;
                }
                
                case IDC_PLAYER_BROWSE: {
                    OPENFILENAMEW ofn = {0};
                    wchar_t szFile[MAX_EXTENDED_PATH] = {0};
                    
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hDlg;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = MAX_EXTENDED_PATH;
                    ofn.lpstrFilter = L"Executable Files\0*.exe\0All Files\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrTitle = L"Select Media Player";
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    
                    if (GetOpenFileNameW(&ofn)) {
                        SetDlgItemTextW(hDlg, IDC_PLAYER_PATH, szFile);
                    }
                    return TRUE;
                }
                
                case IDC_ENABLE_DEBUG:
                    // Handle debug checkbox change - update visibility immediately
                    if (HIWORD(wParam) == BN_CLICKED) {
                        BOOL enableDebug = (IsDlgButtonChecked(hDlg, IDC_ENABLE_DEBUG) == BST_CHECKED);
                        BOOL currentEnableDebug, currentEnableLogfile;
                        GetDebugState(&currentEnableDebug, &currentEnableLogfile);
                        SetDebugState(enableDebug, currentEnableLogfile);
                        
                        // Find the main dialog window and update debug control visibility
                        HWND hMainDlg = GetParent(hDlg);
                        if (hMainDlg) {
                            UpdateDebugControlVisibility(hMainDlg);
                        }
                    }
                    return TRUE;
                
                case IDOK:
                    // Save settings to registry
                    SaveSettings(hDlg);
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                    
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

// Progress dialog procedure
INT_PTR CALLBACK ProgressDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static ProgressDialog* pProgress = NULL;
    
    switch (message) {
        case WM_INITDIALOG: {
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            // Store the ProgressDialog pointer passed via lParam
            pProgress = (ProgressDialog*)lParam;
            if (pProgress) {
                pProgress->hDialog = hDlg;
                pProgress->hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_PROGRESS);
                pProgress->hStatusText = GetDlgItem(hDlg, IDC_PROGRESS_STATUS);
                pProgress->hCancelButton = GetDlgItem(hDlg, IDC_PROGRESS_CANCEL);
                pProgress->cancelled = FALSE;
                
                // Initialize progress bar
                SendMessageW(pProgress->hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
                SendMessageW(pProgress->hProgressBar, PBM_SETPOS, 0, 0);
                
                // Center the dialog with HiDPI awareness
                HWND hParent = GetParent(hDlg);
                RECT parentRect, dialogRect;
                
                GetWindowRect(hDlg, &dialogRect);
                int dialogWidth = dialogRect.right - dialogRect.left;
                int dialogHeight = dialogRect.bottom - dialogRect.top;
                
                // Get monitor-specific work area for HiDPI awareness
                HMONITOR hMonitor;
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                
                if (hParent && GetWindowRect(hParent, &parentRect)) {
                    hMonitor = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
                } else {
                    hMonitor = MonitorFromWindow(hDlg, MONITOR_DEFAULTTONEAREST);
                }
                
                GetMonitorInfoW(hMonitor, &mi);
                RECT screenRect = mi.rcWork;
                
                int x, y;
                
                if (hParent && GetWindowRect(hParent, &parentRect)) {
                    // Center on parent window
                    x = parentRect.left + (parentRect.right - parentRect.left - dialogWidth) / 2;
                    y = parentRect.top + (parentRect.bottom - parentRect.top - dialogHeight) / 2;
                } else {
                    // Center on screen if no parent
                    x = screenRect.left + (screenRect.right - screenRect.left - dialogWidth) / 2;
                    y = screenRect.top + (screenRect.bottom - screenRect.top - dialogHeight) / 2;
                }
                
                // Adjust if any edge would be off screen
                if (x < screenRect.left) x = screenRect.left;
                if (y < screenRect.top) y = screenRect.top;
                if (x + dialogWidth > screenRect.right) x = screenRect.right - dialogWidth;
                if (y + dialogHeight > screenRect.bottom) y = screenRect.bottom - dialogHeight;
                
                SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return TRUE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_PROGRESS_CANCEL:
                    if (pProgress) {
                        pProgress->cancelled = TRUE;
                    }
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            if (pProgress) {
                pProgress->cancelled = TRUE;
            }
            return TRUE;
    }
    return FALSE;
}

// Dialog procedure function
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            // Initialize brushes for text field colors (created on-demand in ApplicationState)
            SetCurrentBrush(GetBrush(BRUSH_WHITE));
            
            // Initialize cache manager
            wchar_t downloadPath[MAX_EXTENDED_PATH];
            if (!LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
            }
            
            // Initialize ListView with columns
            HWND hListView = GetDlgItem(hDlg, IDC_LIST);
            InitializeCacheListView(hListView);
            
            if (InitializeCacheManager(GetCacheManager(), downloadPath)) {
                // Scan for existing videos in download folder
                ScanDownloadFolderForVideos(GetCacheManager(), downloadPath);
                
                // Refresh the UI with cached videos
                RefreshCacheList(hListView, GetCacheManager());
                UpdateCacheListStatus(hDlg, GetCacheManager());
            } else {
                // Initialize dialog controls with defaults if cache fails
                SetDlgItemTextW(hDlg, IDC_LABEL2, L"Status: Cache initialization failed");
                SetDlgItemTextW(hDlg, IDC_LABEL3, L"Items: 0");
            }
            
            // Initialize cached video metadata
            InitializeCachedMetadata(GetCachedVideoMetadata());
            
            // Load debug settings from registry
            wchar_t buffer[MAX_EXTENDED_PATH];
            BOOL enableDebug = FALSE, enableLogfile = FALSE, enableAutopaste = TRUE;
            
            if (LoadSettingFromRegistry(REG_ENABLE_DEBUG, buffer, MAX_EXTENDED_PATH)) {
                enableDebug = (wcscmp(buffer, L"1") == 0);
            }
            if (LoadSettingFromRegistry(REG_ENABLE_LOGFILE, buffer, MAX_EXTENDED_PATH)) {
                enableLogfile = (wcscmp(buffer, L"1") == 0);
            }
            if (LoadSettingFromRegistry(REG_ENABLE_AUTOPASTE, buffer, MAX_EXTENDED_PATH)) {
                enableAutopaste = (wcscmp(buffer, L"1") == 0);
            }
            
            // Set the state using the application state functions
            SetDebugState(enableDebug, enableLogfile);
            SetAutopasteState(enableAutopaste);
            
            // Write session start marker to logfile if logging is enabled
            WriteSessionStartToLogfile();
            
            // Update debug control visibility
            UpdateDebugControlVisibility(hDlg);
            
            // Check command line first, then clipboard
            const wchar_t* cmdURL = GetCommandLineURL();
            if (wcslen(cmdURL) > 0) {
                SetProgrammaticChangeFlag(TRUE);
                SetDlgItemTextW(hDlg, IDC_TEXT_FIELD, cmdURL);
                SetCurrentBrush(GetBrush(BRUSH_LIGHT_TEAL));
                InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                SetProgrammaticChangeFlag(FALSE);
            } else {
                // Check clipboard for YouTube URL
                CheckClipboardForYouTubeURL(hDlg);
            }
            
            // Set focus to the text field for immediate typing
            SetFocus(GetDlgItem(hDlg, IDC_TEXT_FIELD));
            
            // Subclass the text field to detect paste operations
            HWND hTextField = GetDlgItem(hDlg, IDC_TEXT_FIELD);
            WNDPROC originalProc = (WNDPROC)SetWindowLongPtr(hTextField, GWLP_WNDPROC, (LONG_PTR)TextFieldSubclassProc);
            SetOriginalTextFieldProc(originalProc);
            
            // Calculate and set optimal default window size based on DPI
            HDC hdc = GetDC(hDlg);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hDlg, hdc);
            
            double scaleX = (double)dpiX / 96.0;
            double scaleY = (double)dpiY / 96.0;
            
            int defaultWidth, defaultHeight;
            CalculateDefaultWindowSize(&defaultWidth, &defaultHeight, scaleX, scaleY);
            
            // Set the calculated default window size
            SetWindowPos(hDlg, NULL, 0, 0, defaultWidth, defaultHeight, SWP_NOMOVE | SWP_NOZORDER);
            
            return FALSE; // Return FALSE since we set focus manually
        }
            
        case WM_SIZE:
            ResizeControls(hDlg);
            return TRUE;
            
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            
            // Get DPI for dynamic minimum size calculation
            HDC hdc = GetDC(hDlg);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hDlg, hdc);
            
            // Calculate DPI scaling factors (96 DPI = 100% scaling)
            double scaleX = (double)dpiX / 96.0;
            double scaleY = (double)dpiY / 96.0;
            
            // Calculate minimum window size based on content requirements
            int minWidth, minHeight;
            CalculateMinimumWindowSize(&minWidth, &minHeight, scaleX, scaleY);
            
            mmi->ptMinTrackSize.x = minWidth;
            mmi->ptMinTrackSize.y = minHeight;
            return 0;
        }
        
        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) {
                // Window is being activated, check clipboard
                CheckClipboardForYouTubeURL(hDlg);
            }
            break;
            
        case WM_CTLCOLOREDIT:
            if ((HWND)lParam == GetDlgItem(hDlg, IDC_TEXT_FIELD)) {
                HDC hdc = (HDC)wParam;
                // Set the background color based on current brush
                HBRUSH currentBrush = GetCurrentBrush();
                if (currentBrush == GetBrush(BRUSH_LIGHT_GREEN)) {
                    SetBkColor(hdc, COLOR_LIGHT_GREEN);
                } else if (currentBrush == GetBrush(BRUSH_LIGHT_BLUE)) {
                    SetBkColor(hdc, COLOR_LIGHT_BLUE);
                } else if (currentBrush == GetBrush(BRUSH_LIGHT_TEAL)) {
                    SetBkColor(hdc, COLOR_LIGHT_TEAL);
                } else {
                    SetBkColor(hdc, COLOR_WHITE);
                }
                return (INT_PTR)currentBrush;
            }
            break;
            
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlType == ODT_BUTTON) {
                HBRUSH hBrush = NULL;
                COLORREF color = RGB(255, 255, 255); // Default white
                
                // Determine color based on button ID
                switch (lpDrawItem->CtlID) {
                    case IDC_COLOR_GREEN:
                        color = COLOR_LIGHT_GREEN;
                        break;
                    case IDC_COLOR_TEAL:
                        color = COLOR_LIGHT_TEAL;
                        break;
                    case IDC_COLOR_BLUE:
                        color = COLOR_LIGHT_BLUE;
                        break;
                    case IDC_COLOR_WHITE:
                        color = COLOR_WHITE;
                        break;
                }
                
                // Create brush and fill the button
                hBrush = CreateSolidBrush(color);
                if (hBrush) {
                    FillRect(lpDrawItem->hDC, &lpDrawItem->rcItem, hBrush);
                    DeleteObject(hBrush);
                }
                
                // Draw button border
                if (lpDrawItem->itemState & ODS_SELECTED) {
                    DrawEdge(lpDrawItem->hDC, &lpDrawItem->rcItem, EDGE_SUNKEN, BF_RECT);
                } else {
                    DrawEdge(lpDrawItem->hDC, &lpDrawItem->rcItem, EDGE_RAISED, BF_RECT);
                }
                
                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_EDIT_SELECTALL: {
                    HWND hFocus = GetFocus();
                    if (hFocus == GetDlgItem(hDlg, IDC_TEXT_FIELD)) {
                        SendMessage(hFocus, EM_SETSEL, 0, -1);
                    }
                    return TRUE;
                }
                    
                case ID_FILE_SETTINGS:
                    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SETTINGS), hDlg, SettingsDialogProc);
                    return TRUE;
                    
                case ID_FILE_EXIT:
                    DestroyWindow(hDlg);
                    return TRUE;
                    
                case ID_HELP_INSTALL_YTDLP:
                    InstallYtDlpWithWinget(hDlg);
                    return TRUE;
                    
                case ID_HELP_ABOUT:
                    // TODO: Implement about dialog
                    return TRUE;
                    
                case IDC_TEXT_FIELD:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // Skip processing if this is a programmatic change
                        if (GetProgrammaticChangeFlag()) {
                            break;
                        }
                        
                        // Clear cached metadata when URL changes
                        FreeCachedMetadata(GetCachedVideoMetadata());
                        
                        // Get current text
                        wchar_t buffer[MAX_BUFFER_SIZE];
                        GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, buffer, MAX_BUFFER_SIZE);
                        
                        // Handle different user input scenarios
                        HBRUSH currentBrush = GetCurrentBrush();
                        if (currentBrush == GetBrush(BRUSH_LIGHT_GREEN)) {
                            // User has modified the autopasted content - return to white
                            SetCurrentBrush(GetBrush(BRUSH_WHITE));
                        } else if (currentBrush == GetBrush(BRUSH_LIGHT_BLUE)) {
                            // User is editing manually pasted content - return to white
                            SetCurrentBrush(GetBrush(BRUSH_WHITE));
                        } else if (GetManualPasteFlag() && IsYouTubeURL(buffer)) {
                            // Manual paste of YouTube URL - set to light blue
                            SetCurrentBrush(GetBrush(BRUSH_LIGHT_BLUE));
                            SetManualPasteFlag(FALSE); // Reset flag after use
                        } else if (GetManualPasteFlag()) {
                            // Manual paste of non-YouTube content - keep white but reset flag
                            SetManualPasteFlag(FALSE);
                        }
                        // Note: Light teal (command line) is preserved during editing
                        // Regular typing in white background stays white
                        
                        InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    }
                    break;
                    


                case IDC_DOWNLOAD_BTN: {
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    if (wcslen(url) == 0) {
                        ShowWarningMessage(hDlg, L"No URL Provided", L"Please enter a YouTube URL to download.");
                        break;
                    }
                    
                    // Start unified download process
                    if (!StartUnifiedDownload(hDlg, url)) {
                        ShowConfigurationError(hDlg, L"Failed to start download. Please check your yt-dlp configuration.");
                    }
                    break;
                }
                    
                case IDC_GETINFO_BTN: {
                    // Check if download is in progress
                    if (GetDownloadingState()) {
                        ShowWarningMessage(hDlg, L"Download in Progress", L"Please wait for the current download to complete before getting video information.");
                        break;
                    }
                    
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    // Validate URL is provided
                    if (wcslen(url) == 0) {
                        ShowWarningMessage(hDlg, L"No URL Provided", L"Please enter a YouTube URL to get video information.");
                        break;
                    }
                    
                    // Check if we already have cached data for this URL
                    if (IsCachedMetadataValid(GetCachedVideoMetadata(), url)) {
                        VideoMetadata metadata;
                        if (GetCachedMetadata(GetCachedVideoMetadata(), &metadata)) {
                            // Update UI with cached information
                            if (metadata.title) {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, metadata.title);
                            } else {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, L"Unknown Title");
                            }
                            
                            if (metadata.duration) {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, metadata.duration);
                            } else {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, L"Unknown");
                            }
                            
                            // Show progress briefly to indicate completion
                            ShowMainProgressBar(hDlg, TRUE);
                            UpdateMainProgressBar(hDlg, 100, L"Video information (cached)");
                            
                            FreeVideoMetadata(&metadata);
                            break;
                        }
                    }
                    
                    // Show progress bar with indeterminate animation
                    ShowMainProgressBar(hDlg, TRUE);
                    // Set progress bar to marquee (indeterminate) style for indefinite operation
                    SetProgressBarMarquee(hDlg, TRUE);
                    UpdateMainProgressBar(hDlg, -1, L"Getting video information...");
                    
                    // Start non-blocking Get Info operation
                    if (!StartNonBlockingGetInfo(hDlg, url, GetCachedVideoMetadata())) {
                        // Failed to start operation
                        SetProgressBarMarquee(hDlg, FALSE);
                        UpdateMainProgressBar(hDlg, 0, L"Failed to start operation");
                        ShowWarningMessage(hDlg, L"Operation Failed", L"Could not start video information retrieval. Please try again.");
                    }
                    
                    break;
                }
                    
                case IDC_BUTTON2: { // Play button
                    HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                    wchar_t* selectedVideoId = GetSelectedVideoId(hListView);
                    
                    if (!selectedVideoId) {
                        ShowWarningMessage(hDlg, L"No Selection", 
                                         L"Please select a video from the list to play.");
                        break;
                    }
                    
                    // Get player path from settings
                    wchar_t playerPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_PLAYER_PATH, playerPath, MAX_EXTENDED_PATH)) {
                        ShowWarningMessage(hDlg, L"Player Not Configured", 
                                         L"Please configure a media player in File > Settings.");
                        break;
                    }
                    
                    // Validate player exists
                    DWORD attributes = GetFileAttributesW(playerPath);
                    if (attributes == INVALID_FILE_ATTRIBUTES) {
                        ShowWarningMessage(hDlg, L"Player Not Found", 
                                         L"The configured media player was not found. Please check the path in Settings.");
                        break;
                    }
                    
                    // Play the video (no success popup)
                    if (!PlayCacheEntry(GetCacheManager(), selectedVideoId, playerPath)) {
                        ShowWarningMessage(hDlg, L"Playback Failed", 
                                         L"Failed to launch the video. The file may have been moved or deleted.");
                        // Refresh cache to remove invalid entries
                        RefreshCacheList(hListView, GetCacheManager());
                        UpdateCacheListStatus(hDlg, GetCacheManager());
                    }
                    break;
                }
                    
                case IDC_BUTTON3: { // Delete button
                    HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                    int selectedCount = 0;
                    wchar_t** selectedVideoIds = GetSelectedVideoIds(hListView, &selectedCount);
                    
                    if (!selectedVideoIds || selectedCount == 0) {
                        ShowWarningMessage(hDlg, L"No Selection", 
                                         L"Please select one or more videos from the list to delete.");
                        break;
                    }
                    
                    // Build confirmation message
                    wchar_t confirmMsg[1024];
                    if (selectedCount == 1) {
                        // Single video - show title if available
                        CacheEntry* entry = FindCacheEntry(GetCacheManager(), selectedVideoIds[0]);
                        if (entry && entry->title) {
                            swprintf(confirmMsg, 1024, 
                                    L"Are you sure you want to delete \"%ls\"?\n\n"
                                    L"This will permanently delete the video file and any associated subtitle files.",
                                    entry->title);
                        } else {
                            wcscpy(confirmMsg, L"Are you sure you want to delete the selected video?\n\n"
                                             L"This will permanently delete the video file and any associated subtitle files.");
                        }
                    } else {
                        // Multiple videos
                        swprintf(confirmMsg, 1024, 
                                L"Are you sure you want to delete %d selected videos?\n\n"
                                L"This will permanently delete all video files and any associated subtitle files.",
                                selectedCount);
                    }
                    
                    // Confirm deletion
                    int result = MessageBoxW(hDlg, confirmMsg, L"Confirm Delete", 
                                           MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                    
                    if (result == IDYES) {
                        // Delete all selected videos
                        int totalErrors = 0;
                        int totalSuccessful = 0;
                        wchar_t* combinedErrorDetails = NULL;
                        size_t combinedErrorSize = 0;
                        
                        for (int i = 0; i < selectedCount; i++) {
                            DeleteResult* deleteResult = DeleteCacheEntryFilesDetailed(GetCacheManager(), selectedVideoIds[i]);
                            
                            if (deleteResult) {
                                if (deleteResult->errorCount == 0) {
                                    totalSuccessful++;
                                } else {
                                    totalErrors += deleteResult->errorCount;
                                    
                                    // Combine error details
                                    wchar_t* errorDetails = FormatDeleteErrorDetails(deleteResult);
                                    if (errorDetails) {
                                        size_t errorLen = wcslen(errorDetails);
                                        size_t newSize = combinedErrorSize + errorLen + 100; // Extra space for headers
                                        
                                        wchar_t* newCombined = (wchar_t*)realloc(combinedErrorDetails, newSize * sizeof(wchar_t));
                                        if (newCombined) {
                                            combinedErrorDetails = newCombined;
                                            
                                            if (combinedErrorSize == 0) {
                                                wcscpy(combinedErrorDetails, L"Multiple Delete Operation Results:\n");
                                                wcscpy(combinedErrorDetails + wcslen(combinedErrorDetails), L"=====================================\n\n");
                                            }
                                            
                                            // Add video identifier
                                            CacheEntry* entry = FindCacheEntry(GetCacheManager(), selectedVideoIds[i]);
                                            if (entry && entry->title) {
                                                swprintf(combinedErrorDetails + wcslen(combinedErrorDetails), 
                                                        newSize - wcslen(combinedErrorDetails),
                                                        L"Video: %ls\n", entry->title);
                                            } else {
                                                swprintf(combinedErrorDetails + wcslen(combinedErrorDetails), 
                                                        newSize - wcslen(combinedErrorDetails),
                                                        L"Video ID: %ls\n", selectedVideoIds[i]);
                                            }
                                            
                                            wcscat(combinedErrorDetails, errorDetails);
                                            wcscat(combinedErrorDetails, L"\n");
                                            
                                            combinedErrorSize = wcslen(combinedErrorDetails);
                                        }
                                        
                                        free(errorDetails);
                                    }
                                }
                                
                                FreeDeleteResult(deleteResult);
                            }
                        }
                        
                        // Show results
                        if (totalErrors == 0) {
                            // All deletions successful (no success popup)
                        } else {
                            // Some deletions failed - show combined error details
                            if (combinedErrorDetails) {
                                // Add summary at the beginning
                                wchar_t summary[256];
                                swprintf(summary, 256, 
                                        L"Summary: %d videos processed, %d successful, %d failed\n\n",
                                        selectedCount, totalSuccessful, selectedCount - totalSuccessful);
                                
                                size_t summaryLen = wcslen(summary);
                                size_t totalLen = summaryLen + wcslen(combinedErrorDetails) + 1;
                                wchar_t* finalDetails = (wchar_t*)malloc(totalLen * sizeof(wchar_t));
                                
                                if (finalDetails) {
                                    wcscpy(finalDetails, summary);
                                    wcscat(finalDetails, combinedErrorDetails);
                                    
                                    EnhancedErrorDialog* errorDialog = CreateEnhancedErrorDialog(
                                        L"Multiple Delete Failed",
                                        L"Some files failed to delete. They may be in use or you may not have permission.",
                                        finalDetails,
                                        L"Check if files are currently open in another application or if you have sufficient permissions.",
                                        L"• Close any applications that might be using the files\n"
                                        L"• Run as administrator if permission is denied\n"
                                        L"• Check if files are read-only or protected\n"
                                        L"• Restart the application and try again",
                                        ERROR_TYPE_PERMISSIONS
                                    );
                                    
                                    if (errorDialog) {
                                        ShowEnhancedErrorDialog(hDlg, errorDialog);
                                        FreeEnhancedErrorDialog(errorDialog);
                                    }
                                    
                                    free(finalDetails);
                                }
                            } else {
                                ShowWarningMessage(hDlg, L"Delete Failed", 
                                                 L"Failed to delete some or all files. They may be in use or you may not have permission.");
                            }
                        }
                        
                        if (combinedErrorDetails) {
                            free(combinedErrorDetails);
                        }
                        
                        // Refresh the list to show current state
                        RefreshCacheList(hListView, GetCacheManager());
                        UpdateCacheListStatus(hDlg, GetCacheManager());
                    }
                    
                    // Clean up selected video IDs
                    FreeSelectedVideoIds(selectedVideoIds, selectedCount);
                    break;
                }

                case IDC_BUTTON1: { // Add button (for debugging)
                    // Get download path
                    wchar_t downloadPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                        GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
                    }
                    
                    // Create download directory if it doesn't exist
                    CreateDownloadDirectoryIfNeeded(downloadPath);
                    
                    // Add dummy video (no success popup)
                    if (AddDummyVideo(GetCacheManager(), downloadPath)) {
                        // Refresh the list
                        HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                        RefreshCacheList(hListView, GetCacheManager());
                        UpdateCacheListStatus(hDlg, GetCacheManager());
                    } else {
                        ShowWarningMessage(hDlg, L"Add Failed", L"Failed to add dummy video to cache.");
                    }
                    break;
                }
                    
                case IDC_COLOR_GREEN:
                    SetCurrentBrush(GetBrush(BRUSH_LIGHT_GREEN));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_TEAL:
                    SetCurrentBrush(GetBrush(BRUSH_LIGHT_TEAL));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_BLUE:
                    SetCurrentBrush(GetBrush(BRUSH_LIGHT_BLUE));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_WHITE:
                    SetCurrentBrush(GetBrush(BRUSH_WHITE));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDCANCEL:
                    DestroyWindow(hDlg);
                    return TRUE;
            }
            break;
            
        case WM_SHOWWINDOW:
            // Apply delayed theming when window is shown
            if (wParam) { // Window is being shown
                ApplyDelayedTheming(hDlg);
            }
            return FALSE;
            
        case WM_TIMER:
            // Handle delayed theming timer
            if (wParam == 9999) {
                KillTimer(hDlg, 9999);
                ApplyModernThemeToDialog(hDlg);
                return TRUE;
            }
            return FALSE;
            
        case WM_CLOSE:
            // Restore original text field window procedure
            WNDPROC originalProc = GetOriginalTextFieldProc();
            if (originalProc) {
                HWND hTextField = GetDlgItem(hDlg, IDC_TEXT_FIELD);
                SetWindowLongPtr(hTextField, GWLP_WNDPROC, (LONG_PTR)originalProc);
            }
            
            // Clean up application state (including brushes)
            ApplicationState* appState = GetApplicationState();
            if (appState) {
                CleanupApplicationState(appState);
            }
            
            DestroyWindow(hDlg);
            return TRUE;
            
        case WM_USER + 100: {
            // Handle thread-safe progress updates from worker threads
            int percentage = (int)wParam;
            const wchar_t* status = (const wchar_t*)lParam;
            UpdateMainProgressBar(hDlg, percentage, status);
            return TRUE;
        }
        
        case WM_USER + 101: {
            // Handle video info thread completion
            VideoInfoThreadData* data = (VideoInfoThreadData*)lParam;
            if (data) {
                if (data->success) {
                    UpdateMainProgressBar(hDlg, 90, L"Updating interface...");
                    
                    // Update the UI with the retrieved information
                    UpdateVideoInfoUI(hDlg, data->title, data->duration);
                    
                    UpdateMainProgressBar(hDlg, 100, L"Video information retrieved successfully");
                    // Keep progress bar visible - don't hide it
                    // Success dialog removed - info is displayed in the UI fields
                } else {
                    UpdateMainProgressBar(hDlg, 0, L"Failed to retrieve video information");
                    // Keep progress bar visible to show failure status
                    
                    // Clear any existing video info
                    UpdateVideoInfoUI(hDlg, L"", L"");
                    
                    ShowWarningMessage(hDlg, L"Information Retrieval Failed", 
                        L"Could not retrieve video information. Please check:\n\n"
                        L"• The URL is valid and accessible\n"
                        L"• yt-dlp is properly installed and configured\n"
                        L"• You have an internet connection\n"
                        L"• The video is not private or restricted");
                }
                
                // Cleanup thread data
                if (data->hThread) {
                    CloseHandle(data->hThread);
                }
                free(data);
            }
            return TRUE;
        }
        

        
        case WM_DOWNLOAD_COMPLETE: {
            OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE message received\n");
            
            // Handle download completion
            YtDlpResult* result = (YtDlpResult*)wParam;
            NonBlockingDownloadContext* downloadContext = (NonBlockingDownloadContext*)lParam;
            
            if (!result) {
                OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - NULL result\n");
                return TRUE;
            }
            
            if (!downloadContext) {
                OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - NULL downloadContext\n");
                return TRUE;
            }
            
            wchar_t debugMsg[256];
            swprintf(debugMsg, 256, L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - success=%d, exitCode=%d\n", 
                    result->success, result->exitCode);
            OutputDebugStringW(debugMsg);
            
            HandleDownloadCompletion(hDlg, result, downloadContext);
            return TRUE;
        }
        
        case WM_UNIFIED_DOWNLOAD_UPDATE: {
            int updateType = (int)wParam;
            LPARAM data = lParam;
            
            switch (updateType) {
                case 1: { // Update video title
                    wchar_t* title = (wchar_t*)data;
                    if (title) {
                        // Debug: Log the title received via message
                        wchar_t debugMsg[1024];
                        swprintf(debugMsg, 1024, L"YouTubeCacher: Received title via message: %ls (length: %zu)\n", title, wcslen(title));
                        OutputDebugStringW(debugMsg);
                        
                        // Debug: Log individual characters
                        OutputDebugStringW(L"YouTubeCacher: Message title character codes: ");
                        for (size_t i = 0; i < min(wcslen(title), 20); i++) {
                            swprintf(debugMsg, 1024, L"U+%04X ", (unsigned int)title[i]);
                            OutputDebugStringW(debugMsg);
                        }
                        OutputDebugStringW(L"\n");
                        
                        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, title);
                        free(title);
                    }
                    break;
                }
                case 2: { // Update video duration
                    wchar_t* duration = (wchar_t*)data;
                    if (duration) {
                        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, duration);
                        free(duration);
                    }
                    break;
                }
                case 3: { // Update progress percentage
                    int percentage = (int)data;
                    if (percentage == -1) {
                        // Indeterminate progress - use marquee mode
                        SetProgressBarMarquee(hDlg, TRUE);
                    } else {
                        // Determinate progress - use percentage mode
                        UpdateMainProgressBar(hDlg, percentage, NULL);
                    }
                    break;
                }
                case 4: { // Start marquee
                    SetProgressBarMarquee(hDlg, TRUE);
                    break;
                }
                case 5: { // Update status text
                    wchar_t* status = (wchar_t*)data;
                    if (status) {
                        SetDlgItemTextW(hDlg, IDC_PROGRESS_TEXT, status);
                        free(status);
                    }
                    break;
                }
                case 6: { // Stop marquee
                    SetProgressBarMarquee(hDlg, FALSE);
                    break;
                }
                case 7: { // Download failed
                    UpdateMainProgressBar(hDlg, 0, L"Download failed");
                    SetDownloadUIState(hDlg, FALSE);
                    Sleep(500);
                    ShowMainProgressBar(hDlg, FALSE);
                    break;
                }
            }
            return TRUE;
        }
            
        case WM_DESTROY:
            // Write session end marker for clean shutdown
            WriteSessionEndToLogfile(L"Clean program shutdown");
            
            // Clean up cache manager
            CleanupCacheManager(GetCacheManager());
            
            // Clean up ListView item data
            CleanupListViewItemData(GetDlgItem(hDlg, IDC_LIST));
            
            // Clean up application state (including brushes)
            ApplicationState* state = GetApplicationState();
            if (state) {
                CleanupApplicationState(state);
            }
            
            PostQuitMessage(0);
            return TRUE;
    }
    return FALSE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    // Initialize error logging
    InitializeErrorLogging();
    
    // Force visual styles activation before anything else
    ForceVisualStylesActivation();
    
    // Initialize Common Controls v6 for modern Windows theming
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS |      // Progress bars
                 ICC_LISTVIEW_CLASSES |    // List boxes and list views
                 ICC_TAB_CLASSES |         // Tab controls (for error dialog)
                 ICC_UPDOWN_CLASS |        // Up-down controls
                 ICC_BAR_CLASSES |         // Toolbar, status bar, trackbar, tooltip
                 ICC_STANDARD_CLASSES |    // Button, edit, static, listbox, combobox, scrollbar
                 ICC_WIN95_CLASSES;        // All Win95 common controls
    
    if (!InitCommonControlsEx(&icex)) {
        // Fallback to basic initialization if extended version fails
        InitCommonControls();
    }
    
    // Enable Windows XP+ Visual Styles programmatically (backup to manifest)
    // This ensures theming works even if manifest processing fails
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme) {
        // Try to enable theming programmatically as backup
        typedef BOOL (WINAPI *SetThemeAppPropertiesFunc)(DWORD);
        SetThemeAppPropertiesFunc SetThemeAppProperties = 
            (SetThemeAppPropertiesFunc)(void*)GetProcAddress(hUxTheme, "SetThemeAppProperties");
        
        if (SetThemeAppProperties) {
            // Enable all theming properties
            SetThemeAppProperties(0x7); // STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT
        }
        
        FreeLibrary(hUxTheme);
    }
    
    // Also try the older method for compatibility
    typedef BOOL (WINAPI *InitCommonControlsExFunc)(LPINITCOMMONCONTROLSEX);
    HMODULE hComCtl32 = LoadLibraryW(L"comctl32.dll");
    if (hComCtl32) {
        InitCommonControlsExFunc InitCommonControlsExPtr = 
            (InitCommonControlsExFunc)(void*)GetProcAddress(hComCtl32, "InitCommonControlsEx");
        
        if (InitCommonControlsExPtr) {
            // Re-initialize with visual styles
            INITCOMMONCONTROLSEX icex2;
            icex2.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex2.dwICC = ICC_WIN95_CLASSES;
            InitCommonControlsExPtr(&icex2);
        }
        
        FreeLibrary(hComCtl32);
    }
    
    // Enable DPI awareness for HiDPI support
    typedef BOOL (WINAPI *SetProcessDPIAwareFunc)(void);
    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    if (hUser32) {
        FARPROC proc = GetProcAddress(hUser32, "SetProcessDPIAware");
        if (proc) {
            SetProcessDPIAwareFunc SetProcessDPIAware = (SetProcessDPIAwareFunc)(void*)proc;
            SetProcessDPIAware();
        }
        FreeLibrary(hUser32);
    }
    
    // Initialize the global IPC system for efficient cross-thread communication
    if (!InitializeGlobalIPC()) {
        MessageBoxW(NULL, L"Failed to initialize inter-process communication system.", 
                   L"Initialization Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Check if command line contains YouTube URL
    if (lpCmdLine && wcslen(lpCmdLine) > 0 && IsYouTubeURL(lpCmdLine)) {
        SetCommandLineURL(lpCmdLine);
    }
    
    // Load accelerator table
    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_MAIN_MENU));
    
    // Create the dialog as modeless with explicit theming support
    HWND hDlg = CreateThemedDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, DialogProc);
    if (hDlg == NULL) {
        return 0;
    }
    
    ShowWindow(hDlg, nCmdShow);
    
    // Initialize and validate yt-dlp system after UI is shown
    InitializeYtDlpSystem(hDlg);
    
    // Message loop with accelerator handling
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hDlg, hAccel, &msg) && !IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // Cleanup the global IPC system
    CleanupGlobalIPC();
    
    // Cleanup error logging
    CleanupErrorLogging();
    
    return (int)msg.wParam;
}

// ANSI entry point wrapper for Unicode main function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Convert ANSI command line to Unicode
    wchar_t* lpCmdLineW = NULL;
    if (lpCmdLine && strlen(lpCmdLine) > 0) {
        int len = MultiByteToWideChar(CP_UTF8, 0, lpCmdLine, -1, NULL, 0);
        if (len > 0) {
            lpCmdLineW = (wchar_t*)malloc(len * sizeof(wchar_t));
            if (lpCmdLineW) {
                MultiByteToWideChar(CP_UTF8, 0, lpCmdLine, -1, lpCmdLineW, len);
            }
        }
    }
    
    // Call Unicode main function
    int result = wWinMain(hInstance, hPrevInstance, lpCmdLineW ? lpCmdLineW : L"", nCmdShow);
    
    // Clean up
    if (lpCmdLineW) {
        free(lpCmdLineW);
    }
    
    return result;
}
