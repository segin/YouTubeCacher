#ifndef YTDLP_H
#define YTDLP_H

#include "appstate.h"
#include "threading.h"

// Custom message for download completion
#define WM_DOWNLOAD_COMPLETE (WM_USER + 102)

// YtDlp configuration and validation functions
BOOL InitializeYtDlpConfig(YtDlpConfig* config);
void CleanupYtDlpConfig(YtDlpConfig* config);
BOOL ValidateYtDlpExecutable(const wchar_t* path);
BOOL ValidateYtDlpComprehensive(const wchar_t* path, ValidationInfo* info);
void FreeValidationInfo(ValidationInfo* info);

// YtDlp operations
BOOL GetVideoTitleAndDurationSync(const wchar_t* url, wchar_t* title, size_t titleSize, wchar_t* duration, size_t durationSize);
BOOL GetVideoTitleAndDuration(HWND hDlg, const wchar_t* url, wchar_t* title, size_t titleSize, wchar_t* duration, size_t durationSize);
BOOL StartUnifiedDownload(HWND hDlg, const wchar_t* url);
BOOL StartNonBlockingDownload(YtDlpConfig* config, YtDlpRequest* request, HWND parentWindow);

// Process management
BOOL StartSubprocessExecution(SubprocessContext* context);
BOOL IsSubprocessRunning(const SubprocessContext* context);
BOOL CancelSubprocessExecution(SubprocessContext* context);
BOOL WaitForSubprocessCompletion(SubprocessContext* context, DWORD timeoutMs);
void FreeSubprocessContext(SubprocessContext* context);

// Temporary directory management
BOOL CreateYtDlpTempDirWithFallback(wchar_t* tempPath, size_t pathSize);
BOOL CleanupTempDirectory(const wchar_t* tempDir);

// Installation and setup
void InstallYtDlpWithWinget(HWND hParent);

// Request/Result management
YtDlpRequest* CreateYtDlpRequest(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath);
void FreeYtDlpRequest(YtDlpRequest* request);
YtDlpResult* ExecuteYtDlpRequest(const YtDlpConfig* config, const YtDlpRequest* request);
void FreeYtDlpResult(YtDlpResult* result);

// Command line argument construction
BOOL GetYtDlpArgsForOperation(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath, 
                             const YtDlpConfig* config, wchar_t* args, size_t argsSize);
BOOL ValidateYtDlpArguments(const wchar_t* args);
BOOL SanitizeYtDlpArguments(wchar_t* args, size_t argsSize);

// Context management
SubprocessContext* CreateSubprocessContext(const YtDlpConfig* config, const YtDlpRequest* request, 
                                          ProgressCallback progressCallback, void* callbackUserData, HWND parentWindow);

// Temporary directory functions
BOOL CreateTempDirectory(const YtDlpConfig* config, wchar_t* tempDir, size_t tempDirSize);

// Progress dialog functions
ProgressDialog* CreateProgressDialog(HWND parent, const wchar_t* title);
void UpdateProgressDialog(ProgressDialog* dialog, int progress, const wchar_t* status);
BOOL IsProgressDialogCancelled(const ProgressDialog* dialog);
void DestroyProgressDialog(ProgressDialog* dialog);

// Error analysis functions
ErrorAnalysis* AnalyzeYtDlpError(const YtDlpResult* result);
void FreeErrorAnalysis(ErrorAnalysis* analysis);

// Video metadata functions
BOOL GetVideoMetadata(const wchar_t* url, VideoMetadata* metadata);
BOOL ParseVideoMetadataFromJson(const wchar_t* jsonOutput, VideoMetadata* metadata);
void FreeVideoMetadata(VideoMetadata* metadata);

// Worker thread functions
DWORD WINAPI SubprocessWorkerThread(LPVOID lpParam);
DWORD WINAPI UnifiedDownloadWorkerThread(LPVOID lpParam);
DWORD WINAPI NonBlockingDownloadThread(LPVOID lpParam);
DWORD WINAPI GetVideoInfoThread(LPVOID lpParam);
DWORD WINAPI VideoInfoWorkerThread(LPVOID lpParam);

// Multithreaded execution
YtDlpResult* ExecuteYtDlpRequestMultithreaded(const YtDlpConfig* config, const YtDlpRequest* request, 
                                             HWND parentWindow, const wchar_t* operationTitle);

#endif // YTDLP_H