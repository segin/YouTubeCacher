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

#endif // UI_H