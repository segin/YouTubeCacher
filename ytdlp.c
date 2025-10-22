#include "YouTubeCacher.h"

// Function to install yt-dlp using winget
void InstallYtDlpWithWinget(HWND hParent) {
    DebugOutput(L"YouTubeCacher: InstallYtDlpWithWinget - Starting yt-dlp installation");
    
    // Create process to run winget install yt-dlp
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW; // Show the command window so user can see progress
    
    wchar_t cmdLine[] = L"winget install yt-dlp";
    
    // Create the process
    BOOL processCreated = CreateProcessW(
        NULL,           // No module name (use command line)
        cmdLine,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi             // Pointer to PROCESS_INFORMATION structure
    );
    
    if (!processCreated) {
        DWORD error = GetLastError();
        wchar_t debugMsg[256];
        swprintf(debugMsg, 256, L"YouTubeCacher: InstallYtDlpWithWinget - Failed to create winget process, error: %lu", error);
        DebugOutput(debugMsg);
        
        // Create unified warning dialog for WinGet not available
        static wchar_t details[512];
        swprintf(details, 512, L"Process creation failed with error code: %lu\r\n\r\n"
                              L"winget is the Windows Package Manager that should be available on:\r\n"
                              L"- Windows 10 version 1809 and later\r\n"
                              L"- Windows 11 (all versions)\r\n\r\n"
                              L"winget is typically installed automatically when an admin user first logs in to modern Windows systems.",
                              error);
        
        static wchar_t diagnostics[256];
        wcscpy(diagnostics, L"The system was unable to execute the 'winget install yt-dlp' command. "
                           L"This indicates winget may not be installed, not in PATH, or access is restricted.");
        
        static wchar_t solutions[512];
        wcscpy(solutions, L"Manual yt-dlp Installation:\r\n"
                         L"1. Visit: https://github.com/yt-dlp/yt-dlp/releases\r\n"
                         L"2. Download the latest yt-dlp.exe\r\n"
                         L"3. Place it in a folder in your PATH or configure the path in File > Settings\r\n\r\n"
                         L"Alternative - Install winget:\r\n"
                         L"- Download MSIX bundle from: https://github.com/microsoft/winget-cli/releases\r\n"
                         L"- Install the .msixbundle file (requires sideloading enabled)");
        
        UnifiedDialogConfig config = {0};
        config.dialogType = UNIFIED_DIALOG_WARNING;
        config.title = L"winget Not Available";
        config.message = L"Could not run 'winget install yt-dlp'. winget may not be installed or available on this system.";
        config.details = details;
        config.tab1_name = L"Details";
        config.tab2_content = diagnostics;
        config.tab2_name = L"Diagnostics";
        config.tab3_content = solutions;
        config.tab3_name = L"Solutions";
        config.showDetailsButton = TRUE;
        config.showCopyButton = TRUE;
        
        ShowUnifiedDialog(hParent, &config);
        return;
    }
    
    DebugOutput(L"YouTubeCacher: InstallYtDlpWithWinget - WinGet process started successfully");
    
    // Show simple informational dialog about installation progress using unified dialog
    UnifiedDialogConfig config = {0};
    config.dialogType = UNIFIED_DIALOG_INFO;
    config.title = L"Installing yt-dlp";
    config.message = L"winget is installing yt-dlp. Please wait for the installation to complete.\r\n\r\n"
                    L"A command window will show the installation progress. "
                    L"After installation completes, you may need to restart YouTubeCacher or update the yt-dlp path in File > Settings.";
    config.details = L"The winget package manager is downloading and installing yt-dlp automatically. "
                    L"This process may take a few minutes depending on your internet connection.";
    config.tab1_name = L"Details";
    config.showDetailsButton = TRUE;
    config.showCopyButton = TRUE;
    
    ShowUnifiedDialog(hParent, &config);
    
    // Don't wait for the process - let it run independently
    // Close handles to avoid resource leaks
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
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
        
        // Safe line-based processing for synchronous execution
        static char syncLineAccumulator[8192] = {0};
        static size_t syncFillCounter = 0;
        
        while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            
            // Safely add new bytes with bounds checking
            size_t spaceAvailable = sizeof(syncLineAccumulator) - syncFillCounter - 1;
            size_t bytesToCopy = (bytesRead < spaceAvailable) ? bytesRead : spaceAvailable;
            
            if (bytesToCopy > 0) {
                memcpy(syncLineAccumulator + syncFillCounter, buffer, bytesToCopy);
                syncFillCounter += bytesToCopy;
                syncLineAccumulator[syncFillCounter] = '\0';
            }
            
            // Process complete lines
            char* start = syncLineAccumulator;
            char* newline;
            while ((newline = strchr(start, '\n')) != NULL) {
                *newline = '\0';
                
                // Remove \r if present
                size_t lineLen = newline - start;
                if (lineLen > 0 && start[lineLen - 1] == '\r') {
                    start[lineLen - 1] = '\0';
                    lineLen--;
                }
                
                // Convert complete UTF-8 line to wide chars
                if (lineLen > 0) {
                    wchar_t tempOutput[2048];
                    int converted = MultiByteToWideChar(CP_UTF8, 0, start, (int)lineLen, tempOutput, 2047);
                    if (converted > 0) {
                        tempOutput[converted] = L'\0';
                        
                        size_t currentLen = wcslen(outputBuffer);
                        size_t newLen = currentLen + converted + 2; // +2 for \n and null
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
                        wcscat(outputBuffer, L"\n");
                    }
                }
                
                start = newline + 1;
            }
            
            // Move remaining incomplete data to start
            size_t remaining = syncFillCounter - (start - syncLineAccumulator);
            if (remaining > 0 && start != syncLineAccumulator) {
                memmove(syncLineAccumulator, start, remaining);
            }
            syncFillCounter = remaining;
            syncLineAccumulator[syncFillCounter] = '\0';
        }
        
        // Process any remaining data at end
        if (syncFillCounter > 0) {
            wchar_t tempOutput[2048];
            int converted = MultiByteToWideChar(CP_UTF8, 0, syncLineAccumulator, (int)syncFillCounter, tempOutput, 2047);
            if (converted > 0) {
                tempOutput[converted] = L'\0';
                
                size_t currentLen = wcslen(outputBuffer);
                size_t newLen = currentLen + converted + 1;
                if (newLen >= outputSize) {
                    outputSize = newLen * 2;
                    wchar_t* newBuffer = (wchar_t*)realloc(outputBuffer, outputSize * sizeof(wchar_t));
                    if (newBuffer) {
                        outputBuffer = newBuffer;
                    }
                }
                
                if (outputBuffer) {
                    wcscat(outputBuffer, tempOutput);
                }
            }
            syncFillCounter = 0;
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
                swprintf(operationArgs, 1024, L"--get-title --no-download --no-warnings --encoding utf-8 \"%ls\"", url);
            } else {
                return FALSE;
            }
            break;
            
        case YTDLP_OP_GET_DURATION:
            if (url && wcslen(url) > 0) {
                swprintf(operationArgs, 1024, L"--get-duration --no-download --no-warnings --encoding utf-8 \"%ls\"", url);
            } else {
                return FALSE;
            }
            break;
            
        case YTDLP_OP_GET_TITLE_DURATION:
            if (url && wcslen(url) > 0) {
                swprintf(operationArgs, 1024, L"--get-title --get-duration --no-download --no-warnings --encoding utf-8 \"%ls\"", url);
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
        // Parse the output - first line is title, second line is duration
        wchar_t* output = _wcsdup(result->output);
        if (output) {
            wchar_t* context = NULL;
            wchar_t* line = wcstok(output, L"\n", &context);
            if (line) {
                // First line is title
                metadata->title = _wcsdup(line);
                
                // Second line is duration
                line = wcstok(NULL, L"\n", &context);
                if (line) {
                    metadata->duration = _wcsdup(line);
                }
            }
            free(output);
            
            metadata->success = (metadata->title != NULL);
            success = metadata->success;
        }
    }
    
    // Cleanup
    if (result) FreeYtDlpResult(result);
    FreeYtDlpRequest(request);
    CleanupYtDlpConfig(&config);
    
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
        // Handle both "download:123|456|789|10" and raw "123|456|789|10" formats
        const wchar_t* dataStart = line;
        if (wcsstr(line, L"download:") == line) {
            dataStart = line + 9; // Skip "download:" prefix
        }
        
        // Parse pipe-delimited format: downloaded_bytes|total_bytes|speed_bytes_per_sec|eta_seconds
        wchar_t* lineCopy = _wcsdup(dataStart);
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
        } else {
            // No total size available - use marquee mode (indicated by -1)
            progress->percentage = -1;
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
        } else if (downloadedBytes > 0) {
            // We have downloaded bytes but no total - show downloaded amount with speed if available
            wchar_t downloadedStr[32];
            if (downloadedBytes >= 1024 * 1024 * 1024) {
                swprintf(downloadedStr, 32, L"%.1f GB", downloadedBytes / (1024.0 * 1024.0 * 1024.0));
            } else if (downloadedBytes >= 1024 * 1024) {
                swprintf(downloadedStr, 32, L"%.1f MB", downloadedBytes / (1024.0 * 1024.0));
            } else if (downloadedBytes >= 1024) {
                swprintf(downloadedStr, 32, L"%.1f KB", downloadedBytes / 1024.0);
            } else {
                swprintf(downloadedStr, 32, L"%lld B", downloadedBytes);
            }
            
            if (progress->speed) {
                swprintf(statusMsg, 256, L"Downloaded %ls at %ls", downloadedStr, progress->speed);
            } else {
                swprintf(statusMsg, 256, L"Downloaded %ls", downloadedStr);
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
                
                // Format the duration properly
                FormatDuration(duration, durationSize);
                
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

// VideoInfoThreadData moved to ytdlp.h

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

// Configuration management functions

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

//
// Start unified download process
BOOL StartUnifiedDownload(HWND hDlg, const wchar_t* url) {
    DebugOutput(L"YouTubeCacher: StartUnifiedDownload - Entry");
    
    if (!hDlg || !url) {
        DebugOutput(L"YouTubeCacher: StartUnifiedDownload - Invalid parameters");
        return FALSE;
    }
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: StartUnifiedDownload - URL: %ls\n", url);
    OutputDebugStringW(debugMsg);
    
    // Initialize configuration
    DebugOutput(L"YouTubeCacher: StartUnifiedDownload - Initializing config");
    YtDlpConfig config = {0};
    if (!InitializeYtDlpConfig(&config)) {
        DebugOutput(L"YouTubeCacher: StartUnifiedDownload - Failed to initialize config");
        return FALSE;
    }
    DebugOutput(L"YouTubeCacher: StartUnifiedDownload - Config initialized successfully");
    
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
    DebugOutput(L"YouTubeCacher: StartUnifiedDownload - Setting up UI");
    ShowMainProgressBar(hDlg, TRUE);
    SetProgressBarMarquee(hDlg, TRUE);  // Start with marquee for indefinite "Starting..." phase
    UpdateMainProgressBar(hDlg, -1, L"Starting download...");
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
    DebugOutput(L"YouTubeCacher: StartUnifiedDownload - Worker thread started successfully");
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

// Multithreaded subprocess execution implementation
SubprocessContext* CreateSubprocessContext(const YtDlpConfig* config, const YtDlpRequest* request, 
                                          ProgressCallback progressCallback, void* callbackUserData, HWND parentWindow) {
    DebugOutput(L"YouTubeCacher: CreateSubprocessContext - ENTRY");
    
    if (!config || !request) {
        DebugOutput(L"YouTubeCacher: CreateSubprocessContext - NULL config or request, RETURNING NULL");
        return NULL;
    }
    
    DebugOutput(L"YouTubeCacher: CreateSubprocessContext - Parameters valid, proceeding");
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Config ytDlpPath: %ls", config->ytDlpPath);
    DebugOutput(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Request URL: %ls", request->url ? request->url : L"NULL");
    DebugOutput(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Request outputPath: %ls", request->outputPath ? request->outputPath : L"NULL");
    DebugOutput(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: CreateSubprocessContext - Request tempDir: %ls", request->tempDir ? request->tempDir : L"NULL");
    DebugOutput(debugMsg);
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
            CleanupThreadContext(&context->threadContext);
            free(context->config);
            free(context->request);
            free(context);
            return NULL;
        }
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - URL string duplicated successfully\n");
    }
    
    if (request->outputPath) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Duplicating outputPath string\n");
        context->request->outputPath = _wcsdup(request->outputPath);
        if (!context->request->outputPath) {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to duplicate outputPath string\n");
            if (context->request->url) free(context->request->url);
            CleanupThreadContext(&context->threadContext);
            free(context->config);
            free(context->request);
            free(context);
            return NULL;
        }
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - outputPath string duplicated successfully\n");
    }
    
    if (request->tempDir) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Duplicating tempDir string\n");
        context->request->tempDir = _wcsdup(request->tempDir);
        if (!context->request->tempDir) {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to duplicate tempDir string\n");
            if (context->request->url) free(context->request->url);
            if (context->request->outputPath) free(context->request->outputPath);
            CleanupThreadContext(&context->threadContext);
            free(context->config);
            free(context->request);
            free(context);
            return NULL;
        }
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - tempDir string duplicated successfully\n");
    }
    
    if (request->customArgs) {
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Duplicating customArgs string\n");
        context->request->customArgs = _wcsdup(request->customArgs);
        if (!context->request->customArgs) {
            OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Failed to duplicate customArgs string\n");
            if (context->request->url) free(context->request->url);
            if (context->request->outputPath) free(context->request->outputPath);
            if (context->request->tempDir) free(context->request->tempDir);
            CleanupThreadContext(&context->threadContext);
            free(context->config);
            free(context->request);
            free(context);
            return NULL;
        }
        OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - customArgs string duplicated successfully\n");
    }
    
    // Set callback information
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Setting callback information\n");
    context->progressCallback = progressCallback;
    context->callbackUserData = callbackUserData;
    context->parentWindow = parentWindow;
    
    OutputDebugStringW(L"YouTubeCacher: CreateSubprocessContext - Context created successfully, RETURNING\n");
    return context;
}

void FreeSubprocessContext(SubprocessContext* context) {
    if (!context) return;
    
    // Cleanup thread context
    CleanupThreadContext(&context->threadContext);
    
    // Free deep-copied strings
    if (context->request) {
        if (context->request->url) free(context->request->url);
        if (context->request->outputPath) free(context->request->outputPath);
        if (context->request->tempDir) free(context->request->tempDir);
        if (context->request->customArgs) free(context->request->customArgs);
        free(context->request);
    }
    
    if (context->config) {
        free(context->config);
    }
    
    // Free result if it exists
    if (context->result) {
        FreeYtDlpResult(context->result);
    }
    
    // Free accumulated output
    if (context->accumulatedOutput) {
        free(context->accumulatedOutput);
    }
    
    // Close handles
    if (context->hProcess) {
        CloseHandle(context->hProcess);
    }
    if (context->hOutputRead) {
        CloseHandle(context->hOutputRead);
    }
    if (context->hOutputWrite) {
        CloseHandle(context->hOutputWrite);
    }
    
    free(context);
}

BOOL StartSubprocessExecution(SubprocessContext* context) {
    if (!context) return FALSE;
    
    // Create and start the worker thread
    HANDLE hThread = CreateThread(NULL, 0, SubprocessWorkerThread, context, 0, NULL);
    if (!hThread) {
        return FALSE;
    }
    
    // Don't wait for completion - thread will run independently
    CloseHandle(hThread);
    return TRUE;
}

BOOL IsSubprocessRunning(const SubprocessContext* context) {
    if (!context) return FALSE;
    
    EnterCriticalSection((LPCRITICAL_SECTION)&context->threadContext.criticalSection);
    BOOL isRunning = context->threadContext.isRunning;
    LeaveCriticalSection((LPCRITICAL_SECTION)&context->threadContext.criticalSection);
    
    return isRunning;
}

BOOL CancelSubprocessExecution(SubprocessContext* context) {
    if (!context) return FALSE;
    
    return SetCancellationFlag((ThreadContext*)&context->threadContext);
}

BOOL WaitForSubprocessCompletion(SubprocessContext* context, DWORD timeoutMs) {
    if (!context) return FALSE;
    
    DWORD startTime = GetTickCount();
    
    while (!context->completed) {
        if (timeoutMs != INFINITE) {
            DWORD elapsed = GetTickCount() - startTime;
            if (elapsed >= timeoutMs) {
                return FALSE; // Timeout
            }
        }
        
        Sleep(100); // Check every 100ms
    }
    
    return TRUE;
}

// Convenience function for simple multithreaded execution with progress dialog
YtDlpResult* ExecuteYtDlpRequestMultithreaded(const YtDlpConfig* config, const YtDlpRequest* request, 
                                             HWND parentWindow, const wchar_t* operationTitle) {
    if (!config || !request) return NULL;
    
    // Create progress dialog
    ProgressDialog* progressDialog = CreateProgressDialog(parentWindow, operationTitle);
    if (!progressDialog) {
        // Fallback to synchronous execution if progress dialog fails
        return ExecuteYtDlpRequest(config, request);
    }
    
    // Create subprocess context with progress callback
    SubprocessContext* context = CreateSubprocessContext(config, request, NULL, progressDialog, parentWindow);
    if (!context) {
        DestroyProgressDialog(progressDialog);
        return ExecuteYtDlpRequest(config, request);
    }
    
    // Start execution
    if (!StartSubprocessExecution(context)) {
        FreeSubprocessContext(context);
        DestroyProgressDialog(progressDialog);
        return ExecuteYtDlpRequest(config, request);
    }
    
    // Wait for completion with progress updates
    while (!context->completed && !IsProgressDialogCancelled(progressDialog)) {
        Sleep(100);
        
        // Update progress dialog
        UpdateProgressDialog(progressDialog, -1, L"Processing...");
    }
    
    // Handle cancellation
    if (IsProgressDialogCancelled(progressDialog)) {
        CancelSubprocessExecution(context);
        
        // Wait a bit for graceful cancellation
        WaitForSubprocessCompletion(context, 5000);
    }
    
    // Get result
    YtDlpResult* result = NULL;
    if (context->result) {
        // Transfer ownership of result
        result = context->result;
        context->result = NULL;
    }
    
    // Cleanup
    FreeSubprocessContext(context);
    DestroyProgressDialog(progressDialog);
    
    return result;
}

// Worker thread function that executes yt-dlp subprocess
DWORD WINAPI SubprocessWorkerThread(LPVOID lpParam) {
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread started");
    
    SubprocessContext* context = (SubprocessContext*)lpParam;
    if (!context || !context->config || !context->request) {
        DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - invalid context");
        return 1;
    }
    
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - context valid");
    
    wchar_t debugMsg[512];
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - URL: %ls", context->request->url ? context->request->url : L"NULL");
    DebugOutput(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - OutputPath: %ls", context->request->outputPath ? context->request->outputPath : L"NULL");
    DebugOutput(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - TempDir: %ls", context->request->tempDir ? context->request->tempDir : L"NULL");
    DebugOutput(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Operation: %d", context->request->operation);
    DebugOutput(debugMsg);
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - YtDlpPath: %ls", context->config->ytDlpPath);
    DebugOutput(debugMsg);
    
    // Mark thread as running
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Marking thread as running");
    EnterCriticalSection(&context->threadContext.criticalSection);
    context->threadContext.isRunning = TRUE;
    LeaveCriticalSection(&context->threadContext.criticalSection);
    
    // Initialize result structure
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Initializing result structure");
    context->result = (YtDlpResult*)malloc(sizeof(YtDlpResult));
    if (!context->result) {
        DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Failed to allocate result structure");
        context->completed = TRUE;
        return 1;
    }
    memset(context->result, 0, sizeof(YtDlpResult));
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Result structure initialized");
    
    // Report initial progress
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Reporting initial progress");
    if (context->progressCallback) {
        context->progressCallback(0, L"Initializing yt-dlp process...", context->callbackUserData);
    }
    
    // Build command line arguments
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Building command line arguments");
    wchar_t arguments[2048];
    if (!GetYtDlpArgsForOperation(context->request->operation, context->request->url, 
                                 context->request->outputPath, context->config, arguments, 2048)) {
        DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - FAILED to build yt-dlp arguments");
        context->result->success = FALSE;
        context->result->exitCode = 1;
        context->result->errorMessage = _wcsdup(L"Failed to build yt-dlp arguments");
        context->completed = TRUE;
        return 1;
    }
    swprintf(debugMsg, 512, L"YouTubeCacher: SubprocessWorkerThread - Arguments: %ls", arguments);
    DebugOutput(debugMsg);
    
    // Check for cancellation before starting process
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Checking for cancellation");
    if (IsCancellationRequested(&context->threadContext)) {
        DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Operation was cancelled");
        context->result->success = FALSE;
        context->result->errorMessage = _wcsdup(L"Operation cancelled by user");
        context->completed = TRUE;
        return 0;
    }
    
    // Execute using the synchronous method for simplicity in this implementation
    YtDlpResult* result = ExecuteYtDlpRequest(context->config, context->request);
    if (result) {
        // Transfer result
        context->result = result;
    } else {
        context->result->success = FALSE;
        context->result->errorMessage = _wcsdup(L"Failed to execute yt-dlp request");
    }
    
    // Final progress update
    if (context->progressCallback) {
        if (context->result->success) {
            context->progressCallback(100, L"Completed successfully", context->callbackUserData);
        } else {
            context->progressCallback(100, L"Operation failed", context->callbackUserData);
        }
    }
    
    // Mark as completed
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Marking as completed");
    context->completed = TRUE;
    context->completionTime = GetTickCount();
    
    // Mark thread as no longer running
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - Marking thread as no longer running");
    EnterCriticalSection(&context->threadContext.criticalSection);
    context->threadContext.isRunning = FALSE;
    LeaveCriticalSection(&context->threadContext.criticalSection);
    
    DebugOutput(L"YouTubeCacher: SubprocessWorkerThread - EXITING");
    return 0;
}

// Unified download worker thread - simplified version that delegates to subprocess worker
DWORD WINAPI UnifiedDownloadWorkerThread(LPVOID lpParam) {
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread started\n");
    
    UnifiedDownloadContext* context = (UnifiedDownloadContext*)lpParam;
    if (!context) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Invalid context\n");
        return 1;
    }
    
    // Execute the download using the synchronous method
    YtDlpResult* result = ExecuteYtDlpRequest(&context->config, context->request);
    
    // Create completion context
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Creating download completion context\n");
    NonBlockingDownloadContext* downloadContext = (NonBlockingDownloadContext*)malloc(sizeof(NonBlockingDownloadContext));
    if (downloadContext) {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Download context allocated successfully\n");
        memcpy(&downloadContext->config, &context->config, sizeof(YtDlpConfig));
        downloadContext->request = context->request; // Transfer ownership
        downloadContext->parentWindow = context->hDialog;
        wcscpy(downloadContext->tempDir, context->tempDir);
        wcscpy(downloadContext->url, context->url);
        
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Posting WM_DOWNLOAD_COMPLETE message\n");
        PostMessageW(context->hDialog, WM_DOWNLOAD_COMPLETE, (WPARAM)result, (LPARAM)downloadContext);
    } else {
        OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Failed to allocate download context\n");
        if (result) {
            OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Freeing result due to context allocation failure\n");
            FreeYtDlpResult(result);
        }
    }
    
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - Freeing worker context\n");
    free(context);
    OutputDebugStringW(L"YouTubeCacher: UnifiedDownloadWorkerThread - EXITING with success\n");
    return 0;
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
}BOOL
 ValidateYtDlpConfiguration(const YtDlpConfig* config, ValidationInfo* validationInfo) {
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
}BOOL 
SetupDefaultYtDlpConfiguration(YtDlpConfig* config) {
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