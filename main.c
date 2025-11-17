#include "YouTubeCacher.h"

// MinGW32 provides _wcsdup and _wcsicmp, no custom implementation needed

// TextFieldSubclassProc moved to ui.c



// WriteToLogfile moved to log.c

// WriteSessionStartToLogfile moved to log.c

// WriteSessionEndToLogfile moved to log.c

// DebugOutput moved to log.c



// UpdateDebugControlVisibility moved to ui.c

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

// GetSubprocessResult moved to ytdlp.c

// FreeSubprocessContext moved to ytdlp.c

// SubprocessProgressCallback moved to threading.c

// ExecuteYtDlpRequestMultithreaded moved to ytdlp.c



// Custom window messages moved to ytdlp.h

// UnifiedDownloadContext moved to YouTubeCacher.h

// UnifiedDownloadWorkerThread moved to ytdlp.c

// UnifiedDownloadProgressCallback moved to threading.c

// StartUnifiedDownload moved to ytdlp.c

// NonBlockingDownloadThread moved to ytdlp.c

// StartNonBlockingDownload moved to ytdlp.c

// HandleDownloadCompletion moved to threading.c

// VideoInfoThreadData moved to ytdlp.h

// GetVideoInfoThread moved to ytdlp.c

// VideoInfoThread moved to ytdlp.h

// VideoInfoWorkerThread moved to ytdlp.c

// GetVideoTitleAndDurationSync moved to ytdlp.c

// GetVideoTitleAndDuration moved to ytdlp.c

// UpdateVideoInfoUI moved to ui.c

// SetDownloadUIState moved to ui.c

// UpdateMainProgressBar moved to ui.c

// ShowMainProgressBar moved to ui.c

// SetProgressBarMarquee moved to ui.c

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

// InitializeYtDlpSystem moved to ytdlp.c

// LoadYtDlpConfig and SaveYtDlpConfig moved to ytdlp.c

// TestYtDlpFunctionality moved to ytdlp.c

// ValidateYtDlpConfiguration moved to ytdlp.c

// MigrateYtDlpConfiguration moved to ytdlp.c

// SetupDefaultYtDlpConfiguration moved to ytdlp.c

// NotifyConfigurationIssues moved to ytdlp.c





// CheckClipboardForYouTubeURL moved to ui.c

// Window sizing functions moved to ui.c

// Theming functions moved to ui.c

// ResizeControls moved to ui.c

// Dialog procedures moved to ui.c


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    // Initialize error logging
    InitializeErrorLogging();
    
    // Initialize memory manager for leak detection and safe allocation
    if (!InitializeMemoryManager()) {
        SHOW_ERROR_DIALOG(NULL, YTC_SEVERITY_FATAL, YTC_ERROR_INITIALIZATION, 
                         L"Failed to initialize memory management system.\r\n\r\n"
                         L"The application cannot continue without proper memory management.");
        return 1;
    }
    
    // Initialize thread safety system for global state protection
    if (!InitializeThreadSafety()) {
        SHOW_ERROR_DIALOG(NULL, YTC_SEVERITY_FATAL, YTC_ERROR_INITIALIZATION, 
                         L"Failed to initialize thread safety system.\r\n\r\n"
                         L"The application cannot continue without proper thread synchronization.");
        CleanupMemoryManager();
        CleanupErrorLogging();
        return 1;
    }
    
    // REMOVED: TestMemoryAllocationFailureScenarios() - was slowing startup with debug output
    
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
    
    // SIMPLIFIED: Removed excessive DLL loading that was slowing startup
    // The manifest and InitCommonControlsEx above should be sufficient for theming
    
    // Enable DPI awareness with per-monitor v2 support and fallbacks
    InitializeDPIAwareness();
    
    // Initialize global DPI manager for per-monitor DPI tracking
    g_dpiManager = CreateDPIManager();
    if (!g_dpiManager) {
        SHOW_ERROR_DIALOG(NULL, YTC_SEVERITY_WARNING, YTC_ERROR_INITIALIZATION, 
                         L"Failed to initialize DPI management system.\r\n\r\n"
                         L"The application will continue but may not scale properly on high-DPI displays.");
    }
    
    // Initialize the global IPC system for efficient cross-thread communication
    if (!InitializeGlobalIPC()) {
        SHOW_ERROR_DIALOG(NULL, YTC_SEVERITY_FATAL, YTC_ERROR_INITIALIZATION, 
                         L"Failed to initialize inter-process communication system.\r\n\r\n"
                         L"The application cannot continue without proper IPC functionality.");
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
    
    // Cleanup DPI manager
    if (g_dpiManager) {
        DestroyDPIManager(g_dpiManager);
        g_dpiManager = NULL;
    }
    
    // Cleanup thread safety system
    CleanupThreadSafety();
    
    // Cleanup memory manager
    CleanupMemoryManager();
    
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
            lpCmdLineW = (wchar_t*)SAFE_MALLOC(len * sizeof(wchar_t));
            if (lpCmdLineW) {
                MultiByteToWideChar(CP_UTF8, 0, lpCmdLine, -1, lpCmdLineW, len);
            }
        }
    }
    
    // Call Unicode main function
    int result = wWinMain(hInstance, hPrevInstance, lpCmdLineW ? lpCmdLineW : L"", nCmdShow);
    
    // Clean up
    if (lpCmdLineW) {
        SAFE_FREE(lpCmdLineW);
    }
    
    return result;
}
