#ifndef UI_H
#define UI_H

#include <windows.h>
#include <commctrl.h>

// UI management function prototypes
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ProgressDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TextFieldSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// UI utility functions
void ResizeControls(HWND hDlg);
void ApplyModernThemeToDialog(HWND hDlg);
void ApplyDelayedTheming(HWND hDlg);
void ForceVisualStylesActivation(void);
HWND CreateThemedDialog(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc);
void UpdateDebugControlVisibility(HWND hDlg);
void CheckClipboardForYouTubeURL(HWND hDlg);

// Progress and status functions
void UpdateVideoInfoUI(HWND hDlg, const wchar_t* title, const wchar_t* duration);
void SetDownloadUIState(HWND hDlg, BOOL isDownloading);
void UpdateMainProgressBar(HWND hDlg, int percentage, const wchar_t* status);
void ShowMainProgressBar(HWND hDlg, BOOL show);
void SetProgressBarMarquee(HWND hDlg, BOOL enable);

// Window sizing functions
void CalculateMinimumWindowSize(int* minWidth, int* minHeight, double dpiScaleX, double dpiScaleY);
void CalculateDefaultWindowSize(int* defaultWidth, int* defaultHeight, double dpiScaleX, double dpiScaleY);



// Unified dialog function - single entry point for all dialog types
INT_PTR ShowUnifiedDialog(HWND parent, const UnifiedDialogConfig* config);
INT_PTR CALLBACK UnifiedDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void ResizeUnifiedDialog(HWND hDlg, BOOL expanded);
void ShowUnifiedDialogTab(HWND hDlg, int tabIndex);
BOOL CopyUnifiedDialogToClipboard(const UnifiedDialogConfig* config);
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
INT_PTR ShowMemoryError(HWND parent, const wchar_t* operation);
INT_PTR ShowConfigurationError(HWND parent, const wchar_t* details);
INT_PTR ShowUIError(HWND parent, const wchar_t* operation);

void ShowAboutDialog(HWND parent);
void ShowLogViewerDialog(HWND parent);

// Error logging functions
BOOL InitializeErrorLogging(void);
void LogError(const wchar_t* category, const wchar_t* message, const wchar_t* details);
void LogWarning(const wchar_t* category, const wchar_t* message);
void LogInfo(const wchar_t* category, const wchar_t* message);
void CleanupErrorLogging(void);

#endif // UI_H