#ifndef YOUTUBECACHER_H
#define YOUTUBECACHER_H

#include <windows.h>
#include "resource.h"

// Application constants
#define APP_NAME            "YouTube Cacher"
#define APP_VERSION         "1.0"
#define MAX_URL_LENGTH      1024
#define MAX_BUFFER_SIZE     1024

// Window sizing constants
#define MIN_WINDOW_WIDTH    500
#define MIN_WINDOW_HEIGHT   350
#define DEFAULT_WIDTH       550
#define DEFAULT_HEIGHT      400

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
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Global variables (extern declarations)
extern char cmdLineURL[MAX_URL_LENGTH];
extern HBRUSH hBrushWhite;
extern HBRUSH hBrushLightGreen;
extern HBRUSH hBrushLightBlue;
extern HBRUSH hBrushLightTeal;
extern HBRUSH hCurrentBrush;

#endif // YOUTUBECACHER_H