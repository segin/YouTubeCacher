#include <windows.h>
#include <string.h>
#include "uri.h"

#define IDC_TEXT_FIELD    1001
#define IDC_LABEL1        1002
#define IDC_LABEL2        1003
#define IDC_LABEL3        1004
#define IDC_LIST          1005
#define IDC_BUTTON1       1006
#define IDC_BUTTON2       1007
#define IDC_BUTTON3       1008
#define IDC_DOWNLOAD_BTN  1009
#define IDC_GETINFO_BTN   1010

#define IDD_MAIN_DIALOG   101

// Global variable for command line URL
char cmdLineURL[1024] = {0};

// Global variables for text field colors
HBRUSH hBrushWhite = NULL;
HBRUSH hBrushLightGreen = NULL;
HBRUSH hBrushLightBlue = NULL;
HBRUSH hBrushLightTeal = NULL;
HBRUSH hCurrentBrush = NULL;



void CheckClipboardForYouTubeURL(HWND hDlg) {
    if (OpenClipboard(hDlg)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData != NULL) {
            char* clipText = (char*)GlobalLock(hData);
            if (clipText != NULL && IsYouTubeURL(clipText)) {
                SetDlgItemText(hDlg, IDC_TEXT_FIELD, clipText);
                hCurrentBrush = hBrushLightGreen;
                InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
            }
            GlobalUnlock(hData);
        }
        CloseClipboard();
    }
}

void ResizeControls(HWND hDlg) {
    RECT rect;
    GetClientRect(hDlg, &rect);
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    // Resize URL text field (leave space for buttons)
    SetWindowPos(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, 45, 12, width - 200, 20, SWP_NOZORDER);
    
    // Position URL buttons (aligned to right)
    int buttonX = width - 145;
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_BTN), NULL, buttonX, 10, 70, 24, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_GETINFO_BTN), NULL, buttonX, 36, 70, 24, SWP_NOZORDER);
    
    // Resize listbox (leave space for side buttons)
    SetWindowPos(GetDlgItem(hDlg, IDC_LIST), NULL, 10, 90, width - 100, height - 100, SWP_NOZORDER);
    
    // Position side buttons (aligned to right)
    int sideButtonX = width - 80;
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON2), NULL, sideButtonX, 90, 70, 30, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON3), NULL, sideButtonX, 125, 70, 30, SWP_NOZORDER);
}

// Dialog procedure function
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            // Create brushes for text field colors
            hBrushWhite = CreateSolidBrush(RGB(255, 255, 255));
            hBrushLightGreen = CreateSolidBrush(RGB(220, 255, 220));
            hBrushLightBlue = CreateSolidBrush(RGB(220, 220, 255));
            hBrushLightTeal = CreateSolidBrush(RGB(220, 255, 255));
            hCurrentBrush = hBrushWhite;
            
            // Initialize dialog controls
            SetDlgItemText(hDlg, IDC_LABEL2, "Status: Ready");
            SetDlgItemText(hDlg, IDC_LABEL3, "Items: 0");
            
            // Add some sample items to the listbox
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)"Sample Video 1.mp4");
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)"Sample Video 2.mp4");
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)"Sample Video 3.mp4");
            
            // Check command line first, then clipboard
            if (strlen(cmdLineURL) > 0) {
                SetDlgItemText(hDlg, IDC_TEXT_FIELD, cmdLineURL);
                hCurrentBrush = hBrushLightTeal;
                InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
            } else {
                // Check clipboard for YouTube URL
                CheckClipboardForYouTubeURL(hDlg);
            }
            
            // Set minimum window size
            SetWindowPos(hDlg, NULL, 0, 0, 550, 400, SWP_NOMOVE | SWP_NOZORDER);
            
            return TRUE;
        }
            
        case WM_SIZE:
            ResizeControls(hDlg);
            return TRUE;
            
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 500;
            mmi->ptMinTrackSize.y = 350;
            return 0;
        }
        
        case WM_CTLCOLOREDIT:
            if ((HWND)lParam == GetDlgItem(hDlg, IDC_TEXT_FIELD)) {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
                return (INT_PTR)hCurrentBrush;
            }
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_TEXT_FIELD:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // Check if it's a paste operation with YouTube URL
                        char buffer[1024];
                        GetDlgItemText(hDlg, IDC_TEXT_FIELD, buffer, sizeof(buffer));
                        
                        if (IsYouTubeURL(buffer) && hCurrentBrush != hBrushLightGreen) {
                            // Manual paste of YouTube URL - set to light blue
                            hCurrentBrush = hBrushLightBlue;
                        } else if (hCurrentBrush != hBrushLightGreen) {
                            // Manual input - set to white
                            hCurrentBrush = hBrushWhite;
                        }
                        InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    }
                    break;
                    
                case IDC_DOWNLOAD_BTN:
                    MessageBox(hDlg, "Download functionality not implemented yet", "Download", MB_OK);
                    break;
                    
                case IDC_GETINFO_BTN:
                    MessageBox(hDlg, "Get Info functionality not implemented yet", "Get Info", MB_OK);
                    break;
                    
                case IDC_BUTTON2:
                    MessageBox(hDlg, "Play functionality not implemented yet", "Play", MB_OK);
                    break;
                    
                case IDC_BUTTON3:
                    MessageBox(hDlg, "Delete functionality not implemented yet", "Delete", MB_OK);
                    break;
                    
                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            // Clean up brushes
            if (hBrushWhite) DeleteObject(hBrushWhite);
            if (hBrushLightGreen) DeleteObject(hBrushLightGreen);
            if (hBrushLightBlue) DeleteObject(hBrushLightBlue);
            if (hBrushLightTeal) DeleteObject(hBrushLightTeal);
            
            EndDialog(hDlg, 0);
            return TRUE;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    // Check if command line contains YouTube URL
    if (lpCmdLine && strlen(lpCmdLine) > 0 && IsYouTubeURL(lpCmdLine)) {
        strncpy(cmdLineURL, lpCmdLine, sizeof(cmdLineURL) - 1);
        cmdLineURL[sizeof(cmdLineURL) - 1] = '\0';
    }
    
    // Create and show the dialog
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, DialogProc);
    
    return 0;
}