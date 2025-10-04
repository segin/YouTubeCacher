#include <string.h>
#include "YouTubeCacher.h"
#include "uri.h"

// Global variable for command line URL
char cmdLineURL[MAX_URL_LENGTH] = {0};

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
    
    // Calculate shared button position (same padding as list buttons)
    int buttonX = width - BUTTON_PADDING;
    
    // Resize URL text field (leave space for buttons with same padding)
    SetWindowPos(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, 45, 12, buttonX - 55, TEXT_FIELD_HEIGHT, SWP_NOZORDER);
    
    // Position URL buttons (same X position as list buttons)
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_BTN), NULL, buttonX, 10, BUTTON_WIDTH, BUTTON_HEIGHT_SMALL, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_GETINFO_BTN), NULL, buttonX, 36, BUTTON_WIDTH, BUTTON_HEIGHT_SMALL, SWP_NOZORDER);
    
    // Resize listbox (leave space for side buttons)
    SetWindowPos(GetDlgItem(hDlg, IDC_LIST), NULL, 10, 70, buttonX - 20, height - 80, SWP_NOZORDER);
    
    // Position side buttons (same X position as URL buttons)
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON2), NULL, buttonX, 70, BUTTON_WIDTH, BUTTON_HEIGHT_LARGE, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON3), NULL, buttonX, 105, BUTTON_WIDTH, BUTTON_HEIGHT_LARGE, SWP_NOZORDER);
}

// Dialog procedure function
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            // Create brushes for text field colors
            hBrushWhite = CreateSolidBrush(COLOR_WHITE);
            hBrushLightGreen = CreateSolidBrush(COLOR_LIGHT_GREEN);
            hBrushLightBlue = CreateSolidBrush(COLOR_LIGHT_BLUE);
            hBrushLightTeal = CreateSolidBrush(COLOR_LIGHT_TEAL);
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
            
            // Set focus to the text field for immediate typing
            SetFocus(GetDlgItem(hDlg, IDC_TEXT_FIELD));
            
            // Set minimum window size
            SetWindowPos(hDlg, NULL, 0, 0, DEFAULT_WIDTH, DEFAULT_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
            
            return FALSE; // Return FALSE since we set focus manually
        }
            
        case WM_SIZE:
            ResizeControls(hDlg);
            return TRUE;
            
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = MIN_WINDOW_WIDTH;
            mmi->ptMinTrackSize.y = MIN_WINDOW_HEIGHT;
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
                case ID_FILE_EXIT:
                    DestroyWindow(hDlg);
                    return TRUE;
                    
                case IDC_TEXT_FIELD:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // Check if it's a paste operation with YouTube URL
                        char buffer[MAX_BUFFER_SIZE];
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
                    DestroyWindow(hDlg);
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            // Clean up brushes
            if (hBrushWhite) DeleteObject(hBrushWhite);
            if (hBrushLightGreen) DeleteObject(hBrushLightGreen);
            if (hBrushLightBlue) DeleteObject(hBrushLightBlue);
            if (hBrushLightTeal) DeleteObject(hBrushLightTeal);
            
            DestroyWindow(hDlg);
            return TRUE;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return TRUE;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    // Check if command line contains YouTube URL
    if (lpCmdLine && strlen(lpCmdLine) > 0 && IsYouTubeURL(lpCmdLine)) {
        strncpy(cmdLineURL, lpCmdLine, MAX_URL_LENGTH - 1);
        cmdLineURL[MAX_URL_LENGTH - 1] = '\0';
    }
    
    // Load accelerator table
    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_MAIN_MENU));
    
    // Create the dialog as modeless to handle accelerators
    HWND hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, DialogProc);
    if (hDlg == NULL) {
        return 0;
    }
    
    ShowWindow(hDlg, nCmdShow);
    
    // Message loop with accelerator handling
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hDlg, hAccel, &msg) && !IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    return (int)msg.wParam;
}