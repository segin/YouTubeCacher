#include "YouTubeCacher.h"

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

// Format duration string to proper time format
// Handles: single numbers (seconds), single digits, ensures proper MM:SS or HH:MM:SS format
void FormatDuration(wchar_t* duration, size_t bufferSize) {
    if (!duration || bufferSize == 0) return;
    
    // Remove any whitespace
    wchar_t* src = duration;
    wchar_t* dst = duration;
    while (*src) {
        if (!iswspace(*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = L'\0';
    
    // If empty, leave as is
    if (wcslen(duration) == 0) return;
    
    // Check if it's already in time format (contains colon)
    if (wcschr(duration, L':') != NULL) {
        // Parse existing time format and reformat with proper leading zeros
        wchar_t* firstColon = wcschr(duration, L':');
        wchar_t* secondColon = wcschr(firstColon + 1, L':');
        
        if (secondColon != NULL) {
            // Format: H:M:S or HH:MM:SS - parse and reformat
            int hours = 0, minutes = 0, seconds = 0;
            if (swscanf(duration, L"%d:%d:%d", &hours, &minutes, &seconds) == 3) {
                wchar_t temp[32];
                swprintf(temp, 32, L"%02d:%02d:%02d", hours, minutes, seconds);
                wcsncpy(duration, temp, bufferSize - 1);
                duration[bufferSize - 1] = L'\0';
            }
        } else {
            // Format: M:S or MM:SS - parse and reformat
            int minutes = 0, seconds = 0;
            if (swscanf(duration, L"%d:%d", &minutes, &seconds) == 2) {
                wchar_t temp[32];
                swprintf(temp, 32, L"%02d:%02d", minutes, seconds);
                wcsncpy(duration, temp, bufferSize - 1);
                duration[bufferSize - 1] = L'\0';
            }
        }
        return;
    }
    
    // Check if it's a pure number (seconds)
    BOOL isNumber = TRUE;
    for (size_t i = 0; i < wcslen(duration); i++) {
        if (!iswdigit(duration[i])) {
            isNumber = FALSE;
            break;
        }
    }
    
    if (isNumber) {
        int totalSeconds = _wtoi(duration);
        
        if (totalSeconds < 0) {
            // Invalid number, leave as is
            return;
        }
        
        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;
        
        wchar_t formatted[32];
        
        if (hours > 0) {
            // Format as HH:MM:SS (leading zeros for all segments, hours can go past 99)
            swprintf(formatted, 32, L"%02d:%02d:%02d", hours, minutes, seconds);
        } else {
            // Format as MM:SS (leading zeros for minutes and seconds)
            swprintf(formatted, 32, L"%02d:%02d", minutes, seconds);
        }
        
        wcsncpy(duration, formatted, bufferSize - 1);
        duration[bufferSize - 1] = L'\0';
    } else {
        // Not a pure number, check for single digit case
        if (wcslen(duration) == 1 && iswdigit(duration[0])) {
            // Single digit - treat as seconds and format as "00:0X"
            wchar_t temp[32];
            swprintf(temp, 32, L"00:0%lc", duration[0]);
            wcsncpy(duration, temp, bufferSize - 1);
            duration[bufferSize - 1] = L'\0';
        }
        // For other non-numeric formats, leave as is
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
    
    // Load debug setting
    if (LoadSettingFromRegistry(REG_ENABLE_DEBUG, buffer, MAX_EXTENDED_PATH)) {
        BOOL enableDebug = (wcscmp(buffer, L"1") == 0);
        CheckDlgButton(hDlg, IDC_ENABLE_DEBUG, enableDebug ? BST_CHECKED : BST_UNCHECKED);
        BOOL enableLogfile;
        GetDebugState(&enableLogfile, &enableLogfile);  // Get current logfile state
        SetDebugState(enableDebug, enableLogfile);
    } else {
        // Default to unchecked
        CheckDlgButton(hDlg, IDC_ENABLE_DEBUG, BST_UNCHECKED);
        BOOL enableLogfile;
        GetDebugState(&enableLogfile, &enableLogfile);  // Get current logfile state
        SetDebugState(FALSE, enableLogfile);
    }
    
    // Load logfile setting
    if (LoadSettingFromRegistry(REG_ENABLE_LOGFILE, buffer, MAX_EXTENDED_PATH)) {
        BOOL enableLogfile = (wcscmp(buffer, L"1") == 0);
        CheckDlgButton(hDlg, IDC_ENABLE_LOGFILE, enableLogfile ? BST_CHECKED : BST_UNCHECKED);
        BOOL enableDebug;
        GetDebugState(&enableDebug, &enableDebug);  // Get current debug state
        SetDebugState(enableDebug, enableLogfile);
    } else {
        // Default to unchecked
        CheckDlgButton(hDlg, IDC_ENABLE_LOGFILE, BST_UNCHECKED);
        BOOL enableDebug;
        GetDebugState(&enableDebug, &enableDebug);  // Get current debug state
        SetDebugState(enableDebug, FALSE);
    }
    
    // Load autopaste setting
    if (LoadSettingFromRegistry(REG_ENABLE_AUTOPASTE, buffer, MAX_EXTENDED_PATH)) {
        BOOL enableAutopaste = (wcscmp(buffer, L"1") == 0);
        CheckDlgButton(hDlg, IDC_ENABLE_AUTOPASTE, enableAutopaste ? BST_CHECKED : BST_UNCHECKED);
        SetAutopasteState(enableAutopaste);
    } else {
        // Default to checked (enabled)
        CheckDlgButton(hDlg, IDC_ENABLE_AUTOPASTE, BST_CHECKED);
        SetAutopasteState(TRUE);
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
            UnifiedDialogConfig config = {0};
            config.dialogType = UNIFIED_DIALOG_WARNING;
            config.title = L"Invalid Arguments";
            config.message = L"Custom yt-dlp arguments contain potentially dangerous options and were not saved.";
            config.details = L"The custom arguments you entered contain options that could be used maliciously or cause system instability. For security reasons, these arguments have been rejected.";
            config.tab1_name = L"Details";
            config.tab2_content = L"Blocked argument types:\n\n"
                                L"• --exec (executes arbitrary commands)\n"
                                L"• --batch-file (processes batch files)\n"
                                L"• Other potentially harmful options\n\n"
                                L"Safe alternatives:\n"
                                L"• Use format selection: -f best[height<=720]\n"
                                L"• Add metadata: --add-metadata\n"
                                L"• Embed subtitles: --embed-subs\n"
                                L"• Set output template: -o \"%(title)s.%(ext)s\"\n\n"
                                L"Please remove the dangerous arguments and try again.";
            config.tab2_name = L"Safe Options";
            config.showDetailsButton = TRUE;
            config.showCopyButton = FALSE;
            
            ShowUnifiedDialog(hDlg, &config);
        }
    } else {
        // Empty arguments - save empty string
        SaveSettingToRegistry(REG_CUSTOM_ARGS, L"");
    }
    
    // Save debug setting
    BOOL enableDebug = (IsDlgButtonChecked(hDlg, IDC_ENABLE_DEBUG) == BST_CHECKED);
    SaveSettingToRegistry(REG_ENABLE_DEBUG, enableDebug ? L"1" : L"0");
    
    // Save logfile setting
    BOOL enableLogfile = (IsDlgButtonChecked(hDlg, IDC_ENABLE_LOGFILE) == BST_CHECKED);
    
    // If logging is being disabled, write session end marker first
    BOOL currentEnableDebug, currentEnableLogfile;
    GetDebugState(&currentEnableDebug, &currentEnableLogfile);
    if (currentEnableLogfile && !enableLogfile) {
        WriteSessionEndToLogfile(L"Logging disabled by user");
    }
    
    SaveSettingToRegistry(REG_ENABLE_LOGFILE, enableLogfile ? L"1" : L"0");
    SetDebugState(enableDebug, enableLogfile);
    
    // Save autopaste setting
    BOOL enableAutopaste = (IsDlgButtonChecked(hDlg, IDC_ENABLE_AUTOPASTE) == BST_CHECKED);
    SaveSettingToRegistry(REG_ENABLE_AUTOPASTE, enableAutopaste ? L"1" : L"0");
    SetAutopasteState(enableAutopaste);
}