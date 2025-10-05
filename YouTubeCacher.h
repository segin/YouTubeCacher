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

// Function prototypes
void CheckClipboardForYouTubeURL(HWND hDlg);
void ResizeControls(HWND hDlg);
void GetDefaultDownloadPath(wchar_t* path, size_t pathSize);
void GetDefaultYtDlpPath(wchar_t* path, size_t pathSize);
BOOL CreateDownloadDirectoryIfNeeded(const wchar_t* path);
BOOL ValidateYtDlpExecutable(const wchar_t* path);
BOOL LoadSettingFromRegistry(const wchar_t* valueName, wchar_t* buffer, DWORD bufferSize);
BOOL SaveSettingToRegistry(const wchar_t* valueName, const wchar_t* value);
void LoadSettings(HWND hDlg);
void SaveSettings(HWND hDlg);
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Global variables (extern declarations)
extern wchar_t cmdLineURL[MAX_URL_LENGTH];
extern HBRUSH hBrushWhite;
extern HBRUSH hBrushLightGreen;
extern HBRUSH hBrushLightBlue;
extern HBRUSH hBrushLightTeal;
extern HBRUSH hCurrentBrush;
extern BOOL bProgrammaticChange;

#endif // YOUTUBECACHER_H