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
            ShowWarningMessage(hDlg, L"Invalid Arguments", 
                             L"Custom yt-dlp arguments contain potentially dangerous options and were not saved.\n\nPlease remove any --exec, --batch-file, or other potentially harmful arguments.");
        }
    } else {
        // Empty arguments - save empty string
        SaveSettingToRegistry(REG_CUSTOM_ARGS, L"");
    }
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
            swprintf(tempDir, tempDirSize, L"%ls\\Temp\\", localAppDataW);
            CoTaskMemFree(localAppDataW);
        } else {
            // Ultimate fallback to current directory
            wcscpy(tempDir, L".\\");
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
            swprintf(tempDir, tempDirSize, L"%ls\\YouTubeCacher\\Temp\\", localAppDataW);
            CoTaskMemFree(localAppDataW);
        } else {
            wcscpy(tempDir, L".\\temp\\");
        }
    }
    
    // Append unique subdirectory name
    wchar_t uniqueName[64];
    swprintf(uniqueName, 64, L"YouTubeCacher_%lu", GetTickCount());
    
    if (wcslen(tempDir) + wcslen(uniqueName) + 2 >= tempDirSize) {
        return FALSE;
    }
    
    wcscat(tempDir, uniqueName);
    wcscat(tempDir, L"\\");
    
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
        swprintf(tempPath, pathSize, L"%ls\\YouTubeCacher\\Temp\\%ls\\", localAppDataW, uniqueName);
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
            swprintf(uniqueName, 64, L"YouTubeCacher_%lu\\", GetTickCount());
            
            if (wcslen(tempPath) + wcslen(uniqueName) < pathSize) {
                wcscat(tempPath, uniqueName);
                if (CreateDirectoryW(tempPath, NULL)) {
                    return TRUE;
                }
            }
        }
    }
    
    // Final fallback to current directory
    wcscpy(tempPath, L".\\temp\\");
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
    
    // Cleanup
    free(cmdLine);
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Set result
    result->success = (exitCode == 0);
    result->exitCode = exitCode;
    result->output = outputBuffer;
    
    if (!result->success && (!result->output || wcslen(result->output) == 0)) {
        result->errorMessage = _wcsdup(L"yt-dlp process failed");
    }
    
    return result;
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
                swprintf(operationArgs, 1024, L"--dump-json --no-download \"%ls\"", url);
            } else {
                wcscpy(operationArgs, L"--version");
            }
            break;
            
        case YTDLP_OP_DOWNLOAD:
            if (url && outputPath) {
                swprintf(operationArgs, 1024, L"-o \"%ls\\%%(title)s.%%(ext)s\" \"%ls\"", outputPath, url);
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

void ResizeControls(HWND hDlg) {
    RECT rect;
    GetClientRect(hDlg, &rect);
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    // Calculate button position with proper margin (20px from window edge, 10px from group box edge)
    int buttonX = width - BUTTON_WIDTH - 20;  // 20 pixel margin from window edge
    int sideButtonX = width - BUTTON_WIDTH - 20;
    
    // Resize Download video group box with 10px margin from window edges
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_GROUP), NULL, 10, 10, width - 20, 110, SWP_NOZORDER);
    
    // Position URL label (within download group) - moved down to align better
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL1), NULL, 20, 32, 30, 16, SWP_NOZORDER);
    
    // Resize URL text field (within download group) - leave 10px gap before buttons
    SetWindowPos(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, 55, 30, buttonX - 55 - 10, 22, SWP_NOZORDER);
    
    // Position progress bar below URL field with same width as text field
    SetWindowPos(GetDlgItem(hDlg, IDC_PROGRESS_BAR), NULL, 55, 58, buttonX - 55 - 10, 16, SWP_NOZORDER);
    
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
    SetWindowPos(GetDlgItem(hDlg, IDC_LIST), NULL, 20, listY, sideButtonX - 20 - 10, listHeight, SWP_NOZORDER);
    
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
                    


                case IDC_DOWNLOAD_BTN: {
                    // Initialize YtDlp configuration
                    YtDlpConfig config = {0};
                    if (!InitializeYtDlpConfig(&config)) {
                        EnhancedErrorDialog* errorDialog = CreateEnhancedErrorDialog(
                            L"Configuration Error",
                            L"Failed to initialize yt-dlp configuration.",
                            L"The application could not load or initialize the yt-dlp configuration settings.",
                            L"This may indicate a problem with registry access, memory allocation, or corrupted settings.",
                            L"1. Try restarting the application\r\n"
                            L"2. Check if you have permission to access the registry\r\n"
                            L"3. Reset settings through the Settings dialog\r\n"
                            L"4. Reinstall the application if the problem persists",
                            ERROR_TYPE_UNKNOWN
                        );
                        if (errorDialog) {
                            ShowEnhancedErrorDialog(hDlg, errorDialog);
                            FreeEnhancedErrorDialog(errorDialog);
                        }
                        break;
                    }
                    
                    // Perform comprehensive validation
                    ValidationInfo validationInfo = {0};
                    if (!ValidateYtDlpComprehensive(config.ytDlpPath, &validationInfo)) {
                        // Show enhanced error dialog with diagnostic information
                        wchar_t diagnosticReport[2048];
                        swprintf(diagnosticReport, 2048, 
                            L"yt-dlp validation failed:\n\n"
                            L"Issue: %ls\n\n"
                            L"Suggestions:\n%ls\n\n"
                            L"Path checked: %ls",
                            validationInfo.errorDetails ? validationInfo.errorDetails : L"Unknown validation error",
                            validationInfo.suggestions ? validationInfo.suggestions : L"Please check yt-dlp installation",
                            config.ytDlpPath);
                        
                        ShowValidationError(hDlg, &validationInfo);
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
                        break;
                    }
                    
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    if (wcslen(url) == 0) {
                        EnhancedErrorDialog* errorDialog = CreateEnhancedErrorDialog(
                            L"No URL Provided",
                            L"Please enter a URL to download.",
                            L"A valid YouTube or supported video URL is required to perform the download operation.",
                            L"The URL field is currently empty. Please paste or type a valid video URL.",
                            L"1. Copy a video URL from your browser\r\n"
                            L"2. Paste it into the URL field\r\n"
                            L"3. Ensure the URL is from a supported site (YouTube, etc.)\r\n"
                            L"4. Check that the URL is complete and properly formatted",
                            ERROR_TYPE_URL_INVALID
                        );
                        if (errorDialog) {
                            ShowEnhancedErrorDialog(hDlg, errorDialog);
                            FreeEnhancedErrorDialog(errorDialog);
                        }
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
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
                        ShowTempDirError(hDlg, downloadPath, GetLastError());
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
                        break;
                    }
                    
                    // Create YtDlp request for download operation
                    YtDlpRequest* request = CreateYtDlpRequest(YTDLP_OP_DOWNLOAD, url, downloadPath);
                    if (!request) {
                        ShowMemoryError(hDlg, L"yt-dlp request creation");
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
                        break;
                    }
                    
                    // Create temporary directory for download operation
                    wchar_t tempDir[MAX_EXTENDED_PATH];
                    if (!CreateTempDirectory(&config, tempDir, MAX_EXTENDED_PATH)) {
                        // Try alternative temp directory strategies
                        if (!CreateYtDlpTempDirWithFallback(tempDir, MAX_EXTENDED_PATH)) {
                            ShowTempDirError(hDlg, L"System temporary directory", GetLastError());
                            FreeYtDlpRequest(request);
                            FreeValidationInfo(&validationInfo);
                            CleanupYtDlpConfig(&config);
                            break;
                        }
                    }
                    request->tempDir = _wcsdup(tempDir);
                    
                    // Create progress dialog for the download operation
                    ProgressDialog* progress = CreateProgressDialog(hDlg, L"Downloading Video");
                    if (!progress) {
                        ShowUIError(hDlg, L"progress dialog");
                        CleanupTempDirectory(tempDir);
                        FreeYtDlpRequest(request);
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
                        break;
                    }
                    
                    // Execute the download operation with enhanced error handling and progress tracking
                    UpdateProgressDialog(progress, 5, L"Initializing download...");
                    YtDlpResult* result = ExecuteYtDlpRequest(&config, request);
                    
                    if (result && result->success) {
                        UpdateProgressDialog(progress, 100, L"Download completed successfully");
                        Sleep(1000);
                        DestroyProgressDialog(progress);
                        
                        // Show success message with download location
                        wchar_t successMsg[MAX_EXTENDED_PATH + 100];
                        swprintf(successMsg, MAX_EXTENDED_PATH + 100, 
                            L"Download completed successfully!\n\nFiles saved to:\n%ls", downloadPath);
                        ShowSuccessMessage(hDlg, L"Download Complete", successMsg);
                    } else {
                        UpdateProgressDialog(progress, 0, L"Download failed");
                        Sleep(500);
                        DestroyProgressDialog(progress);
                        
                        // Show enhanced error dialog with comprehensive diagnostics
                        if (result) {
                            ShowYtDlpError(hDlg, result, request);
                        } else {
                            ShowConfigurationError(hDlg, L"Download operation failed to initialize properly. Please check your yt-dlp configuration.");
                        }
                    }
                    
                    // Cleanup resources
                    CleanupTempDirectory(tempDir);
                    if (result) FreeYtDlpResult(result);
                    FreeYtDlpRequest(request);
                    FreeValidationInfo(&validationInfo);
                    CleanupYtDlpConfig(&config);
                    break;
                }
                    
                case IDC_GETINFO_BTN: {
                    // Initialize YtDlp configuration
                    YtDlpConfig config = {0};
                    if (!InitializeYtDlpConfig(&config)) {
                        ShowConfigurationError(hDlg, L"Failed to initialize yt-dlp configuration. Please check your settings.");
                        break;
                    }
                    
                    // Perform comprehensive validation
                    ValidationInfo validationInfo = {0};
                    if (!ValidateYtDlpComprehensive(config.ytDlpPath, &validationInfo)) {
                        // Show enhanced error dialog with diagnostic information
                        wchar_t diagnosticReport[2048];
                        swprintf(diagnosticReport, 2048, 
                            L"yt-dlp validation failed:\n\n"
                            L"Issue: %ls\n\n"
                            L"Suggestions:\n%ls\n\n"
                            L"Path checked: %ls",
                            validationInfo.errorDetails ? validationInfo.errorDetails : L"Unknown validation error",
                            validationInfo.suggestions ? validationInfo.suggestions : L"Please check yt-dlp installation",
                            config.ytDlpPath);
                        
                        ShowValidationError(hDlg, &validationInfo);
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
                        break;
                    }
                    
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    // Create YtDlp request for info operation
                    YtDlpRequest* request = CreateYtDlpRequest(YTDLP_OP_GET_INFO, 
                                                              wcslen(url) > 0 ? url : NULL, 
                                                              NULL);
                    if (!request) {
                        ShowMemoryError(hDlg, L"yt-dlp request creation");
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
                        break;
                    }
                    
                    // Create temporary directory for operation
                    wchar_t tempDir[MAX_EXTENDED_PATH];
                    if (!CreateTempDirectory(&config, tempDir, MAX_EXTENDED_PATH)) {
                        ShowTempDirError(hDlg, tempDir, GetLastError());
                        FreeYtDlpRequest(request);
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
                        break;
                    }
                    request->tempDir = _wcsdup(tempDir);
                    
                    // Create progress dialog for the operation
                    ProgressDialog* progress = CreateProgressDialog(hDlg, L"Getting Video Information");
                    if (!progress) {
                        ShowUIError(hDlg, L"progress dialog");
                        CleanupTempDirectory(tempDir);
                        FreeYtDlpRequest(request);
                        FreeValidationInfo(&validationInfo);
                        CleanupYtDlpConfig(&config);
                        break;
                    }
                    
                    // Execute the yt-dlp operation with enhanced error handling
                    UpdateProgressDialog(progress, 10, L"Initializing yt-dlp operation...");
                    YtDlpResult* result = ExecuteYtDlpRequest(&config, request);
                    
                    if (result && result->success) {
                        UpdateProgressDialog(progress, 100, L"Operation completed successfully");
                        Sleep(500);
                        DestroyProgressDialog(progress);
                        
                        // Display the video information
                        if (result->output && wcslen(result->output) > 0) {
                            ShowInfoMessage(hDlg, L"Video Information", result->output);
                        } else {
                            ShowWarningMessage(hDlg, L"No Information", L"Operation completed successfully but no information was returned.");
                        }
                    } else {
                        UpdateProgressDialog(progress, 0, L"Operation failed");
                        Sleep(500);
                        DestroyProgressDialog(progress);
                        
                        // Show enhanced error dialog with comprehensive diagnostics
                        ShowYtDlpError(hDlg, result, request);
                    }
                    
                    // Cleanup resources
                    CleanupTempDirectory(tempDir);
                    if (result) FreeYtDlpResult(result);
                    FreeYtDlpRequest(request);
                    FreeValidationInfo(&validationInfo);
                    CleanupYtDlpConfig(&config);
                    break;
                }
                    
                case IDC_BUTTON2:
                    ShowInfoMessage(hDlg, L"Play", L"Play functionality not implemented yet");
                    break;
                    
                case IDC_BUTTON3:
                    ShowInfoMessage(hDlg, L"Delete", L"Delete functionality not implemented yet");
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    // Initialize error logging
    InitializeErrorLogging();
    
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
