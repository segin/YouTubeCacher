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

// MinGW32 provides _wcsdup and _wcsicmp, no custom implementation needed

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

// Global cache manager
CacheManager g_cacheManager;

// Global cached video metadata
CachedVideoMetadata g_cachedVideoMetadata;

// Global download state flag
BOOL g_isDownloading = FALSE;

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
    
    // Load debug setting
    if (LoadSettingFromRegistry(REG_ENABLE_DEBUG, buffer, MAX_EXTENDED_PATH)) {
        BOOL enableDebug = (wcscmp(buffer, L"1") == 0);
        CheckDlgButton(hDlg, IDC_ENABLE_DEBUG, enableDebug ? BST_CHECKED : BST_UNCHECKED);
    } else {
        // Default to unchecked
        CheckDlgButton(hDlg, IDC_ENABLE_DEBUG, BST_UNCHECKED);
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
            ShowWarningMessage(hDlg, L"Invalid Arguments", 
                             L"Custom yt-dlp arguments contain potentially dangerous options and were not saved.\n\nPlease remove any --exec, --batch-file, or other potentially harmful arguments.");
        }
    } else {
        // Empty arguments - save empty string
        SaveSettingToRegistry(REG_CUSTOM_ARGS, L"");
    }
    
    // Save debug setting
    BOOL enableDebug = (IsDlgButtonChecked(hDlg, IDC_ENABLE_DEBUG) == BST_CHECKED);
    SaveSettingToRegistry(REG_ENABLE_DEBUG, enableDebug ? L"1" : L"0");
}

// Stub implementations for YtDlp Manager system functions
// These are placeholders for the actual implementations from tasks 1-7

BOOL InitializeYtDlpConfig(YtDlpConfig* config) {
    if (!config) return FALSE;
    
    // Initialize with default values
    memset(config, 0, sizeof(YtDlpConfig));
    
    // Load yt-dlp path from registry or use default
    if (!LoadSettingFromRegistry(REG_YTDLP_PATH, config->ytDlpPath, MAX_EXTENDED_PATH)) {
        GetDefaultYtDlpPath(config->ytDlpPath, MAX_EXTENDED_PATH);
    }
    
    // Load custom yt-dlp arguments from registry
    if (!LoadSettingFromRegistry(REG_CUSTOM_ARGS, config->defaultArgs, 1024)) {
        // Use empty default if not found in registry
        config->defaultArgs[0] = L'\0';
    }
    
    // Set default timeout
    config->timeoutSeconds = 300; // 5 minutes
    config->tempDirStrategy = TEMP_DIR_SYSTEM;
    config->enableVerboseLogging = FALSE;
    config->autoRetryOnFailure = FALSE;
    
    return TRUE;
}

void CleanupYtDlpConfig(YtDlpConfig* config) {
    // Placeholder - no dynamic memory to clean up in current implementation
    (void)config;
}

BOOL ValidateYtDlpComprehensive(const wchar_t* path, ValidationInfo* info) {
    if (!path || !info) return FALSE;
    
    // Initialize validation info
    memset(info, 0, sizeof(ValidationInfo));
    
    // Use existing validation function
    if (ValidateYtDlpExecutable(path)) {
        info->result = VALIDATION_OK;
        info->version = _wcsdup(L"Unknown version");
        info->errorDetails = NULL;
        info->suggestions = NULL;
        return TRUE;
    } else {
        info->result = VALIDATION_NOT_FOUND;
        info->version = NULL;
        info->errorDetails = _wcsdup(L"yt-dlp executable not found or invalid");
        info->suggestions = _wcsdup(L"Please check the path in File > Settings and ensure yt-dlp is properly installed");
        return FALSE;
    }
}

void FreeValidationInfo(ValidationInfo* info) {
    if (!info) return;
    
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

YtDlpRequest* CreateYtDlpRequest(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath) {
    YtDlpRequest* request = (YtDlpRequest*)malloc(sizeof(YtDlpRequest));
    if (!request) return NULL;
    
    memset(request, 0, sizeof(YtDlpRequest));
    request->operation = operation;
    
    if (url) {
        request->url = _wcsdup(url);
    }
    if (outputPath) {
        request->outputPath = _wcsdup(outputPath);
    }
    
    return request;
}

void FreeYtDlpRequest(YtDlpRequest* request) {
    if (!request) return;
    
    if (request->url) {
        free(request->url);
    }
    if (request->outputPath) {
        free(request->outputPath);
    }
    if (request->tempDir) {
        free(request->tempDir);
    }
    if (request->customArgs) {
        free(request->customArgs);
    }
    
    free(request);
}

BOOL CreateTempDirectory(const YtDlpConfig* config, wchar_t* tempDir, size_t tempDirSize) {
    (void)config; // Unused parameter
    
    // Get system temp directory
    DWORD result = GetTempPathW((DWORD)tempDirSize, tempDir);
    if (result == 0 || result >= tempDirSize) {
        // Fallback to user's local app data if GetTempPath fails
        PWSTR localAppDataW = NULL;
        HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &localAppDataW);
        if (SUCCEEDED(hr) && localAppDataW) {
            swprintf(tempDir, tempDirSize, L"%ls\\Temp", localAppDataW);
            CoTaskMemFree(localAppDataW);
        } else {
            // Ultimate fallback to current directory
            wcscpy(tempDir, L".");
        }
    }
    
    // Validate that we're not trying to use a system directory
    if (wcsstr(tempDir, L"\\Windows\\") != NULL || 
        wcsstr(tempDir, L"\\System32\\") != NULL ||
        wcsstr(tempDir, L"\\SysWOW64\\") != NULL) {
        // Force fallback to user's local app data
        PWSTR localAppDataW = NULL;
        HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &localAppDataW);
        if (SUCCEEDED(hr) && localAppDataW) {
            swprintf(tempDir, tempDirSize, L"%ls\\YouTubeCacher\\Temp", localAppDataW);
            CoTaskMemFree(localAppDataW);
        } else {
            wcscpy(tempDir, L".\\temp");
        }
    }
    
    // Append unique subdirectory name
    wchar_t uniqueName[64];
    swprintf(uniqueName, 64, L"YouTubeCacher_%lu", GetTickCount());
    
    if (wcslen(tempDir) + wcslen(uniqueName) + 2 >= tempDirSize) {
        return FALSE;
    }
    
    wcscat(tempDir, L"\\");
    wcscat(tempDir, uniqueName);
    
    // Create the directory (and parent directories if needed)
    wchar_t parentDir[MAX_EXTENDED_PATH];
    wcscpy(parentDir, tempDir);
    wchar_t* lastSlash = wcsrchr(parentDir, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
        CreateDirectoryW(parentDir, NULL); // Create parent if it doesn't exist
    }
    
    return CreateDirectoryW(tempDir, NULL);
}

BOOL CreateYtDlpTempDirWithFallback(wchar_t* tempPath, size_t pathSize) {
    // Try user's local app data first (safer than system temp)
    PWSTR localAppDataW = NULL;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &localAppDataW);
    if (SUCCEEDED(hr) && localAppDataW) {
        wchar_t uniqueName[64];
        swprintf(uniqueName, 64, L"YouTubeCacher_%lu", GetTickCount());
        swprintf(tempPath, pathSize, L"%ls\\YouTubeCacher\\Temp\\%ls", localAppDataW, uniqueName);
        CoTaskMemFree(localAppDataW);
        
        // Create parent directories
        wchar_t parentDir[MAX_EXTENDED_PATH];
        wcscpy(parentDir, tempPath);
        wchar_t* lastSlash = wcsrchr(parentDir, L'\\');
        if (lastSlash) {
            *lastSlash = L'\0';
            // Create intermediate directories
            wchar_t* slash = wcschr(parentDir + 3, L'\\'); // Skip drive letter
            while (slash) {
                *slash = L'\0';
                CreateDirectoryW(parentDir, NULL);
                *slash = L'\\';
                slash = wcschr(slash + 1, L'\\');
            }
            CreateDirectoryW(parentDir, NULL);
        }
        
        if (CreateDirectoryW(tempPath, NULL)) {
            return TRUE;
        }
    }
    
    // Try system temp directory as second option
    DWORD result = GetTempPathW((DWORD)pathSize, tempPath);
    if (result > 0 && result < pathSize) {
        // Validate it's not a system directory
        if (wcsstr(tempPath, L"\\Windows\\") == NULL && 
            wcsstr(tempPath, L"\\System32\\") == NULL &&
            wcsstr(tempPath, L"\\SysWOW64\\") == NULL) {
            
            wchar_t uniqueName[64];
            swprintf(uniqueName, 64, L"YouTubeCacher_%lu", GetTickCount());
            
            if (wcslen(tempPath) + wcslen(uniqueName) + 1 < pathSize) {
                wcscat(tempPath, uniqueName);
                if (CreateDirectoryW(tempPath, NULL)) {
                    return TRUE;
                }
            }
        }
    }
    
    // Final fallback to current directory
    wcscpy(tempPath, L".\\temp");
    return CreateDirectoryW(tempPath, NULL);
}

BOOL CleanupTempDirectory(const wchar_t* tempDir) {
    if (!tempDir || wcslen(tempDir) == 0) return FALSE;
    
    // Simple cleanup - remove directory if empty
    return RemoveDirectoryW(tempDir);
}

ProgressDialog* CreateProgressDialog(HWND parent, const wchar_t* title) {
    ProgressDialog* dialog = (ProgressDialog*)malloc(sizeof(ProgressDialog));
    if (!dialog) return NULL;
    
    memset(dialog, 0, sizeof(ProgressDialog));
    
    // Create the progress dialog
    dialog->hDialog = CreateDialogParamW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PROGRESS), 
                                        parent, ProgressDialogProc, (LPARAM)dialog);
    
    if (!dialog->hDialog) {
        free(dialog);
        return NULL;
    }
    
    // Set the title
    SetWindowTextW(dialog->hDialog, title);
    ShowWindow(dialog->hDialog, SW_SHOW);
    
    return dialog;
}

void UpdateProgressDialog(ProgressDialog* dialog, int progress, const wchar_t* status) {
    if (!dialog || !dialog->hDialog) return;
    
    if (dialog->hProgressBar) {
        SendMessageW(dialog->hProgressBar, PBM_SETPOS, progress, 0);
    }
    
    if (dialog->hStatusText && status) {
        SetWindowTextW(dialog->hStatusText, status);
    }
    
    // Process messages to keep UI responsive
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
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

YtDlpResult* ExecuteYtDlpRequest(const YtDlpConfig* config, const YtDlpRequest* request) {
    if (!config || !request) return NULL;
    
    YtDlpResult* result = (YtDlpResult*)malloc(sizeof(YtDlpResult));
    if (!result) return NULL;
    
    memset(result, 0, sizeof(YtDlpResult));
    
    // Build command line arguments
    wchar_t arguments[2048];
    if (!GetYtDlpArgsForOperation(request->operation, request->url, request->outputPath, config, arguments, 2048)) {
        result->success = FALSE;
        result->exitCode = 1;
        result->errorMessage = _wcsdup(L"Failed to build yt-dlp arguments");
        return result;
    }
    
    // Execute yt-dlp using existing process creation logic
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        result->success = FALSE;
        result->exitCode = 1;
        result->errorMessage = _wcsdup(L"Failed to create output pipe");
        return result;
    }
    
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;
    
    PROCESS_INFORMATION pi = {0};
    
    // Build command line
    size_t cmdLineLen = wcslen(config->ytDlpPath) + wcslen(arguments) + 10;
    wchar_t* cmdLine = (wchar_t*)malloc(cmdLineLen * sizeof(wchar_t));
    if (!cmdLine) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        result->success = FALSE;
        result->exitCode = 1;
        result->errorMessage = _wcsdup(L"Memory allocation failed");
        return result;
    }
    
    swprintf(cmdLine, cmdLineLen, L"\"%ls\" %ls", config->ytDlpPath, arguments);
    
    // Create process
    BOOL processCreated = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    
    if (!processCreated) {
        free(cmdLine);
        CloseHandle(hRead);
        CloseHandle(hWrite);
        result->success = FALSE;
        result->exitCode = GetLastError();
        result->errorMessage = _wcsdup(L"Failed to start yt-dlp process");
        return result;
    }
    
    CloseHandle(hWrite);
    
    // Read output
    char buffer[4096];
    DWORD bytesRead;
    size_t outputSize = 8192;
    wchar_t* outputBuffer = (wchar_t*)malloc(outputSize * sizeof(wchar_t));
    if (outputBuffer) {
        outputBuffer[0] = L'\0';
        
        while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            wchar_t tempOutput[2048];
            int converted = MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, tempOutput, 2047);
            if (converted > 0) {
                tempOutput[converted] = L'\0';
                
                size_t currentLen = wcslen(outputBuffer);
                size_t newLen = currentLen + converted + 1;
                if (newLen >= outputSize) {
                    outputSize = newLen * 2;
                    wchar_t* newBuffer = (wchar_t*)realloc(outputBuffer, outputSize * sizeof(wchar_t));
                    if (newBuffer) {
                        outputBuffer = newBuffer;
                    } else {
                        break;
                    }
                }
                
                wcscat(outputBuffer, tempOutput);
            }
        }
    }
    
    // Wait for process completion
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    // Get exit code
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    // Save command line for diagnostics before freeing
    wchar_t* savedCmdLine = _wcsdup(cmdLine);
    
    // Cleanup
    free(cmdLine);
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Set result
    result->success = (exitCode == 0);
    result->exitCode = exitCode;
    result->output = outputBuffer;
    
    // For failed processes, extract meaningful error information from output
    if (!result->success) {
        if (result->output && wcslen(result->output) > 0) {
            // Use the actual yt-dlp output as the error message
            result->errorMessage = _wcsdup(result->output);
            
            // Generate diagnostic information based on the output
            wchar_t* diagnostics = (wchar_t*)malloc(2048 * sizeof(wchar_t));
            if (diagnostics) {
                swprintf(diagnostics, 2048,
                    L"yt-dlp process exited with code %lu\n\n"
                    L"Command executed: %ls\n\n"
                    L"Process output:\n%ls",
                    exitCode, savedCmdLine ? savedCmdLine : L"Unknown", result->output);
                result->diagnostics = diagnostics;
            }
        } else {
            // Fallback for cases with no output
            result->errorMessage = _wcsdup(L"yt-dlp process failed with no output");
            
            wchar_t* diagnostics = (wchar_t*)malloc(1024 * sizeof(wchar_t));
            if (diagnostics) {
                swprintf(diagnostics, 1024,
                    L"yt-dlp process exited with code %lu but produced no output\n\n"
                    L"Command executed: %ls\n\n"
                    L"This may indicate:\n"
                    L"- yt-dlp executable not found or corrupted\n"
                    L"- Missing dependencies (Python runtime)\n"
                    L"- Permission issues\n"
                    L"- Invalid command line arguments",
                    exitCode, savedCmdLine ? savedCmdLine : L"Unknown");
                result->diagnostics = diagnostics;
            }
        }
    }
    
    // Cleanup saved command line
    if (savedCmdLine) free(savedCmdLine);
    
    return result;
}

// Multithreaded subprocess execution implementation

// Initialize thread context with critical section for thread safety
BOOL InitializeThreadContext(ThreadContext* threadContext) {
    if (!threadContext) return FALSE;
    
    memset(threadContext, 0, sizeof(ThreadContext));
    
    // Initialize critical section for thread-safe access
    // Note: InitializeCriticalSection can raise exceptions on low memory,
    // but we'll handle this with standard error checking
    InitializeCriticalSection(&threadContext->criticalSection);
    return TRUE;
}

// Clean up thread context and wait for thread completion
void CleanupThreadContext(ThreadContext* threadContext) {
    if (!threadContext) return;
    
    // Wait for thread to complete if still running
    if (threadContext->hThread && threadContext->isRunning) {
        // Signal cancellation first
        EnterCriticalSection(&threadContext->criticalSection);
        threadContext->cancelRequested = TRUE;
        LeaveCriticalSection(&threadContext->criticalSection);
        
        // Wait up to 5 seconds for graceful shutdown
        if (WaitForSingleObject(threadContext->hThread, 5000) == WAIT_TIMEOUT) {
            // Force terminate if thread doesn't respond
            TerminateThread(threadContext->hThread, 1);
        }
        
        CloseHandle(threadContext->hThread);
        threadContext->hThread = NULL;
    }
    
    DeleteCriticalSection(&threadContext->criticalSection);
    threadContext->isRunning = FALSE;
}

// Thread-safe cancellation flag setting
BOOL SetCancellationFlag(ThreadContext* threadContext) {
    if (!threadContext) return FALSE;
    
    EnterCriticalSection(&threadContext->criticalSection);
    threadContext->cancelRequested = TRUE;
    LeaveCriticalSection(&threadContext->criticalSection);
    
    return TRUE;
}

// Thread-safe cancellation check
BOOL IsCancellationRequested(const ThreadContext* threadContext) {
    if (!threadContext) return FALSE;
    
    BOOL cancelled = FALSE;
    EnterCriticalSection((CRITICAL_SECTION*)&threadContext->criticalSection);
    cancelled = threadContext->cancelRequested;
    LeaveCriticalSection((CRITICAL_SECTION*)&threadContext->criticalSection);
    
    return cancelled;
}

// Worker thread function that executes yt-dlp subprocess
DWORD WINAPI SubprocessWorkerThread(LPVOID lpParam) {
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread started\n");
    
    SubprocessContext* context = (SubprocessContext*)lpParam;
    if (!context || !context->config || !context->request) {
        OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - invalid context\n");
        return 1;
    }
    
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - context valid\n");
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - URL: %ls\n", context->request->url ? context->request->url : L"NULL");
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - OutputPath: %ls\n", context->request->outputPath ? context->request->outputPath : L"NULL");
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - TempDir: %ls\n", context->request->tempDir ? context->request->tempDir : L"NULL");
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Operation: %d\n", context->request->operation);
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - YtDlpPath: %ls\n", context->config->ytDlpPath);
    OutputDebugStringW(debugMsg);
    
    // Mark thread as running
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Marking thread as running\n");
    EnterCriticalSection(&context->threadContext.criticalSection);
    context->threadContext.isRunning = TRUE;
    LeaveCriticalSection(&context->threadContext.criticalSection);
    
    // Initialize result structure
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Initializing result structure\n");
    context->result = (YtDlpResult*)malloc(sizeof(YtDlpResult));
    if (!context->result) {
        OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Failed to allocate result structure\n");
        context->completed = TRUE;
        return 1;
    }
    memset(context->result, 0, sizeof(YtDlpResult));
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Result structure initialized\n");
    
    // Report initial progress
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Reporting initial progress\n");
    if (context->progressCallback) {
        context->progressCallback(0, L"Initializing yt-dlp process...", context->callbackUserData);
    }
    
    // Build command line arguments
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Building command line arguments\n");
    wchar_t arguments[2048];
    if (!GetYtDlpArgsForOperation(context->request->operation, context->request->url, 
                                 context->request->outputPath, context->config, arguments, 2048)) {
        OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - FAILED to build yt-dlp arguments\n");
        context->result->success = FALSE;
        context->result->exitCode = 1;
        context->result->errorMessage = _wcsdup(L"Failed to build yt-dlp arguments");
        context->completed = TRUE;
        return 1;
    }
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Arguments: %ls\n", arguments);
    OutputDebugStringW(debugMsg);
    
    // Check for cancellation before starting process
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Checking for cancellation\n");
    if (IsCancellationRequested(&context->threadContext)) {
        OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Operation was cancelled\n");
        context->result->success = FALSE;
        context->result->errorMessage = _wcsdup(L"Operation cancelled by user");
        context->completed = TRUE;
        return 0;
    }
    
    // Create pipes for output capture
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Creating pipes for output capture\n");
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&context->hOutputRead, &context->hOutputWrite, &sa, 0)) {
        DWORD error = GetLastError();
        swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - FAILED to create output pipe, error: %lu\n", error);
        OutputDebugStringW(debugMsg);
        context->result->success = FALSE;
        context->result->exitCode = 1;
        context->result->errorMessage = _wcsdup(L"Failed to create output pipe");
        context->completed = TRUE;
        return 1;
    }
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Pipes created successfully\n");
    
    SetHandleInformation(context->hOutputRead, HANDLE_FLAG_INHERIT, 0);
    
    // Setup process startup info
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Setting up process startup info\n");
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = context->hOutputWrite;
    si.hStdError = context->hOutputWrite;
    si.hStdInput = NULL;
    
    PROCESS_INFORMATION pi = {0};
    
    // Build command line
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Building command line\n");
    size_t cmdLineLen = wcslen(context->config->ytDlpPath) + wcslen(arguments) + 10;
    wchar_t* cmdLine = (wchar_t*)malloc(cmdLineLen * sizeof(wchar_t));
    if (!cmdLine) {
        OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - FAILED to allocate command line memory\n");
        CloseHandle(context->hOutputRead);
        CloseHandle(context->hOutputWrite);
        context->result->success = FALSE;
        context->result->exitCode = 1;
        context->result->errorMessage = _wcsdup(L"Memory allocation failed");
        context->completed = TRUE;
        return 1;
    }
    
    swprintf(cmdLine, cmdLineLen, L"\"%ls\" %ls", context->config->ytDlpPath, arguments);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Full command line: %ls\n", cmdLine);
    OutputDebugStringW(debugMsg);
    
    // Report progress
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Reporting progress: Starting yt-dlp process\n");
    if (context->progressCallback) {
        context->progressCallback(10, L"Starting yt-dlp process...", context->callbackUserData);
    }
    
    // Create the yt-dlp process
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Creating yt-dlp process\n");
    BOOL processCreated = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    
    if (!processCreated) {
        DWORD error = GetLastError();
        swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - FAILED to create process, error: %lu\n", error);
        OutputDebugStringW(debugMsg);
        free(cmdLine);
        CloseHandle(context->hOutputRead);
        CloseHandle(context->hOutputWrite);
        context->result->success = FALSE;
        context->result->exitCode = error;
        context->result->errorMessage = _wcsdup(L"Failed to start yt-dlp process");
        context->completed = TRUE;
        return 1;
    }
    
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Process created successfully, PID: %lu\n", pi.dwProcessId);
    OutputDebugStringW(debugMsg);
    
    // Store process handle for potential cancellation
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Storing process handle and closing write pipe\n");
    context->hProcess = pi.hProcess;
    CloseHandle(context->hOutputWrite);
    context->hOutputWrite = NULL;
    
    // Initialize output buffer
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Initializing output buffer\n");
    context->outputBufferSize = 8192;
    context->accumulatedOutput = (wchar_t*)malloc(context->outputBufferSize * sizeof(wchar_t));
    if (context->accumulatedOutput) {
        context->accumulatedOutput[0] = L'\0';
    }
    
    // Read output and monitor process with enhanced progress parsing
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Starting output reading loop\n");
    char buffer[4096];
    DWORD bytesRead;
    BOOL processRunning = TRUE;
    int lastProgressPercentage = 20;
    wchar_t lineBuffer[2048] = L"";
    size_t lineBufferPos = 0;
    int loopCount = 0;
    
    while (processRunning && !IsCancellationRequested(&context->threadContext)) {
        loopCount++;
        if (loopCount % 100 == 0) { // Every 10 seconds (100 * 100ms)
            swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Still in output loop, count: %d\n", loopCount);
            OutputDebugStringW(debugMsg);
        }
        
        // Check if process is still running
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 100); // 100ms timeout
        if (waitResult == WAIT_OBJECT_0) {
            OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Process has exited\n");
            processRunning = FALSE;
        }
        
        // Try to read output
        DWORD bytesAvailable = 0;
        if (PeekNamedPipe(context->hOutputRead, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
            if (ReadFile(context->hOutputRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                
                swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Read %lu bytes from process\n", bytesRead);
                OutputDebugStringW(debugMsg);
                

                // Debug: Output to DebugView
                wchar_t debugMsg[256];
                swprintf(debugMsg, 256, L"YouTubeCacher: Read %lu bytes from yt-dlp\n", bytesRead);
                OutputDebugStringW(debugMsg);
                
                // Convert to wide char and append to output buffer
                wchar_t tempOutput[2048];
                int converted = MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, tempOutput, 2047);
                if (converted > 0 && context->accumulatedOutput) {
                    tempOutput[converted] = L'\0';
                    
                    size_t currentLen = wcslen(context->accumulatedOutput);
                    size_t newLen = currentLen + converted + 1;
                    if (newLen >= context->outputBufferSize) {
                        context->outputBufferSize = newLen * 2;
                        wchar_t* newBuffer = (wchar_t*)realloc(context->accumulatedOutput, 
                                                              context->outputBufferSize * sizeof(wchar_t));
                        if (newBuffer) {
                            context->accumulatedOutput = newBuffer;
                        }
                    }
                    
                    if (context->accumulatedOutput) {
                        wcscat(context->accumulatedOutput, tempOutput);
                    }
                    
                    // Process output line by line for progress parsing
                    for (int i = 0; i < converted; i++) {
                        wchar_t ch = tempOutput[i];
                        
                        if (ch == L'\n' || ch == L'\r') {
                            if (lineBufferPos > 0) {
                                lineBuffer[lineBufferPos] = L'\0';
                                
                                // Parse progress from this line
                                wchar_t debugLine[512];
                                swprintf(debugLine, 512, L"YouTubeCacher: Line: %ls\n", lineBuffer);
                                OutputDebugStringW(debugLine);
                                
                                // Debug: Check for pipe lines
                                if (wcsstr(lineBuffer, L"|")) {
                                    OutputDebugStringW(L"YouTubeCacher: Found pipe line!\n");
                                }
                                
                                // Parse progress from this line
                                ProgressInfo progressInfo;
                                

                                if (ParseProgressOutput(lineBuffer, &progressInfo)) {
                                    // Debug: Show what we parsed
                                    wchar_t parseDebug[256];
                                    swprintf(parseDebug, 256, L"YouTubeCacher: Parsed - percentage=%d, downloaded=%lld, total=%lld\n", 
                                            progressInfo.percentage, progressInfo.downloadedBytes, progressInfo.totalBytes);
                                    OutputDebugStringW(parseDebug);
                                    
                                    // Send progress update if percentage increased OR if we have indeterminate progress (-1) OR if we have valid download data
                                    if (progressInfo.percentage > lastProgressPercentage || progressInfo.percentage == -1 || 
                                        (progressInfo.downloadedBytes > 0 && progressInfo.status)) {
                                        // Only update lastProgressPercentage for real percentages, not indeterminate (-1)
                                        if (progressInfo.percentage >= 0) {
                                            lastProgressPercentage = progressInfo.percentage;
                                        }
                                        
                                        // Create heap-allocated progress data for PostMessage
                                        HeapProgressData* heapProgress = (HeapProgressData*)malloc(sizeof(HeapProgressData));
                                        if (heapProgress) {
                                            heapProgress->percentage = progressInfo.percentage;
                                            heapProgress->isComplete = progressInfo.isComplete;
                                            
                                            // Copy status
                                            if (progressInfo.status) {
                                                wcsncpy(heapProgress->status, progressInfo.status, 63);
                                                heapProgress->status[63] = L'\0';
                                            } else {
                                                wcscpy(heapProgress->status, L"Downloading");
                                            }
                                            
                                            // Copy speed
                                            if (progressInfo.speed) {
                                                wcsncpy(heapProgress->speed, progressInfo.speed, 31);
                                                heapProgress->speed[31] = L'\0';
                                            } else {
                                                heapProgress->speed[0] = L'\0';
                                            }
                                            
                                            // Copy ETA
                                            if (progressInfo.eta) {
                                                wcsncpy(heapProgress->eta, progressInfo.eta, 15);
                                                heapProgress->eta[15] = L'\0';
                                            } else {
                                                heapProgress->eta[0] = L'\0';
                                            }
                                            
                                            // Debug: Sending progress update
                                            wchar_t debugSend[256];
                                            swprintf(debugSend, 256, L"YouTubeCacher: Sending PostMessage WM_USER+104, percentage=%d\n", heapProgress->percentage);
                                            OutputDebugStringW(debugSend);
                                            
                                            // Send progress update via progress callback
                                            if (context->progressCallback) {
                                                context->progressCallback(progressInfo.percentage, progressInfo.status, context->callbackUserData);
                                            }
                                            
                                            // Free the heap progress data since we're using callback instead
                                            free(heapProgress);
                                        }
                                    }
                                    FreeProgressInfo(&progressInfo);
                                }
                                
                                lineBufferPos = 0;
                            }
                        } else if (lineBufferPos < sizeof(lineBuffer)/sizeof(wchar_t) - 1) {
                            lineBuffer[lineBufferPos++] = ch;
                        }
                    }
                }
                
                // Fallback progress update if no progress was parsed
                if (lastProgressPercentage == 20) {
                    lastProgressPercentage = min(90, lastProgressPercentage + 5);
                    if (context->progressCallback) {
                        context->progressCallback(lastProgressPercentage, L"Processing...", context->callbackUserData);
                    }
                }
            }
        }
        
        // Small delay to prevent excessive CPU usage
        Sleep(50);
    }
    
    // Handle cancellation
    if (IsCancellationRequested(&context->threadContext) && processRunning) {
        TerminateProcess(pi.hProcess, 1);
        context->result->success = FALSE;
        context->result->errorMessage = _wcsdup(L"Operation cancelled by user");
    } else {
        // Get final exit code
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        context->result->success = (exitCode == 0);
        context->result->exitCode = exitCode;
        context->result->output = context->accumulatedOutput;
        context->accumulatedOutput = NULL; // Transfer ownership to result
        
        // For failed processes, extract meaningful error information from output
        if (!context->result->success) {
            if (context->result->output && wcslen(context->result->output) > 0) {
                // Use the actual yt-dlp output as the error message
                context->result->errorMessage = _wcsdup(context->result->output);
                
                // Generate diagnostic information based on the output
                wchar_t* diagnostics = (wchar_t*)malloc(2048 * sizeof(wchar_t));
                if (diagnostics) {
                    swprintf(diagnostics, 2048,
                        L"yt-dlp process exited with code %lu\n\n"
                        L"Command executed: %ls\n\n"
                        L"Process output:\n%ls",
                        exitCode, cmdLine, context->result->output);
                    context->result->diagnostics = diagnostics;
                }
            } else {
                // Fallback for cases with no output
                context->result->errorMessage = _wcsdup(L"yt-dlp process failed with no output");
                
                wchar_t* diagnostics = (wchar_t*)malloc(1024 * sizeof(wchar_t));
                if (diagnostics) {
                    swprintf(diagnostics, 1024,
                        L"yt-dlp process exited with code %lu but produced no output\n\n"
                        L"Command executed: %ls\n\n"
                        L"This may indicate:\n"
                        L"- yt-dlp executable not found or corrupted\n"
                        L"- Missing dependencies (Python runtime)\n"
                        L"- Permission issues\n"
                        L"- Invalid command line arguments",
                        exitCode, cmdLine);
                    context->result->diagnostics = diagnostics;
                }
            }
        }
    }
    
    // Final progress update
    if (context->progressCallback) {
        if (context->result->success) {
            context->progressCallback(100, L"Completed successfully", context->callbackUserData);
        } else {
            context->progressCallback(100, L"Operation failed", context->callbackUserData);
        }
    }
    
    // Cleanup
    free(cmdLine);
    if (context->hOutputRead) {
        CloseHandle(context->hOutputRead);
        context->hOutputRead = NULL;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    context->hProcess = NULL;
    
    // Mark as completed
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Marking as completed\n");
    context->completed = TRUE;
    context->completionTime = GetTickCount();
    
    // Debug the final result
    if (context->result) {
        swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Final result: success=%d, exitCode=%d\n", 
                context->result->success, context->result->exitCode);
        OutputDebugStringW(debugMsg);
        if (context->result->errorMessage) {
            swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Error message: %ls\n", context->result->errorMessage);
            OutputDebugStringW(debugMsg);
        }
        if (context->result->output) {
            swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Output length: %zu chars\n", wcslen(context->result->output));
            OutputDebugStringW(debugMsg);
        }
    } else {
        OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - RESULT IS NULL!\n");
    }
    
    // Mark thread as no longer running
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - Marking thread as no longer running\n");
    EnterCriticalSection(&context->threadContext.criticalSection);
    context->threadContext.isRunning = FALSE;
    LeaveCriticalSection(&context->threadContext.criticalSection);
    
    OutputDebugStringW(L"YouTubeCacher: SubprocessWorkerThread - EXITING\n");
    return 0;
}

// Create subprocess context for multithreaded execution
SubprocessContext* CreateSubprocessContext(const YtDlpConfig* config, const YtDlpRequest* request, 
                                          ProgressCallback progressCallback, void* callbackUserData, HWND parentWindow) {
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - ENTRY\n");
    
    if (!config || !request) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - NULL config or request, RETURNING NULL\n");
        return NULL;
    }
    
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Parameters valid, proceeding\n");
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Config ytDlpPath: %ls\n", config->ytDlpPath);
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Request URL: %ls\n", request->url ? request->url : L"NULL");
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Request outputPath: %ls\n", request->outputPath ? request->outputPath : L"NULL");
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Request tempDir: %ls\n", request->tempDir ? request->tempDir : L"NULL");
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Request operation: %d\n", request->operation);
    OutputDebugStringW(debugMsg);
    
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Allocating context memory\n");
    SubprocessContext* context = (SubprocessContext*)malloc(sizeof(SubprocessContext));
    if (!context) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to allocate context memory, RETURNING NULL\n");
        return NULL;
    }
    
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Context allocated, zeroing memory\n");
    memset(context, 0, sizeof(SubprocessContext));
    
    // Initialize thread context
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Initializing thread context\n");
    if (!InitializeThreadContext(&context->threadContext)) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to initialize thread context\n");
        free(context);
        return NULL;
    }
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Thread context initialized successfully\n");
    
    // Copy configuration and request (deep copy for thread safety)
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Allocating config and request copies\n");
    context->config = (YtDlpConfig*)malloc(sizeof(YtDlpConfig));
    context->request = (YtDlpRequest*)malloc(sizeof(YtDlpRequest));
    
    if (!context->config || !context->request) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to allocate config or request memory\n");
        CleanupThreadContext(&context->threadContext);
        if (context->config) free(context->config);
        if (context->request) free(context->request);
        free(context);
        return NULL;
    }
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Config and request memory allocated\n");
    
    // Deep copy config
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Deep copying config\n");
    memcpy(context->config, config, sizeof(YtDlpConfig));
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Config copied successfully\n");
    
    // Deep copy request
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Deep copying request structure\n");
    memcpy(context->request, request, sizeof(YtDlpRequest));
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Request structure copied\n");
    
    if (request->url) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Duplicating URL string\n");
        context->request->url = _wcsdup(request->url);
        if (!context->request->url) {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to duplicate URL string\n");
        } else {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - URL string duplicated successfully\n");
        }
    }
    if (request->outputPath) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Duplicating outputPath string\n");
        context->request->outputPath = _wcsdup(request->outputPath);
        if (!context->request->outputPath) {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to duplicate outputPath string\n");
        } else {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - OutputPath string duplicated successfully\n");
        }
    }
    if (request->tempDir) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Duplicating tempDir string\n");
        context->request->tempDir = _wcsdup(request->tempDir);
        if (!context->request->tempDir) {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to duplicate tempDir string\n");
        } else {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - TempDir string duplicated successfully\n");
        }
    }
    if (request->customArgs) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Duplicating customArgs string\n");
        context->request->customArgs = _wcsdup(request->customArgs);
        if (!context->request->customArgs) {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to duplicate customArgs string\n");
        } else {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - CustomArgs string duplicated successfully\n");
        }
    }
    
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Setting callback parameters\n");
    context->progressCallback = progressCallback;
    context->callbackUserData = callbackUserData;
    context->parentWindow = parentWindow;
    
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - SUCCESS, returning context\n");
    return context;
}

// Start subprocess execution in background thread
BOOL StartSubprocessExecution(SubprocessContext* context) {
    OutputDebugStringW(L"YouTubeCacher: StartSubprocessExecution - ENTRY\n");
    
    if (!context) {
        OutputDebugStringW(L"YouTubeCacher: StartSubprocessExecution - NULL context, RETURNING FALSE\n");
        return FALSE;
    }
    
    OutputDebugStringW(L"YouTubeCacher: StartSubprocessExecution - Context valid, creating worker thread\n");
    
    // Create worker thread
    context->threadContext.hThread = CreateThread(NULL, 0, SubprocessWorkerThread, context, 0, 
                                                 &context->threadContext.threadId);
    
    if (!context->threadContext.hThread) {
        DWORD error = GetLastError();
        wchar_t debugMsg[256];
        swprintf(debugMsg, 256, L"YouTubeCacher: StartSubprocessExecution - CreateThread FAILED, error: %lu\n", error);
        OutputDebugStringW(debugMsg);
        return FALSE;
    }
    
    OutputDebugStringW(L"YouTubeCacher: StartSubprocessExecution - Worker thread created successfully\n");
    
    return TRUE;
}

// Check if subprocess is still running
BOOL IsSubprocessRunning(const SubprocessContext* context) {
    if (!context) return FALSE;
    
    BOOL running = FALSE;
    EnterCriticalSection((CRITICAL_SECTION*)&context->threadContext.criticalSection);
    running = context->threadContext.isRunning;
    LeaveCriticalSection((CRITICAL_SECTION*)&context->threadContext.criticalSection);
    
    return running;
}

// Cancel subprocess execution
BOOL CancelSubprocessExecution(SubprocessContext* context) {
    if (!context) return FALSE;
    
    // Set cancellation flag
    SetCancellationFlag(&context->threadContext);
    
    // If process is running, terminate it
    if (context->hProcess) {
        TerminateProcess(context->hProcess, 1);
    }
    
    return TRUE;
}

// Wait for subprocess completion with timeout
BOOL WaitForSubprocessCompletion(SubprocessContext* context, DWORD timeoutMs) {
    if (!context || !context->threadContext.hThread) return FALSE;
    
    DWORD waitResult = WaitForSingleObject(context->threadContext.hThread, timeoutMs);
    return (waitResult == WAIT_OBJECT_0);
}

// Get subprocess result (transfers ownership to caller)
YtDlpResult* GetSubprocessResult(SubprocessContext* context) {
    OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - ENTRY\n");
    
    if (!context) {
        OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - NULL context, returning NULL\n");
        return NULL;
    }
    
    if (!context->completed) {
        OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - Context not completed, returning NULL\n");
        return NULL;
    }
    
    OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - Context is completed\n");
    
    if (!context->result) {
        OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - Context result is NULL, returning NULL\n");
        return NULL;
    }
    
    wchar_t debugMsg[256];
    swprintf(debugMsg, 256, L"YouTubeCacher: GetSubprocessResult - Transferring result: success=%d, exitCode=%d\n", 
            context->result->success, context->result->exitCode);
    OutputDebugStringW(debugMsg);
    
    YtDlpResult* result = context->result;
    context->result = NULL; // Transfer ownership
    
    OutputDebugStringW(L"YouTubeCacher: GetSubprocessResult - Result transferred successfully\n");
    return result;
}

// Free subprocess context and all associated resources
void FreeSubprocessContext(SubprocessContext* context) {
    if (!context) return;
    
    // Clean up thread context (waits for thread completion)
    CleanupThreadContext(&context->threadContext);
    
    // Free configuration copy
    if (context->config) {
        free(context->config);
    }
    
    // Free request copy
    if (context->request) {
        if (context->request->url) free(context->request->url);
        if (context->request->outputPath) free(context->request->outputPath);
        if (context->request->tempDir) free(context->request->tempDir);
        if (context->request->customArgs) free(context->request->customArgs);
        free(context->request);
    }
    
    // Free result if not transferred
    if (context->result) {
        FreeYtDlpResult(context->result);
    }
    
    // Free accumulated output if not transferred
    if (context->accumulatedOutput) {
        free(context->accumulatedOutput);
    }
    
    free(context);
}

// Progress callback for updating progress dialog
void SubprocessProgressCallback(int percentage, const wchar_t* status, void* userData) {
    ProgressDialog* progress = (ProgressDialog*)userData;
    if (progress) {
        UpdateProgressDialog(progress, percentage, status);
    }
}

// Convenience function for simple multithreaded execution with progress dialog
YtDlpResult* ExecuteYtDlpRequestMultithreaded(const YtDlpConfig* config, const YtDlpRequest* request, 
                                             HWND parentWindow, const wchar_t* operationTitle) {
    if (!config || !request) return NULL;
    
    // Create progress dialog
    ProgressDialog* progress = CreateProgressDialog(parentWindow, operationTitle ? operationTitle : L"Processing");
    if (!progress) {
        // Fallback to synchronous execution if progress dialog fails
        return ExecuteYtDlpRequest(config, request);
    }
    
    // Create subprocess context
    SubprocessContext* context = CreateSubprocessContext(config, request, SubprocessProgressCallback, progress, parentWindow);
    if (!context) {
        DestroyProgressDialog(progress);
        return ExecuteYtDlpRequest(config, request);
    }
    
    // Start subprocess execution
    if (!StartSubprocessExecution(context)) {
        FreeSubprocessContext(context);
        DestroyProgressDialog(progress);
        return ExecuteYtDlpRequest(config, request);
    }
    
    // Wait for completion with UI message pumping
    YtDlpResult* result = NULL;
    MSG msg;
    
    while (IsSubprocessRunning(context)) {
        // Process Windows messages to keep UI responsive
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (!IsDialogMessageW(progress->hDialog, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        
        // Check if user cancelled via progress dialog
        if (IsProgressDialogCancelled(progress)) {
            CancelSubprocessExecution(context);
            break;
        }
        
        // Small delay to prevent excessive CPU usage
        Sleep(50);
    }
    
    // Wait a bit more for thread cleanup
    WaitForSubprocessCompletion(context, 1000);
    
    // Get result
    result = GetSubprocessResult(context);
    
    // Cleanup
    FreeSubprocessContext(context);
    DestroyProgressDialog(progress);
    
    return result;
}



// Custom window messages
#define WM_DOWNLOAD_COMPLETE (WM_USER + 102)
#define WM_UNIFIED_DOWNLOAD_UPDATE (WM_USER + 113)

// Unified download context
typedef struct {
    HWND hDialog;
    wchar_t url[MAX_URL_LENGTH];
    YtDlpConfig config;
    YtDlpRequest* request;
    wchar_t tempDir[MAX_EXTENDED_PATH];
} UnifiedDownloadContext;

// Unified download worker thread
DWORD WINAPI UnifiedDownloadWorkerThread(LPVOID lpParam) {
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - ENTRY\n");
    
    UnifiedDownloadContext* context = (UnifiedDownloadContext*)lpParam;
    if (!context) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - NULL CONTEXT, EXITING\n");
        return 1;
    }
    
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Context valid, proceeding\n");
    HWND hDlg = context->hDialog;
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: UnifiedDownloadWorkerThread - URL: %ls\n", context->url);
    OutputDebugStringW(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: UnifiedDownloadWorkerThread - TempDir: %ls\n", context->tempDir);
    OutputDebugStringW(debugMsg);
    
    // Step 1: Get video info if not cached
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - STEP 1: Checking cached metadata\n");
    VideoMetadata metadata;
    BOOL hasMetadata = FALSE;
    
    if (IsCachedMetadataValid(&g_cachedVideoMetadata, context->url)) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Cached metadata is valid\n");
        if (GetCachedMetadata(&g_cachedVideoMetadata, &metadata)) {
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Successfully retrieved cached metadata\n");
            hasMetadata = TRUE;
            // Update UI immediately with cached data
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting cached title to UI\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 1, (LPARAM)_wcsdup(metadata.title ? metadata.title : L"Unknown Title"));
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting cached duration to UI\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 2, (LPARAM)_wcsdup(metadata.duration ? metadata.duration : L"Unknown"));
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting 15% progress\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, 15); // Progress: 15%
        } else {
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Failed to retrieve cached metadata\n");
        }
    } else {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - No valid cached metadata found\n");
    }
    
    if (!hasMetadata) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Need to fetch metadata from yt-dlp\n");
        // Show marquee progress while getting info
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Starting marquee animation\n");
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 4, 0); // Start marquee
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting 'Getting video information' status\n");
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)_wcsdup(L"Getting video information..."));
        
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Calling GetVideoMetadata\n");
        if (GetVideoMetadata(context->url, &metadata)) {
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - GetVideoMetadata SUCCESS\n");
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Storing metadata in cache\n");
            StoreCachedMetadata(&g_cachedVideoMetadata, context->url, &metadata);
            hasMetadata = TRUE;
            
            // Update UI with retrieved data
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Stopping marquee animation\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 6, 0); // Stop marquee
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting retrieved title to UI\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 1, (LPARAM)_wcsdup(metadata.title ? metadata.title : L"Unknown Title"));
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting retrieved duration to UI\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 2, (LPARAM)_wcsdup(metadata.duration ? metadata.duration : L"Unknown"));
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting 15% progress after metadata\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, 15); // Progress: 15%
        } else {
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - GetVideoMetadata FAILED\n");
            // Failed to get metadata, continue anyway
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Stopping marquee after failure\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 6, 0); // Stop marquee
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting unknown title after failure\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 1, (LPARAM)_wcsdup(L"Unknown Title"));
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting unknown duration after failure\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 2, (LPARAM)_wcsdup(L"Unknown"));
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting 15% progress after failure\n");
            PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, 15); // Progress: 15%
        }
    }
    
    // Step 2: Execute download with progress updates
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - STEP 2: Starting download process\n");
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting 'Starting download' status\n");
    PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)_wcsdup(L"Starting download..."));
    
    // Create subprocess context for download
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Creating subprocess context\n");
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - About to call CreateSubprocessContext\n");
    SubprocessContext* subprocessContext = CreateSubprocessContext(&context->config, context->request, 
                                                                   UnifiedDownloadProgressCallback, hDlg, hDlg);
    if (!subprocessContext) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - CreateSubprocessContext FAILED\n");
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting download failed message\n");
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 7, 0); // Download failed
        goto cleanup;
    }
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - CreateSubprocessContext SUCCESS\n");
    
    // Start download
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Starting subprocess execution\n");
    if (!StartSubprocessExecution(subprocessContext)) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - StartSubprocessExecution FAILED\n");
        FreeSubprocessContext(subprocessContext);
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting download failed after subprocess start failure\n");
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 7, 0); // Download failed
        goto cleanup;
    }
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - StartSubprocessExecution SUCCESS\n");
    
    // Wait for completion
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Entering wait loop for subprocess completion\n");
    int waitCount = 0;
    while (!subprocessContext->completed) {
        Sleep(100);
        waitCount++;
        if (waitCount % 50 == 0) { // Every 5 seconds
            swprintf(debugMsg, 512, L"YouTubeCacher: UnifiedDownloadWorkerThread - Still waiting for subprocess completion (count: %d)\n", waitCount);
            OutputDebugStringW(debugMsg);
        }
        
        // Safety timeout after 5 minutes
        if (waitCount > 3000) {
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Timeout waiting for subprocess, breaking\n");
            break;
        }
    }
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Subprocess completed, waiting for cleanup\n");
    
    WaitForSubprocessCompletion(subprocessContext, 1000);
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Getting subprocess result\n");
    YtDlpResult* result = GetSubprocessResult(subprocessContext);
    
    if (result) {
        swprintf(debugMsg, 512, L"YouTubeCacher: UnifiedDownloadWorkerThread - Result: success=%d, exitCode=%d\n", 
                result->success, result->exitCode);
        OutputDebugStringW(debugMsg);
        if (result->errorMessage) {
            swprintf(debugMsg, 512, L"YouTubeCacher: UnifiedDownloadWorkerThread - Error message: %ls\n", result->errorMessage);
            OutputDebugStringW(debugMsg);
        }
    } else {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Result is NULL\n");
    }
    
    // Create completion context
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Creating download completion context\n");
    NonBlockingDownloadContext* downloadContext = (NonBlockingDownloadContext*)malloc(sizeof(NonBlockingDownloadContext));
    if (downloadContext) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Download context allocated successfully\n");
        memcpy(&downloadContext->config, &context->config, sizeof(YtDlpConfig));
        downloadContext->request = context->request; // Transfer ownership
        downloadContext->parentWindow = hDlg;
        wcscpy(downloadContext->tempDir, context->tempDir);
        wcscpy(downloadContext->url, context->url);
        
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting WM_DOWNLOAD_COMPLETE message\n");
        PostMessageW(hDlg, WM_DOWNLOAD_COMPLETE, (WPARAM)result, (LPARAM)downloadContext);
    } else {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Failed to allocate download context\n");
        if (result) {
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Freeing result due to context allocation failure\n");
            FreeYtDlpResult(result);
        }
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting download failed due to context allocation failure\n");
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 7, 0); // Download failed
    }
    
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Freeing subprocess context\n");
    FreeSubprocessContext(subprocessContext);

cleanup:
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - CLEANUP phase\n");
    if (hasMetadata) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Freeing video metadata\n");
        FreeVideoMetadata(&metadata);
    }
    
    // Don't free context->request here - ownership transferred to completion handler
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Freeing worker context\n");
    free(context);
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - EXITING with success\n");
    return 0;
}

// Progress callback for unified download
void UnifiedDownloadProgressCallback(int percentage, const wchar_t* status, void* userData) {
    HWND hDlg = (HWND)userData;
    if (!hDlg) return;
    
    PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, percentage); // Progress update
    if (status) {
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)_wcsdup(status)); // Status update
    }
}

// Start unified download process
BOOL StartUnifiedDownload(HWND hDlg, const wchar_t* url) {
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Entry\n");
    
    if (!hDlg || !url) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Invalid parameters\n");
        return FALSE;
    }
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: StartUnifiedDownload - URL: %ls\n", url);
    OutputDebugStringW(debugMsg);
    
    // Initialize configuration
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Initializing config\n");
    YtDlpConfig config = {0};
    if (!InitializeYtDlpConfig(&config)) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Failed to initialize config\n");
        return FALSE;
    }
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Config initialized successfully\n");
    
    // Validate configuration
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Validating config\n");
    ValidationInfo validationInfo = {0};
    if (!ValidateYtDlpComprehensive(config.ytDlpPath, &validationInfo)) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Config validation failed\n");
        FreeValidationInfo(&validationInfo);
        CleanupYtDlpConfig(&config);
        return FALSE;
    }
    FreeValidationInfo(&validationInfo);
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Config validated successfully\n");
    
    // Get download path
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Getting download path\n");
    wchar_t downloadPath[MAX_EXTENDED_PATH];
    if (!LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Using default download path\n");
        GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
    }
    swprintf(debugMsg, 512, L"YouTubeCacher: StartUnifiedDownload - Download path: %ls\n", downloadPath);
    OutputDebugStringW(debugMsg);
    
    // Create download directory
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Creating download directory\n");
    if (!CreateDownloadDirectoryIfNeeded(downloadPath)) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Failed to create download directory\n");
        CleanupYtDlpConfig(&config);
        return FALSE;
    }
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Download directory ready\n");
    
    // Create request
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Creating YtDlp request\n");
    YtDlpRequest* request = CreateYtDlpRequest(YTDLP_OP_DOWNLOAD, url, downloadPath);
    if (!request) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Failed to create YtDlp request\n");
        CleanupYtDlpConfig(&config);
        return FALSE;
    }
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - YtDlp request created successfully\n");
    
    // Create temp directory
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Creating temp directory\n");
    wchar_t tempDir[MAX_EXTENDED_PATH];
    if (!CreateTempDirectory(&config, tempDir, MAX_EXTENDED_PATH)) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Primary temp dir failed, trying fallback\n");
        if (!CreateYtDlpTempDirWithFallback(tempDir, MAX_EXTENDED_PATH)) {
            OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Fallback temp dir also failed\n");
            FreeYtDlpRequest(request);
            CleanupYtDlpConfig(&config);
            return FALSE;
        }
    }
    request->tempDir = _wcsdup(tempDir);
    swprintf(debugMsg, 512, L"YouTubeCacher: StartUnifiedDownload - Temp dir: %ls\n", tempDir);
    OutputDebugStringW(debugMsg);
    
    // Create context
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Creating context\n");
    UnifiedDownloadContext* context = (UnifiedDownloadContext*)malloc(sizeof(UnifiedDownloadContext));
    if (!context) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Failed to allocate context\n");
        FreeYtDlpRequest(request);
        CleanupYtDlpConfig(&config);
        return FALSE;
    }
    
    context->hDialog = hDlg;
    wcscpy(context->url, url);
    context->config = config; // Copy config
    context->request = request;
    wcscpy(context->tempDir, tempDir);
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Context created successfully\n");
    
    // Show progress and disable UI
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Setting up UI\n");
    ShowMainProgressBar(hDlg, TRUE);
    SetDownloadUIState(hDlg, TRUE);
    
    // Start worker thread
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Starting worker thread\n");
    HANDLE hThread = CreateThread(NULL, 0, UnifiedDownloadWorkerThread, context, 0, NULL);
    if (!hThread) {
        OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Failed to create worker thread\n");
        free(context);
        FreeYtDlpRequest(request);
        CleanupYtDlpConfig(&config);
        return FALSE;
    }
    
    CloseHandle(hThread);
    OutputDebugStringW(L"YouTubeCacher: StartUnifiedDownload - Worker thread started successfully\n");
    return TRUE;
}

// Thread function for non-blocking download
DWORD WINAPI NonBlockingDownloadThread(LPVOID lpParam) {
    OutputDebugStringW(L"YouTubeCacher: NonBlockingDownloadThread started\n");
    
    NonBlockingDownloadContext* downloadContext = (NonBlockingDownloadContext*)lpParam;
    if (!downloadContext) {
        OutputDebugStringW(L"YouTubeCacher: Invalid downloadContext\n");
        return 1;
    }
    
    // Execute the download using multithreaded approach with progress
    YtDlpResult* result = ExecuteYtDlpRequestMultithreaded(&downloadContext->config, downloadContext->request, 
                                                          downloadContext->parentWindow, L"Downloading Video");
    
    // Post completion message to main window with result
    PostMessageW(downloadContext->parentWindow, WM_DOWNLOAD_COMPLETE, (WPARAM)result, (LPARAM)downloadContext);
    
    return 0;
}

// Start non-blocking download operation
BOOL StartNonBlockingDownload(YtDlpConfig* config, YtDlpRequest* request, HWND parentWindow) {
    if (!config || !request || !parentWindow) return FALSE;
    
    // Create download context
    NonBlockingDownloadContext* downloadContext = (NonBlockingDownloadContext*)malloc(sizeof(NonBlockingDownloadContext));
    if (!downloadContext) return FALSE;
    
    // Copy configuration and request data
    memcpy(&downloadContext->config, config, sizeof(YtDlpConfig));
    downloadContext->request = request; // Transfer ownership
    downloadContext->parentWindow = parentWindow;
    
    // Copy temp directory and URL for cleanup
    if (request->tempDir) {
        wcscpy(downloadContext->tempDir, request->tempDir);
    }
    if (request->url) {
        wcscpy(downloadContext->url, request->url);
    }
    
    // Set progress bar to indeterminate (marquee) mode before starting download
    HWND hProgressBar = GetDlgItem(parentWindow, IDC_PROGRESS_BAR);
    if (hProgressBar) {
        SetWindowLongW(hProgressBar, GWL_STYLE, 
            GetWindowLongW(hProgressBar, GWL_STYLE) | PBS_MARQUEE);
        SendMessageW(hProgressBar, PBM_SETMARQUEE, TRUE, 50); // 50ms animation speed
    }
    
    // Create and start the download thread
    HANDLE hThread = CreateThread(NULL, 0, NonBlockingDownloadThread, downloadContext, 0, NULL);
    if (!hThread) {
        free(downloadContext);
        return FALSE;
    }
    
    // Don't wait for thread - it will notify us via message when complete
    CloseHandle(hThread);
    return TRUE;
}

// Handle download completion
void HandleDownloadCompletion(HWND hDlg, YtDlpResult* result, NonBlockingDownloadContext* downloadContext) {
    if (!hDlg || !downloadContext) return;
    
    if (result && result->success) {
        UpdateMainProgressBar(hDlg, 100, L"Download completed successfully");
        // Re-enable UI controls
        SetDownloadUIState(hDlg, FALSE);
        // Keep progress bar visible - don't hide it
        
        // Add to cache - extract video ID from URL
        OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Extracting video ID from URL\n");
        wchar_t* videoId = ExtractVideoIdFromUrl(downloadContext->url);
        OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - ExtractVideoIdFromUrl returned\n");
        
        if (videoId) {
            wchar_t debugMsg[256];
            swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Video ID: %ls\n", videoId);
            OutputDebugStringW(debugMsg);
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Getting UI text fields\n");
            
            // Get video title and duration from UI if available
            wchar_t title[512] = {0};
            wchar_t duration[64] = {0};
            GetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, title, 512);
            GetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, duration, 64);
            
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - UI text fields retrieved\n");
            
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Title will be base64 encoded for cache storage\n");
            
            // Get download path from config
            wchar_t downloadPath[MAX_EXTENDED_PATH];
            if (LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Download path: %ls\n", downloadPath);
                OutputDebugStringW(debugMsg);
                // Find the downloaded video file (look for files starting with video ID)
                wchar_t videoPattern[MAX_EXTENDED_PATH];
                swprintf(videoPattern, MAX_EXTENDED_PATH, L"%ls\\%ls.*", downloadPath, videoId);
                swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Searching pattern: %ls\n", videoPattern);
                OutputDebugStringW(debugMsg);
                
                WIN32_FIND_DATAW findData;
                HANDLE hFind = FindFirstFileW(videoPattern, &findData);
                
                if (hFind != INVALID_HANDLE_VALUE) {
                    OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Found files matching pattern\n");
                    do {
                        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                            // Check if it's a video file
                            wchar_t* ext = wcsrchr(findData.cFileName, L'.');
                            if (ext && (wcsicmp(ext, L".mp4") == 0 || wcsicmp(ext, L".mkv") == 0 || 
                                       wcsicmp(ext, L".webm") == 0 || wcsicmp(ext, L".avi") == 0)) {
                                
                                wchar_t fullVideoPath[MAX_EXTENDED_PATH];
                                swprintf(fullVideoPath, MAX_EXTENDED_PATH, L"%ls\\%ls", downloadPath, findData.cFileName);
                                
                                // Find subtitle files
                                wchar_t** subtitleFiles = NULL;
                                int subtitleCount = 0;
                                FindSubtitleFiles(fullVideoPath, &subtitleFiles, &subtitleCount);
                                
                                // Add to cache
                                swprintf(debugMsg, 256, L"YouTubeCacher: HandleDownloadCompletion - Adding to cache: %ls\n", findData.cFileName);
                                OutputDebugStringW(debugMsg);
                                
                                AddCacheEntry(&g_cacheManager, videoId, 
                                            wcslen(title) > 0 ? title : findData.cFileName,
                                            wcslen(duration) > 0 ? duration : L"Unknown",
                                            fullVideoPath, subtitleFiles, subtitleCount);
                                
                                OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Cache entry added successfully\n");
                                
                                // Clean up subtitle files array
                                if (subtitleFiles) {
                                    for (int i = 0; i < subtitleCount; i++) {
                                        if (subtitleFiles[i]) free(subtitleFiles[i]);
                                    }
                                    free(subtitleFiles);
                                }
                                
                                break; // Found the main video file
                            }
                        }
                    } while (FindNextFileW(hFind, &findData));
                    FindClose(hFind);
                }
            }
            
            free(videoId);
            
            // Refresh the cache list UI
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Refreshing cache list UI\n");
            RefreshCacheList(GetDlgItem(hDlg, IDC_LIST), &g_cacheManager);
            UpdateCacheListStatus(hDlg, &g_cacheManager);
            OutputDebugStringW(L"YouTubeCacher: HandleDownloadCompletion - Cache list refreshed\n");
        }
    } else {
        UpdateMainProgressBar(hDlg, 0, L"Download failed");
        // Re-enable UI controls
        SetDownloadUIState(hDlg, FALSE);
        Sleep(500);
        ShowMainProgressBar(hDlg, FALSE);
        
        // Show enhanced error dialog with actual yt-dlp output and Windows API errors
        if (result) {
            // Enhance error details with Windows API error information if applicable
            if (result->exitCode != 0 && result->exitCode > 1000) {
                // This looks like a Windows error code
                wchar_t windowsError[512];
                DWORD errorCode = result->exitCode;
                
                if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                  windowsError, 512, NULL)) {
                    
                    // Remove trailing newlines
                    size_t len = wcslen(windowsError);
                    while (len > 0 && (windowsError[len-1] == L'\n' || windowsError[len-1] == L'\r')) {
                        windowsError[len-1] = L'\0';
                        len--;
                    }
                    
                    // Enhance the diagnostics with Windows error information
                    size_t newDiagSize = (result->diagnostics ? wcslen(result->diagnostics) : 0) + 1024;
                    wchar_t* enhancedDiag = (wchar_t*)malloc(newDiagSize * sizeof(wchar_t));
                    if (enhancedDiag) {
                        swprintf(enhancedDiag, newDiagSize,
                            L"%ls\n\n=== WINDOWS API ERROR ===\n"
                            L"Error Code: %lu (0x%08lX)\n"
                            L"Error Message: %ls\n",
                            result->diagnostics ? result->diagnostics : L"No diagnostic information available",
                            errorCode, errorCode, windowsError);
                        
                        // Replace the diagnostics
                        if (result->diagnostics) free(result->diagnostics);
                        result->diagnostics = enhancedDiag;
                    }
                }
            }
            
            ShowYtDlpError(hDlg, result, downloadContext->request);
        } else {
            ShowConfigurationError(hDlg, L"Download operation failed to initialize properly. Please check your yt-dlp configuration.");
        }
    }
    
    // Cleanup resources
    if (downloadContext->tempDir[0] != L'\0') {
        CleanupTempDirectory(downloadContext->tempDir);
    }
    if (result) FreeYtDlpResult(result);
    if (downloadContext->request) FreeYtDlpRequest(downloadContext->request);
    CleanupYtDlpConfig(&downloadContext->config);
    free(downloadContext);
}

// Structure for passing data to the video info thread
typedef struct {
    HWND hDlg;
    wchar_t url[MAX_URL_LENGTH];
    wchar_t title[512];
    wchar_t duration[64];
    BOOL success;
    HANDLE hThread;
    DWORD threadId;
} VideoInfoThreadData;

// Thread function for getting video info without blocking UI
DWORD WINAPI GetVideoInfoThread(LPVOID lpParam) {
    VideoInfoThreadData* data = (VideoInfoThreadData*)lpParam;
    if (!data) return 1;
    
    // Get video title and duration using the existing synchronous function
    data->success = GetVideoTitleAndDurationSync(data->url, data->title, 512, data->duration, 64);
    
    // Notify main thread that we're done
    PostMessageW(data->hDlg, WM_USER + 101, 0, (LPARAM)data);
    
    return 0;
}

// Synchronous version of the video info function (renamed from original)
// Structure for concurrent video info retrieval
typedef struct {
    YtDlpConfig* config;
    YtDlpRequest* request;
    YtDlpResult* result;
    HANDLE hThread;
    DWORD threadId;
    BOOL completed;
} VideoInfoThread;

// Thread function for getting video info
DWORD WINAPI VideoInfoWorkerThread(LPVOID lpParam) {
    VideoInfoThread* threadInfo = (VideoInfoThread*)lpParam;
    if (!threadInfo || !threadInfo->config || !threadInfo->request) {
        return 1;
    }
    
    threadInfo->result = ExecuteYtDlpRequest(threadInfo->config, threadInfo->request);
    threadInfo->completed = TRUE;
    return 0;
}

BOOL GetVideoTitleAndDurationSync(const wchar_t* url, wchar_t* title, size_t titleSize, wchar_t* duration, size_t durationSize) {
    if (!url || !title || !duration || titleSize == 0 || durationSize == 0) return FALSE;
    
    // Initialize output strings
    title[0] = L'\0';
    duration[0] = L'\0';
    
    // Initialize YtDlp configuration
    YtDlpConfig config = {0};
    if (!InitializeYtDlpConfig(&config)) {
        return FALSE;
    }
    
    // Validate yt-dlp installation
    ValidationInfo validationInfo = {0};
    if (!ValidateYtDlpComprehensive(config.ytDlpPath, &validationInfo)) {
        FreeValidationInfo(&validationInfo);
        CleanupYtDlpConfig(&config);
        return FALSE;
    }
    FreeValidationInfo(&validationInfo);
    
    BOOL success = FALSE;
    
    // Create temporary directory for operations
    wchar_t tempDir[MAX_EXTENDED_PATH];
    if (!CreateTempDirectory(&config, tempDir, MAX_EXTENDED_PATH)) {
        CleanupYtDlpConfig(&config);
        return FALSE;
    }
    
    // Create requests for both title and duration
    YtDlpRequest* titleRequest = CreateYtDlpRequest(YTDLP_OP_GET_TITLE, url, NULL);
    YtDlpRequest* durationRequest = CreateYtDlpRequest(YTDLP_OP_GET_DURATION, url, NULL);
    
    if (!titleRequest || !durationRequest) {
        if (titleRequest) FreeYtDlpRequest(titleRequest);
        if (durationRequest) FreeYtDlpRequest(durationRequest);
        CleanupTempDirectory(tempDir);
        CleanupYtDlpConfig(&config);
        return FALSE;
    }
    
    titleRequest->tempDir = _wcsdup(tempDir);
    durationRequest->tempDir = _wcsdup(tempDir);
    
    // Create thread info structures
    VideoInfoThread titleThread = {0};
    VideoInfoThread durationThread = {0};
    
    titleThread.config = &config;
    titleThread.request = titleRequest;
    
    durationThread.config = &config;
    durationThread.request = durationRequest;
    
    // Start both threads concurrently
    titleThread.hThread = CreateThread(NULL, 0, VideoInfoWorkerThread, &titleThread, 0, &titleThread.threadId);
    durationThread.hThread = CreateThread(NULL, 0, VideoInfoWorkerThread, &durationThread, 0, &durationThread.threadId);
    
    if (titleThread.hThread && durationThread.hThread) {
        // Wait for both threads to complete (with timeout)
        HANDLE threads[2] = {titleThread.hThread, durationThread.hThread};
        DWORD waitResult = WaitForMultipleObjects(2, threads, TRUE, 30000); // 30 second timeout
        
        if (waitResult == WAIT_OBJECT_0) {
            // Both threads completed successfully
            
            // Process title result
            if (titleThread.result && titleThread.result->success && titleThread.result->output) {
                wchar_t* cleanTitle = titleThread.result->output;
                
                // Remove trailing newlines and whitespace
                size_t len = wcslen(cleanTitle);
                while (len > 0 && (cleanTitle[len-1] == L'\n' || cleanTitle[len-1] == L'\r' || cleanTitle[len-1] == L' ')) {
                    cleanTitle[len-1] = L'\0';
                    len--;
                }
                
                wcsncpy(title, cleanTitle, titleSize - 1);
                title[titleSize - 1] = L'\0';
                success = TRUE;
            }
            
            // Process duration result
            if (durationThread.result && durationThread.result->success && durationThread.result->output) {
                wchar_t* cleanDuration = durationThread.result->output;
                
                // Remove trailing newlines and whitespace
                size_t len = wcslen(cleanDuration);
                while (len > 0 && (cleanDuration[len-1] == L'\n' || cleanDuration[len-1] == L'\r' || cleanDuration[len-1] == L' ')) {
                    cleanDuration[len-1] = L'\0';
                    len--;
                }
                
                wcsncpy(duration, cleanDuration, durationSize - 1);
                duration[durationSize - 1] = L'\0';
                
                // If we got duration but not title, still consider it a partial success
                if (!success && wcslen(duration) > 0) {
                    success = TRUE;
                }
            }
        } else {
            // Timeout or error - terminate threads
            if (titleThread.hThread) {
                TerminateThread(titleThread.hThread, 1);
            }
            if (durationThread.hThread) {
                TerminateThread(durationThread.hThread, 1);
            }
        }
    }
    
    // Cleanup threads
    if (titleThread.hThread) {
        CloseHandle(titleThread.hThread);
    }
    if (durationThread.hThread) {
        CloseHandle(durationThread.hThread);
    }
    
    // Cleanup results
    if (titleThread.result) FreeYtDlpResult(titleThread.result);
    if (durationThread.result) FreeYtDlpResult(durationThread.result);
    
    // Cleanup requests
    FreeYtDlpRequest(titleRequest);
    FreeYtDlpRequest(durationRequest);
    
    // Cleanup
    CleanupTempDirectory(tempDir);
    CleanupYtDlpConfig(&config);
    
    return success;
}

// Threaded version that doesn't block the UI
BOOL GetVideoTitleAndDuration(HWND hDlg, const wchar_t* url, wchar_t* title, size_t titleSize, wchar_t* duration, size_t durationSize) {
    UNREFERENCED_PARAMETER(title);      // Not used in threaded version
    UNREFERENCED_PARAMETER(titleSize);  // Not used in threaded version
    UNREFERENCED_PARAMETER(duration);   // Not used in threaded version
    UNREFERENCED_PARAMETER(durationSize); // Not used in threaded version
    
    if (!hDlg || !url) return FALSE;
    
    // Create thread data structure
    VideoInfoThreadData* data = (VideoInfoThreadData*)malloc(sizeof(VideoInfoThreadData));
    if (!data) return FALSE;
    
    // Initialize thread data
    data->hDlg = hDlg;
    wcsncpy(data->url, url, MAX_URL_LENGTH - 1);
    data->url[MAX_URL_LENGTH - 1] = L'\0';
    data->title[0] = L'\0';
    data->duration[0] = L'\0';
    data->success = FALSE;
    
    // Create thread to get video info
    data->hThread = CreateThread(NULL, 0, GetVideoInfoThread, data, 0, &data->threadId);
    if (!data->hThread) {
        free(data);
        return FALSE;
    }
    
    // Don't wait for completion - let the thread notify us when done
    // The thread will send WM_USER + 101 message when complete
    return TRUE; // Return TRUE to indicate thread started successfully
}

// Update the video info UI fields with title and duration
void UpdateVideoInfoUI(HWND hDlg, const wchar_t* title, const wchar_t* duration) {
    if (!hDlg) return;
    
    // Update video title field
    if (title && wcslen(title) > 0) {
        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, title);
    } else {
        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, L"Title not available");
    }
    
    // Update video duration field
    if (duration && wcslen(duration) > 0) {
        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, duration);
    } else {
        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, L"Unknown");
    }
    
    // Force redraw of the updated fields
    InvalidateRect(GetDlgItem(hDlg, IDC_VIDEO_TITLE), NULL, TRUE);
    InvalidateRect(GetDlgItem(hDlg, IDC_VIDEO_DURATION), NULL, TRUE);
}

// Enable/disable UI controls during download operations
void SetDownloadUIState(HWND hDlg, BOOL isDownloading) {
    if (!hDlg) return;
    
    // Disable/enable URL input field
    EnableWindow(GetDlgItem(hDlg, IDC_TEXT_FIELD), !isDownloading);
    
    // Disable/enable Get Info button
    EnableWindow(GetDlgItem(hDlg, IDC_GETINFO_BTN), !isDownloading);
    
    // Disable/enable Download button
    EnableWindow(GetDlgItem(hDlg, IDC_DOWNLOAD_BTN), !isDownloading);
    
    // Update global state
    g_isDownloading = isDownloading;
}

// Update the main window's progress bar instead of using separate dialogs
void UpdateMainProgressBar(HWND hDlg, int percentage, const wchar_t* status) {
    if (!hDlg) return;
    
    // Update the progress bar
    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
    if (hProgressBar) {
        SendMessageW(hProgressBar, PBM_SETPOS, percentage, 0);
        
        // Enable/show the progress bar during operations
        EnableWindow(hProgressBar, TRUE);
        ShowWindow(hProgressBar, SW_SHOW);
    }
    
    // Update the progress text if we have it
    HWND hProgressText = GetDlgItem(hDlg, IDC_PROGRESS_TEXT);
    if (hProgressText && status) {
        SetWindowTextW(hProgressText, status);
    }
    
    // Force UI update
    UpdateWindow(hDlg);
}

// Show or hide the main window's progress bar
void ShowMainProgressBar(HWND hDlg, BOOL show) {
    if (!hDlg) return;
    
    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
    if (hProgressBar) {
        ShowWindow(hProgressBar, show ? SW_SHOW : SW_HIDE);
        EnableWindow(hProgressBar, show);
        
        if (!show) {
            // Reset progress bar when hiding
            SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);
        }
    }
    
    // Clear progress text when hiding
    HWND hProgressText = GetDlgItem(hDlg, IDC_PROGRESS_TEXT);
    if (hProgressText) {
        SetWindowTextW(hProgressText, show ? L"" : L"");
    }
}

// Progress callback for the main window (thread-safe) - Legacy function, kept for compatibility
void MainWindowProgressCallback(int percentage, const wchar_t* status, void* userData) {
    HWND hDlg = (HWND)userData;
    if (!hDlg) return;
    
    // Use unified download update message
    PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 3, percentage); // Progress update
    if (status) {
        PostMessageW(hDlg, WM_UNIFIED_DOWNLOAD_UPDATE, 5, (LPARAM)_wcsdup(status)); // Status update
    }
}

void FreeYtDlpResult(YtDlpResult* result) {
    if (!result) return;
    
    if (result->output) {
        free(result->output);
    }
    if (result->errorMessage) {
        free(result->errorMessage);
    }
    if (result->diagnostics) {
        free(result->diagnostics);
    }
    
    free(result);
}

ErrorAnalysis* AnalyzeYtDlpError(const YtDlpResult* result) {
    if (!result || result->success) return NULL;
    
    ErrorAnalysis* analysis = (ErrorAnalysis*)malloc(sizeof(ErrorAnalysis));
    if (!analysis) return NULL;
    
    memset(analysis, 0, sizeof(ErrorAnalysis));
    
    // Simple error analysis based on exit code and output
    if (result->exitCode == 1) {
        analysis->type = ERROR_TYPE_URL_INVALID;
        analysis->description = _wcsdup(L"Invalid URL or video not available");
        analysis->solution = _wcsdup(L"Please check the URL and try again");
    } else if (result->exitCode == 2) {
        analysis->type = ERROR_TYPE_NETWORK;
        analysis->description = _wcsdup(L"Network connection error");
        analysis->solution = _wcsdup(L"Please check your internet connection");
    } else {
        analysis->type = ERROR_TYPE_UNKNOWN;
        analysis->description = _wcsdup(L"Unknown error occurred");
        analysis->solution = _wcsdup(L"Please try again or check yt-dlp configuration");
    }
    
    if (result->output) {
        analysis->technicalDetails = _wcsdup(result->output);
    }
    
    return analysis;
}

void FreeErrorAnalysis(ErrorAnalysis* analysis) {
    if (!analysis) return;
    
    if (analysis->description) {
        free(analysis->description);
    }
    if (analysis->solution) {
        free(analysis->solution);
    }
    if (analysis->technicalDetails) {
        free(analysis->technicalDetails);
    }
    
    free(analysis);
}

// Additional stub implementations for missing functions
BOOL ValidateYtDlpArguments(const wchar_t* args) {
    if (!args) return TRUE;
    
    // Simple validation - reject potentially dangerous arguments
    if (wcsstr(args, L"--exec") || wcsstr(args, L"--batch-file")) {
        return FALSE;
    }
    
    return TRUE;
}

BOOL SanitizeYtDlpArguments(wchar_t* args, size_t argsSize) {
    if (!args || argsSize == 0) return FALSE;
    
    // Simple sanitization - remove dangerous arguments
    // This is a basic implementation
    (void)argsSize; // Suppress unused parameter warning
    return TRUE;
}

BOOL GetYtDlpArgsForOperation(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath, 
                             const YtDlpConfig* config, wchar_t* args, size_t argsSize) {
    if (!args || argsSize == 0) return FALSE;
    
    // Start with custom arguments if they exist
    wchar_t baseArgs[2048] = L"";
    if (config && config->defaultArgs[0] != L'\0') {
        wcscpy(baseArgs, config->defaultArgs);
        wcscat(baseArgs, L" ");
    }
    
    // Build operation-specific arguments
    wchar_t operationArgs[1024];
    switch (operation) {
        case YTDLP_OP_GET_INFO:
            if (url && wcslen(url) > 0) {
                // Use JSON output for structured data extraction
                swprintf(operationArgs, 1024, L"--dump-json --no-download --no-warnings \"%ls\"", url);
            } else {
                wcscpy(operationArgs, L"--version");
            }
            break;
            
        case YTDLP_OP_GET_TITLE:
            if (url && wcslen(url) > 0) {
                swprintf(operationArgs, 1024, L"--get-title --no-download --no-warnings \"%ls\"", url);
            } else {
                return FALSE;
            }
            break;
            
        case YTDLP_OP_GET_DURATION:
            if (url && wcslen(url) > 0) {
                swprintf(operationArgs, 1024, L"--get-duration --no-download --no-warnings \"%ls\"", url);
            } else {
                return FALSE;
            }
            break;
            
        case YTDLP_OP_GET_TITLE_DURATION:
            if (url && wcslen(url) > 0) {
                swprintf(operationArgs, 1024, L"--get-title --get-duration --no-download --no-warnings \"%ls\"", url);
            } else {
                return FALSE;
            }
            break;
            
        case YTDLP_OP_DOWNLOAD:
            if (url && outputPath) {
                // Machine-parseable progress format with pipe delimiters and raw numeric values
                // Format: downloaded_bytes|total_bytes|speed_bytes_per_sec|eta_seconds
                swprintf(operationArgs, 1024, 
                    L"--newline --no-colors --force-overwrites "
                    L"--progress-template \"download:%%(progress.downloaded_bytes)s|%%(progress.total_bytes_estimate)s|%%(progress.speed)s|%%(progress.eta)s\" "
                    L"--output \"%ls\\%%(id)s.%%(ext)s\" \"%ls\"", 
                    outputPath, url);
            } else {
                return FALSE;
            }
            break;
            
        case YTDLP_OP_VALIDATE:
            wcscpy(operationArgs, L"--version");
            break;
            
        default:
            return FALSE;
    }
    
    // Combine custom arguments with operation-specific arguments
    if (wcslen(baseArgs) + wcslen(operationArgs) + 1 >= argsSize) {
        return FALSE; // Not enough space
    }
    
    wcscpy(args, baseArgs);
    wcscat(args, operationArgs);
    
    return TRUE;
}

// Video metadata extraction functions

// Free video metadata structure
void FreeVideoMetadata(VideoMetadata* metadata) {
    if (!metadata) return;
    
    if (metadata->title) {
        free(metadata->title);
        metadata->title = NULL;
    }
    if (metadata->duration) {
        free(metadata->duration);
        metadata->duration = NULL;
    }
    if (metadata->id) {
        free(metadata->id);
        metadata->id = NULL;
    }
    metadata->success = FALSE;
}

// Parse JSON output from yt-dlp to extract metadata
BOOL ParseVideoMetadataFromJson(const wchar_t* jsonOutput, VideoMetadata* metadata) {
    if (!jsonOutput || !metadata) return FALSE;
    
    // Initialize metadata
    memset(metadata, 0, sizeof(VideoMetadata));
    
    // Simple JSON parsing for title
    const wchar_t* titleStart = wcsstr(jsonOutput, L"\"title\":");
    if (titleStart) {
        titleStart = wcschr(titleStart, L'"');
        if (titleStart) {
            titleStart++; // Skip opening quote
            titleStart = wcschr(titleStart, L'"');
            if (titleStart) {
                titleStart++; // Skip second quote
                const wchar_t* titleEnd = wcschr(titleStart, L'"');
                if (titleEnd) {
                    size_t titleLen = titleEnd - titleStart;
                    metadata->title = (wchar_t*)malloc((titleLen + 1) * sizeof(wchar_t));
                    if (metadata->title) {
                        wcsncpy(metadata->title, titleStart, titleLen);
                        metadata->title[titleLen] = L'\0';
                    }
                }
            }
        }
    }
    
    // Simple JSON parsing for duration
    const wchar_t* durationStart = wcsstr(jsonOutput, L"\"duration\":");
    if (durationStart) {
        durationStart += 11; // Skip "duration":
        while (*durationStart == L' ') durationStart++; // Skip spaces
        
        if (*durationStart >= L'0' && *durationStart <= L'9') {
            int seconds = _wtoi(durationStart);
            if (seconds > 0) {
                int minutes = seconds / 60;
                int remainingSeconds = seconds % 60;
                int hours = minutes / 60;
                minutes = minutes % 60;
                
                metadata->duration = (wchar_t*)malloc(32 * sizeof(wchar_t));
                if (metadata->duration) {
                    if (hours > 0) {
                        swprintf(metadata->duration, 32, L"%d:%02d:%02d", hours, minutes, remainingSeconds);
                    } else {
                        swprintf(metadata->duration, 32, L"%d:%02d", minutes, remainingSeconds);
                    }
                }
            }
        }
    }
    
    // Simple JSON parsing for video ID
    const wchar_t* idStart = wcsstr(jsonOutput, L"\"id\":");
    if (idStart) {
        idStart = wcschr(idStart, L'"');
        if (idStart) {
            idStart++; // Skip opening quote
            idStart = wcschr(idStart, L'"');
            if (idStart) {
                idStart++; // Skip second quote
                const wchar_t* idEnd = wcschr(idStart, L'"');
                if (idEnd) {
                    size_t idLen = idEnd - idStart;
                    metadata->id = (wchar_t*)malloc((idLen + 1) * sizeof(wchar_t));
                    if (metadata->id) {
                        wcsncpy(metadata->id, idStart, idLen);
                        metadata->id[idLen] = L'\0';
                    }
                }
            }
        }
    }
    
    metadata->success = (metadata->title != NULL);
    return metadata->success;
}

// Extract video metadata using yt-dlp (optimized version)
// Uses --get-title --get-duration together for faster, simpler parsing
// Output format: First line = title, Second line = duration
BOOL GetVideoMetadata(const wchar_t* url, VideoMetadata* metadata) {
    if (!url || !metadata) return FALSE;
    
    // Initialize metadata
    memset(metadata, 0, sizeof(VideoMetadata));
    
    // Initialize config
    YtDlpConfig config;
    if (!InitializeYtDlpConfig(&config)) {
        return FALSE;
    }
    
    // Create request for getting title and duration together
    YtDlpRequest* request = CreateYtDlpRequest(YTDLP_OP_GET_TITLE_DURATION, url, NULL);
    if (!request) {
        return FALSE;
    }
    
    // Execute the request
    YtDlpResult* result = ExecuteYtDlpRequest(&config, request);
    
    BOOL success = FALSE;
    if (result && result->success && result->output) {
        // Parse the simple output format: first line is title, second line is duration
        wchar_t* output = result->output;
        wchar_t* firstNewline = wcschr(output, L'\n');
        
        if (firstNewline) {
            // Extract title (first line)
            size_t titleLen = firstNewline - output;
            if (titleLen > 0) {
                metadata->title = (wchar_t*)malloc((titleLen + 1) * sizeof(wchar_t));
                if (metadata->title) {
                    wcsncpy(metadata->title, output, titleLen);
                    metadata->title[titleLen] = L'\0';
                    
                    // Remove any trailing carriage return
                    if (titleLen > 0 && metadata->title[titleLen - 1] == L'\r') {
                        metadata->title[titleLen - 1] = L'\0';
                    }
                }
            }
            
            // Extract duration (second line)
            wchar_t* durationStart = firstNewline + 1;
            wchar_t* secondNewline = wcschr(durationStart, L'\n');
            
            size_t durationLen;
            if (secondNewline) {
                durationLen = secondNewline - durationStart;
            } else {
                durationLen = wcslen(durationStart);
            }
            
            if (durationLen > 0) {
                metadata->duration = (wchar_t*)malloc((durationLen + 1) * sizeof(wchar_t));
                if (metadata->duration) {
                    wcsncpy(metadata->duration, durationStart, durationLen);
                    metadata->duration[durationLen] = L'\0';
                    
                    // Remove any trailing carriage return
                    if (durationLen > 0 && metadata->duration[durationLen - 1] == L'\r') {
                        metadata->duration[durationLen - 1] = L'\0';
                    }
                }
            }
            
            // Extract video ID from URL (simple extraction for YouTube URLs)
            wchar_t* videoId = ExtractVideoIdFromUrl(url);
            if (videoId) {
                metadata->id = videoId;
            }
            
            success = (metadata->title != NULL && metadata->duration != NULL);
        }
    }
    
    metadata->success = success;
    
    // Cleanup
    if (result) {
        if (result->output) free(result->output);
        if (result->errorMessage) free(result->errorMessage);
        if (result->diagnostics) free(result->diagnostics);
        free(result);
    }
    FreeYtDlpRequest(request);
    
    return success;
}

// Cached metadata functions

// Initialize cached metadata structure
void InitializeCachedMetadata(CachedVideoMetadata* cached) {
    if (!cached) return;
    
    memset(cached, 0, sizeof(CachedVideoMetadata));
    cached->isValid = FALSE;
}

// Free cached metadata structure
void FreeCachedMetadata(CachedVideoMetadata* cached) {
    if (!cached) return;
    
    if (cached->url) {
        free(cached->url);
        cached->url = NULL;
    }
    
    FreeVideoMetadata(&cached->metadata);
    cached->isValid = FALSE;
}

// Check if cached metadata is valid for the given URL
BOOL IsCachedMetadataValid(const CachedVideoMetadata* cached, const wchar_t* url) {
    if (!cached || !url || !cached->isValid || !cached->url) {
        return FALSE;
    }
    
    return (wcscmp(cached->url, url) == 0);
}

// Store metadata in cache
void StoreCachedMetadata(CachedVideoMetadata* cached, const wchar_t* url, const VideoMetadata* metadata) {
    if (!cached || !url || !metadata) return;
    
    // Free existing data
    FreeCachedMetadata(cached);
    
    // Store new data
    cached->url = _wcsdup(url);
    
    // Copy metadata
    if (metadata->title) {
        cached->metadata.title = _wcsdup(metadata->title);
    }
    if (metadata->duration) {
        cached->metadata.duration = _wcsdup(metadata->duration);
    }
    if (metadata->id) {
        cached->metadata.id = _wcsdup(metadata->id);
    }
    cached->metadata.success = metadata->success;
    
    cached->isValid = TRUE;
}

// Get cached metadata
BOOL GetCachedMetadata(const CachedVideoMetadata* cached, VideoMetadata* metadata) {
    if (!cached || !metadata || !cached->isValid) {
        return FALSE;
    }
    
    // Initialize output metadata
    memset(metadata, 0, sizeof(VideoMetadata));
    
    // Copy cached data
    if (cached->metadata.title) {
        metadata->title = _wcsdup(cached->metadata.title);
    }
    if (cached->metadata.duration) {
        metadata->duration = _wcsdup(cached->metadata.duration);
    }
    if (cached->metadata.id) {
        metadata->id = _wcsdup(cached->metadata.id);
    }
    metadata->success = cached->metadata.success;
    
    return TRUE;
}

// Non-blocking Get Info worker thread
DWORD WINAPI GetInfoWorkerThread(LPVOID lpParam) {
    GetInfoContext* context = (GetInfoContext*)lpParam;
    if (!context) return 1;
    
    VideoMetadata* metadata = (VideoMetadata*)malloc(sizeof(VideoMetadata));
    if (!metadata) {
        free(context);
        return 1;
    }
    
    BOOL success = GetVideoMetadata(context->url, metadata);
    
    // Send result back to main thread (metadata will be freed by the main thread)
    PostMessageW(context->hDialog, WM_USER + 103, (WPARAM)success, (LPARAM)metadata);
    
    free(context);
    return 0;
}

// Start non-blocking Get Info operation
BOOL StartNonBlockingGetInfo(HWND hDlg, const wchar_t* url, CachedVideoMetadata* cachedMetadata) {
    if (!hDlg || !url || !cachedMetadata) return FALSE;
    
    GetInfoContext* context = (GetInfoContext*)malloc(sizeof(GetInfoContext));
    if (!context) return FALSE;
    
    context->hDialog = hDlg;
    wcscpy(context->url, url);
    context->cachedMetadata = cachedMetadata;
    
    HANDLE hThread = CreateThread(NULL, 0, GetInfoWorkerThread, context, 0, NULL);
    if (!hThread) {
        free(context);
        return FALSE;
    }
    
    CloseHandle(hThread);
    return TRUE;
}



// Progress parsing functions

// Free progress info structure
void FreeProgressInfo(ProgressInfo* progress) {
    if (!progress) return;
    
    if (progress->status) {
        free(progress->status);
        progress->status = NULL;
    }
    if (progress->speed) {
        free(progress->speed);
        progress->speed = NULL;
    }
    if (progress->eta) {
        free(progress->eta);
        progress->eta = NULL;
    }
}

// Parse yt-dlp progress output (pipe-delimited machine format)
// Expected format: downloaded_bytes|total_bytes|speed_bytes_per_sec|eta_seconds
// Example: 5562368|104857600|1290000.0|77
BOOL ParseProgressOutput(const wchar_t* line, ProgressInfo* progress) {
    if (!line || !progress) return FALSE;
    
    // Initialize progress info
    memset(progress, 0, sizeof(ProgressInfo));
    
    // Check for pipe-delimited progress format (downloaded|total|speed|eta)
    const wchar_t* firstPipe = wcschr(line, L'|');
    if (firstPipe) {
        // Parse pipe-delimited format: downloaded_bytes|total_bytes|speed_bytes_per_sec|eta_seconds
        wchar_t* lineCopy = _wcsdup(line);
        if (!lineCopy) return FALSE;
        
        wchar_t* context = NULL;
        wchar_t* token = wcstok(lineCopy, L"|", &context);
        int tokenIndex = 0;
        
        long long downloadedBytes = 0;
        long long totalBytes = 0;
        double speedBytesPerSec = 0.0;
        long long etaSeconds = 0;
        
        while (token && tokenIndex < 4) {
            switch (tokenIndex) {
                case 0: { // Downloaded bytes
                    if (wcslen(token) > 0 && wcscmp(token, L"N/A") != 0) {
                        downloadedBytes = wcstoll(token, NULL, 10);
                        progress->downloadedBytes = downloadedBytes;
                    }
                    break;
                }
                case 1: { // Total bytes
                    if (wcslen(token) > 0 && wcscmp(token, L"N/A") != 0) {
                        totalBytes = wcstoll(token, NULL, 10);
                        progress->totalBytes = totalBytes;
                    }
                    break;
                }
                case 2: { // Speed in bytes per second (can be decimal)
                    if (wcslen(token) > 0 && wcscmp(token, L"N/A") != 0) {
                        speedBytesPerSec = wcstod(token, NULL);
                        if (speedBytesPerSec > 0) {
                            // Convert to human-readable format for display
                            wchar_t speedStr[64];
                            if (speedBytesPerSec >= 1024 * 1024 * 1024) {
                                swprintf(speedStr, 64, L"%.1f GB/s", speedBytesPerSec / (1024.0 * 1024.0 * 1024.0));
                            } else if (speedBytesPerSec >= 1024 * 1024) {
                                swprintf(speedStr, 64, L"%.1f MB/s", speedBytesPerSec / (1024.0 * 1024.0));
                            } else if (speedBytesPerSec >= 1024) {
                                swprintf(speedStr, 64, L"%.1f KB/s", speedBytesPerSec / 1024.0);
                            } else {
                                swprintf(speedStr, 64, L"%.0f B/s", speedBytesPerSec);
                            }
                            progress->speed = _wcsdup(speedStr);
                        }
                    }
                    break;
                }
                case 3: { // ETA in seconds
                    if (wcslen(token) > 0 && wcscmp(token, L"N/A") != 0) {
                        etaSeconds = wcstoll(token, NULL, 10);
                        if (etaSeconds > 0) {
                            // Convert seconds to human-readable format
                            wchar_t etaStr[32];
                            if (etaSeconds >= 3600) {
                                int hours = (int)(etaSeconds / 3600);
                                int minutes = (int)((etaSeconds % 3600) / 60);
                                int seconds = (int)(etaSeconds % 60);
                                swprintf(etaStr, 32, L"%d:%02d:%02d", hours, minutes, seconds);
                            } else if (etaSeconds >= 60) {
                                int minutes = (int)(etaSeconds / 60);
                                int seconds = (int)(etaSeconds % 60);
                                swprintf(etaStr, 32, L"%d:%02d", minutes, seconds);
                            } else {
                                swprintf(etaStr, 32, L"%llds", etaSeconds);
                            }
                            progress->eta = _wcsdup(etaStr);
                        }
                    }
                    break;
                }
            }
            token = wcstok(NULL, L"|", &context);
            tokenIndex++;
        }
        
        free(lineCopy);
        
        // Calculate percentage ourselves from the raw data
        if (totalBytes > 0 && downloadedBytes >= 0) {
            progress->percentage = (int)((downloadedBytes * 100) / totalBytes);
            if (progress->percentage > 100) progress->percentage = 100;
        }
        
        // Build comprehensive status message
        wchar_t statusMsg[256];
        if (downloadedBytes > 0 && totalBytes > 0) {
            // Convert bytes to human-readable format
            wchar_t downloadedStr[32], totalStr[32];
            
            if (totalBytes >= 1024 * 1024 * 1024) {
                swprintf(downloadedStr, 32, L"%.1f GB", downloadedBytes / (1024.0 * 1024.0 * 1024.0));
                swprintf(totalStr, 32, L"%.1f GB", totalBytes / (1024.0 * 1024.0 * 1024.0));
            } else if (totalBytes >= 1024 * 1024) {
                swprintf(downloadedStr, 32, L"%.1f MB", downloadedBytes / (1024.0 * 1024.0));
                swprintf(totalStr, 32, L"%.1f MB", totalBytes / (1024.0 * 1024.0));
            } else if (totalBytes >= 1024) {
                swprintf(downloadedStr, 32, L"%.1f KB", downloadedBytes / 1024.0);
                swprintf(totalStr, 32, L"%.1f KB", totalBytes / 1024.0);
            } else {
                swprintf(downloadedStr, 32, L"%lld B", downloadedBytes);
                swprintf(totalStr, 32, L"%lld B", totalBytes);
            }
            
            if (progress->speed && progress->eta) {
                swprintf(statusMsg, 256, L"Downloading %ls of %ls at %ls (ETA: %ls)", 
                        downloadedStr, totalStr, progress->speed, progress->eta);
            } else if (progress->speed) {
                swprintf(statusMsg, 256, L"Downloading %ls of %ls at %ls", 
                        downloadedStr, totalStr, progress->speed);
            } else {
                swprintf(statusMsg, 256, L"Downloading %ls of %ls (%d%%)", downloadedStr, totalStr, progress->percentage);
            }
        } else if (progress->speed) {
            swprintf(statusMsg, 256, L"Downloading at %ls", progress->speed);
        } else {
            wcscpy(statusMsg, L"Downloading");
        }
        
        progress->status = _wcsdup(statusMsg);
        
        // Check for completion
        if (progress->percentage >= 100 || (totalBytes > 0 && downloadedBytes >= totalBytes)) {
            progress->isComplete = TRUE;
            if (progress->status) {
                free(progress->status);
                progress->status = _wcsdup(L"Download complete");
            }
        }
        
        return TRUE;
    }
    
    // Fallback: Look for old format [download] lines for compatibility
    if (wcsstr(line, L"[download]") == NULL) {
        return FALSE;
    }
    
    // Look for percentage in format like "50.0%"
    const wchar_t* percentPos = wcsstr(line, L"%");
    if (percentPos) {
        // Go backwards to find the start of the number
        const wchar_t* start = percentPos - 1;
        while (start > line && (iswdigit(*start) || *start == L'.' || *start == L' ')) {
            start--;
        }
        start++; // Move to first digit
        
        if (start < percentPos) {
            double percent = wcstod(start, NULL);
            progress->percentage = (int)percent;
        }
    }
    
    // Look for speed (pattern like "at 1.23MiB/s")
    const wchar_t* atPos = wcsstr(line, L" at ");
    if (atPos) {
        const wchar_t* speedStart = atPos + 4; // Skip " at "
        const wchar_t* speedEnd = wcsstr(speedStart, L" ETA");
        if (!speedEnd) speedEnd = speedStart + wcslen(speedStart);
        
        if (speedEnd > speedStart) {
            size_t speedLen = speedEnd - speedStart;
            progress->speed = (wchar_t*)malloc((speedLen + 1) * sizeof(wchar_t));
            if (progress->speed) {
                wcsncpy(progress->speed, speedStart, speedLen);
                progress->speed[speedLen] = L'\0';
            }
        }
    }
    
    // Look for ETA (pattern like "ETA 01:17")
    const wchar_t* etaPos = wcsstr(line, L" ETA ");
    if (etaPos) {
        const wchar_t* etaStart = etaPos + 5; // Skip " ETA "
        const wchar_t* etaEnd = etaStart;
        while (*etaEnd && *etaEnd != L' ' && *etaEnd != L'\n' && *etaEnd != L'\r') {
            etaEnd++;
        }
        
        if (etaEnd > etaStart) {
            size_t etaLen = etaEnd - etaStart;
            progress->eta = (wchar_t*)malloc((etaLen + 1) * sizeof(wchar_t));
            if (progress->eta) {
                wcsncpy(progress->eta, etaStart, etaLen);
                progress->eta[etaLen] = L'\0';
            }
        }
    }
    
    // Set status
    progress->status = _wcsdup(L"Downloading");
    
    // Check for completion
    if (wcsstr(line, L"100%") || wcsstr(line, L"has already been downloaded")) {
        progress->percentage = 100;
        progress->isComplete = TRUE;
        if (progress->status) {
            free(progress->status);
            progress->status = _wcsdup(L"Download complete");
        }
    } else {
        progress->isComplete = (progress->percentage >= 100);
    }
    
    return TRUE;
}

// Startup validation and configuration management functions

BOOL InitializeYtDlpSystem(HWND hMainWindow) {
    // Load configuration from registry
    YtDlpConfig config;
    if (!LoadYtDlpConfig(&config)) {
        // Failed to load configuration, set up defaults
        if (!SetupDefaultYtDlpConfiguration(&config)) {
            ShowConfigurationError(hMainWindow, L"Failed to initialize yt-dlp configuration with default values.");
            return FALSE;
        }
        
        // Save the default configuration
        SaveYtDlpConfig(&config);
    }
    
    // Validate the loaded/default configuration
    ValidationInfo validationInfo;
    if (!ValidateYtDlpConfiguration(&config, &validationInfo)) {
        // Configuration validation failed - notify user
        NotifyConfigurationIssues(hMainWindow, &validationInfo);
        FreeValidationInfo(&validationInfo);
        return FALSE;
    }
    
    FreeValidationInfo(&validationInfo);
    return TRUE;
}

BOOL LoadYtDlpConfig(YtDlpConfig* config) {
    if (!config) return FALSE;
    
    // Initialize with default values first
    memset(config, 0, sizeof(YtDlpConfig));
    
    // Load yt-dlp path from registry
    if (!LoadSettingFromRegistry(REG_YTDLP_PATH, config->ytDlpPath, MAX_EXTENDED_PATH)) {
        // No path in registry, use default
        GetDefaultYtDlpPath(config->ytDlpPath, MAX_EXTENDED_PATH);
    }
    
    // Load default temporary directory
    wchar_t tempBuffer[MAX_EXTENDED_PATH];
    if (LoadSettingFromRegistry(L"DefaultTempDir", tempBuffer, MAX_EXTENDED_PATH)) {
        wcscpy(config->defaultTempDir, tempBuffer);
    } else {
        // Use system temp directory as default
        DWORD result = GetTempPathW(MAX_EXTENDED_PATH, config->defaultTempDir);
        if (result == 0 || result >= MAX_EXTENDED_PATH) {
            wcscpy(config->defaultTempDir, L"C:\\Temp\\");
        }
    }
    
    // Load custom yt-dlp arguments
    if (!LoadSettingFromRegistry(REG_CUSTOM_ARGS, config->defaultArgs, 1024)) {
        config->defaultArgs[0] = L'\0';
    }
    
    // Load timeout setting (stored as string in registry)
    wchar_t timeoutStr[32];
    if (LoadSettingFromRegistry(L"TimeoutSeconds", timeoutStr, 32)) {
        config->timeoutSeconds = (DWORD)wcstoul(timeoutStr, NULL, 10);
        if (config->timeoutSeconds == 0) {
            config->timeoutSeconds = 300; // Default 5 minutes
        }
    } else {
        config->timeoutSeconds = 300; // Default 5 minutes
    }
    
    // Load boolean settings
    wchar_t boolBuffer[16];
    if (LoadSettingFromRegistry(L"EnableVerboseLogging", boolBuffer, 16)) {
        config->enableVerboseLogging = (wcscmp(boolBuffer, L"1") == 0);
    } else {
        config->enableVerboseLogging = FALSE;
    }
    
    if (LoadSettingFromRegistry(L"AutoRetryOnFailure", boolBuffer, 16)) {
        config->autoRetryOnFailure = (wcscmp(boolBuffer, L"1") == 0);
    } else {
        config->autoRetryOnFailure = FALSE;
    }
    
    // Load temp directory strategy
    wchar_t strategyStr[32];
    if (LoadSettingFromRegistry(L"TempDirStrategy", strategyStr, 32)) {
        config->tempDirStrategy = (TempDirStrategy)wcstoul(strategyStr, NULL, 10);
        if (config->tempDirStrategy > TEMP_DIR_APPDATA) {
            config->tempDirStrategy = TEMP_DIR_SYSTEM; // Default
        }
    } else {
        config->tempDirStrategy = TEMP_DIR_SYSTEM;
    }
    
    return TRUE;
}

BOOL SaveYtDlpConfig(const YtDlpConfig* config) {
    if (!config) return FALSE;
    
    BOOL allSuccess = TRUE;
    
    // Save yt-dlp path
    if (!SaveSettingToRegistry(REG_YTDLP_PATH, config->ytDlpPath)) {
        allSuccess = FALSE;
    }
    
    // Save default temporary directory
    if (!SaveSettingToRegistry(L"DefaultTempDir", config->defaultTempDir)) {
        allSuccess = FALSE;
    }
    
    // Save custom yt-dlp arguments
    if (!SaveSettingToRegistry(REG_CUSTOM_ARGS, config->defaultArgs)) {
        allSuccess = FALSE;
    }
    
    // Save timeout setting
    wchar_t timeoutStr[32];
    swprintf(timeoutStr, 32, L"%lu", config->timeoutSeconds);
    if (!SaveSettingToRegistry(L"TimeoutSeconds", timeoutStr)) {
        allSuccess = FALSE;
    }
    
    // Save boolean settings
    if (!SaveSettingToRegistry(L"EnableVerboseLogging", config->enableVerboseLogging ? L"1" : L"0")) {
        allSuccess = FALSE;
    }
    
    if (!SaveSettingToRegistry(L"AutoRetryOnFailure", config->autoRetryOnFailure ? L"1" : L"0")) {
        allSuccess = FALSE;
    }
    
    // Save temp directory strategy
    wchar_t strategyStr[32];
    swprintf(strategyStr, 32, L"%d", (int)config->tempDirStrategy);
    if (!SaveSettingToRegistry(L"TempDirStrategy", strategyStr)) {
        allSuccess = FALSE;
    }
    
    return allSuccess;
}

BOOL TestYtDlpFunctionality(const wchar_t* path) {
    if (!path || wcslen(path) == 0) return FALSE;
    
    // Build command line to test yt-dlp version
    size_t cmdLineLen = wcslen(path) + 20;
    wchar_t* cmdLine = (wchar_t*)malloc(cmdLineLen * sizeof(wchar_t));
    if (!cmdLine) return FALSE;
    
    swprintf(cmdLine, cmdLineLen, L"\"%ls\" --version", path);
    
    // Create pipes for output capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        free(cmdLine);
        return FALSE;
    }
    
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    
    // Set up process creation
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;
    
    PROCESS_INFORMATION pi = {0};
    
    // Create process
    BOOL processCreated = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    
    if (!processCreated) {
        free(cmdLine);
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return FALSE;
    }
    
    CloseHandle(hWrite);
    
    // Wait for process completion with timeout
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 10000); // 10 second timeout
    
    BOOL success = FALSE;
    if (waitResult == WAIT_OBJECT_0) {
        // Process completed, check exit code
        DWORD exitCode;
        if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode == 0) {
            success = TRUE;
        }
    } else {
        // Process timed out or failed, terminate it
        TerminateProcess(pi.hProcess, 1);
    }
    
    // Cleanup
    free(cmdLine);
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return success;
}

BOOL ValidateYtDlpConfiguration(const YtDlpConfig* config, ValidationInfo* validationInfo) {
    if (!config || !validationInfo) return FALSE;
    
    // Initialize validation info
    memset(validationInfo, 0, sizeof(ValidationInfo));
    
    // Check if yt-dlp path is set and valid
    if (wcslen(config->ytDlpPath) == 0) {
        validationInfo->result = VALIDATION_NOT_FOUND;
        validationInfo->errorDetails = _wcsdup(L"yt-dlp path is not configured");
        validationInfo->suggestions = _wcsdup(L"Please configure the yt-dlp path in File > Settings");
        return FALSE;
    }
    
    // Validate the yt-dlp executable
    if (!ValidateYtDlpExecutable(config->ytDlpPath)) {
        validationInfo->result = VALIDATION_NOT_EXECUTABLE;
        validationInfo->errorDetails = _wcsdup(L"yt-dlp executable not found or not accessible");
        validationInfo->suggestions = _wcsdup(L"Please check the yt-dlp path in File > Settings and ensure the file exists and is executable");
        return FALSE;
    }
    
    // Skip functionality test during startup - it will be tested when actually used
    
    // Validate temporary directory access
    if (wcslen(config->defaultTempDir) > 0) {
        DWORD attributes = GetFileAttributesW(config->defaultTempDir);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            // Try to create the directory
            if (!CreateDirectoryW(config->defaultTempDir, NULL)) {
                validationInfo->result = VALIDATION_PERMISSION_DENIED;
                validationInfo->errorDetails = _wcsdup(L"Default temporary directory is not accessible");
                validationInfo->suggestions = _wcsdup(L"Please check permissions for the temporary directory or choose a different location");
                return FALSE;
            }
        }
    }
    
    // Validate custom arguments if present
    if (wcslen(config->defaultArgs) > 0) {
        if (!ValidateYtDlpArguments(config->defaultArgs)) {
            validationInfo->result = VALIDATION_PERMISSION_DENIED;
            validationInfo->errorDetails = _wcsdup(L"Custom yt-dlp arguments contain potentially dangerous options");
            validationInfo->suggestions = _wcsdup(L"Please remove --exec, --batch-file, or other potentially harmful arguments from custom arguments");
            return FALSE;
        }
    }
    
    // All validation passed
    validationInfo->result = VALIDATION_OK;
    validationInfo->version = _wcsdup(L"Configuration validated successfully");
    return TRUE;
}

BOOL MigrateYtDlpConfiguration(YtDlpConfig* config) {
    if (!config) return FALSE;
    
    BOOL migrationPerformed = FALSE;
    
    // Check if this is an old configuration that needs migration
    // For now, we'll just ensure all fields have reasonable defaults
    
    // Migrate timeout if it's unreasonable
    if (config->timeoutSeconds < 30 || config->timeoutSeconds > 3600) {
        config->timeoutSeconds = 300; // 5 minutes default
        migrationPerformed = TRUE;
    }
    
    // Migrate temp directory strategy if invalid
    if (config->tempDirStrategy > TEMP_DIR_APPDATA) {
        config->tempDirStrategy = TEMP_DIR_SYSTEM;
        migrationPerformed = TRUE;
    }
    
    // Ensure temp directory is set
    if (wcslen(config->defaultTempDir) == 0) {
        DWORD result = GetTempPathW(MAX_EXTENDED_PATH, config->defaultTempDir);
        if (result == 0 || result >= MAX_EXTENDED_PATH) {
            wcscpy(config->defaultTempDir, L"C:\\Temp\\");
        }
        migrationPerformed = TRUE;
    }
    
    // If migration was performed, save the updated configuration
    if (migrationPerformed) {
        SaveYtDlpConfig(config);
    }
    
    return TRUE;
}

BOOL SetupDefaultYtDlpConfiguration(YtDlpConfig* config) {
    if (!config) return FALSE;
    
    // Initialize all fields to default values
    memset(config, 0, sizeof(YtDlpConfig));
    
    // Set default yt-dlp path
    GetDefaultYtDlpPath(config->ytDlpPath, MAX_EXTENDED_PATH);
    
    // Set default temporary directory
    DWORD result = GetTempPathW(MAX_EXTENDED_PATH, config->defaultTempDir);
    if (result == 0 || result >= MAX_EXTENDED_PATH) {
        wcscpy(config->defaultTempDir, L"C:\\Temp\\");
    }
    
    // Set default arguments (empty)
    config->defaultArgs[0] = L'\0';
    
    // Set default timeout (5 minutes)
    config->timeoutSeconds = 300;
    
    // Set default boolean options
    config->enableVerboseLogging = FALSE;
    config->autoRetryOnFailure = FALSE;
    
    // Set default temp directory strategy
    config->tempDirStrategy = TEMP_DIR_SYSTEM;
    
    return TRUE;
}

void NotifyConfigurationIssues(HWND hParent, const ValidationInfo* validationInfo) {
    if (!validationInfo) return;
    
    wchar_t title[256];
    wchar_t message[1024];
    
    switch (validationInfo->result) {
        case VALIDATION_NOT_FOUND:
            wcscpy(title, L"yt-dlp Not Found");
            swprintf(message, 1024, L"yt-dlp could not be found.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please check your configuration");
            break;
            
        case VALIDATION_NOT_EXECUTABLE:
            wcscpy(title, L"yt-dlp Not Executable");
            swprintf(message, 1024, L"yt-dlp executable is not valid or accessible.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please check your configuration");
            break;
            
        case VALIDATION_MISSING_DEPENDENCIES:
            wcscpy(title, L"yt-dlp Dependencies Missing");
            swprintf(message, 1024, L"yt-dlp is installed but missing required dependencies.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please install Python and yt-dlp dependencies");
            break;
            
        case VALIDATION_VERSION_INCOMPATIBLE:
            wcscpy(title, L"yt-dlp Version Incompatible");
            swprintf(message, 1024, L"yt-dlp version is not compatible.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please update yt-dlp");
            break;
            
        case VALIDATION_PERMISSION_DENIED:
            wcscpy(title, L"Configuration Permission Error");
            swprintf(message, 1024, L"Configuration has permission or security issues.\n\n%ls\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Unknown error",
                    validationInfo->suggestions ? validationInfo->suggestions : L"Please check permissions");
            break;
            
        default:
            wcscpy(title, L"Configuration Error");
            swprintf(message, 1024, L"An unknown configuration error occurred.\n\n%ls", 
                    validationInfo->errorDetails ? validationInfo->errorDetails : L"Please check your yt-dlp configuration");
            break;
    }
    
    MessageBoxW(hParent, message, title, MB_OK | MB_ICONWARNING);
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

// Function to calculate minimum window dimensions based on DPI and content requirements
void CalculateMinimumWindowSize(int* minWidth, int* minHeight, double dpiScaleX, double dpiScaleY) {
    if (!minWidth || !minHeight) return;
    
    // Base measurements in logical units (96 DPI)
    const int BASE_MARGIN = 10;
    const int BASE_WINDOW_MARGIN = 10;
    const int BASE_BUTTON_WIDTH = 78;
    const int BASE_TEXT_HEIGHT = 22;
    const int BASE_LABEL_HEIGHT = 16;
    const int BASE_PROGRESS_HEIGHT = 16;
    const int BASE_GROUP_TITLE_HEIGHT = 18;
    const int BASE_LIST_MIN_HEIGHT = 100;  // Minimum height for the offline videos list
    const int BASE_SIDE_BUTTON_HEIGHT = 32;
    
    // Scale measurements to current DPI
    int margin = (int)(BASE_MARGIN * dpiScaleX);
    int windowMargin = (int)(BASE_WINDOW_MARGIN * dpiScaleX);
    int buttonWidth = (int)(BASE_BUTTON_WIDTH * dpiScaleX);
    int textHeight = (int)(BASE_TEXT_HEIGHT * dpiScaleY);
    int labelHeight = (int)(BASE_LABEL_HEIGHT * dpiScaleY);
    int progressHeight = (int)(BASE_PROGRESS_HEIGHT * dpiScaleY);
    int groupTitleHeight = (int)(BASE_GROUP_TITLE_HEIGHT * dpiScaleY);
    int listMinHeight = (int)(BASE_LIST_MIN_HEIGHT * dpiScaleY);
    int sideButtonHeight = (int)(BASE_SIDE_BUTTON_HEIGHT * dpiScaleY);
    
    // Calculate minimum width requirements
    // Logic: Window margins (20) + text field min width (200) + gap (10) + button width (78) + margin (10) = 318
    int minTextFieldWidth = (int)(200 * dpiScaleX);  // Minimum usable text field width
    int minContentWidth = minTextFieldWidth + margin + buttonWidth + margin;  // Content within group
    int totalMinWidth = (2 * windowMargin) + minContentWidth + (2 * margin);  // Add group margins
    
    // Ensure minimum for UI elements like labels
    int minUIWidth = (int)(400 * dpiScaleX);  // Absolute minimum for UI readability
    *minWidth = (totalMinWidth > minUIWidth) ? totalMinWidth : minUIWidth;
    
    // Calculate minimum height requirements with detailed breakdown
    
    // DOWNLOAD GROUP HEIGHT CALCULATION (130px at 96 DPI) - TWO-LINE VIDEO INFO:
    // - Group title area: 18px (group box border + "Download video" text)
    // - Top margin: 10px (breathing room after title)
    // - URL input row: 22px (standard edit control height)
    // - Reduced margin: 8px (3/4 × 10px for spacing before progress bar)
    // - Progress bar: 16px (standard progress control height, restored to original position)
    // - Reduced margin: 8px (space before video info section)
    // - Video title line: 16px (title text, truncates before "Get Info" button)
    // - Small margin: 6px (1/2 × 10px between title and duration lines)
    // - Duration + status line: 16px (duration + download progress on same line)
    // - Bottom margin: 10px (separation from offline videos group)
    int downloadGroupHeight = groupTitleHeight +           // 18px: group title space
                             margin +                      // 10px: top margin
                             textHeight +                  // 22px: URL field
                             (margin * 3/4) +             //  8px: reduced margin
                             progressHeight +              // 16px: progress bar
                             (margin * 3/4) +             //  8px: reduced margin
                             labelHeight +                 // 16px: video title line
                             (margin / 2) +               //  6px: small margin
                             labelHeight +                 // 16px: duration + status line
                             margin;                       // 10px: bottom margin
                                                          // Total: 130px
    
    // OFFLINE VIDEOS GROUP MINIMUM HEIGHT CALCULATION (159px at 96 DPI):
    // - Group title area: 18px (group box border + "Offline videos" text)
    // - Small margin: 5px (1/2 × 10px for tight spacing after title)
    // - Status labels row: 16px (shows "Status: Ready" and "Items: 0")
    // - Standard margin: 10px (separation before main list content)
    // - Minimum list height: 100px (enough to display 4-5 video entries comfortably)
    // - Bottom margin: 10px (space from window bottom edge)
    int offlineGroupMinHeight = groupTitleHeight +         // 18px: group title space
                               (margin / 2) +             //  5px: small top margin
                               labelHeight +               // 16px: status labels
                               margin +                    // 10px: standard margin
                               listMinHeight +             // 100px: minimum list space
                               margin;                     // 10px: bottom margin
                                                          // Total: 159px
    
    // SIDE BUTTON SPACE VALIDATION:
    // Ensure offline group can accommodate side buttons (Play/Delete)
    // - Two buttons: 2 × 32px = 64px
    // - Spacing between: 5px (1/2 × 10px)
    // - Total button area needed: 69px
    // Since our list minimum (100px) > button area (69px), we have adequate space
    int minSideButtonSpace = (2 * sideButtonHeight) + (margin / 2);  // 64 + 5 = 69px
    if (offlineGroupMinHeight < (groupTitleHeight + (margin / 2) + labelHeight + margin + minSideButtonSpace + margin)) {
        // This should never trigger with our current calculations, but provides safety
        offlineGroupMinHeight = groupTitleHeight + (margin / 2) + labelHeight + margin + minSideButtonSpace + margin;
    }
    
    // TOTAL WINDOW HEIGHT CALCULATION:
    // - Top window margin: 10px (space from window top edge)
    // - Download group: 136px (calculated above)
    // - Inter-group margin: 10px (visual separation between main sections)
    // - Offline group: 159px (calculated above)
    // - Bottom window margin: 10px (space from window bottom edge)
    // - Content subtotal: 325px
    // - Window chrome: ~60px (title bar, menu bar, window borders - OS dependent)
    // - Total minimum: ~385px
    *minHeight = windowMargin +                    // 10px: top margin
                downloadGroupHeight +              // 136px: download section
                margin +                           // 10px: inter-group spacing
                offlineGroupMinHeight +            // 159px: offline section
                windowMargin;                      // 10px: bottom margin
                                                  // Subtotal: 325px
    
    // Add window chrome space (title bar, menu, borders)
    // This varies by Windows version and theme, but ~60px is typical
    *minHeight += (int)(60 * dpiScaleY);          // 60px: window chrome
                                                  // Final total: ~385px at 96 DPI
}

// Function to calculate optimal default window dimensions based on DPI and content requirements
void CalculateDefaultWindowSize(int* defaultWidth, int* defaultHeight, double dpiScaleX, double dpiScaleY) {
    if (!defaultWidth || !defaultHeight) return;
    
    // Start with minimum size as baseline
    CalculateMinimumWindowSize(defaultWidth, defaultHeight, dpiScaleX, dpiScaleY);
    
    // Add comfortable extra space for better user experience
    
    // Width reasoning:
    // - Keep minimum width as default - user can resize window if needed for long titles
    // - No extra horizontal space required since video info wraps appropriately
    // - Focus on vertical space for better video list viewing instead
    int extraWidth = (int)(50 * dpiScaleX);   // Add minimal 50px for slightly more comfortable text field
    *defaultWidth += extraWidth;
    
    // Height reasoning:
    // - Minimum gives us ~100px list height (shows ~3-4 items)
    // - For default, target ~200px list height (shows ~8-10 items comfortably)
    // - Users typically have multiple downloaded videos, need to see more at once
    int extraHeight = (int)(120 * dpiScaleY);  // Add 120px for more comfortable list viewing
    *defaultHeight += extraHeight;
    
    // Ensure we don't exceed reasonable screen space (80% of typical small screen)
    int maxReasonableWidth = (int)(1090 * dpiScaleX);   // 80% of 1366px width
    int maxReasonableHeight = (int)(614 * dpiScaleY);   // 80% of 768px height
    
    if (*defaultWidth > maxReasonableWidth) {
        *defaultWidth = maxReasonableWidth;
    }
    if (*defaultHeight > maxReasonableHeight) {
        *defaultHeight = maxReasonableHeight;
    }
}

// Apply modern Windows theming to dialog and its controls
void ApplyModernThemeToDialog(HWND hDlg) {
    if (!hDlg) return;
    
    // Load UxTheme library for theming functions
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!hUxTheme) return;
    
    // Define constants that might not be available in older headers
    #ifndef ETDT_ENABLETAB
    #define ETDT_ENABLETAB 0x00000006
    #endif
    #ifndef ETDT_USETABTEXTURE
    #define ETDT_USETABTEXTURE 0x00000004
    #endif
    
    // Function pointers for theming APIs
    typedef BOOL (WINAPI *EnableThemeDialogTextureFunc)(HWND, DWORD);
    typedef HRESULT (WINAPI *SetWindowThemeFunc)(HWND, LPCWSTR, LPCWSTR);
    typedef BOOL (WINAPI *IsThemeActiveFunc)(void);
    typedef BOOL (WINAPI *IsAppThemedFunc)(void);
    
    EnableThemeDialogTextureFunc EnableThemeDialogTexture = 
        (EnableThemeDialogTextureFunc)GetProcAddress(hUxTheme, "EnableThemeDialogTexture");
    SetWindowThemeFunc SetWindowTheme = 
        (SetWindowThemeFunc)GetProcAddress(hUxTheme, "SetWindowTheme");
    IsThemeActiveFunc IsThemeActive = 
        (IsThemeActiveFunc)GetProcAddress(hUxTheme, "IsThemeActive");
    IsAppThemedFunc IsAppThemed = 
        (IsAppThemedFunc)GetProcAddress(hUxTheme, "IsAppThemed");
    
    // Only apply theming if themes are active and app is themed
    if (IsThemeActive && IsThemeActive() && IsAppThemed && IsAppThemed()) {
        // Enable dialog texture with both tab texture and tab support
        if (EnableThemeDialogTexture) {
            EnableThemeDialogTexture(hDlg, ETDT_ENABLETAB | ETDT_USETABTEXTURE);
        }
        
        // Apply modern theme to specific control types
        if (SetWindowTheme) {
            // Theme the dialog itself first
            SetWindowTheme(hDlg, L"Explorer", NULL);
            
            // Find and theme all child controls recursively
            HWND hChild = GetWindow(hDlg, GW_CHILD);
            while (hChild) {
                wchar_t className[256];
                if (GetClassNameW(hChild, className, 256)) {
                    BOOL themed = FALSE;
                    
                    // Apply specific themes based on control type
                    if (wcscmp(className, L"Button") == 0) {
                        // Check if it's a group box (different styling)
                        LONG style = GetWindowLong(hChild, GWL_STYLE);
                        if ((style & BS_GROUPBOX) == BS_GROUPBOX) {
                            SetWindowTheme(hChild, L"Explorer", NULL);
                        } else {
                            SetWindowTheme(hChild, L"Explorer", NULL);
                        }
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"Edit") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ListBox") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ComboBox") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"msctls_progress32") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"SysTabControl32") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"Static") == 0) {
                        // For group boxes and other static controls
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ScrollBar") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    
                    // Force redraw if we applied theming
                    if (themed) {
                        InvalidateRect(hChild, NULL, TRUE);
                        UpdateWindow(hChild);
                    }
                }
                hChild = GetWindow(hChild, GW_HWNDNEXT);
            }
        }
        
        // Force redraw of the entire dialog
        InvalidateRect(hDlg, NULL, TRUE);
        UpdateWindow(hDlg);
    }
    
    FreeLibrary(hUxTheme);
}

// Apply theming with a slight delay to ensure all controls are ready
void ApplyDelayedTheming(HWND hDlg) {
    if (!hDlg) return;
    
    // Use a timer to apply theming after the dialog is fully initialized
    SetTimer(hDlg, 9999, 100, NULL); // 100ms delay
}

// Force visual styles activation using multiple methods
void ForceVisualStylesActivation(void) {
    // Method 1: Try to activate visual styles through UxTheme
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme) {
        typedef BOOL (WINAPI *SetThemeAppPropertiesFunc)(DWORD);
        typedef HRESULT (WINAPI *EnableThemingFunc)(BOOL);
        
        SetThemeAppPropertiesFunc SetThemeAppProperties = 
            (SetThemeAppPropertiesFunc)GetProcAddress(hUxTheme, "SetThemeAppProperties");
        EnableThemingFunc EnableTheming = 
            (EnableThemingFunc)GetProcAddress(hUxTheme, "EnableTheming");
        
        if (SetThemeAppProperties) {
            // Enable all theming properties
            SetThemeAppProperties(0x7); // STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT
        }
        
        if (EnableTheming) {
            EnableTheming(TRUE);
        }
        
        FreeLibrary(hUxTheme);
    }
    
    // Method 2: Re-initialize Common Controls with explicit visual styles
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | 
                 ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
}

// Create a dialog with explicit theming support
HWND CreateThemedDialog(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc) {
    // Ensure visual styles are active before creating dialog
    ForceVisualStylesActivation();
    
    // Create the dialog
    HWND hDlg = CreateDialogW(hInstance, lpTemplate, hWndParent, lpDialogFunc);
    
    if (hDlg) {
        // Apply theming immediately after creation
        ApplyModernThemeToDialog(hDlg);
        
        // Show the dialog
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);
        
        // Apply theming again after showing (sometimes needed)
        ApplyDelayedTheming(hDlg);
    }
    
    return hDlg;
}

void ResizeControls(HWND hDlg) {
    RECT rect;
    GetClientRect(hDlg, &rect);
    
    // Get DPI for this window to scale all measurements appropriately
    HDC hdc = GetDC(hDlg);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hDlg, hdc);
    
    // Calculate DPI scaling factors (96 DPI = 100% scaling)
    double scaleX = (double)dpiX / 96.0;
    double scaleY = (double)dpiY / 96.0;
    
    // Base measurements in logical units (96 DPI)
    const int BASE_MARGIN = 10;           // Standard margin between elements
    const int BASE_WINDOW_MARGIN = 10;    // Margin from window edges
    const int BASE_BUTTON_WIDTH = 78;     // Standard button width
    const int BASE_BUTTON_HEIGHT = 26;    // Standard button height

    const int BASE_TEXT_HEIGHT = 22;      // Text field height
    const int BASE_LABEL_HEIGHT = 16;     // Label height
    const int BASE_PROGRESS_HEIGHT = 16;  // Progress bar height
    const int BASE_GROUP_TITLE_HEIGHT = 18; // Group box title area height
    
    // Scale all measurements to current DPI
    int margin = (int)(BASE_MARGIN * scaleX);
    int windowMargin = (int)(BASE_WINDOW_MARGIN * scaleX);
    int buttonWidth = (int)(BASE_BUTTON_WIDTH * scaleX);
    int buttonHeight = (int)(BASE_BUTTON_HEIGHT * scaleY);

    int textHeight = (int)(BASE_TEXT_HEIGHT * scaleY);
    int labelHeight = (int)(BASE_LABEL_HEIGHT * scaleY);
    int progressHeight = (int)(BASE_PROGRESS_HEIGHT * scaleY);
    int groupTitleHeight = (int)(BASE_GROUP_TITLE_HEIGHT * scaleY);
    
    // Calculate available space
    int clientWidth = rect.right - rect.left;
    int clientHeight = rect.bottom - rect.top;
    
    // Calculate Download video group dimensions
    // Two-line video info layout: URL field + progress bar + title line + duration line
    // Vertical layout: Group title (18) + margin (10) + URL row (22) + margin (8) + 
    //                  progress bar (16) + margin (8) + title line (16) + small margin (6) +
    //                  duration line (16) + bottom margin (10) = 130 total
    int downloadGroupHeight = groupTitleHeight + margin + textHeight + (margin * 3/4) + 
                             progressHeight + (margin * 3/4) + labelHeight + (margin / 2) + 
                             labelHeight + margin;
    
    // Position Download video group box
    int downloadGroupX = windowMargin;
    int downloadGroupY = windowMargin;
    int downloadGroupWidth = clientWidth - (2 * windowMargin);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_GROUP), NULL, 
                downloadGroupX, downloadGroupY, downloadGroupWidth, downloadGroupHeight, SWP_NOZORDER);
    
    // Calculate button positions (right-aligned within group) - restore original logic
    int buttonX = downloadGroupX + downloadGroupWidth - buttonWidth - margin;
    int availableTextWidth = buttonX - downloadGroupX - (3 * margin); // Space for text controls
    
    // Position controls within Download video group
    int currentY = downloadGroupY + groupTitleHeight + margin;
    
    // URL row - restore original position
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL1), NULL, 
                downloadGroupX + margin, currentY + 2, (int)(30 * scaleX), labelHeight, SWP_NOZORDER);
    
    int urlFieldX = downloadGroupX + margin + (int)(35 * scaleX);
    int urlFieldWidth = availableTextWidth - (int)(35 * scaleX);
    SetWindowPos(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, 
                urlFieldX, currentY, urlFieldWidth, textHeight, SWP_NOZORDER);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_BTN), NULL, 
                buttonX, currentY - 1, buttonWidth, buttonHeight, SWP_NOZORDER);
    
    currentY += textHeight + (margin * 3/4);
    
    // Progress bar row - restore to original position (right after URL field)
    SetWindowPos(GetDlgItem(hDlg, IDC_PROGRESS_BAR), NULL, 
                urlFieldX, currentY, urlFieldWidth, progressHeight, SWP_NOZORDER);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_GETINFO_BTN), NULL, 
                buttonX, currentY - 1, buttonWidth, buttonHeight, SWP_NOZORDER);
    
    currentY += progressHeight + (margin * 3/4);
    
    // CORRECTED: Two-line video info layout below progress bar
    
    // LINE 1: Video title (truncates to account for "Get Info" button)
    // Mathematical reasoning: availableTextWidth already accounts for button space
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_TITLE_LABEL), NULL, 
                downloadGroupX + margin, currentY, (int)(35 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Title text width = availableTextWidth - label width (35px)
    // This ensures title truncates BEFORE hitting the "Get Info" button
    int titleTextWidth = availableTextWidth - (int)(35 * scaleX);
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_TITLE), NULL, 
                urlFieldX, currentY, titleTextWidth, labelHeight, SWP_NOZORDER);
    
    // Move to next line for duration + download status
    currentY += labelHeight + (margin / 2);  // 6px spacing between lines
    
    // LINE 2: Duration + Download Status (on same line, below title)
    // Duration label: "Duration:" (50px width)
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_DURATION_LABEL), NULL, 
                downloadGroupX + margin, currentY, (int)(50 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Duration value: "5:23" format (60px width - enough for "99:59:59")
    int durationValueX = downloadGroupX + margin + (int)(55 * scaleX);  // After label + 5px gap
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_DURATION), NULL, 
                durationValueX, currentY, (int)(60 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Download status: positioned after duration with 10px gap, uses remaining width
    int statusX = durationValueX + (int)(60 * scaleX) + (int)(10 * scaleX);  // After duration + gap
    int statusWidth = (downloadGroupX + downloadGroupWidth - margin) - statusX;
    SetWindowPos(GetDlgItem(hDlg, IDC_PROGRESS_TEXT), NULL, 
                statusX, currentY, statusWidth, labelHeight, SWP_NOZORDER);
    
    // Position color indicator buttons (hidden by default, positioned for future use)
    // These will be positioned relative to the Add button later in the layout code
    
    // Calculate Offline videos group position and size
    int offlineGroupY = downloadGroupY + downloadGroupHeight + margin;
    int offlineGroupHeight = clientHeight - offlineGroupY - windowMargin;
    
    // Ensure minimum height for offline group
    if (offlineGroupHeight < (int)(100 * scaleY)) {
        offlineGroupHeight = (int)(100 * scaleY);
    }
    
    SetWindowPos(GetDlgItem(hDlg, IDC_OFFLINE_GROUP), NULL, 
                downloadGroupX, offlineGroupY, downloadGroupWidth, offlineGroupHeight, SWP_NOZORDER);
    
    // Position controls within Offline videos group
    int offlineContentY = offlineGroupY + groupTitleHeight + (margin / 2);
    
    // Status labels
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL2), NULL, 
                downloadGroupX + margin, offlineContentY, (int)(150 * scaleX), labelHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL3), NULL, 
                downloadGroupX + margin + (int)(160 * scaleX), offlineContentY, (int)(100 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Calculate listbox and side buttons
    int listY = offlineContentY + labelHeight + margin;
    int listHeight = offlineGroupY + offlineGroupHeight - listY - margin;
    int sideButtonX = downloadGroupX + downloadGroupWidth - buttonWidth - margin;
    int listWidth = sideButtonX - downloadGroupX - (2 * margin);
    
    // Ensure minimum dimensions
    if (listWidth < (int)(200 * scaleX)) {
        listWidth = (int)(200 * scaleX);
    }
    if (listHeight < (int)(50 * scaleY)) {
        listHeight = (int)(50 * scaleY);
    }
    
    SetWindowPos(GetDlgItem(hDlg, IDC_LIST), NULL, 
                downloadGroupX + margin, listY, listWidth, listHeight, SWP_NOZORDER);
    
    // Resize ListView columns
    ResizeCacheListViewColumns(GetDlgItem(hDlg, IDC_LIST), listWidth);
    
    // Side buttons (Play, Delete, and Add)
    int sideButtonHeight = (int)(32 * scaleY);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON2), NULL, 
                sideButtonX, listY, buttonWidth, sideButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON3), NULL, 
                sideButtonX, listY + sideButtonHeight + (margin / 2), buttonWidth, sideButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON1), NULL, 
                sideButtonX, listY + (sideButtonHeight + (margin / 2)) * 2, buttonWidth, sideButtonHeight, SWP_NOZORDER);
    
    // Position colored buttons in 2x2 grid below Add button
    int addButtonY = listY + (sideButtonHeight + (margin / 2)) * 2;
    int colorButtonStartY = addButtonY + sideButtonHeight + (margin / 2);
    int colorButtonWidth = (int)(36 * scaleX);
    int colorButtonHeight = (int)(20 * scaleY);
    int colorButtonSpacing = (int)(6 * scaleX); // Small gap between buttons
    
    // Top row: Green and Teal
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_GREEN), NULL, 
                sideButtonX, colorButtonStartY, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_TEAL), NULL, 
                sideButtonX + colorButtonWidth + colorButtonSpacing, colorButtonStartY, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    
    // Bottom row: Blue and White
    int colorButtonRow2Y = colorButtonStartY + colorButtonHeight + (int)(4 * scaleY);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_BLUE), NULL, 
                sideButtonX, colorButtonRow2Y, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_WHITE), NULL, 
                sideButtonX + colorButtonWidth + colorButtonSpacing, colorButtonRow2Y, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
}

// Settings dialog procedure
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    switch (message) {
        case WM_INITDIALOG: {
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            // Load settings from registry (with defaults if not found)
            LoadSettings(hDlg);
            
            // Apply DPI-aware positioning (similar to error dialog)
            HWND hParent = GetParent(hDlg);
            if (hParent) {
                RECT parentRect, dialogRect;
                GetWindowRect(hDlg, &dialogRect);
                GetWindowRect(hParent, &parentRect);
                
                int dialogWidth = dialogRect.right - dialogRect.left;
                int dialogHeight = dialogRect.bottom - dialogRect.top;
                
                // Get monitor-specific work area for HiDPI awareness
                HMONITOR hMonitor = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                GetMonitorInfoW(hMonitor, &mi);
                RECT screenRect = mi.rcWork;
                
                // Center on parent window
                int x = parentRect.left + (parentRect.right - parentRect.left - dialogWidth) / 2;
                int y = parentRect.top + (parentRect.bottom - parentRect.top - dialogHeight) / 2;
                
                // Adjust if any edge would be off screen
                if (x < screenRect.left) x = screenRect.left;
                if (y < screenRect.top) y = screenRect.top;
                if (x + dialogWidth > screenRect.right) x = screenRect.right - dialogWidth;
                if (y + dialogHeight > screenRect.bottom) y = screenRect.bottom - dialogHeight;
                
                SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            
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
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
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
                
                // Center the dialog with HiDPI awareness
                HWND hParent = GetParent(hDlg);
                RECT parentRect, dialogRect;
                
                GetWindowRect(hDlg, &dialogRect);
                int dialogWidth = dialogRect.right - dialogRect.left;
                int dialogHeight = dialogRect.bottom - dialogRect.top;
                
                // Get monitor-specific work area for HiDPI awareness
                HMONITOR hMonitor;
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                
                if (hParent && GetWindowRect(hParent, &parentRect)) {
                    hMonitor = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
                } else {
                    hMonitor = MonitorFromWindow(hDlg, MONITOR_DEFAULTTONEAREST);
                }
                
                GetMonitorInfoW(hMonitor, &mi);
                RECT screenRect = mi.rcWork;
                
                int x, y;
                
                if (hParent && GetWindowRect(hParent, &parentRect)) {
                    // Center on parent window
                    x = parentRect.left + (parentRect.right - parentRect.left - dialogWidth) / 2;
                    y = parentRect.top + (parentRect.bottom - parentRect.top - dialogHeight) / 2;
                } else {
                    // Center on screen if no parent
                    x = screenRect.left + (screenRect.right - screenRect.left - dialogWidth) / 2;
                    y = screenRect.top + (screenRect.bottom - screenRect.top - dialogHeight) / 2;
                }
                
                // Adjust if any edge would be off screen
                if (x < screenRect.left) x = screenRect.left;
                if (y < screenRect.top) y = screenRect.top;
                if (x + dialogWidth > screenRect.right) x = screenRect.right - dialogWidth;
                if (y + dialogHeight > screenRect.bottom) y = screenRect.bottom - dialogHeight;
                
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
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            // Create brushes for text field colors
            hBrushWhite = CreateSolidBrush(COLOR_WHITE);
            hBrushLightGreen = CreateSolidBrush(COLOR_LIGHT_GREEN);
            hBrushLightBlue = CreateSolidBrush(COLOR_LIGHT_BLUE);
            hBrushLightTeal = CreateSolidBrush(COLOR_LIGHT_TEAL);
            hCurrentBrush = hBrushWhite;
            
            // Initialize cache manager
            wchar_t downloadPath[MAX_EXTENDED_PATH];
            if (!LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
            }
            
            // Initialize ListView with columns
            HWND hListView = GetDlgItem(hDlg, IDC_LIST);
            InitializeCacheListView(hListView);
            
            if (InitializeCacheManager(&g_cacheManager, downloadPath)) {
                // Scan for existing videos in download folder
                ScanDownloadFolderForVideos(&g_cacheManager, downloadPath);
                
                // Refresh the UI with cached videos
                RefreshCacheList(hListView, &g_cacheManager);
                UpdateCacheListStatus(hDlg, &g_cacheManager);
            } else {
                // Initialize dialog controls with defaults if cache fails
                SetDlgItemTextW(hDlg, IDC_LABEL2, L"Status: Cache initialization failed");
                SetDlgItemTextW(hDlg, IDC_LABEL3, L"Items: 0");
            }
            
            // Initialize cached video metadata
            InitializeCachedMetadata(&g_cachedVideoMetadata);
            
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
            
            // Calculate and set optimal default window size based on DPI
            HDC hdc = GetDC(hDlg);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hDlg, hdc);
            
            double scaleX = (double)dpiX / 96.0;
            double scaleY = (double)dpiY / 96.0;
            
            int defaultWidth, defaultHeight;
            CalculateDefaultWindowSize(&defaultWidth, &defaultHeight, scaleX, scaleY);
            
            // Set the calculated default window size
            SetWindowPos(hDlg, NULL, 0, 0, defaultWidth, defaultHeight, SWP_NOMOVE | SWP_NOZORDER);
            
            return FALSE; // Return FALSE since we set focus manually
        }
            
        case WM_SIZE:
            ResizeControls(hDlg);
            return TRUE;
            
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            
            // Get DPI for dynamic minimum size calculation
            HDC hdc = GetDC(hDlg);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hDlg, hdc);
            
            // Calculate DPI scaling factors (96 DPI = 100% scaling)
            double scaleX = (double)dpiX / 96.0;
            double scaleY = (double)dpiY / 96.0;
            
            // Calculate minimum window size based on content requirements
            int minWidth, minHeight;
            CalculateMinimumWindowSize(&minWidth, &minHeight, scaleX, scaleY);
            
            mmi->ptMinTrackSize.x = minWidth;
            mmi->ptMinTrackSize.y = minHeight;
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
                    
                case ID_HELP_INSTALL_YTDLP:
                    // TODO: Implement yt-dlp installation help
                    return TRUE;
                    
                case ID_HELP_ABOUT:
                    // TODO: Implement about dialog
                    return TRUE;
                    
                case IDC_TEXT_FIELD:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // Skip processing if this is a programmatic change
                        if (bProgrammaticChange) {
                            break;
                        }
                        
                        // Clear cached metadata when URL changes
                        FreeCachedMetadata(&g_cachedVideoMetadata);
                        
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
                    


                case IDC_DOWNLOAD_BTN: {
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    if (wcslen(url) == 0) {
                        ShowWarningMessage(hDlg, L"No URL Provided", L"Please enter a YouTube URL to download.");
                        break;
                    }
                    
                    // Start unified download process
                    if (!StartUnifiedDownload(hDlg, url)) {
                        ShowConfigurationError(hDlg, L"Failed to start download. Please check your yt-dlp configuration.");
                    }
                    break;
                }
                    
                case IDC_GETINFO_BTN: {
                    // Check if download is in progress
                    if (g_isDownloading) {
                        ShowWarningMessage(hDlg, L"Download in Progress", L"Please wait for the current download to complete before getting video information.");
                        break;
                    }
                    
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    // Validate URL is provided
                    if (wcslen(url) == 0) {
                        ShowWarningMessage(hDlg, L"No URL Provided", L"Please enter a YouTube URL to get video information.");
                        break;
                    }
                    
                    // Check if we already have cached data for this URL
                    if (IsCachedMetadataValid(&g_cachedVideoMetadata, url)) {
                        VideoMetadata metadata;
                        if (GetCachedMetadata(&g_cachedVideoMetadata, &metadata)) {
                            // Update UI with cached information
                            if (metadata.title) {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, metadata.title);
                            } else {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, L"Unknown Title");
                            }
                            
                            if (metadata.duration) {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, metadata.duration);
                            } else {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, L"Unknown");
                            }
                            
                            // Show progress briefly to indicate completion
                            ShowMainProgressBar(hDlg, TRUE);
                            UpdateMainProgressBar(hDlg, 100, L"Video information (cached)");
                            
                            FreeVideoMetadata(&metadata);
                            break;
                        }
                    }
                    
                    // Show progress bar with indeterminate animation
                    ShowMainProgressBar(hDlg, TRUE);
                    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
                    if (hProgressBar) {
                        // Set progress bar to marquee (indeterminate) style
                        SetWindowLongW(hProgressBar, GWL_STYLE, 
                            GetWindowLongW(hProgressBar, GWL_STYLE) | PBS_MARQUEE);
                        SendMessageW(hProgressBar, PBM_SETMARQUEE, TRUE, 50); // 50ms animation speed
                    }
                    UpdateMainProgressBar(hDlg, 0, L"Getting video information...");
                    
                    // Start non-blocking Get Info operation
                    if (!StartNonBlockingGetInfo(hDlg, url, &g_cachedVideoMetadata)) {
                        // Failed to start operation
                        if (hProgressBar) {
                            SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
                            SetWindowLongW(hProgressBar, GWL_STYLE, 
                                GetWindowLongW(hProgressBar, GWL_STYLE) & ~PBS_MARQUEE);
                        }
                        UpdateMainProgressBar(hDlg, 0, L"Failed to start operation");
                        ShowWarningMessage(hDlg, L"Operation Failed", L"Could not start video information retrieval. Please try again.");
                    }
                    
                    break;
                }
                    
                case IDC_BUTTON2: { // Play button
                    HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                    wchar_t* selectedVideoId = GetSelectedVideoId(hListView);
                    
                    if (!selectedVideoId) {
                        ShowWarningMessage(hDlg, L"No Selection", 
                                         L"Please select a video from the list to play.");
                        break;
                    }
                    
                    // Get player path from settings
                    wchar_t playerPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_PLAYER_PATH, playerPath, MAX_EXTENDED_PATH)) {
                        ShowWarningMessage(hDlg, L"Player Not Configured", 
                                         L"Please configure a media player in File > Settings.");
                        break;
                    }
                    
                    // Validate player exists
                    DWORD attributes = GetFileAttributesW(playerPath);
                    if (attributes == INVALID_FILE_ATTRIBUTES) {
                        ShowWarningMessage(hDlg, L"Player Not Found", 
                                         L"The configured media player was not found. Please check the path in Settings.");
                        break;
                    }
                    
                    // Play the video (no success popup)
                    if (!PlayCacheEntry(&g_cacheManager, selectedVideoId, playerPath)) {
                        ShowWarningMessage(hDlg, L"Playback Failed", 
                                         L"Failed to launch the video. The file may have been moved or deleted.");
                        // Refresh cache to remove invalid entries
                        RefreshCacheList(hListView, &g_cacheManager);
                        UpdateCacheListStatus(hDlg, &g_cacheManager);
                    }
                    break;
                }
                    
                case IDC_BUTTON3: { // Delete button
                    HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                    int selectedCount = 0;
                    wchar_t** selectedVideoIds = GetSelectedVideoIds(hListView, &selectedCount);
                    
                    if (!selectedVideoIds || selectedCount == 0) {
                        ShowWarningMessage(hDlg, L"No Selection", 
                                         L"Please select one or more videos from the list to delete.");
                        break;
                    }
                    
                    // Build confirmation message
                    wchar_t confirmMsg[1024];
                    if (selectedCount == 1) {
                        // Single video - show title if available
                        CacheEntry* entry = FindCacheEntry(&g_cacheManager, selectedVideoIds[0]);
                        if (entry && entry->title) {
                            swprintf(confirmMsg, 1024, 
                                    L"Are you sure you want to delete \"%ls\"?\n\n"
                                    L"This will permanently delete the video file and any associated subtitle files.",
                                    entry->title);
                        } else {
                            wcscpy(confirmMsg, L"Are you sure you want to delete the selected video?\n\n"
                                             L"This will permanently delete the video file and any associated subtitle files.");
                        }
                    } else {
                        // Multiple videos
                        swprintf(confirmMsg, 1024, 
                                L"Are you sure you want to delete %d selected videos?\n\n"
                                L"This will permanently delete all video files and any associated subtitle files.",
                                selectedCount);
                    }
                    
                    // Confirm deletion
                    int result = MessageBoxW(hDlg, confirmMsg, L"Confirm Delete", 
                                           MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                    
                    if (result == IDYES) {
                        // Delete all selected videos
                        int totalErrors = 0;
                        int totalSuccessful = 0;
                        wchar_t* combinedErrorDetails = NULL;
                        size_t combinedErrorSize = 0;
                        
                        for (int i = 0; i < selectedCount; i++) {
                            DeleteResult* deleteResult = DeleteCacheEntryFilesDetailed(&g_cacheManager, selectedVideoIds[i]);
                            
                            if (deleteResult) {
                                if (deleteResult->errorCount == 0) {
                                    totalSuccessful++;
                                } else {
                                    totalErrors += deleteResult->errorCount;
                                    
                                    // Combine error details
                                    wchar_t* errorDetails = FormatDeleteErrorDetails(deleteResult);
                                    if (errorDetails) {
                                        size_t errorLen = wcslen(errorDetails);
                                        size_t newSize = combinedErrorSize + errorLen + 100; // Extra space for headers
                                        
                                        wchar_t* newCombined = (wchar_t*)realloc(combinedErrorDetails, newSize * sizeof(wchar_t));
                                        if (newCombined) {
                                            combinedErrorDetails = newCombined;
                                            
                                            if (combinedErrorSize == 0) {
                                                wcscpy(combinedErrorDetails, L"Multiple Delete Operation Results:\n");
                                                wcscpy(combinedErrorDetails + wcslen(combinedErrorDetails), L"=====================================\n\n");
                                            }
                                            
                                            // Add video identifier
                                            CacheEntry* entry = FindCacheEntry(&g_cacheManager, selectedVideoIds[i]);
                                            if (entry && entry->title) {
                                                swprintf(combinedErrorDetails + wcslen(combinedErrorDetails), 
                                                        newSize - wcslen(combinedErrorDetails),
                                                        L"Video: %ls\n", entry->title);
                                            } else {
                                                swprintf(combinedErrorDetails + wcslen(combinedErrorDetails), 
                                                        newSize - wcslen(combinedErrorDetails),
                                                        L"Video ID: %ls\n", selectedVideoIds[i]);
                                            }
                                            
                                            wcscat(combinedErrorDetails, errorDetails);
                                            wcscat(combinedErrorDetails, L"\n");
                                            
                                            combinedErrorSize = wcslen(combinedErrorDetails);
                                        }
                                        
                                        free(errorDetails);
                                    }
                                }
                                
                                FreeDeleteResult(deleteResult);
                            }
                        }
                        
                        // Show results
                        if (totalErrors == 0) {
                            // All deletions successful (no success popup)
                        } else {
                            // Some deletions failed - show combined error details
                            if (combinedErrorDetails) {
                                // Add summary at the beginning
                                wchar_t summary[256];
                                swprintf(summary, 256, 
                                        L"Summary: %d videos processed, %d successful, %d failed\n\n",
                                        selectedCount, totalSuccessful, selectedCount - totalSuccessful);
                                
                                size_t summaryLen = wcslen(summary);
                                size_t totalLen = summaryLen + wcslen(combinedErrorDetails) + 1;
                                wchar_t* finalDetails = (wchar_t*)malloc(totalLen * sizeof(wchar_t));
                                
                                if (finalDetails) {
                                    wcscpy(finalDetails, summary);
                                    wcscat(finalDetails, combinedErrorDetails);
                                    
                                    EnhancedErrorDialog* errorDialog = CreateEnhancedErrorDialog(
                                        L"Multiple Delete Failed",
                                        L"Some files failed to delete. They may be in use or you may not have permission.",
                                        finalDetails,
                                        L"Check if files are currently open in another application or if you have sufficient permissions.",
                                        L"• Close any applications that might be using the files\n"
                                        L"• Run as administrator if permission is denied\n"
                                        L"• Check if files are read-only or protected\n"
                                        L"• Restart the application and try again",
                                        ERROR_TYPE_PERMISSIONS
                                    );
                                    
                                    if (errorDialog) {
                                        ShowEnhancedErrorDialog(hDlg, errorDialog);
                                        FreeEnhancedErrorDialog(errorDialog);
                                    }
                                    
                                    free(finalDetails);
                                }
                            } else {
                                ShowWarningMessage(hDlg, L"Delete Failed", 
                                                 L"Failed to delete some or all files. They may be in use or you may not have permission.");
                            }
                        }
                        
                        if (combinedErrorDetails) {
                            free(combinedErrorDetails);
                        }
                        
                        // Refresh the list to show current state
                        RefreshCacheList(hListView, &g_cacheManager);
                        UpdateCacheListStatus(hDlg, &g_cacheManager);
                    }
                    
                    // Clean up selected video IDs
                    FreeSelectedVideoIds(selectedVideoIds, selectedCount);
                    break;
                }

                case IDC_BUTTON1: { // Add button (for debugging)
                    // Get download path
                    wchar_t downloadPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                        GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
                    }
                    
                    // Create download directory if it doesn't exist
                    CreateDownloadDirectoryIfNeeded(downloadPath);
                    
                    // Add dummy video (no success popup)
                    if (AddDummyVideo(&g_cacheManager, downloadPath)) {
                        // Refresh the list
                        HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                        RefreshCacheList(hListView, &g_cacheManager);
                        UpdateCacheListStatus(hDlg, &g_cacheManager);
                    } else {
                        ShowWarningMessage(hDlg, L"Add Failed", L"Failed to add dummy video to cache.");
                    }
                    break;
                }
                    
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
            
        case WM_SHOWWINDOW:
            // Apply delayed theming when window is shown
            if (wParam) { // Window is being shown
                ApplyDelayedTheming(hDlg);
            }
            return FALSE;
            
        case WM_TIMER:
            // Handle delayed theming timer
            if (wParam == 9999) {
                KillTimer(hDlg, 9999);
                ApplyModernThemeToDialog(hDlg);
                return TRUE;
            }
            return FALSE;
            
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
            
        case WM_USER + 100: {
            // Handle thread-safe progress updates from worker threads
            int percentage = (int)wParam;
            const wchar_t* status = (const wchar_t*)lParam;
            UpdateMainProgressBar(hDlg, percentage, status);
            return TRUE;
        }
        
        case WM_USER + 101: {
            // Handle video info thread completion
            VideoInfoThreadData* data = (VideoInfoThreadData*)lParam;
            if (data) {
                if (data->success) {
                    UpdateMainProgressBar(hDlg, 90, L"Updating interface...");
                    
                    // Update the UI with the retrieved information
                    UpdateVideoInfoUI(hDlg, data->title, data->duration);
                    
                    UpdateMainProgressBar(hDlg, 100, L"Video information retrieved successfully");
                    // Keep progress bar visible - don't hide it
                    // Success dialog removed - info is displayed in the UI fields
                } else {
                    UpdateMainProgressBar(hDlg, 0, L"Failed to retrieve video information");
                    // Keep progress bar visible to show failure status
                    
                    // Clear any existing video info
                    UpdateVideoInfoUI(hDlg, L"", L"");
                    
                    ShowWarningMessage(hDlg, L"Information Retrieval Failed", 
                        L"Could not retrieve video information. Please check:\n\n"
                        L"• The URL is valid and accessible\n"
                        L"• yt-dlp is properly installed and configured\n"
                        L"• You have an internet connection\n"
                        L"• The video is not private or restricted");
                }
                
                // Cleanup thread data
                if (data->hThread) {
                    CloseHandle(data->hThread);
                }
                free(data);
            }
            return TRUE;
        }
        

        
        case WM_DOWNLOAD_COMPLETE: {
            OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE message received\n");
            
            // Handle download completion
            YtDlpResult* result = (YtDlpResult*)wParam;
            NonBlockingDownloadContext* downloadContext = (NonBlockingDownloadContext*)lParam;
            
            if (!result) {
                OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - NULL result\n");
                return TRUE;
            }
            
            if (!downloadContext) {
                OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - NULL downloadContext\n");
                return TRUE;
            }
            
            wchar_t debugMsg[256];
            swprintf(debugMsg, 256, L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - success=%d, exitCode=%d\n", 
                    result->success, result->exitCode);
            OutputDebugStringW(debugMsg);
            
            HandleDownloadCompletion(hDlg, result, downloadContext);
            return TRUE;
        }
        
        case WM_UNIFIED_DOWNLOAD_UPDATE: {
            int updateType = (int)wParam;
            LPARAM data = lParam;
            
            switch (updateType) {
                case 1: { // Update video title
                    wchar_t* title = (wchar_t*)data;
                    if (title) {
                        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, title);
                        free(title);
                    }
                    break;
                }
                case 2: { // Update video duration
                    wchar_t* duration = (wchar_t*)data;
                    if (duration) {
                        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, duration);
                        free(duration);
                    }
                    break;
                }
                case 3: { // Update progress percentage
                    int percentage = (int)data;
                    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
                    if (hProgressBar) {
                        // Ensure we're not in marquee mode
                        LONG style = GetWindowLongW(hProgressBar, GWL_STYLE);
                        if (style & PBS_MARQUEE) {
                            SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
                            SetWindowLongW(hProgressBar, GWL_STYLE, style & ~PBS_MARQUEE);
                        }
                        SendMessageW(hProgressBar, PBM_SETPOS, percentage, 0);
                    }
                    break;
                }
                case 4: { // Start marquee
                    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
                    if (hProgressBar) {
                        SetWindowLongW(hProgressBar, GWL_STYLE, 
                            GetWindowLongW(hProgressBar, GWL_STYLE) | PBS_MARQUEE);
                        SendMessageW(hProgressBar, PBM_SETMARQUEE, TRUE, 50);
                    }
                    break;
                }
                case 5: { // Update status text
                    wchar_t* status = (wchar_t*)data;
                    if (status) {
                        SetDlgItemTextW(hDlg, IDC_PROGRESS_TEXT, status);
                        free(status);
                    }
                    break;
                }
                case 6: { // Stop marquee
                    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
                    if (hProgressBar) {
                        SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
                        SetWindowLongW(hProgressBar, GWL_STYLE, 
                            GetWindowLongW(hProgressBar, GWL_STYLE) & ~PBS_MARQUEE);
                    }
                    break;
                }
                case 7: { // Download failed
                    UpdateMainProgressBar(hDlg, 0, L"Download failed");
                    SetDownloadUIState(hDlg, FALSE);
                    Sleep(500);
                    ShowMainProgressBar(hDlg, FALSE);
                    break;
                }
            }
            return TRUE;
        }
            
        case WM_DESTROY:
            // Clean up cache manager
            CleanupCacheManager(&g_cacheManager);
            
            // Clean up ListView item data
            CleanupListViewItemData(GetDlgItem(hDlg, IDC_LIST));
            
            // Clean up brushes
            if (hBrushWhite) DeleteObject(hBrushWhite);
            if (hBrushLightGreen) DeleteObject(hBrushLightGreen);
            if (hBrushLightBlue) DeleteObject(hBrushLightBlue);
            if (hBrushLightTeal) DeleteObject(hBrushLightTeal);
            
            PostQuitMessage(0);
            return TRUE;
    }
    return FALSE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    // Initialize error logging
    InitializeErrorLogging();
    
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
    
    // Enable Windows XP+ Visual Styles programmatically (backup to manifest)
    // This ensures theming works even if manifest processing fails
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme) {
        // Try to enable theming programmatically as backup
        typedef BOOL (WINAPI *SetThemeAppPropertiesFunc)(DWORD);
        SetThemeAppPropertiesFunc SetThemeAppProperties = 
            (SetThemeAppPropertiesFunc)GetProcAddress(hUxTheme, "SetThemeAppProperties");
        
        if (SetThemeAppProperties) {
            // Enable all theming properties
            SetThemeAppProperties(0x7); // STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT
        }
        
        FreeLibrary(hUxTheme);
    }
    
    // Also try the older method for compatibility
    typedef BOOL (WINAPI *InitCommonControlsExFunc)(LPINITCOMMONCONTROLSEX);
    HMODULE hComCtl32 = LoadLibraryW(L"comctl32.dll");
    if (hComCtl32) {
        InitCommonControlsExFunc InitCommonControlsExPtr = 
            (InitCommonControlsExFunc)GetProcAddress(hComCtl32, "InitCommonControlsEx");
        
        if (InitCommonControlsExPtr) {
            // Re-initialize with visual styles
            INITCOMMONCONTROLSEX icex2;
            icex2.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex2.dwICC = ICC_WIN95_CLASSES;
            InitCommonControlsExPtr(&icex2);
        }
        
        FreeLibrary(hComCtl32);
    }
    
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
