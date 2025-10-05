#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include "YouTubeCacher.h"
#include "uri.h"
#include <commdlg.h>
#include <shlobj.h>
#include <commctrl.h>
#include <knownfolders.h>

// Global variable for command line URL
wchar_t cmdLineURL[MAX_URL_LENGTH] = {0};

// Global variables for text field colors
HBRUSH hBrushWhite = NULL;
HBRUSH hBrushLightGreen = NULL;
HBRUSH hBrushLightBlue = NULL;
HBRUSH hBrushLightTeal = NULL;
HBRUSH hCurrentBrush = NULL;

// Function to get the default download path (Downloads/YouTubeCacher)
void GetDefaultDownloadPath(wchar_t* path, size_t pathSize) {
    PWSTR downloadsPathW = NULL;
    wchar_t downloadsPath[MAX_EXTENDED_PATH];
    
    // Get the user's Downloads folder using the proper FOLDERID_Downloads
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_Downloads, 0, NULL, &downloadsPathW);
    if (SUCCEEDED(hr) && downloadsPathW != NULL) {
        // Copy the wide string directly
        wcsncpy(downloadsPath, downloadsPathW, MAX_EXTENDED_PATH - 1);
        downloadsPath[MAX_EXTENDED_PATH - 1] = L'\0';
        CoTaskMemFree(downloadsPathW);
    } else {
        // Fallback to user profile + Downloads
        if (GetEnvironmentVariableW(L"USERPROFILE", downloadsPath, MAX_EXTENDED_PATH) > 0) {
            wcscat(downloadsPath, L"\\Downloads");
        } else {
            // Ultimate fallback
            wcscpy(downloadsPath, L"C:\\Users\\Public\\Downloads");
        }
    }
    
    // Append YouTubeCacher subdirectory
    swprintf(path, pathSize, L"%s\\YouTubeCacher", downloadsPath);
}

// Function to get the default yt-dlp path (check WinGet installation)
void GetDefaultYtDlpPath(wchar_t* path, size_t pathSize) {
    wchar_t localAppData[MAX_EXTENDED_PATH];
    wchar_t ytDlpPath[MAX_EXTENDED_PATH];
    
    // Initialize path as empty
    path[0] = L'\0';
    
    // Get %LocalAppData% environment variable
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_EXTENDED_PATH) > 0) {
        // Construct the WinGet yt-dlp path
        int result = swprintf(ytDlpPath, MAX_EXTENDED_PATH, 
                L"%s\\Microsoft\\WinGet\\Packages\\yt-dlp.yt-dlp_Microsoft.Winget.Source_8wekyb3d8bbwe\\yt-dlp.exe",
                localAppData);
        
        // Check if path was truncated
        if (result > 0 && (size_t)result < MAX_EXTENDED_PATH) {
            // Check if the file exists
            DWORD attributes = GetFileAttributesW(ytDlpPath);
            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                // File exists and is not a directory, copy to output
                wcsncpy(path, ytDlpPath, pathSize - 1);
                path[pathSize - 1] = L'\0';
            }
        }
    }
}

// Function to create the download directory if it doesn't exist
BOOL CreateDownloadDirectoryIfNeeded(const wchar_t* path) {
    DWORD attributes = GetFileAttributesW(path);
    
    // If directory doesn't exist, create it
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return CreateDirectoryW(path, NULL);
    }
    
    // If it exists and is a directory, return success
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        return TRUE;
    }
    
    // If it exists but is not a directory, return failure
    return FALSE;
}



void CheckClipboardForYouTubeURL(HWND hDlg) {
    // Only check clipboard if text field is empty
    wchar_t currentText[MAX_BUFFER_SIZE];
    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, currentText, MAX_BUFFER_SIZE);
    
    if (wcslen(currentText) == 0) {
        if (OpenClipboard(hDlg)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData != NULL) {
                wchar_t* clipText = (wchar_t*)GlobalLock(hData);
                if (clipText != NULL && IsYouTubeURL(clipText)) {
                    SetDlgItemTextW(hDlg, IDC_TEXT_FIELD, clipText);
                    hCurrentBrush = hBrushLightGreen;
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
    }
}

void ResizeControls(HWND hDlg) {
    RECT rect;
    GetClientRect(hDlg, &rect);
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    // Calculate button position with proper margin
    int buttonX = width - 90;
    int sideButtonX = width - 90;
    
    // Resize Download video group box (taller to accommodate progress bar)
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_GROUP), NULL, 10, 10, width - 20, 110, SWP_NOZORDER);
    
    // Position URL label (within download group) - moved down to align better
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL1), NULL, 20, 32, 30, 16, SWP_NOZORDER);
    
    // Resize URL text field (within download group) - aligned with label
    SetWindowPos(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, 55, 30, buttonX - 65, 22, SWP_NOZORDER);
    
    // Position progress bar below URL field
    SetWindowPos(GetDlgItem(hDlg, IDC_PROGRESS_BAR), NULL, 55, 58, buttonX - 65, 16, SWP_NOZORDER);
    
    // Position URL buttons (within download group) - aligned with text field and progress bar
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_BTN), NULL, buttonX, 28, BUTTON_WIDTH, 26, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_GETINFO_BTN), NULL, buttonX, 58, BUTTON_WIDTH, 26, SWP_NOZORDER);
    
    // Resize Offline videos group box (moved down to accommodate larger download group)
    int offlineGroupY = 130;
    SetWindowPos(GetDlgItem(hDlg, IDC_OFFLINE_GROUP), NULL, 10, offlineGroupY, width - 20, height - offlineGroupY - 10, SWP_NOZORDER);
    
    // Position the status labels at the top of the offline group (inside the group box)
    int labelY = offlineGroupY + 18;  // 18 pixels below group box top (accounting for group box border/title)
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL2), NULL, 20, labelY, 150, 16, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL3), NULL, 200, labelY, 100, 16, SWP_NOZORDER);
    
    // Position listbox below the labels with more spacing
    int listY = labelY + 37;  // 37 pixels below labels (15 more pixels for label space)
    int listHeight = height - listY - 20;  // 20 pixels from bottom
    SetWindowPos(GetDlgItem(hDlg, IDC_LIST), NULL, 20, listY, sideButtonX - 30, listHeight, SWP_NOZORDER);
    
    // Position side buttons aligned with the listbox
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON2), NULL, sideButtonX, listY, BUTTON_WIDTH, 32, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON3), NULL, sideButtonX, listY + 37, BUTTON_WIDTH, 32, SWP_NOZORDER);
}

// Settings dialog procedure
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    switch (message) {
        case WM_INITDIALOG: {
            // Set default yt-dlp path (check WinGet installation)
            wchar_t defaultYtDlpPath[MAX_EXTENDED_PATH];
            GetDefaultYtDlpPath(defaultYtDlpPath, MAX_EXTENDED_PATH);
            SetDlgItemTextW(hDlg, IDC_YTDLP_PATH, defaultYtDlpPath);
            
            // Set default download folder path
            wchar_t defaultDownloadPath[MAX_EXTENDED_PATH];
            GetDefaultDownloadPath(defaultDownloadPath, MAX_EXTENDED_PATH);
            SetDlgItemTextW(hDlg, IDC_FOLDER_PATH, defaultDownloadPath);
            
            // Set default media player path
            SetDlgItemTextW(hDlg, IDC_PLAYER_PATH, L"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe");
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
                
                case IDOK:
                    // TODO: Save settings to registry or config file
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
            SetDlgItemTextW(hDlg, IDC_LABEL2, L"Status: Ready");
            SetDlgItemTextW(hDlg, IDC_LABEL3, L"Items: 0");
            
            // Add some sample items to the listbox
            SendDlgItemMessageW(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)L"Sample Video 1.mp4");
            SendDlgItemMessageW(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)L"Sample Video 2.mp4");
            SendDlgItemMessageW(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)L"Sample Video 3.mp4");
            
            // Check command line first, then clipboard
            if (wcslen(cmdLineURL) > 0) {
                SetDlgItemTextW(hDlg, IDC_TEXT_FIELD, cmdLineURL);
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
                if (hCurrentBrush == hBrushLightGreen) {
                    SetBkColor(hdc, COLOR_LIGHT_GREEN);
                } else if (hCurrentBrush == hBrushLightBlue) {
                    SetBkColor(hdc, COLOR_LIGHT_BLUE);
                } else if (hCurrentBrush == hBrushLightTeal) {
                    SetBkColor(hdc, COLOR_LIGHT_TEAL);
                } else {
                    SetBkColor(hdc, COLOR_WHITE);
                }
                return (INT_PTR)hCurrentBrush;
            }
            break;
            

            
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
                    
                case IDC_TEXT_FIELD:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // Check if it's a paste operation with YouTube URL
                        wchar_t buffer[MAX_BUFFER_SIZE];
                        GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, buffer, MAX_BUFFER_SIZE);
                        
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
                    
                case IDC_DOWNLOAD_BTN: {
                    // Get the current download folder path from settings (for now use default)
                    wchar_t downloadPath[MAX_EXTENDED_PATH];
                    GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
                    
                    // Create the download directory if it doesn't exist
                    if (!CreateDownloadDirectoryIfNeeded(downloadPath)) {
                        MessageBoxW(hDlg, L"Failed to create download directory", L"Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    // TODO: Implement actual download functionality
                    wchar_t message[MAX_EXTENDED_PATH + 100];
                    swprintf(message, MAX_EXTENDED_PATH + 100, L"Download directory ready: %s\n\nDownload functionality not implemented yet", downloadPath);
                    MessageBoxW(hDlg, message, L"Download", MB_OK);
                    break;
                }
                    
                case IDC_GETINFO_BTN:
                    MessageBoxW(hDlg, L"Get Info functionality not implemented yet", L"Get Info", MB_OK);
                    break;
                    
                case IDC_BUTTON2:
                    MessageBoxW(hDlg, L"Play functionality not implemented yet", L"Play", MB_OK);
                    break;
                    
                case IDC_BUTTON3:
                    MessageBoxW(hDlg, L"Delete functionality not implemented yet", L"Delete", MB_OK);
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    // Initialize common controls for progress bar
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
    
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
    
    // Check if command line contains YouTube URL
    if (lpCmdLine && wcslen(lpCmdLine) > 0 && IsYouTubeURL(lpCmdLine)) {
        wcsncpy(cmdLineURL, lpCmdLine, MAX_URL_LENGTH - 1);
        cmdLineURL[MAX_URL_LENGTH - 1] = L'\0';
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