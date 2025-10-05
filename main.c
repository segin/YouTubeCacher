#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include "YouTubeCacher.h"
#include "uri.h"
#include <commdlg.h>
#include <shlobj.h>
#include <commctrl.h>
#include <knownfolders.h>
#include <tlhelp32.h>
#include <psapi.h>

// Function declarations for missing functions
wchar_t* _wcsdup(const wchar_t* str);
int _wcsicmp(const wchar_t* str1, const wchar_t* str2);

// Implementation of missing functions
wchar_t* _wcsdup(const wchar_t* str) {
    if (!str) return NULL;
    size_t len = wcslen(str) + 1;
    wchar_t* copy = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (copy) {
        wcscpy(copy, str);
    }
    return copy;
}

int _wcsicmp(const wchar_t* str1, const wchar_t* str2) {
    if (!str1 || !str2) return (str1 == str2) ? 0 : (str1 ? 1 : -1);
    
    while (*str1 && *str2) {
        wchar_t c1 = towlower(*str1);
        wchar_t c2 = towlower(*str2);
        if (c1 != c2) return (c1 < c2) ? -1 : 1;
        str1++;
        str2++;
    }
    return (*str1 == *str2) ? 0 : (*str1 ? 1 : -1);
}

// Global variable for command line URL
wchar_t cmdLineURL[MAX_URL_LENGTH] = {0};

// Global variables for text field colors
HBRUSH hBrushWhite = NULL;
HBRUSH hBrushLightGreen = NULL;
HBRUSH hBrushLightBlue = NULL;
HBRUSH hBrushLightTeal = NULL;
HBRUSH hCurrentBrush = NULL;

// Flag to track programmatic text changes
BOOL bProgrammaticChange = FALSE;

// Flag to track manual paste operations
BOOL bManualPaste = FALSE;

// Original text field window procedure
WNDPROC OriginalTextFieldProc = NULL;

// Subclass procedure for text field to detect paste operations
LRESULT CALLBACK TextFieldSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PASTE:
            // User is manually pasting - set flag
            bManualPaste = TRUE;
            break;
            
        case WM_KEYDOWN:
            // Check for Ctrl+V
            if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                bManualPaste = TRUE;
            }
            break;
    }
    
    // Call original window procedure
    return CallWindowProc(OriginalTextFieldProc, hwnd, uMsg, wParam, lParam);
}

// Function to get the default download path (Downloads/YouTubeCacher)
void GetDefaultDownloadPath(wchar_t* path, size_t pathSize) {
    PWSTR downloadsPathW = NULL;
    wchar_t downloadsPath[MAX_EXTENDED_PATH];
    
    // Initialize path as empty
    path[0] = L'\0';
    
    // Get the user's Downloads folder using the proper FOLDERID_Downloads
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_Downloads, 0, NULL, &downloadsPathW);
    if (SUCCEEDED(hr) && downloadsPathW != NULL) {
        // Copy the wide string directly
        wcsncpy(downloadsPath, downloadsPathW, MAX_EXTENDED_PATH - 1);
        downloadsPath[MAX_EXTENDED_PATH - 1] = L'\0';
        CoTaskMemFree(downloadsPathW);
    } else {
        // Fallback: try to get user profile folder and append Downloads
        PWSTR userProfileW = NULL;
        hr = SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &userProfileW);
        if (SUCCEEDED(hr) && userProfileW != NULL) {
            wcsncpy(downloadsPath, userProfileW, MAX_EXTENDED_PATH - 1);
            downloadsPath[MAX_EXTENDED_PATH - 1] = L'\0';
            wcscat(downloadsPath, L"\\Downloads");
            CoTaskMemFree(userProfileW);
        } else {
            // Ultimate fallback
            wcscpy(downloadsPath, L"C:\\Users\\Public\\Downloads");
        }
    }
    
    // Safely construct the full path
    if (wcslen(downloadsPath) + wcslen(L"\\YouTubeCacher") < pathSize) {
        wcscpy(path, downloadsPath);
        wcscat(path, L"\\YouTubeCacher");
    } else {
        // Path would be too long, use a shorter fallback
        wcscpy(path, L"C:\\YouTubeCacher");
    }
}

// Function to get the default yt-dlp path (check WinGet installation)
void GetDefaultYtDlpPath(wchar_t* path, size_t pathSize) {
    PWSTR localAppDataW = NULL;
    wchar_t ytDlpPath[MAX_EXTENDED_PATH];
    
    // Initialize path as empty
    path[0] = L'\0';
    
    // Get the user's LocalAppData folder using the proper API
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &localAppDataW);
    if (SUCCEEDED(hr) && localAppDataW != NULL) {
        // Safely construct the WinGet yt-dlp path
        size_t localAppDataLen = wcslen(localAppDataW);
        const wchar_t* wingetSuffix = L"\\Microsoft\\WinGet\\Packages\\yt-dlp.yt-dlp_Microsoft.Winget.Source_8wekyb3d8bbwe\\yt-dlp.exe";
        size_t suffixLen = wcslen(wingetSuffix);
        
        // Check if the combined path would fit
        if (localAppDataLen + suffixLen < MAX_EXTENDED_PATH) {
            // Safely construct the path
            wcscpy(ytDlpPath, localAppDataW);
            wcscat(ytDlpPath, wingetSuffix);
            
            // Check if the file exists
            DWORD attributes = GetFileAttributesW(ytDlpPath);
            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                // File exists and is not a directory, copy to output
                if (wcslen(ytDlpPath) < pathSize) {
                    wcscpy(path, ytDlpPath);
                }
            }
        }
        
        // Free the allocated path string
        CoTaskMemFree(localAppDataW);
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

// Function to validate that yt-dlp executable exists and is accessible
BOOL ValidateYtDlpExecutable(const wchar_t* path) {
    // Check if path is empty
    if (!path || wcslen(path) == 0) {
        return FALSE;
    }
    
    // Check if file exists
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return FALSE;
    }
    
    // Check if it's a file (not a directory)
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        return FALSE;
    }
    
    // Check if it's an executable file (has .exe, .cmd, .bat, .py, or .ps1 extension)
    const wchar_t* ext = wcsrchr(path, L'.');
    if (ext != NULL) {
        // Convert extension to lowercase for comparison
        wchar_t lowerExt[10];
        wcscpy(lowerExt, ext);
        for (int i = 0; lowerExt[i]; i++) {
            lowerExt[i] = towlower(lowerExt[i]);
        }
        
        if (wcscmp(lowerExt, L".exe") == 0 ||
            wcscmp(lowerExt, L".cmd") == 0 ||
            wcscmp(lowerExt, L".bat") == 0 ||
            wcscmp(lowerExt, L".py") == 0 ||
            wcscmp(lowerExt, L".ps1") == 0) {
            return TRUE;
        }
    }
    
    return FALSE;
}

// Function to load a setting from the registry
BOOL LoadSettingFromRegistry(const wchar_t* valueName, wchar_t* buffer, DWORD bufferSize) {
    HKEY hKey;
    DWORD dataType;
    DWORD dataSize = bufferSize * sizeof(wchar_t);
    BOOL result = FALSE;
    
    // Open the registry key
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        // Query the value
        if (RegQueryValueExW(hKey, valueName, NULL, &dataType, (LPBYTE)buffer, &dataSize) == ERROR_SUCCESS) {
            if (dataType == REG_SZ) {
                result = TRUE;
            }
        }
        RegCloseKey(hKey);
    }
    
    // If failed to load, clear the buffer
    if (!result) {
        buffer[0] = L'\0';
    }
    
    return result;
}

// Function to save a setting to the registry
BOOL SaveSettingToRegistry(const wchar_t* valueName, const wchar_t* value) {
    HKEY hKey;
    BOOL result = FALSE;
    DWORD disposition;
    
    // Create or open the registry key
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, 
                       KEY_WRITE, NULL, &hKey, &disposition) == ERROR_SUCCESS) {
        // Set the value
        DWORD dataSize = (DWORD)((wcslen(value) + 1) * sizeof(wchar_t));
        if (RegSetValueExW(hKey, valueName, 0, REG_SZ, (const BYTE*)value, dataSize) == ERROR_SUCCESS) {
            result = TRUE;
        }
        RegCloseKey(hKey);
    }
    
    return result;
}

// Function to load settings from registry into dialog controls
void LoadSettings(HWND hDlg) {
    wchar_t buffer[MAX_EXTENDED_PATH];
    
    // Load yt-dlp path
    if (LoadSettingFromRegistry(REG_YTDLP_PATH, buffer, MAX_EXTENDED_PATH)) {
        SetDlgItemTextW(hDlg, IDC_YTDLP_PATH, buffer);
    } else {
        // Use default if not found in registry
        GetDefaultYtDlpPath(buffer, MAX_EXTENDED_PATH);
        SetDlgItemTextW(hDlg, IDC_YTDLP_PATH, buffer);
    }
    
    // Load download path
    if (LoadSettingFromRegistry(REG_DOWNLOAD_PATH, buffer, MAX_EXTENDED_PATH)) {
        SetDlgItemTextW(hDlg, IDC_FOLDER_PATH, buffer);
    } else {
        // Use default if not found in registry
        GetDefaultDownloadPath(buffer, MAX_EXTENDED_PATH);
        SetDlgItemTextW(hDlg, IDC_FOLDER_PATH, buffer);
    }
    
    // Load player path
    if (LoadSettingFromRegistry(REG_PLAYER_PATH, buffer, MAX_EXTENDED_PATH)) {
        SetDlgItemTextW(hDlg, IDC_PLAYER_PATH, buffer);
    } else {
        // Use default if not found in registry
        SetDlgItemTextW(hDlg, IDC_PLAYER_PATH, L"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe");
    }
    
    // Load custom yt-dlp arguments
    if (LoadSettingFromRegistry(REG_CUSTOM_ARGS, buffer, MAX_EXTENDED_PATH)) {
        SetDlgItemTextW(hDlg, IDC_CUSTOM_ARGS_FIELD, buffer);
    } else {
        // Use empty default
        SetDlgItemTextW(hDlg, IDC_CUSTOM_ARGS_FIELD, L"");
    }
}

// Function to save settings from dialog controls to registry
void SaveSettings(HWND hDlg) {
    wchar_t buffer[MAX_EXTENDED_PATH];
    
    // Save yt-dlp path
    GetDlgItemTextW(hDlg, IDC_YTDLP_PATH, buffer, MAX_EXTENDED_PATH);
    SaveSettingToRegistry(REG_YTDLP_PATH, buffer);
    
    // Save download path
    GetDlgItemTextW(hDlg, IDC_FOLDER_PATH, buffer, MAX_EXTENDED_PATH);
    SaveSettingToRegistry(REG_DOWNLOAD_PATH, buffer);
    
    // Save player path
    GetDlgItemTextW(hDlg, IDC_PLAYER_PATH, buffer, MAX_EXTENDED_PATH);
    SaveSettingToRegistry(REG_PLAYER_PATH, buffer);
    
    // Save custom yt-dlp arguments
    GetDlgItemTextW(hDlg, IDC_CUSTOM_ARGS_FIELD, buffer, MAX_EXTENDED_PATH);
    
    // Validate and sanitize the custom arguments before saving
    if (wcslen(buffer) > 0) {
        if (ValidateYtDlpArguments(buffer)) {
            SanitizeYtDlpArguments(buffer, MAX_EXTENDED_PATH);
            SaveSettingToRegistry(REG_CUSTOM_ARGS, buffer);
        } else {
            // Invalid arguments - show warning and don't save
            MessageBoxW(hDlg, L"Custom yt-dlp arguments contain potentially dangerous options and were not saved.\n\nPlease remove any --exec, --batch-file, or other potentially harmful arguments.", 
                       L"Invalid Arguments", MB_OK | MB_ICONWARNING);
        }
    } else {
        // Empty arguments - save empty string
        SaveSettingToRegistry(REG_CUSTOM_ARGS, L"");
    }
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
                    bProgrammaticChange = TRUE;
                    SetDlgItemTextW(hDlg, IDC_TEXT_FIELD, clipText);
                    hCurrentBrush = hBrushLightGreen;
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    bProgrammaticChange = FALSE;
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
    
    // Position color buttons under progress bar
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_GREEN), NULL, 55, 78, 20, 16, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_TEAL), NULL, 80, 78, 20, 16, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_BLUE), NULL, 105, 78, 20, 16, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_WHITE), NULL, 130, 78, 20, 16, SWP_NOZORDER);
    
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
            // Load settings from registry (with defaults if not found)
            LoadSettings(hDlg);
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
                
                // Center the dialog
                RECT rcParent, rcDialog;
                HWND hParent = GetParent(hDlg);
                if (hParent) {
                    GetWindowRect(hParent, &rcParent);
                } else {
                    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcParent, 0);
                }
                GetWindowRect(hDlg, &rcDialog);
                
                int x = rcParent.left + (rcParent.right - rcParent.left - (rcDialog.right - rcDialog.left)) / 2;
                int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDialog.bottom - rcDialog.top)) / 2;
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
                bProgrammaticChange = TRUE;
                SetDlgItemTextW(hDlg, IDC_TEXT_FIELD, cmdLineURL);
                hCurrentBrush = hBrushLightTeal;
                InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                bProgrammaticChange = FALSE;
            } else {
                // Check clipboard for YouTube URL
                CheckClipboardForYouTubeURL(hDlg);
            }
            
            // Set focus to the text field for immediate typing
            SetFocus(GetDlgItem(hDlg, IDC_TEXT_FIELD));
            
            // Subclass the text field to detect paste operations
            HWND hTextField = GetDlgItem(hDlg, IDC_TEXT_FIELD);
            OriginalTextFieldProc = (WNDPROC)SetWindowLongPtr(hTextField, GWLP_WNDPROC, (LONG_PTR)TextFieldSubclassProc);
            
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
                    
                case IDC_TEXT_FIELD:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // Skip processing if this is a programmatic change
                        if (bProgrammaticChange) {
                            break;
                        }
                        
                        // Get current text
                        wchar_t buffer[MAX_BUFFER_SIZE];
                        GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, buffer, MAX_BUFFER_SIZE);
                        
                        // Handle different user input scenarios
                        if (hCurrentBrush == hBrushLightGreen) {
                            // User has modified the autopasted content - return to white
                            hCurrentBrush = hBrushWhite;
                        } else if (hCurrentBrush == hBrushLightBlue) {
                            // User is editing manually pasted content - return to white
                            hCurrentBrush = hBrushWhite;
                        } else if (bManualPaste && IsYouTubeURL(buffer)) {
                            // Manual paste of YouTube URL - set to light blue
                            hCurrentBrush = hBrushLightBlue;
                            bManualPaste = FALSE; // Reset flag after use
                        } else if (bManualPaste) {
                            // Manual paste of non-YouTube content - keep white but reset flag
                            bManualPaste = FALSE;
                        }
                        // Note: Light teal (command line) is preserved during editing
                        // Regular typing in white background stays white
                        
                        InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    }
                    break;
                    
                // Helper function to execute yt-dlp with progress dialog
                BOOL ExecuteYtDlpWithProgress(HWND hParent, const wchar_t* ytDlpPath, const wchar_t* arguments, 
                                            const wchar_t* operationTitle, wchar_t** output) {
                    // Create progress dialog
                    ProgressDialog* progress = CreateProgressDialog(hParent, operationTitle);
                    if (!progress) {
                        MessageBoxW(hParent, L"Failed to create progress dialog", L"Error", MB_OK | MB_ICONERROR);
                        return FALSE;
                    }
                    
                    // Update initial status
                    UpdateProgressDialog(progress, 0, L"Initializing yt-dlp...");
                    
                    // Set up process creation
                    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
                    HANDLE hRead = NULL, hWrite = NULL;
                    
                    // Create pipes for stdout/stderr
                    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
                        UpdateProgressDialog(progress, 0, L"Failed to create output pipe");
                        Sleep(1000);
                        DestroyProgressDialog(progress);
                        MessageBoxW(hParent, L"Failed to create pipe for yt-dlp output capture.", L"System Error", MB_OK | MB_ICONERROR);
                        return FALSE;
                    }
                    
                    // Prevent the read handle from being inherited
                    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
                    
                    // Set up STARTUPINFO to redirect standard handles
                    STARTUPINFOW si = {0};
                    si.cb = sizeof(si);
                    si.dwFlags = STARTF_USESTDHANDLES;
                    si.hStdOutput = hWrite;
                    si.hStdError = hWrite;
                    si.hStdInput = NULL;
                    
                    PROCESS_INFORMATION pi = {0};
                    
                    // Build command line
                    size_t cmdLineLen = wcslen(ytDlpPath) + wcslen(arguments) + 10;
                    wchar_t* cmdLine = (wchar_t*)malloc(cmdLineLen * sizeof(wchar_t));
                    if (!cmdLine) {
                        CloseHandle(hRead);
                        CloseHandle(hWrite);
                        DestroyProgressDialog(progress);
                        MessageBoxW(hParent, L"Memory allocation failed for command line.", L"System Error", MB_OK | MB_ICONERROR);
                        return FALSE;
                    }
                    
                    swprintf(cmdLine, cmdLineLen, L"\"%ls\" %ls", ytDlpPath, arguments);
                    
                    UpdateProgressDialog(progress, 10, L"Starting yt-dlp process...");
                    
                    // Create process
                    BOOL processCreated = CreateProcessW(
                        NULL,               // application name
                        cmdLine,            // command line string
                        NULL, NULL,         // process and thread security attributes
                        TRUE,               // inherit handles
                        CREATE_NO_WINDOW,   // hide console window
                        NULL,               // environment
                        NULL,               // current directory
                        &si, &pi            // startup and process info
                    );
                    
                    if (!processCreated) {
                        DWORD errorCode = GetLastError();
                        wchar_t errorMsg[512];
                        swprintf(errorMsg, 512, L"Failed to start yt-dlp process.\nError code: %lu", errorCode);
                        UpdateProgressDialog(progress, 0, errorMsg);
                        Sleep(2000);
                        
                        free(cmdLine);
                        CloseHandle(hRead);
                        CloseHandle(hWrite);
                        DestroyProgressDialog(progress);
                        return FALSE;
                    }
                    
                    // Close the write handle in the parent
                    CloseHandle(hWrite);
                    
                    UpdateProgressDialog(progress, 20, L"Reading yt-dlp output...");
                    
                    // Allocate output buffer
                    size_t outputSize = 16384;
                    wchar_t* outputBuffer = (wchar_t*)malloc(outputSize * sizeof(wchar_t));
                    if (!outputBuffer) {
                        free(cmdLine);
                        CloseHandle(hRead);
                        TerminateProcess(pi.hProcess, 1);
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        DestroyProgressDialog(progress);
                        return FALSE;
                    }
                    outputBuffer[0] = L'\0';
                    
                    // Read output with progress updates
                    char buffer[4096];
                    DWORD bytesRead;
                    wchar_t tempOutput[2048];
                    int progressValue = 20;
                    
                    while (TRUE) {
                        // Check for cancellation
                        if (IsProgressDialogCancelled(progress)) {
                            UpdateProgressDialog(progress, progressValue, L"Cancelling operation...");
                            TerminateProcess(pi.hProcess, 1);
                            break;
                        }
                        
                        // Try to read output
                        BOOL readResult = ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
                        if (!readResult || bytesRead == 0) {
                            // Check if process is still running
                            DWORD exitCode;
                            if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                                break; // Process finished
                            }
                            
                            // Update progress and continue
                            progressValue = min(progressValue + 5, 90);
                            UpdateProgressDialog(progress, progressValue, L"Processing...");
                            Sleep(100);
                            continue;
                        }
                        
                        // Convert and append output
                        buffer[bytesRead] = '\0';
                        int converted = MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, tempOutput, 2047);
                        if (converted > 0) {
                            tempOutput[converted] = L'\0';
                            
                            // Expand buffer if needed
                            size_t currentLen = wcslen(outputBuffer);
                            size_t newLen = currentLen + converted + 1;
                            if (newLen >= outputSize) {
                                outputSize = newLen * 2;
                                wchar_t* newBuffer = (wchar_t*)realloc(outputBuffer, outputSize * sizeof(wchar_t));
                                if (newBuffer) {
                                    outputBuffer = newBuffer;
                                } else {
                                    break; // Out of memory
                                }
                            }
                            
                            wcscat(outputBuffer, tempOutput);
                            
                            // Update progress based on output content
                            if (wcsstr(tempOutput, L"Downloading") || wcsstr(tempOutput, L"download")) {
                                progressValue = min(progressValue + 2, 80);
                                UpdateProgressDialog(progress, progressValue, L"Downloading...");
                            } else if (wcsstr(tempOutput, L"Extracting") || wcsstr(tempOutput, L"extract")) {
                                progressValue = min(progressValue + 1, 85);
                                UpdateProgressDialog(progress, progressValue, L"Extracting information...");
                            }
                        }
                    }
                    
                    // Wait for process completion
                    UpdateProgressDialog(progress, 95, L"Finalizing...");
                    WaitForSingleObject(pi.hProcess, 5000); // 5 second timeout
                    
                    // Get exit code
                    DWORD exitCode;
                    GetExitCodeProcess(pi.hProcess, &exitCode);
                    
                    UpdateProgressDialog(progress, 100, L"Complete");
                    Sleep(500);
                    
                    // Cleanup
                    free(cmdLine);
                    CloseHandle(hRead);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    DestroyProgressDialog(progress);
                    
                    // Set output
                    if (output) {
                        *output = outputBuffer;
                    } else {
                        free(outputBuffer);
                    }
                    
                    return (exitCode == 0 && !IsProgressDialogCancelled(progress));
                }

                case IDC_DOWNLOAD_BTN: {
                    // First, validate that yt-dlp is configured and exists
                    wchar_t ytDlpPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_YTDLP_PATH, ytDlpPath, MAX_EXTENDED_PATH)) {
                        // Fall back to default if not found in registry
                        GetDefaultYtDlpPath(ytDlpPath, MAX_EXTENDED_PATH);
                    }
                    
                    // Validate yt-dlp executable
                    if (!ValidateYtDlpExecutable(ytDlpPath)) {
                        wchar_t errorMsg[MAX_EXTENDED_PATH + 200];
                        if (wcslen(ytDlpPath) == 0) {
                            swprintf(errorMsg, MAX_EXTENDED_PATH + 200, 
                                L"yt-dlp executable is not configured.\n\nPlease go to File > Settings and specify the path to yt-dlp.exe");
                        } else {
                            swprintf(errorMsg, MAX_EXTENDED_PATH + 200, 
                                L"yt-dlp executable not found or invalid:\n%ls\n\nPlease check the path in File > Settings", ytDlpPath);
                        }
                        MessageBoxW(hDlg, errorMsg, L"yt-dlp Not Found", MB_OK | MB_ICONWARNING);
                        break;
                    }
                    
                    // Get the current download folder path from registry settings
                    wchar_t downloadPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                        // Fall back to default if not found in registry
                        GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
                    }
                    
                    // Create the download directory if it doesn't exist
                    if (!CreateDownloadDirectoryIfNeeded(downloadPath)) {
                        MessageBoxW(hDlg, L"Failed to create download directory", L"Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    if (wcslen(url) == 0) {
                        MessageBoxW(hDlg, L"Please enter a URL to download.", L"No URL", MB_OK | MB_ICONWARNING);
                        break;
                    }
                    
                    // Build yt-dlp arguments for download using new argument management
                    wchar_t arguments[2048];
                    if (!GetYtDlpArgsForOperation(YTDLP_OP_DOWNLOAD, url, downloadPath, arguments, 2048)) {
                        MessageBoxW(hDlg, L"Failed to build yt-dlp arguments for download.", L"Configuration Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    // Execute yt-dlp with progress dialog
                    wchar_t* output = NULL;
                    BOOL success = ExecuteYtDlpWithProgress(hDlg, ytDlpPath, arguments, L"Downloading Video", &output);
                    
                    if (success) {
                        MessageBoxW(hDlg, L"Download completed successfully!", L"Download Complete", MB_OK | MB_ICONINFORMATION);
                    } else {
                        wchar_t errorMsg[1024];
                        if (output && wcslen(output) > 0) {
                            swprintf(errorMsg, 1024, L"Download failed. yt-dlp output:\n\n%ls", output);
                        } else {
                            wcscpy(errorMsg, L"Download failed or was cancelled.");
                        }
                        MessageBoxW(hDlg, errorMsg, L"Download Failed", MB_OK | MB_ICONERROR);
                    }
                    
                    if (output) {
                        free(output);
                    }
                    break;
                }
                    
                case IDC_GETINFO_BTN: {
                    // Validate that yt-dlp is configured and exists
                    wchar_t ytDlpPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_YTDLP_PATH, ytDlpPath, MAX_EXTENDED_PATH)) {
                        // Fall back to default if not found in registry
                        GetDefaultYtDlpPath(ytDlpPath, MAX_EXTENDED_PATH);
                    }
                    
                    // Validate yt-dlp executable
                    if (!ValidateYtDlpExecutable(ytDlpPath)) {
                        wchar_t errorMsg[MAX_EXTENDED_PATH + 200];
                        if (wcslen(ytDlpPath) == 0) {
                            swprintf(errorMsg, MAX_EXTENDED_PATH + 200, 
                                L"yt-dlp executable is not configured.\n\nPlease go to File > Settings and specify the path to yt-dlp.exe");
                        } else {
                            swprintf(errorMsg, MAX_EXTENDED_PATH + 200, 
                                L"yt-dlp executable not found or invalid:\n%ls\n\nPlease check the path in File > Settings", ytDlpPath);
                        }
                        MessageBoxW(hDlg, errorMsg, L"yt-dlp Not Found", MB_OK | MB_ICONWARNING);
                        break;
                    }
                    
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    wchar_t arguments[1024];
                    if (wcslen(url) > 0) {
                        // Get info for specific URL using new argument management
                        if (!GetYtDlpArgsForOperation(YTDLP_OP_GET_INFO, url, NULL, arguments, 1024)) {
                            MessageBoxW(hDlg, L"Failed to build yt-dlp arguments for info retrieval.", L"Configuration Error", MB_OK | MB_ICONERROR);
                            break;
                        }
                    } else {
                        // Just get version info
                        if (!GetYtDlpArgsForOperation(YTDLP_OP_VALIDATE, NULL, NULL, arguments, 1024)) {
                            wcscpy(arguments, L"--version");
                        }
                    }
                    
                    // Execute yt-dlp with progress dialog
                    wchar_t* output = NULL;
                    BOOL success = ExecuteYtDlpWithProgress(hDlg, ytDlpPath, arguments, L"Getting Video Information", &output);
                    
                    if (success && output && wcslen(output) > 0) {
                        // Display the output in a message box
                        MessageBoxW(hDlg, output, L"Video Information", MB_OK | MB_ICONINFORMATION);
                    } else if (!success) {
                        wchar_t errorMsg[1024];
                        if (output && wcslen(output) > 0) {
                            swprintf(errorMsg, 1024, L"Failed to get video information. yt-dlp output:\n\n%ls", output);
                        } else {
                            wcscpy(errorMsg, L"Failed to get video information or operation was cancelled.");
                        }
                        MessageBoxW(hDlg, errorMsg, L"Get Info Failed", MB_OK | MB_ICONERROR);
                    } else {
                        MessageBoxW(hDlg, L"yt-dlp executed successfully but no output was captured.", L"No Output", MB_OK | MB_ICONWARNING);
                    }
                    
                    if (output) {
                        free(output);
                    }
                    break;
                }
                    
                case IDC_BUTTON2:
                    MessageBoxW(hDlg, L"Play functionality not implemented yet", L"Play", MB_OK);
                    break;
                    
                case IDC_BUTTON3:
                    MessageBoxW(hDlg, L"Delete functionality not implemented yet", L"Delete", MB_OK);
                    break;
                    
                case IDC_COLOR_GREEN:
                    hCurrentBrush = hBrushLightGreen;
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_TEAL:
                    hCurrentBrush = hBrushLightTeal;
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_BLUE:
                    hCurrentBrush = hBrushLightBlue;
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_WHITE:
                    hCurrentBrush = hBrushWhite;
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDCANCEL:
                    DestroyWindow(hDlg);
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            // Restore original text field window procedure
            if (OriginalTextFieldProc) {
                HWND hTextField = GetDlgItem(hDlg, IDC_TEXT_FIELD);
                SetWindowLongPtr(hTextField, GWLP_WNDPROC, (LONG_PTR)OriginalTextFieldProc);
            }
            
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

// Progress dialog management functions
ProgressDialog* CreateProgressDialog(HWND parent, const wchar_t* title) {
    ProgressDialog* dialog = (ProgressDialog*)malloc(sizeof(ProgressDialog));
    if (!dialog) return NULL;
    
    memset(dialog, 0, sizeof(ProgressDialog));
    
    // Create the dialog
    dialog->hDialog = CreateDialogParamW(GetModuleHandleW(NULL), 
                                        MAKEINTRESOURCEW(IDD_PROGRESS), 
                                        parent, 
                                        ProgressDialogProc, 
                                        (LPARAM)dialog);
    
    if (!dialog->hDialog) {
        free(dialog);
        return NULL;
    }
    
    // Set the title if provided
    if (title) {
        SetWindowTextW(dialog->hDialog, title);
    }
    
    // Show the dialog
    ShowWindow(dialog->hDialog, SW_SHOW);
    UpdateWindow(dialog->hDialog);
    
    return dialog;
}

void UpdateProgressDialog(ProgressDialog* dialog, int progress, const wchar_t* status) {
    if (!dialog || !dialog->hDialog) return;
    
    // Update progress bar
    if (dialog->hProgressBar && progress >= 0 && progress <= 100) {
        SendMessageW(dialog->hProgressBar, PBM_SETPOS, progress, 0);
    }
    
    // Update status text
    if (dialog->hStatusText && status) {
        SetWindowTextW(dialog->hStatusText, status);
    }
    
    // Process messages to keep dialog responsive
    MSG msg;
    while (PeekMessageW(&msg, dialog->hDialog, 0, 0, PM_REMOVE)) {
        if (!IsDialogMessageW(dialog->hDialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

BOOL IsProgressDialogCancelled(const ProgressDialog* dialog) {
    return dialog ? dialog->cancelled : FALSE;
}

void DestroyProgressDialog(ProgressDialog* dialog) {
    if (!dialog) return;
    
    if (dialog->hDialog) {
        DestroyWindow(dialog->hDialog);
    }
    
    free(dialog);
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
// E
// Enhanced validation functions implementation

// Function to detect Python runtime on the system
BOOL DetectPythonRuntime(wchar_t* pythonPath, size_t pathSize) {
    if (!pythonPath || pathSize == 0) {
        return FALSE;
    }
    
    // Initialize output
    pythonPath[0] = L'\0';
    
    // Common Python executable names to check
    const wchar_t* pythonNames[] = {
        L"python.exe",
        L"python3.exe", 
        L"py.exe"
    };
    
    // Check each Python executable in PATH
    for (int i = 0; i < 3; i++) {
        wchar_t fullPath[MAX_EXTENDED_PATH];
        DWORD result = SearchPathW(NULL, pythonNames[i], NULL, MAX_EXTENDED_PATH, fullPath, NULL);
        
        if (result > 0 && result < MAX_EXTENDED_PATH) {
            // Found Python executable, verify it works
            wchar_t cmdLine[MAX_EXTENDED_PATH + 50];
            swprintf(cmdLine, MAX_EXTENDED_PATH + 50, L"\"%ls\" --version", fullPath);
            
            STARTUPINFOW si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi = { 0 };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            
            if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                DWORD exitCode;
                if (WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0) {
                    GetExitCodeProcess(pi.hProcess, &exitCode);
                    if (exitCode == 0) {
                        // Python works, copy path and return success
                        wcsncpy(pythonPath, fullPath, pathSize - 1);
                        pythonPath[pathSize - 1] = L'\0';
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        return TRUE;
                    }
                }
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
    }
    
    return FALSE;
}

// Function to get yt-dlp version
BOOL GetYtDlpVersion(const wchar_t* path, wchar_t* version, size_t versionSize) {
    if (!path || !version || versionSize == 0) {
        return FALSE;
    }
    
    // Initialize output
    version[0] = L'\0';
    
    // Create command line for version check
    wchar_t cmdLine[MAX_EXTENDED_PATH + 50];
    swprintf(cmdLine, MAX_EXTENDED_PATH + 50, L"\"%ls\" --version", path);
    
    // Create pipes for output capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return FALSE;
    }
    
    // Ensure read handle is not inherited
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;
    
    BOOL success = FALSE;
    if (CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);
        hWrite = NULL;
        
        // Wait for process to complete (with timeout)
        if (WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0) {
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            
            if (exitCode == 0) {
                // Read output
                char buffer[512];
                DWORD bytesRead;
                if (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    
                    // Convert to wide string
                    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, version, (int)versionSize);
                    
                    // Clean up version string (remove newlines)
                    for (size_t i = 0; i < versionSize && version[i]; i++) {
                        if (version[i] == L'\r' || version[i] == L'\n') {
                            version[i] = L'\0';
                            break;
                        }
                    }
                    
                    success = TRUE;
                }
            }
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    if (hWrite) CloseHandle(hWrite);
    CloseHandle(hRead);
    
    return success;
}

// Function to check yt-dlp version compatibility
BOOL CheckYtDlpCompatibility(const wchar_t* version) {
    if (!version || wcslen(version) == 0) {
        return FALSE;
    }
    
    // For now, accept any version that we can detect
    // In the future, this could check for minimum version requirements
    // or known incompatible versions
    
    // Basic check: version should contain numbers
    BOOL hasNumbers = FALSE;
    for (size_t i = 0; version[i]; i++) {
        if (iswdigit(version[i])) {
            hasNumbers = TRUE;
            break;
        }
    }
    
    return hasNumbers;
}

// Function to parse validation errors and provide detailed information
BOOL ParseValidationError(DWORD errorCode, const wchar_t* output, ValidationInfo* info) {
    if (!info) {
        return FALSE;
    }
    
    // Parse Windows error codes
    switch (errorCode) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            info->result = VALIDATION_NOT_FOUND;
            info->errorDetails = _wcsdup(L"yt-dlp executable not found at specified path");
            info->suggestions = _wcsdup(L"Verify the path is correct and yt-dlp is installed");
            return TRUE;
            
        case ERROR_ACCESS_DENIED:
            info->result = VALIDATION_PERMISSION_DENIED;
            info->errorDetails = _wcsdup(L"Access denied when trying to execute yt-dlp");
            info->suggestions = _wcsdup(L"Check file permissions or run as administrator");
            return TRUE;
            
        case ERROR_BAD_EXE_FORMAT:
            info->result = VALIDATION_NOT_EXECUTABLE;
            info->errorDetails = _wcsdup(L"Invalid executable format - file may be corrupted");
            info->suggestions = _wcsdup(L"Download a fresh copy of yt-dlp for your system architecture");
            return TRUE;
    }
    
    // Parse yt-dlp specific error messages from output
    if (output && wcslen(output) > 0) {
        // Convert to lowercase for case-insensitive matching
        wchar_t* lowerOutput = _wcsdup(output);
        if (lowerOutput) {
            for (size_t i = 0; lowerOutput[i]; i++) {
                lowerOutput[i] = towlower(lowerOutput[i]);
            }
            
            // Check for common error patterns
            if (wcsstr(lowerOutput, L"python") && wcsstr(lowerOutput, L"not found")) {
                info->result = VALIDATION_MISSING_DEPENDENCIES;
                info->errorDetails = _wcsdup(L"Python runtime not found - required for yt-dlp.py");
                info->suggestions = _wcsdup(L"Install Python or use yt-dlp.exe instead");
            }
            else if (wcsstr(lowerOutput, L"module") && wcsstr(lowerOutput, L"not found")) {
                info->result = VALIDATION_MISSING_DEPENDENCIES;
                info->errorDetails = _wcsdup(L"Missing Python modules required by yt-dlp");
                info->suggestions = _wcsdup(L"Reinstall yt-dlp or update Python modules");
            }
            else if (wcsstr(lowerOutput, L"permission") || wcsstr(lowerOutput, L"access")) {
                info->result = VALIDATION_PERMISSION_DENIED;
                info->errorDetails = _wcsdup(L"Permission error when running yt-dlp");
                info->suggestions = _wcsdup(L"Check file permissions or run as administrator");
            }
            else if (wcsstr(lowerOutput, L"syntax error") || wcsstr(lowerOutput, L"invalid syntax")) {
                info->result = VALIDATION_VERSION_INCOMPATIBLE;
                info->errorDetails = _wcsdup(L"Python syntax error - possible version incompatibility");
                info->suggestions = _wcsdup(L"Update Python or use a compatible yt-dlp version");
            }
            else {
                // Generic error
                info->result = VALIDATION_NOT_EXECUTABLE;
                size_t detailsLen = wcslen(output) + 50;
                wchar_t* details = (wchar_t*)malloc(detailsLen * sizeof(wchar_t));
                if (details) {
                    swprintf(details, detailsLen, L"yt-dlp execution failed: %ls", output);
                    info->errorDetails = details;
                }
                info->suggestions = _wcsdup(L"Check yt-dlp installation and try downloading a fresh copy");
            }
            
            free(lowerOutput);
            return TRUE;
        }
    }
    
    return FALSE;
}

// Function to test yt-dlp with a real URL (more comprehensive test)
BOOL TestYtDlpWithUrl(const wchar_t* path, ValidationInfo* info) {
    if (!path || wcslen(path) == 0) {
        return FALSE;
    }
    
    // Test with a simple YouTube URL info extraction (no download)
    // Using a known stable test URL
    wchar_t cmdLine[MAX_EXTENDED_PATH + 200];
    swprintf(cmdLine, MAX_EXTENDED_PATH + 200, 
        L"\"%ls\" --no-download --quiet --print title \"https://www.youtube.com/watch?v=dQw4w9WgXcQ\"", path);
    
    // Create pipes for output capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return FALSE;
    }
    
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;
    
    BOOL success = FALSE;
    wchar_t output[1024] = {0};
    
    if (CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);
        hWrite = NULL;
        
        // Wait for process to complete (with timeout)
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000); // 30 second timeout
        
        if (waitResult == WAIT_OBJECT_0) {
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            
            // Read output
            char buffer[1024];
            DWORD bytesRead;
            if (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                MultiByteToWideChar(CP_UTF8, 0, buffer, -1, output, 1024);
            }
            
            if (exitCode == 0) {
                success = TRUE;
                if (info) {
                    info->result = VALIDATION_OK;
                    info->errorDetails = _wcsdup(L"yt-dlp successfully extracted video information");
                    info->suggestions = _wcsdup(L"yt-dlp is working correctly");
                }
            } else {
                // Parse the error
                if (info) {
                    ParseValidationError(exitCode, output, info);
                }
            }
        } else if (waitResult == WAIT_TIMEOUT) {
            // Process timed out
            TerminateProcess(pi.hProcess, 1);
            if (info) {
                info->result = VALIDATION_NOT_EXECUTABLE;
                info->errorDetails = _wcsdup(L"yt-dlp operation timed out - may indicate network or performance issues");
                info->suggestions = _wcsdup(L"Check internet connection and yt-dlp installation");
            }
        } else {
            // Wait failed
            if (info) {
                info->result = VALIDATION_NOT_EXECUTABLE;
                info->errorDetails = _wcsdup(L"Failed to monitor yt-dlp process execution");
                info->suggestions = _wcsdup(L"System error - try restarting the application");
            }
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        // Process creation failed
        DWORD error = GetLastError();
        if (info) {
            ParseValidationError(error, NULL, info);
        }
    }
    
    if (hWrite) CloseHandle(hWrite);
    CloseHandle(hRead);
    
    return success;
}

// Function to test basic yt-dlp functionality
BOOL TestYtDlpFunctionality(const wchar_t* path) {
    if (!path || wcslen(path) == 0) {
        return FALSE;
    }
    
    // Test with a simple help command that should always work
    wchar_t cmdLine[MAX_EXTENDED_PATH + 50];
    swprintf(cmdLine, MAX_EXTENDED_PATH + 50, L"\"%ls\" --help", path);
    
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    BOOL success = FALSE;
    if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        // Wait for process to complete (with timeout)
        if (WaitForSingleObject(pi.hProcess, 15000) == WAIT_OBJECT_0) {
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            success = (exitCode == 0);
        } else {
            // Process timed out, terminate it
            TerminateProcess(pi.hProcess, 1);
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    return success;
}

// Command line argument management functions

// Function to get optimized arguments for info retrieval operations
BOOL GetOptimizedArgsForInfo(wchar_t* args, size_t argsSize) {
    if (!args || argsSize == 0) {
        return FALSE;
    }
    
    // Optimized arguments for fast info retrieval
    const wchar_t* infoArgs = L"--no-download --quiet --print title,duration,uploader,view_count,upload_date";
    
    if (wcslen(infoArgs) >= argsSize) {
        return FALSE;
    }
    
    wcscpy(args, infoArgs);
    return TRUE;
}

// Function to get optimized arguments for download operations
BOOL GetOptimizedArgsForDownload(const wchar_t* outputPath, wchar_t* args, size_t argsSize) {
    if (!args || argsSize == 0 || !outputPath) {
        return FALSE;
    }
    
    // Build optimized download arguments
    const wchar_t* format = L"--format \"best[height<=1080]/best\" --embed-metadata --write-thumbnail --output \"%ls\\%%(title)s.%%(ext)s\"";
    
    size_t requiredSize = wcslen(format) + wcslen(outputPath) + 1;
    if (requiredSize > argsSize) {
        return FALSE;
    }
    
    swprintf(args, argsSize, format, outputPath);
    return TRUE;
}

// Function to get fallback argument sets for compatibility issues
BOOL GetFallbackArgs(YtDlpOperation operation, wchar_t* args, size_t argsSize) {
    if (!args || argsSize == 0) {
        return FALSE;
    }
    
    const wchar_t* fallbackArgs = NULL;
    
    switch (operation) {
        case YTDLP_OP_GET_INFO:
            // Simple fallback for info - just get title
            fallbackArgs = L"--no-download --get-title";
            break;
            
        case YTDLP_OP_DOWNLOAD:
            // Simple fallback for download - basic format
            fallbackArgs = L"--format \"best\"";
            break;
            
        case YTDLP_OP_VALIDATE:
            // Fallback for validation - just version
            fallbackArgs = L"--version";
            break;
            
        default:
            return FALSE;
    }
    
    if (wcslen(fallbackArgs) >= argsSize) {
        return FALSE;
    }
    
    wcscpy(args, fallbackArgs);
    return TRUE;
}

// Function to validate yt-dlp arguments for security
BOOL ValidateYtDlpArguments(const wchar_t* args) {
    if (!args) {
        return FALSE;
    }
    
    // Check for potentially dangerous arguments
    const wchar_t* dangerousArgs[] = {
        L"--exec",           // Execute arbitrary commands
        L"--exec-before-dl", // Execute before download
        L"--exec-after-dl",  // Execute after download
        L"--config-location", // Could point to malicious config
        L"--batch-file",     // Could read arbitrary files
        L"--load-info-json", // Could load malicious JSON
        L"--cookies-from-browser", // Privacy concern
        NULL
    };
    
    // Convert to lowercase for case-insensitive checking
    wchar_t* lowerArgs = _wcsdup(args);
    if (!lowerArgs) {
        return FALSE;
    }
    
    for (size_t i = 0; lowerArgs[i]; i++) {
        lowerArgs[i] = towlower(lowerArgs[i]);
    }
    
    // Check for dangerous arguments
    for (int i = 0; dangerousArgs[i]; i++) {
        wchar_t* lowerDangerous = _wcsdup(dangerousArgs[i]);
        if (lowerDangerous) {
            for (size_t j = 0; lowerDangerous[j]; j++) {
                lowerDangerous[j] = towlower(lowerDangerous[j]);
            }
            
            if (wcsstr(lowerArgs, lowerDangerous)) {
                free(lowerDangerous);
                free(lowerArgs);
                return FALSE;
            }
            free(lowerDangerous);
        }
    }
    
    free(lowerArgs);
    return TRUE;
}

// Function to sanitize yt-dlp arguments
BOOL SanitizeYtDlpArguments(wchar_t* args, size_t argsSize) {
    if (!args || argsSize == 0) {
        return FALSE;
    }
    
    // Remove potentially dangerous characters and sequences
    size_t len = wcslen(args);
    size_t writePos = 0;
    
    for (size_t i = 0; i < len && writePos < argsSize - 1; i++) {
        wchar_t c = args[i];
        
        // Skip dangerous characters
        if (c == L'&' || c == L'|' || c == L';' || c == L'`' || c == L'$') {
            continue;
        }
        
        // Replace multiple spaces with single space
        if (c == L' ' && writePos > 0 && args[writePos - 1] == L' ') {
            continue;
        }
        
        args[writePos++] = c;
    }
    
    args[writePos] = L'\0';
    return TRUE;
}

// Main function to get operation-specific argument sets
BOOL GetYtDlpArgsForOperation(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath, 
                             wchar_t* args, size_t argsSize) {
    if (!args || argsSize == 0) {
        return FALSE;
    }
    
    // Initialize args
    args[0] = L'\0';
    
    // Check for custom arguments from registry first
    wchar_t customArgs[1024] = {0};
    if (LoadSettingFromRegistry(REG_CUSTOM_ARGS, customArgs, 1024) && wcslen(customArgs) > 0) {
        // Validate custom arguments
        if (ValidateYtDlpArguments(customArgs)) {
            // Use custom arguments as base
            wcsncpy(args, customArgs, argsSize - 1);
            args[argsSize - 1] = L'\0';
            
            // Sanitize the arguments
            SanitizeYtDlpArguments(args, argsSize);
            
            // Add URL if provided and not already in custom args
            if (url && wcslen(url) > 0 && !wcsstr(args, url)) {
                size_t currentLen = wcslen(args);
                size_t urlLen = wcslen(url);
                
                if (currentLen + urlLen + 10 < argsSize) {
                    if (currentLen > 0) {
                        wcscat(args, L" ");
                    }
                    wcscat(args, L"\"");
                    wcscat(args, url);
                    wcscat(args, L"\"");
                }
            }
            
            return TRUE;
        }
    }
    
    // Use operation-specific optimized arguments
    wchar_t baseArgs[1024] = {0};
    BOOL success = FALSE;
    
    switch (operation) {
        case YTDLP_OP_GET_INFO:
            success = GetOptimizedArgsForInfo(baseArgs, 1024);
            break;
            
        case YTDLP_OP_DOWNLOAD:
            if (outputPath) {
                success = GetOptimizedArgsForDownload(outputPath, baseArgs, 1024);
            } else {
                // Use fallback if no output path
                success = GetFallbackArgs(operation, baseArgs, 1024);
            }
            break;
            
        case YTDLP_OP_VALIDATE:
            success = GetFallbackArgs(operation, baseArgs, 1024);
            break;
            
        default:
            return FALSE;
    }
    
    if (!success) {
        // Try fallback arguments
        success = GetFallbackArgs(operation, baseArgs, 1024);
        if (!success) {
            return FALSE;
        }
    }
    
    // Build final command line with URL
    size_t baseLen = wcslen(baseArgs);
    size_t urlLen = url ? wcslen(url) : 0;
    
    if (baseLen + urlLen + 10 >= argsSize) {
        return FALSE;
    }
    
    wcscpy(args, baseArgs);
    
    if (url && wcslen(url) > 0) {
        wcscat(args, L" \"");
        wcscat(args, url);
        wcscat(args, L"\"");
    }
    
    return TRUE;
}

// Comprehensive yt-dlp validation function
BOOL ValidateYtDlpComprehensive(const wchar_t* path, ValidationInfo* info) {
    if (!path || !info) {
        return FALSE;
    }
    
    // Initialize validation info
    info->result = VALIDATION_NOT_FOUND;
    info->version = NULL;
    info->errorDetails = NULL;
    info->suggestions = NULL;
    
    // Check if path is empty
    if (wcslen(path) == 0) {
        info->result = VALIDATION_NOT_FOUND;
        info->errorDetails = _wcsdup(L"No yt-dlp path specified");
        info->suggestions = _wcsdup(L"Please configure the yt-dlp executable path in Settings");
        return FALSE;
    }
    
    // Check if file exists
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        info->result = VALIDATION_NOT_FOUND;
        
        wchar_t* details = (wchar_t*)malloc((wcslen(path) + 100) * sizeof(wchar_t));
        if (details) {
            swprintf(details, wcslen(path) + 100, L"File not found: %ls", path);
            info->errorDetails = details;
        }
        
        info->suggestions = _wcsdup(L"Check the file path and ensure yt-dlp is installed. You can download it from https://github.com/yt-dlp/yt-dlp");
        return FALSE;
    }
    
    // Check if it's a directory (not a file)
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->result = VALIDATION_NOT_EXECUTABLE;
        info->errorDetails = _wcsdup(L"Path points to a directory, not an executable file");
        info->suggestions = _wcsdup(L"Please select the yt-dlp.exe file, not a folder");
        return FALSE;
    }
    
    // Check file permissions (try to open for reading)
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            info->result = VALIDATION_PERMISSION_DENIED;
            info->errorDetails = _wcsdup(L"Access denied - insufficient permissions to access the file");
            info->suggestions = _wcsdup(L"Check file permissions or try running as administrator");
        } else {
            info->result = VALIDATION_NOT_EXECUTABLE;
            info->errorDetails = _wcsdup(L"Cannot access the file");
            info->suggestions = _wcsdup(L"Ensure the file is not corrupted and is accessible");
        }
        return FALSE;
    }
    CloseHandle(hFile);
    
    // Check if it's an executable file type
    const wchar_t* ext = wcsrchr(path, L'.');
    BOOL isExecutable = FALSE;
    if (ext != NULL) {
        wchar_t lowerExt[10];
        wcscpy(lowerExt, ext);
        for (int i = 0; lowerExt[i]; i++) {
            lowerExt[i] = towlower(lowerExt[i]);
        }
        
        if (wcscmp(lowerExt, L".exe") == 0 ||
            wcscmp(lowerExt, L".cmd") == 0 ||
            wcscmp(lowerExt, L".bat") == 0 ||
            wcscmp(lowerExt, L".py") == 0 ||
            wcscmp(lowerExt, L".ps1") == 0) {
            isExecutable = TRUE;
        }
    }
    
    if (!isExecutable) {
        info->result = VALIDATION_NOT_EXECUTABLE;
        info->errorDetails = _wcsdup(L"File does not appear to be an executable (.exe, .cmd, .bat, .py, .ps1)");
        info->suggestions = _wcsdup(L"Please select a valid yt-dlp executable file");
        return FALSE;
    }
    
    // For Python files, check if Python runtime is available
    if (ext && _wcsicmp(ext, L".py") == 0) {
        wchar_t pythonPath[MAX_EXTENDED_PATH];
        if (!DetectPythonRuntime(pythonPath, MAX_EXTENDED_PATH)) {
            info->result = VALIDATION_MISSING_DEPENDENCIES;
            info->errorDetails = _wcsdup(L"Python runtime not found - required to run yt-dlp.py");
            info->suggestions = _wcsdup(L"Install Python from https://python.org or use the yt-dlp.exe version instead");
            return FALSE;
        }
    }
    
    // Test basic functionality
    if (!TestYtDlpFunctionality(path)) {
        // Try more detailed testing to get better error information
        ValidationInfo tempInfo = {0};
        if (TestYtDlpWithUrl(path, &tempInfo)) {
            // URL test passed but basic test failed - unusual but possible
            FreeValidationInfo(&tempInfo);
        } else {
            // Both tests failed, use the detailed error from URL test
            if (tempInfo.errorDetails) {
                info->errorDetails = tempInfo.errorDetails;
                tempInfo.errorDetails = NULL; // Prevent double-free
            } else {
                info->errorDetails = _wcsdup(L"yt-dlp executable failed to run or timed out");
            }
            
            if (tempInfo.suggestions) {
                info->suggestions = tempInfo.suggestions;
                tempInfo.suggestions = NULL; // Prevent double-free
            } else {
                info->suggestions = _wcsdup(L"The file may be corrupted, incompatible, or missing dependencies. Try downloading a fresh copy of yt-dlp");
            }
            
            info->result = tempInfo.result != VALIDATION_OK ? tempInfo.result : VALIDATION_NOT_EXECUTABLE;
            FreeValidationInfo(&tempInfo);
            return FALSE;
        }
    }
    
    // Get version information
    wchar_t version[256];
    if (GetYtDlpVersion(path, version, sizeof(version) / sizeof(wchar_t))) {
        info->version = _wcsdup(version);
        
        // Check version compatibility
        if (!CheckYtDlpCompatibility(version)) {
            info->result = VALIDATION_VERSION_INCOMPATIBLE;
            info->errorDetails = _wcsdup(L"yt-dlp version may be incompatible or unrecognized");
            info->suggestions = _wcsdup(L"Consider updating to the latest version of yt-dlp");
            return FALSE;
        }
    } else {
        // Could not get version, but functionality test passed
        info->version = _wcsdup(L"Unknown");
    }
    
    // All checks passed
    info->result = VALIDATION_OK;
    info->errorDetails = _wcsdup(L"yt-dlp validation successful");
    info->suggestions = _wcsdup(L"yt-dlp is properly configured and ready to use");
    
    return TRUE;
}

// Function to free ValidationInfo structure
void FreeValidationInfo(ValidationInfo* info) {
    if (!info) {
        return;
    }
    
    if (info->version) {
        free(info->version);
        info->version = NULL;
    }
    
    if (info->errorDetails) {
        free(info->errorDetails);
        info->errorDetails = NULL;
    }
    
    if (info->suggestions) {
        free(info->suggestions);
        info->suggestions = NULL;
    }
}

// Temporary Directory Management Functions

// Function to create a temporary directory using the specified strategy
BOOL CreateYtDlpTempDir(wchar_t* tempPath, size_t pathSize, TempDirStrategy strategy) {
    if (!tempPath || pathSize == 0) {
        return FALSE;
    }
    
    wchar_t basePath[MAX_EXTENDED_PATH];
    BOOL success = FALSE;
    
    switch (strategy) {
        case TEMP_DIR_SYSTEM: {
            // Try system temporary directory
            DWORD result = GetTempPathW(MAX_EXTENDED_PATH, basePath);
            if (result > 0 && result < MAX_EXTENDED_PATH) {
                success = TRUE;
            }
            break;
        }
        
        case TEMP_DIR_DOWNLOAD: {
            // Try download directory from registry settings
            if (LoadSettingFromRegistry(REG_DOWNLOAD_PATH, basePath, MAX_EXTENDED_PATH)) {
                success = TRUE;
            } else {
                // Fall back to default download path
                GetDefaultDownloadPath(basePath, MAX_EXTENDED_PATH);
                success = TRUE;
            }
            break;
        }
        
        case TEMP_DIR_APPDATA: {
            // Try user's AppData\Local directory
            PWSTR localAppDataW = NULL;
            HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &localAppDataW);
            if (SUCCEEDED(hr) && localAppDataW != NULL) {
                wcsncpy(basePath, localAppDataW, MAX_EXTENDED_PATH - 1);
                basePath[MAX_EXTENDED_PATH - 1] = L'\0';
                CoTaskMemFree(localAppDataW);
                success = TRUE;
            }
            break;
        }
        
        case TEMP_DIR_CUSTOM: {
            // For custom strategy, use the current working directory as fallback
            DWORD result = GetCurrentDirectoryW(MAX_EXTENDED_PATH, basePath);
            if (result > 0 && result < MAX_EXTENDED_PATH) {
                success = TRUE;
            }
            break;
        }
        
        default:
            return FALSE;
    }
    
    if (!success) {
        return FALSE;
    }
    
    // Create unique temporary directory name
    wchar_t tempDirName[64];
    SYSTEMTIME st;
    GetSystemTime(&st);
    swprintf(tempDirName, 64, L"ytdlp_temp_%04d%02d%02d_%02d%02d%02d_%03d", 
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    // Construct full temporary directory path
    size_t baseLen = wcslen(basePath);
    size_t nameLen = wcslen(tempDirName);
    
    // Ensure base path ends with backslash
    if (baseLen > 0 && basePath[baseLen - 1] != L'\\') {
        if (baseLen + 1 + nameLen + 1 < pathSize) {
            swprintf(tempPath, pathSize, L"%ls\\%ls", basePath, tempDirName);
        } else {
            return FALSE; // Path too long
        }
    } else {
        if (baseLen + nameLen + 1 < pathSize) {
            swprintf(tempPath, pathSize, L"%ls%ls", basePath, tempDirName);
        } else {
            return FALSE; // Path too long
        }
    }
    
    // Create the directory
    if (!CreateDirectoryW(tempPath, NULL)) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            return FALSE;
        }
    }
    
    // Validate the created directory
    return ValidateTempDirAccess(tempPath);
}

// Function to validate temporary directory access and disk space
BOOL ValidateTempDirAccess(const wchar_t* tempPath) {
    if (!tempPath || wcslen(tempPath) == 0) {
        return FALSE;
    }
    
    // Check if directory exists
    DWORD attributes = GetFileAttributesW(tempPath);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE;
    }
    
    // Test write permissions by creating a temporary test file
    wchar_t testFilePath[MAX_EXTENDED_PATH];
    swprintf(testFilePath, MAX_EXTENDED_PATH, L"%ls\\ytdlp_test_%lu.tmp", tempPath, GetTickCount());
    
    HANDLE hTestFile = CreateFileW(testFilePath, GENERIC_WRITE, 0, NULL, CREATE_NEW, 
                                   FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    
    if (hTestFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    // Write a small test to verify write access
    DWORD bytesWritten;
    const wchar_t* testData = L"test";
    BOOL writeSuccess = WriteFile(hTestFile, testData, (DWORD)(wcslen(testData) * sizeof(wchar_t)), 
                                  &bytesWritten, NULL);
    
    CloseHandle(hTestFile); // File will be deleted automatically due to FILE_FLAG_DELETE_ON_CLOSE
    
    if (!writeSuccess) {
        return FALSE;
    }
    
    // Check available disk space (require at least 100MB free)
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExW(tempPath, &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        const ULONGLONG minRequiredSpace = 100ULL * 1024 * 1024; // 100MB
        if (freeBytesAvailable.QuadPart < minRequiredSpace) {
            return FALSE;
        }
    }
    
    return TRUE;
}

// Function to cleanup temporary directory and its contents
BOOL CleanupYtDlpTempDir(const wchar_t* tempPath) {
    if (!tempPath || wcslen(tempPath) == 0) {
        return FALSE;
    }
    
    // Verify this looks like our temporary directory to avoid accidental deletion
    if (!wcsstr(tempPath, L"ytdlp_temp_")) {
        return FALSE; // Safety check - only delete directories with our naming pattern
    }
    
    // Check if directory exists
    DWORD attributes = GetFileAttributesW(tempPath);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return TRUE; // Directory doesn't exist, consider it cleaned up
    }
    
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE; // Not a directory
    }
    
    // Recursively delete directory contents
    wchar_t searchPath[MAX_EXTENDED_PATH];
    swprintf(searchPath, MAX_EXTENDED_PATH, L"%ls\\*", tempPath);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // Skip . and .. entries
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            
            wchar_t fullPath[MAX_EXTENDED_PATH];
            swprintf(fullPath, MAX_EXTENDED_PATH, L"%ls\\%ls", tempPath, findData.cFileName);
            
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Recursively delete subdirectory
                CleanupYtDlpTempDir(fullPath);
            } else {
                // Delete file
                SetFileAttributesW(fullPath, FILE_ATTRIBUTE_NORMAL); // Remove read-only if set
                DeleteFileW(fullPath);
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
    
    // Remove the directory itself
    return RemoveDirectoryW(tempPath);
}

// Function to create temporary directory with fallback strategies
BOOL CreateYtDlpTempDirWithFallback(wchar_t* tempPath, size_t pathSize) {
    // Try strategies in order of preference
    TempDirStrategy strategies[] = {
        TEMP_DIR_SYSTEM,
        TEMP_DIR_DOWNLOAD,
        TEMP_DIR_APPDATA,
        TEMP_DIR_CUSTOM
    };
    
    for (int i = 0; i < 4; i++) {
        if (CreateYtDlpTempDir(tempPath, pathSize, strategies[i])) {
            return TRUE;
        }
    }
    
    return FALSE;
}

// Process management functions implementation

// Create a yt-dlp process with enhanced error handling and security attributes
ProcessHandle* CreateYtDlpProcess(const wchar_t* commandLine, const ProcessOptions* options) {
    if (!commandLine || !options) {
        return NULL;
    }
    
    ProcessHandle* handle = (ProcessHandle*)malloc(sizeof(ProcessHandle));
    if (!handle) {
        return NULL;
    }
    
    // Initialize handle structure
    memset(handle, 0, sizeof(ProcessHandle));
    handle->hProcess = INVALID_HANDLE_VALUE;
    handle->hThread = INVALID_HANDLE_VALUE;
    handle->hStdOut = INVALID_HANDLE_VALUE;
    handle->hStdErr = INVALID_HANDLE_VALUE;
    handle->processId = 0;
    handle->isRunning = FALSE;
    
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    
    HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
    HANDLE hStdErrRead = NULL, hStdErrWrite = NULL;
    
    // Create pipes for output capture if requested
    if (options->captureOutput) {
        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) ||
            !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
            CleanupProcessHandle(handle);
            return NULL;
        }
        
        // Ensure read handles are not inherited
        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);
        
        handle->hStdOut = hStdOutRead;
        handle->hStdErr = hStdErrRead;
    }
    
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    
    if (options->captureOutput) {
        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdErrWrite;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    
    if (options->hideWindow) {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    
    // Create mutable copy of command line
    size_t cmdLen = wcslen(commandLine) + 1;
    wchar_t* cmdLineCopy = (wchar_t*)malloc(cmdLen * sizeof(wchar_t));
    if (!cmdLineCopy) {
        if (hStdOutWrite) CloseHandle(hStdOutWrite);
        if (hStdErrWrite) CloseHandle(hStdErrWrite);
        CleanupProcessHandle(handle);
        return NULL;
    }
    wcscpy(cmdLineCopy, commandLine);
    
    // Create process with enhanced security attributes
    DWORD creationFlags = CREATE_NO_WINDOW;
    if (options->hideWindow) {
        creationFlags |= CREATE_NEW_CONSOLE;
    }
    
    BOOL success = CreateProcessW(
        NULL,                           // Application name
        cmdLineCopy,                    // Command line
        NULL,                           // Process security attributes
        NULL,                           // Thread security attributes
        options->captureOutput,         // Inherit handles
        creationFlags,                  // Creation flags
        (LPVOID)options->environment,   // Environment
        options->workingDirectory,      // Current directory
        &si,                           // Startup info
        &pi                            // Process information
    );
    
    // Close write handles in parent process
    if (hStdOutWrite) CloseHandle(hStdOutWrite);
    if (hStdErrWrite) CloseHandle(hStdErrWrite);
    
    free(cmdLineCopy);
    
    if (!success) {
        CleanupProcessHandle(handle);
        return NULL;
    }
    
    // Store process information
    handle->hProcess = pi.hProcess;
    handle->hThread = pi.hThread;
    handle->processId = pi.dwProcessId;
    handle->isRunning = TRUE;
    
    return handle;
}

// Wait for process completion with timeout support
BOOL WaitForProcessCompletion(ProcessHandle* handle, DWORD timeoutMs) {
    if (!handle || handle->hProcess == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    DWORD waitResult = WaitForSingleObject(handle->hProcess, timeoutMs);
    
    if (waitResult == WAIT_OBJECT_0) {
        // Process completed normally
        handle->isRunning = FALSE;
        return TRUE;
    } else if (waitResult == WAIT_TIMEOUT) {
        // Process timed out - still running
        return FALSE;
    } else {
        // Wait failed
        handle->isRunning = FALSE;
        return FALSE;
    }
}

// Terminate yt-dlp process gracefully and forcefully if needed
BOOL TerminateProcessSafely(ProcessHandle* handle) {
    if (!handle || handle->hProcess == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    if (!handle->isRunning) {
        return TRUE; // Already terminated
    }
    
    // First try graceful termination by sending WM_CLOSE to console window
    HWND hConsole = GetConsoleWindow();
    if (hConsole) {
        DWORD consoleProcessId;
        GetWindowThreadProcessId(hConsole, &consoleProcessId);
        if (consoleProcessId == handle->processId) {
            PostMessage(hConsole, WM_CLOSE, 0, 0);
            
            // Wait up to 5 seconds for graceful shutdown
            if (WaitForSingleObject(handle->hProcess, 5000) == WAIT_OBJECT_0) {
                handle->isRunning = FALSE;
                return TRUE;
            }
        }
    }
    
    // Try sending CTRL+C signal
    if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, handle->processId)) {
        // Wait up to 3 seconds for graceful shutdown
        if (WaitForSingleObject(handle->hProcess, 3000) == WAIT_OBJECT_0) {
            handle->isRunning = FALSE;
            return TRUE;
        }
    }
    
    // Force termination as last resort
    BOOL result = TerminateProcess(handle->hProcess, 1);
    if (result) {
        // Wait for termination to complete
        WaitForSingleObject(handle->hProcess, 1000);
        handle->isRunning = FALSE;
    }
    
    return result;
}

// Clean up process handle and associated resources
void CleanupProcessHandle(ProcessHandle* handle) {
    if (!handle) {
        return;
    }
    
    // Terminate process if still running
    if (handle->isRunning) {
        TerminateProcessSafely(handle);
    }
    
    // Close all handles
    if (handle->hProcess != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->hProcess);
        handle->hProcess = INVALID_HANDLE_VALUE;
    }
    
    if (handle->hThread != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->hThread);
        handle->hThread = INVALID_HANDLE_VALUE;
    }
    
    if (handle->hStdOut != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->hStdOut);
        handle->hStdOut = INVALID_HANDLE_VALUE;
    }
    
    if (handle->hStdErr != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->hStdErr);
        handle->hStdErr = INVALID_HANDLE_VALUE;
    }
    
    // Reset other fields
    handle->processId = 0;
    handle->isRunning = FALSE;
    
    // Free the handle structure
    free(handle);
}

// Read process output from stdout handle
wchar_t* ReadProcessOutput(ProcessHandle* handle) {
    if (!handle || handle->hStdOut == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    
    const DWORD BUFFER_SIZE = 4096;
    wchar_t* output = (wchar_t*)malloc(BUFFER_SIZE * sizeof(wchar_t));
    if (!output) {
        return NULL;
    }
    
    DWORD totalBytesRead = 0;
    DWORD currentBufferSize = BUFFER_SIZE;
    output[0] = L'\0';
    
    char readBuffer[1024];
    DWORD bytesRead;
    
    // Read output in chunks
    while (ReadFile(handle->hStdOut, readBuffer, sizeof(readBuffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        readBuffer[bytesRead] = '\0';
        
        // Convert from UTF-8 to wide char
        int wideCharsNeeded = MultiByteToWideChar(CP_UTF8, 0, readBuffer, -1, NULL, 0);
        if (wideCharsNeeded > 0) {
            // Check if we need to expand the buffer
            DWORD newTotalSize = totalBytesRead + wideCharsNeeded;
            if (newTotalSize >= currentBufferSize) {
                currentBufferSize = newTotalSize * 2;
                wchar_t* newOutput = (wchar_t*)realloc(output, currentBufferSize * sizeof(wchar_t));
                if (!newOutput) {
                    free(output);
                    return NULL;
                }
                output = newOutput;
            }
            
            // Convert and append to output
            MultiByteToWideChar(CP_UTF8, 0, readBuffer, -1, 
                               output + totalBytesRead, wideCharsNeeded);
            totalBytesRead += wideCharsNeeded - 1; // -1 to not count null terminator
        }
    }
    
    // Ensure null termination
    if (totalBytesRead < currentBufferSize) {
        output[totalBytesRead] = L'\0';
    }
    
    return output;
}

// Process timeout and cancellation support functions

// Monitor process for timeout and cancellation with callback support
typedef struct {
    ProcessHandle* handle;
    DWORD timeoutMs;
    BOOL* cancelFlag;
    BOOL timedOut;
    BOOL cancelled;
} ProcessMonitor;

// Thread function for monitoring process timeout and cancellation
DWORD WINAPI ProcessMonitorThread(LPVOID lpParam) {
    ProcessMonitor* monitor = (ProcessMonitor*)lpParam;
    if (!monitor || !monitor->handle) {
        return 1;
    }
    
    DWORD startTime = GetTickCount();
    DWORD lastActivityCheck = startTime;
    const DWORD CHECK_INTERVAL = 100; // Check every 100ms
    const DWORD HUNG_CHECK_INTERVAL = 5000; // Check for hung process every 5 seconds
    
    while (monitor->handle->isRunning) {
        DWORD currentTime = GetTickCount();
        
        // Check for cancellation request
        if (monitor->cancelFlag && *monitor->cancelFlag) {
            monitor->cancelled = TRUE;
            TerminateProcessSafely(monitor->handle);
            return 0;
        }
        
        // Check for timeout
        if (monitor->timeoutMs != INFINITE) {
            DWORD elapsed = currentTime - startTime;
            if (elapsed >= monitor->timeoutMs) {
                monitor->timedOut = TRUE;
                TerminateProcessSafely(monitor->handle);
                return 0;
            }
        }
        
        // Periodically check if process is hung (every 5 seconds)
        if (currentTime - lastActivityCheck >= HUNG_CHECK_INTERVAL) {
            if (IsProcessHung(monitor->handle, 2000)) { // 2 second responsiveness check
                // Process appears hung, terminate it
                monitor->timedOut = TRUE; // Treat hung process as timeout
                TerminateProcessSafely(monitor->handle);
                return 0;
            }
            lastActivityCheck = currentTime;
        }
        
        // Wait briefly before next check
        Sleep(CHECK_INTERVAL);
    }
    
    return 0;
}

// Execute process with timeout and cancellation support
BOOL ExecuteYtDlpWithTimeout(const wchar_t* commandLine, const ProcessOptions* options, 
                            DWORD timeoutMs, BOOL* cancelFlag, wchar_t** output, DWORD* exitCode) {
    if (!commandLine || !options) {
        return FALSE;
    }
    
    ProcessHandle* handle = CreateYtDlpProcess(commandLine, options);
    if (!handle) {
        return FALSE;
    }
    
    ProcessMonitor monitor = {0};
    monitor.handle = handle;
    monitor.timeoutMs = timeoutMs;
    monitor.cancelFlag = cancelFlag;
    monitor.timedOut = FALSE;
    monitor.cancelled = FALSE;
    
    // Start monitoring thread
    HANDLE hMonitorThread = CreateThread(NULL, 0, ProcessMonitorThread, &monitor, 0, NULL);
    if (!hMonitorThread) {
        CleanupProcessHandle(handle);
        return FALSE;
    }
    
    // Wait for process completion
    BOOL success = WaitForProcessCompletion(handle, INFINITE);
    
    // Stop monitoring thread gracefully
    DWORD threadWaitResult = WaitForSingleObject(hMonitorThread, 2000);
    if (threadWaitResult == WAIT_TIMEOUT) {
        // Force terminate monitoring thread if it doesn't stop
        TerminateThread(hMonitorThread, 1);
    }
    CloseHandle(hMonitorThread);
    
    // Get exit code and output even if process was terminated
    if (exitCode) {
        if (monitor.timedOut || monitor.cancelled) {
            *exitCode = 1; // Indicate abnormal termination
        } else {
            GetExitCodeProcess(handle->hProcess, exitCode);
        }
    }
    
    // Read output if requested and available
    if (output && options->captureOutput) {
        wchar_t* processOutput = ReadProcessOutput(handle);
        if (processOutput) {
            *output = processOutput;
        } else if (monitor.timedOut) {
            // Provide timeout indication in output
            *output = _wcsdup(L"Process terminated due to timeout");
        } else if (monitor.cancelled) {
            // Provide cancellation indication in output
            *output = _wcsdup(L"Process cancelled by user");
        }
    }
    
    CleanupProcessHandle(handle);
    
    // Return success only if process completed without timeout or cancellation
    return success && !monitor.timedOut && !monitor.cancelled;
}

// Check if a process is hung or unresponsive
BOOL IsProcessHung(ProcessHandle* handle, DWORD responseTimeoutMs) {
    if (!handle || !handle->isRunning) {
        return FALSE;
    }
    
    // Try to get process times to check if it's responsive
    FILETIME createTime, exitTime, kernelTime, userTime;
    FILETIME lastKernelTime, lastUserTime;
    
    // Get initial times
    if (!GetProcessTimes(handle->hProcess, &createTime, &exitTime, &lastKernelTime, &lastUserTime)) {
        return TRUE; // Assume hung if we can't get times
    }
    
    // Wait for the specified timeout
    Sleep(responseTimeoutMs);
    
    // Get times again
    if (!GetProcessTimes(handle->hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        return TRUE; // Assume hung if we can't get times
    }
    
    // Compare times to see if process is making progress
    ULARGE_INTEGER lastKernel, lastUser, currentKernel, currentUser;
    
    lastKernel.LowPart = lastKernelTime.dwLowDateTime;
    lastKernel.HighPart = lastKernelTime.dwHighDateTime;
    lastUser.LowPart = lastUserTime.dwLowDateTime;
    lastUser.HighPart = lastUserTime.dwHighDateTime;
    
    currentKernel.LowPart = kernelTime.dwLowDateTime;
    currentKernel.HighPart = kernelTime.dwHighDateTime;
    currentUser.LowPart = userTime.dwLowDateTime;
    currentUser.HighPart = userTime.dwHighDateTime;
    
    // If CPU time hasn't changed, process might be hung
    return (lastKernel.QuadPart == currentKernel.QuadPart && 
            lastUser.QuadPart == currentUser.QuadPart);
}

// Enhanced process monitoring with detailed status
// ProcessStatus is defined in YouTubeCacher.h

// Get detailed status of a process
BOOL GetProcessStatus(ProcessHandle* handle, ProcessStatus* status) {
    if (!handle || !status || handle->hProcess == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    memset(status, 0, sizeof(ProcessStatus));
    status->processId = handle->processId;
    
    // Get process name
    HMODULE hMod;
    DWORD cbNeeded;
    if (EnumProcessModules(handle->hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
        GetModuleBaseNameW(handle->hProcess, hMod, status->processName, MAX_PATH);
    }
    
    // Get CPU time
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(handle->hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER kernel, user;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;
        status->cpuTime = (DWORD)((kernel.QuadPart + user.QuadPart) / 10000); // Convert to milliseconds
    }
    
    // Check if process is responding (simplified check)
    status->isResponding = !IsProcessHung(handle, 1000);
    
    // Get memory usage
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(handle->hProcess, &pmc, sizeof(pmc))) {
        status->memoryUsage = (DWORD)(pmc.WorkingSetSize / 1024); // Convert to KB
    }
    
    return TRUE;
}

// Monitor process with periodic status updates
typedef struct {
    ProcessHandle* handle;
    DWORD timeoutMs;
    BOOL* cancelFlag;
    void (*statusCallback)(const ProcessStatus* status, void* userData);
    void* callbackUserData;
    DWORD statusUpdateIntervalMs;
    BOOL timedOut;
    BOOL cancelled;
} AdvancedProcessMonitor;

// Advanced monitoring thread with status callbacks
DWORD WINAPI AdvancedProcessMonitorThread(LPVOID lpParam) {
    AdvancedProcessMonitor* monitor = (AdvancedProcessMonitor*)lpParam;
    if (!monitor || !monitor->handle) {
        return 1;
    }
    
    DWORD startTime = GetTickCount();
    DWORD lastStatusUpdate = startTime;
    const DWORD CHECK_INTERVAL = 100; // Check every 100ms
    
    while (monitor->handle->isRunning) {
        DWORD currentTime = GetTickCount();
        
        // Check for cancellation request
        if (monitor->cancelFlag && *monitor->cancelFlag) {
            monitor->cancelled = TRUE;
            TerminateProcessSafely(monitor->handle);
            return 0;
        }
        
        // Check for timeout
        if (monitor->timeoutMs != INFINITE) {
            DWORD elapsed = currentTime - startTime;
            if (elapsed >= monitor->timeoutMs) {
                monitor->timedOut = TRUE;
                TerminateProcessSafely(monitor->handle);
                return 0;
            }
        }
        
        // Provide status updates if callback is provided
        if (monitor->statusCallback && 
            (currentTime - lastStatusUpdate >= monitor->statusUpdateIntervalMs)) {
            ProcessStatus status;
            if (GetProcessStatus(monitor->handle, &status)) {
                monitor->statusCallback(&status, monitor->callbackUserData);
            }
            lastStatusUpdate = currentTime;
        }
        
        // Wait briefly before next check
        Sleep(CHECK_INTERVAL);
    }
    
    return 0;
}

// Kill all yt-dlp processes (emergency cleanup)
BOOL KillAllYtDlpProcesses(void) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    PROCESSENTRY32W pe32 = {0};
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    BOOL success = TRUE;
    int processesKilled = 0;
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            // Check if this is a yt-dlp process
            if (_wcsicmp(pe32.szExeFile, L"yt-dlp.exe") == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, 
                                           FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    // Try to terminate the process
                    if (TerminateProcess(hProcess, 1)) {
                        processesKilled++;
                    } else {
                        success = FALSE;
                    }
                    CloseHandle(hProcess);
                }
            }
            // For python.exe, we need to be more careful and check command line
            else if (_wcsicmp(pe32.szExeFile, L"python.exe") == 0) {
                // TODO: Add command line checking to confirm it's running yt-dlp
                // For now, we'll be conservative and not kill python.exe processes
                // unless we can confirm they're yt-dlp instances
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return success;
}

// Execute yt-dlp with advanced monitoring and status callbacks
BOOL ExecuteYtDlpWithAdvancedMonitoring(const wchar_t* commandLine, const ProcessOptions* options,
                                       DWORD timeoutMs, BOOL* cancelFlag, 
                                       ProcessStatusCallback statusCallback, void* callbackUserData,
                                       DWORD statusUpdateIntervalMs, wchar_t** output, DWORD* exitCode) {
    if (!commandLine || !options) {
        return FALSE;
    }
    
    ProcessHandle* handle = CreateYtDlpProcess(commandLine, options);
    if (!handle) {
        return FALSE;
    }
    
    AdvancedProcessMonitor monitor = {0};
    monitor.handle = handle;
    monitor.timeoutMs = timeoutMs;
    monitor.cancelFlag = cancelFlag;
    monitor.statusCallback = statusCallback;
    monitor.callbackUserData = callbackUserData;
    monitor.statusUpdateIntervalMs = statusUpdateIntervalMs > 0 ? statusUpdateIntervalMs : 1000; // Default 1 second
    monitor.timedOut = FALSE;
    monitor.cancelled = FALSE;
    
    // Start advanced monitoring thread
    HANDLE hMonitorThread = CreateThread(NULL, 0, AdvancedProcessMonitorThread, &monitor, 0, NULL);
    if (!hMonitorThread) {
        CleanupProcessHandle(handle);
        return FALSE;
    }
    
    // Wait for process completion
    BOOL success = WaitForProcessCompletion(handle, INFINITE);
    
    // Stop monitoring thread gracefully
    DWORD threadWaitResult = WaitForSingleObject(hMonitorThread, 2000);
    if (threadWaitResult == WAIT_TIMEOUT) {
        // Force terminate monitoring thread if it doesn't stop
        TerminateThread(hMonitorThread, 1);
    }
    CloseHandle(hMonitorThread);
    
    // Get exit code and output
    if (exitCode) {
        if (monitor.timedOut || monitor.cancelled) {
            *exitCode = 1; // Indicate abnormal termination
        } else {
            GetExitCodeProcess(handle->hProcess, exitCode);
        }
    }
    
    // Read output if requested and available
    if (output && options->captureOutput) {
        wchar_t* processOutput = ReadProcessOutput(handle);
        if (processOutput) {
            *output = processOutput;
        } else if (monitor.timedOut) {
            *output = _wcsdup(L"Process terminated due to timeout");
        } else if (monitor.cancelled) {
            *output = _wcsdup(L"Process cancelled by user");
        }
    }
    
    CleanupProcessHandle(handle);
    
    // Return success only if process completed without timeout or cancellation
    return success && !monitor.timedOut && !monitor.cancelled;
}

// Utility functions for cancellation flag management

// Utility function to create a cancellation flag for UI integration
BOOL* CreateCancellationFlag(void) {
    BOOL* flag = (BOOL*)malloc(sizeof(BOOL));
    if (flag) {
        *flag = FALSE;
    }
    return flag;
}

// Utility function to signal cancellation
void SignalCancellation(BOOL* cancelFlag) {
    if (cancelFlag) {
        *cancelFlag = TRUE;
    }
}

// Utility function to check if operation was cancelled
BOOL IsCancelled(const BOOL* cancelFlag) {
    return cancelFlag ? *cancelFlag : FALSE;
}

// Utility function to free cancellation flag
void FreeCancellationFlag(BOOL* cancelFlag) {
    if (cancelFlag) {
        free(cancelFlag);
    }
}

// YtDlp Command Line Integration Functions

// Function to ensure temporary directory is available for yt-dlp operation
BOOL EnsureTempDirForOperation(YtDlpRequest* request, const YtDlpConfig* config) {
    if (!request || !config) {
        return FALSE;
    }
    
    // If temp directory is already specified in request, validate it
    if (request->tempDir && wcslen(request->tempDir) > 0) {
        if (ValidateTempDirAccess(request->tempDir)) {
            return TRUE;
        }
        // If specified temp dir is invalid, fall back to automatic selection
    }
    
    // Create temporary directory using configured strategy
    wchar_t tempPath[MAX_EXTENDED_PATH];
    TempDirStrategy strategy = config->tempDirStrategy;
    
    // Try the configured strategy first
    if (CreateYtDlpTempDir(tempPath, MAX_EXTENDED_PATH, strategy)) {
        // Allocate memory for temp directory in request
        if (request->tempDir) {
            free(request->tempDir);
        }
        request->tempDir = (wchar_t*)malloc((wcslen(tempPath) + 1) * sizeof(wchar_t));
        if (request->tempDir) {
            wcscpy(request->tempDir, tempPath);
            return TRUE;
        }
    }
    
    // If configured strategy failed, try fallback
    if (CreateYtDlpTempDirWithFallback(tempPath, MAX_EXTENDED_PATH)) {
        if (request->tempDir) {
            free(request->tempDir);
        }
        request->tempDir = (wchar_t*)malloc((wcslen(tempPath) + 1) * sizeof(wchar_t));
        if (request->tempDir) {
            wcscpy(request->tempDir, tempPath);
            return TRUE;
        }
    }
    
    return FALSE;
}

// Function to add temporary directory parameter to yt-dlp arguments
BOOL AddTempDirToArgs(const wchar_t* tempDir, wchar_t* args, size_t argsSize) {
    if (!tempDir || !args || argsSize == 0) {
        return FALSE;
    }
    
    // Check if --temp-dir is already present in args
    if (wcsstr(args, L"--temp-dir")) {
        return TRUE; // Already has temp dir argument
    }
    
    // Construct the temp dir argument
    wchar_t tempDirArg[MAX_EXTENDED_PATH + 20];
    swprintf(tempDirArg, MAX_EXTENDED_PATH + 20, L" --temp-dir \"%ls\"", tempDir);
    
    // Check if there's enough space to add the argument
    size_t currentLen = wcslen(args);
    size_t argLen = wcslen(tempDirArg);
    
    if (currentLen + argLen + 1 > argsSize) {
        return FALSE; // Not enough space
    }
    
    // Add the temp dir argument
    wcscat(args, tempDirArg);
    return TRUE;
}

// Function to build complete yt-dlp command line with temp directory integration
BOOL BuildYtDlpCommandLine(const YtDlpRequest* request, const YtDlpConfig* config, 
                          wchar_t* commandLine, size_t commandLineSize) {
    if (!request || !config || !commandLine || commandLineSize == 0) {
        return FALSE;
    }
    
    // Start with the yt-dlp executable path (quoted for safety)
    swprintf(commandLine, commandLineSize, L"\"%ls\"", config->ytDlpPath);
    
    // Add operation-specific arguments
    wchar_t operationArgs[1024] = {0};
    
    switch (request->operation) {
        case YTDLP_OP_GET_INFO:
            wcscpy(operationArgs, L" --dump-json --no-download --no-warnings");
            break;
            
        case YTDLP_OP_DOWNLOAD:
            wcscpy(operationArgs, L" --no-warnings --progress");
            if (request->outputPath && wcslen(request->outputPath) > 0) {
                wchar_t outputArg[MAX_EXTENDED_PATH + 20];
                swprintf(outputArg, MAX_EXTENDED_PATH + 20, L" -o \"%ls\\%%(title)s.%%(ext)s\"", request->outputPath);
                wcscat(operationArgs, outputArg);
            }
            break;
            
        case YTDLP_OP_VALIDATE:
            wcscpy(operationArgs, L" --version");
            break;
            
        default:
            return FALSE;
    }
    
    // Add operation arguments to command line
    if (wcslen(commandLine) + wcslen(operationArgs) + 1 > commandLineSize) {
        return FALSE;
    }
    wcscat(commandLine, operationArgs);
    
    // Add temporary directory if available
    if (request->tempDir && wcslen(request->tempDir) > 0) {
        if (!AddTempDirToArgs(request->tempDir, commandLine, commandLineSize)) {
            return FALSE; // Failed to add temp dir
        }
    }
    
    // Add custom arguments if specified
    if (request->useCustomArgs && request->customArgs && wcslen(request->customArgs) > 0) {
        if (wcslen(commandLine) + wcslen(request->customArgs) + 2 > commandLineSize) {
            return FALSE;
        }
        wcscat(commandLine, L" ");
        wcscat(commandLine, request->customArgs);
    } else if (wcslen(config->defaultArgs) > 0) {
        // Add default arguments from config
        if (wcslen(commandLine) + wcslen(config->defaultArgs) + 2 > commandLineSize) {
            return FALSE;
        }
        wcscat(commandLine, L" ");
        wcscat(commandLine, config->defaultArgs);
    }
    
    // Add URL for operations that need it
    if (request->operation != YTDLP_OP_VALIDATE) {
        if (!request->url || wcslen(request->url) == 0) {
            return FALSE; // URL required for this operation
        }
        
        wchar_t urlArg[MAX_URL_LENGTH + 10];
        swprintf(urlArg, MAX_URL_LENGTH + 10, L" \"%ls\"", request->url);
        
        if (wcslen(commandLine) + wcslen(urlArg) + 1 > commandLineSize) {
            return FALSE;
        }
        wcscat(commandLine, urlArg);
    }
    
    return TRUE;
}

// Function to handle temp directory creation failures with user feedback
BOOL HandleTempDirFailure(HWND hParent, const YtDlpConfig* config) {
    wchar_t errorMsg[1024];
    wchar_t configuredPath[MAX_EXTENDED_PATH] = {0};
    
    // Try to get information about the configured temp directory strategy
    switch (config->tempDirStrategy) {
        case TEMP_DIR_SYSTEM:
            wcscpy(configuredPath, L"System temporary directory");
            break;
        case TEMP_DIR_DOWNLOAD:
            wcscpy(configuredPath, config->defaultTempDir);
            if (wcslen(configuredPath) == 0) {
                wcscpy(configuredPath, L"Download directory");
            }
            break;
        case TEMP_DIR_APPDATA:
            wcscpy(configuredPath, L"User AppData\\Local directory");
            break;
        case TEMP_DIR_CUSTOM:
            wcscpy(configuredPath, config->defaultTempDir);
            if (wcslen(configuredPath) == 0) {
                wcscpy(configuredPath, L"Custom directory");
            }
            break;
        default:
            wcscpy(configuredPath, L"Unknown directory");
            break;
    }
    
    swprintf(errorMsg, 1024, 
        L"Failed to create temporary directory for yt-dlp operation.\n\n"
        L"Attempted location: %ls\n\n"
        L"Possible causes:\n"
        L"• Insufficient disk space (requires at least 100MB)\n"
        L"• No write permissions to the directory\n"
        L"• Directory path is invalid or inaccessible\n\n"
        L"Please check your system's temporary directory settings or "
        L"configure a different temporary directory in Settings.",
        configuredPath);
    
    MessageBoxW(hParent, errorMsg, L"Temporary Directory Error", MB_OK | MB_ICONWARNING);
    return FALSE;
}

// ============================================================================
// Enhanced Error Handling and Analysis Functions
// ============================================================================

// Function to analyze yt-dlp error output and categorize error types
ErrorAnalysis* AnalyzeYtDlpError(const YtDlpResult* result) {
    if (!result) {
        return NULL;
    }
    
    ErrorAnalysis* analysis = (ErrorAnalysis*)malloc(sizeof(ErrorAnalysis));
    if (!analysis) {
        return NULL;
    }
    
    // Initialize structure
    memset(analysis, 0, sizeof(ErrorAnalysis));
    analysis->type = ERROR_TYPE_UNKNOWN;
    
    // Analyze based on exit code and error output
    const wchar_t* errorOutput = result->errorMessage ? result->errorMessage : L"";
    const wchar_t* standardOutput = result->output ? result->output : L"";
    
    // Check for temporary directory errors
    if (wcsstr(errorOutput, L"temp") || wcsstr(errorOutput, L"temporary") ||
        wcsstr(errorOutput, L"Unable to create") || wcsstr(errorOutput, L"Permission denied") ||
        wcsstr(standardOutput, L"temp") || wcsstr(standardOutput, L"temporary")) {
        
        analysis->type = ERROR_TYPE_TEMP_DIR;
        analysis->description = _wcsdup(L"Temporary directory creation or access failed");
        analysis->solution = _wcsdup(L"Try running as administrator, check disk space, or change the temporary directory location in settings");
        analysis->technicalDetails = _wcsdup(L"yt-dlp requires write access to a temporary directory for processing downloads");
    }
    // Check for network-related errors
    else if (wcsstr(errorOutput, L"network") || wcsstr(errorOutput, L"connection") ||
             wcsstr(errorOutput, L"timeout") || wcsstr(errorOutput, L"HTTP") ||
             wcsstr(errorOutput, L"SSL") || wcsstr(errorOutput, L"certificate") ||
             wcsstr(standardOutput, L"Unable to download") || 
             result->exitCode == 1) { // Common network error exit code
        
        analysis->type = ERROR_TYPE_NETWORK;
        analysis->description = _wcsdup(L"Network connection or download error occurred");
        analysis->solution = _wcsdup(L"Check your internet connection, try again later, or verify the URL is accessible");
        analysis->technicalDetails = _wcsdup(L"Network errors can be caused by connectivity issues, server problems, or blocked content");
    }
    // Check for permission errors
    else if (wcsstr(errorOutput, L"permission") || wcsstr(errorOutput, L"access") ||
             wcsstr(errorOutput, L"denied") || wcsstr(errorOutput, L"forbidden") ||
             result->exitCode == 13) { // Permission denied exit code
        
        analysis->type = ERROR_TYPE_PERMISSIONS;
        analysis->description = _wcsdup(L"File system permission error");
        analysis->solution = _wcsdup(L"Run as administrator or check file/folder permissions for the download location");
        analysis->technicalDetails = _wcsdup(L"The application lacks sufficient permissions to write to the specified location");
    }
    // Check for missing dependencies
    else if (wcsstr(errorOutput, L"python") || wcsstr(errorOutput, L"not found") ||
             wcsstr(errorOutput, L"command not found") || wcsstr(errorOutput, L"ffmpeg") ||
             result->exitCode == 127) { // Command not found exit code
        
        analysis->type = ERROR_TYPE_DEPENDENCIES;
        analysis->description = _wcsdup(L"Missing required dependencies (Python, ffmpeg, or yt-dlp)");
        analysis->solution = _wcsdup(L"Install Python and yt-dlp, ensure they are in your PATH, or update the yt-dlp path in settings");
        analysis->technicalDetails = _wcsdup(L"yt-dlp requires Python runtime and may need ffmpeg for certain operations");
    }
    // Check for invalid URL errors
    else if (wcsstr(errorOutput, L"URL") || wcsstr(errorOutput, L"invalid") ||
             wcsstr(errorOutput, L"unsupported") || wcsstr(errorOutput, L"not supported") ||
             wcsstr(standardOutput, L"Unsupported URL")) {
        
        analysis->type = ERROR_TYPE_URL_INVALID;
        analysis->description = _wcsdup(L"Invalid or unsupported URL provided");
        analysis->solution = _wcsdup(L"Verify the URL is correct and from a supported site (YouTube, etc.)");
        analysis->technicalDetails = _wcsdup(L"The provided URL format is not recognized or the site is not supported by yt-dlp");
    }
    // Check for disk space errors
    else if (wcsstr(errorOutput, L"space") || wcsstr(errorOutput, L"disk") ||
             wcsstr(errorOutput, L"full") || wcsstr(errorOutput, L"No space left") ||
             result->exitCode == 28) { // No space left on device
        
        analysis->type = ERROR_TYPE_DISK_SPACE;
        analysis->description = _wcsdup(L"Insufficient disk space for download");
        analysis->solution = _wcsdup(L"Free up disk space or choose a different download location");
        analysis->technicalDetails = _wcsdup(L"The download requires more disk space than is currently available");
    }
    // Default unknown error handling
    else {
        analysis->type = ERROR_TYPE_UNKNOWN;
        
        // Create a more descriptive error message based on exit code
        wchar_t* description = (wchar_t*)malloc(512 * sizeof(wchar_t));
        if (description) {
            if (result->exitCode == 0) {
                wcscpy(description, L"Operation completed but with unexpected output");
            } else {
                swprintf(description, 512, L"yt-dlp operation failed with exit code %lu", result->exitCode);
            }
            analysis->description = description;
        } else {
            analysis->description = _wcsdup(L"Unknown error occurred during yt-dlp operation");
        }
        
        analysis->solution = _wcsdup(L"Check the detailed error output, verify yt-dlp installation, or try a different URL");
        analysis->technicalDetails = _wcsdup(L"The error type could not be automatically determined from the output");
    }
    
    return analysis;
}

// Function to free ErrorAnalysis structure and its allocated memory
void FreeErrorAnalysis(ErrorAnalysis* analysis) {
    if (!analysis) {
        return;
    }
    
    if (analysis->description) {
        free(analysis->description);
        analysis->description = NULL;
    }
    
    if (analysis->solution) {
        free(analysis->solution);
        analysis->solution = NULL;
    }
    
    if (analysis->technicalDetails) {
        free(analysis->technicalDetails);
        analysis->technicalDetails = NULL;
    }
    
    free(analysis);
}

// Function to generate comprehensive diagnostic report for troubleshooting
BOOL GenerateDiagnosticReport(const YtDlpRequest* request, const YtDlpResult* result, 
                             const YtDlpConfig* config, wchar_t* report, size_t reportSize) {
    if (!request || !result || !report || reportSize == 0) {
        return FALSE;
    }
    
    // Initialize report buffer
    report[0] = L'\0';
    
    // Get current system time
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Start building the diagnostic report
    wchar_t tempBuffer[2048];
    
    // Header section
    swprintf(tempBuffer, sizeof(tempBuffer)/sizeof(wchar_t), 
        L"=== YouTube Cacher Diagnostic Report ===\n"
        L"Generated: %04d-%02d-%02d %02d:%02d:%02d\n"
        L"Application: %s %s\n\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        APP_NAME, APP_VERSION);
    
    if (wcslen(report) + wcslen(tempBuffer) < reportSize - 1) {
        wcscat(report, tempBuffer);
    }
    
    // Operation details section
    const wchar_t* operationType = L"Unknown";
    switch (request->operation) {
        case YTDLP_OP_GET_INFO:
            operationType = L"Get Video Information";
            break;
        case YTDLP_OP_DOWNLOAD:
            operationType = L"Download Video";
            break;
        case YTDLP_OP_VALIDATE:
            operationType = L"Validate yt-dlp";
            break;
    }
    
    swprintf(tempBuffer, sizeof(tempBuffer)/sizeof(wchar_t),
        L"=== Operation Details ===\n"
        L"Operation Type: %s\n"
        L"URL: %s\n"
        L"Output Path: %s\n"
        L"Temp Directory: %s\n"
        L"Custom Arguments: %s\n"
        L"Exit Code: %lu\n"
        L"Success: %s\n\n",
        operationType,
        request->url ? request->url : L"(not specified)",
        request->outputPath ? request->outputPath : L"(not specified)",
        request->tempDir ? request->tempDir : L"(not specified)",
        (request->useCustomArgs && request->customArgs) ? request->customArgs : L"(none)",
        result->exitCode,
        result->success ? L"Yes" : L"No");
    
    if (wcslen(report) + wcslen(tempBuffer) < reportSize - 1) {
        wcscat(report, tempBuffer);
    }
    
    // Configuration section
    if (config) {
        swprintf(tempBuffer, sizeof(tempBuffer)/sizeof(wchar_t),
            L"=== Configuration ===\n"
            L"yt-dlp Path: %s\n"
            L"Default Temp Dir: %s\n"
            L"Default Arguments: %s\n"
            L"Timeout (seconds): %lu\n"
            L"Verbose Logging: %s\n"
            L"Auto Retry: %s\n"
            L"Temp Dir Strategy: %d\n\n",
            config->ytDlpPath,
            config->defaultTempDir,
            config->defaultArgs,
            config->timeoutSeconds,
            config->enableVerboseLogging ? L"Yes" : L"No",
            config->autoRetryOnFailure ? L"Yes" : L"No",
            config->tempDirStrategy);
        
        if (wcslen(report) + wcslen(tempBuffer) < reportSize - 1) {
            wcscat(report, tempBuffer);
        }
    }
    
    // System information section
    OSVERSIONINFOEXW osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXW));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
    
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD computerNameSize = sizeof(computerName) / sizeof(wchar_t);
    GetComputerNameW(computerName, &computerNameSize);
    
    // Get available disk space for temp directory
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    BOOL diskSpaceResult = FALSE;
    if (request->tempDir && wcslen(request->tempDir) > 0) {
        diskSpaceResult = GetDiskFreeSpaceExW(request->tempDir, &freeBytesAvailable, &totalBytes, &totalFreeBytes);
    }
    
    swprintf(tempBuffer, sizeof(tempBuffer)/sizeof(wchar_t),
        L"=== System Information ===\n"
        L"Computer Name: %s\n"
        L"Windows Version: %lu.%lu Build %lu\n"
        L"Available Disk Space: %s\n\n",
        computerName,
        osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber,
        diskSpaceResult ? L"Available" : L"Unknown");
    
    if (wcslen(report) + wcslen(tempBuffer) < reportSize - 1) {
        wcscat(report, tempBuffer);
    }
    
    // Error analysis section
    ErrorAnalysis* analysis = AnalyzeYtDlpError(result);
    if (analysis) {
        const wchar_t* errorTypeName = L"Unknown";
        switch (analysis->type) {
            case ERROR_TYPE_TEMP_DIR:
                errorTypeName = L"Temporary Directory";
                break;
            case ERROR_TYPE_NETWORK:
                errorTypeName = L"Network/Connection";
                break;
            case ERROR_TYPE_PERMISSIONS:
                errorTypeName = L"File Permissions";
                break;
            case ERROR_TYPE_DEPENDENCIES:
                errorTypeName = L"Missing Dependencies";
                break;
            case ERROR_TYPE_URL_INVALID:
                errorTypeName = L"Invalid URL";
                break;
            case ERROR_TYPE_DISK_SPACE:
                errorTypeName = L"Insufficient Disk Space";
                break;
            case ERROR_TYPE_UNKNOWN:
            default:
                errorTypeName = L"Unknown";
                break;
        }
        
        swprintf(tempBuffer, sizeof(tempBuffer)/sizeof(wchar_t),
            L"=== Error Analysis ===\n"
            L"Error Type: %s\n"
            L"Description: %s\n"
            L"Suggested Solution: %s\n"
            L"Technical Details: %s\n\n",
            errorTypeName,
            analysis->description ? analysis->description : L"(none)",
            analysis->solution ? analysis->solution : L"(none)",
            analysis->technicalDetails ? analysis->technicalDetails : L"(none)");
        
        if (wcslen(report) + wcslen(tempBuffer) < reportSize - 1) {
            wcscat(report, tempBuffer);
        }
        
        FreeErrorAnalysis(analysis);
    }
    
    // Output section (truncated if too long)
    if (result->output && wcslen(result->output) > 0) {
        wcscat(report, L"=== yt-dlp Output ===\n");
        
        size_t remainingSpace = reportSize - wcslen(report) - 1;
        size_t outputLen = wcslen(result->output);
        
        if (outputLen < remainingSpace - 100) { // Leave some space for error output
            wcscat(report, result->output);
            wcscat(report, L"\n\n");
        } else {
            // Truncate output if too long
            wcsncat(report, result->output, remainingSpace - 150);
            wcscat(report, L"\n... (output truncated) ...\n\n");
        }
    }
    
    // Error output section
    if (result->errorMessage && wcslen(result->errorMessage) > 0) {
        wcscat(report, L"=== Error Output ===\n");
        
        size_t remainingSpace = reportSize - wcslen(report) - 1;
        size_t errorLen = wcslen(result->errorMessage);
        
        if (errorLen < remainingSpace - 50) {
            wcscat(report, result->errorMessage);
            wcscat(report, L"\n\n");
        } else {
            // Truncate error output if too long
            wcsncat(report, result->errorMessage, remainingSpace - 100);
            wcscat(report, L"\n... (error output truncated) ...\n\n");
        }
    }
    
    // Troubleshooting steps section
    wcscat(report, L"=== Troubleshooting Steps ===\n");
    wcscat(report, L"1. Verify yt-dlp is installed and accessible\n");
    wcscat(report, L"2. Check internet connection and URL validity\n");
    wcscat(report, L"3. Ensure sufficient disk space and permissions\n");
    wcscat(report, L"4. Try running as administrator if permission errors occur\n");
    wcscat(report, L"5. Update yt-dlp to the latest version\n");
    wcscat(report, L"6. Check Windows Defender or antivirus software settings\n\n");
    
    // Footer
    wcscat(report, L"=== End of Diagnostic Report ===\n");
    
    return TRUE;
}