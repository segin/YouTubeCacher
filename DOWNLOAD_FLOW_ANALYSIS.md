# Download Flow Analysis and Fixes

## Issue Summary
The yt-dlp process was losing track during downloads, appearing to freeze even though the process was still running. The log showed no output for over 2 minutes, and cancellation failed with error 5 (Access Denied).

## Root Causes Identified

### 1. Incorrect Progress Template Format
**Problem:** The yt-dlp progress template was using incorrect field names with underscores:
- `progress._downloaded_bytes` (incorrect)
- `progress._total_bytes_estimate` (incorrect)
- `progress._speed_str` (incorrect)
- `progress._eta_str` (incorrect)

**Fix:** Updated to use correct yt-dlp field names:
- `progress.downloaded_bytes`
- `progress.total_bytes_estimate`
- `progress.speed`
- `progress.eta`

**Location:** `ytdlp.c` line 901-903

### 2. Missing Timeout Enforcement
**Problem:** The `EnhancedSubprocessWorkerThread` was waiting indefinitely for the process to complete without enforcing the configured timeout (default 5 minutes).

**Fix:** Added timeout tracking and enforcement:
```c
DWORD startTime = GetTickCount();
DWORD timeoutMs = context->config->timeoutSeconds * 1000;
// ... check elapsed time in loop
if (elapsedTime > timeoutMs) {
    TerminateProcess(pi.hProcess, 1);
    UpdateDownloadState(progress, DOWNLOAD_STATE_FAILED, L"Download timed out");
    break;
}
```

**Location:** `parser.c` lines 1073-1095

### 3. No Detection of Silent Downloads
**Problem:** When yt-dlp stops sending output but continues downloading, the UI appears frozen with no indication that work is progressing.

**Fix:** Added detection and warning for extended periods without output:
```c
const DWORD NO_OUTPUT_WARNING_THRESHOLD = 30000; // 30 seconds
if (processRunning && timeSinceLastOutput > NO_OUTPUT_WARNING_THRESHOLD) {
    DebugOutput(L"No output for 30+ seconds, but process still running");
    UpdateDownloadState(progress, DOWNLOAD_STATE_DOWNLOADING, 
                       L"Download in progress (no progress info available)");
}
```

**Location:** `parser.c` lines 1097-1103

### 4. UTF-8 Logging Issues
**Problem:** Log files were not being written in UTF-8 with Windows line endings, causing encoding issues.

**Fix:** Converted all logging functions to use `WideCharToMultiByte(CP_UTF8, ...)` and `WriteFile` instead of `fwprintf`:
- `WriteToLogfile()`
- `WriteSessionStartToLogfile()`
- `WriteSessionEndToLogfile()`
- `WriteStructuredErrorToLogfile()`
- `WriteErrorContextToLogfile()`

**Location:** `log.c` throughout

## Call Flow for Download Operation

### 1. User Initiates Download
```
UI (DialogProc) 
  → StartUnifiedDownload() [ytdlp.c]
```

### 2. Download Thread Creation
```
StartUnifiedDownload()
  → Creates UnifiedDownloadContext
  → Starts UnifiedDownloadWorkerThread [ytdlp.c]
```

### 3. Worker Thread Delegation
```
UnifiedDownloadWorkerThread()
  → Creates EnhancedSubprocessContext
  → Starts EnhancedSubprocessWorkerThread [parser.c]
```

### 4. Process Execution
```
EnhancedSubprocessWorkerThread()
  → GetYtDlpArgsForOperation() [ytdlp.c]
    - Builds command with progress template:
      "--progress-template \"download:%(progress.downloaded_bytes)s|...\""
  → CreateProcessW() - launches yt-dlp
  → SetActiveDownload() - registers process for cancellation
```

### 5. Output Reading Loop
```
EnhancedSubprocessWorkerThread() main loop:
  → Check for cancellation
  → Check for timeout (NEW)
  → WaitForSingleObject(process, 100ms)
  → PeekNamedPipe() - check for available data
  → ReadFile() - read output
  → MultiByteToWideChar(CP_UTF8) - convert to wide chars
  → ProcessYtDlpOutputLine() [parser.c]
    → ParseProgressLine() - parse pipe-delimited format
    → UpdateDownloadState() - update UI state
  → Check for silent download (NEW)
```

### 6. Progress Parsing
```
ProcessYtDlpOutputLine()
  → DetectLineType() - identify line type
  → ParseProgressLine() - if progress line
    → Parse format: "download:bytes|total|speed|eta"
    → Calculate percentage
    → Update EnhancedProgressInfo
  → progressCallback() - notify UI
```

### 7. Completion
```
EnhancedSubprocessWorkerThread()
  → WaitForSingleObject(process, INFINITE)
  → GetExitCodeProcess()
  → UpdateDownloadState(COMPLETED or FAILED)
  → PostMessage(WM_DOWNLOAD_COMPLETE) to UI
```

## Expected yt-dlp Output Format

With the corrected progress template, yt-dlp should output lines like:
```
download:5562368|104857600|1290000|77
download:7340032|104857600|1290000|75
download:9117696|104857600|1290000|73
...
```

Where:
- Field 1: Downloaded bytes (numeric)
- Field 2: Total bytes estimate (numeric)
- Field 3: Speed in bytes/sec (numeric)
- Field 4: ETA in seconds (numeric)

## Testing Recommendations

1. **Test with verbose logging enabled** to see actual yt-dlp output
2. **Test with videos that have known format issues** (like the one in the log)
3. **Monitor log file** for timeout warnings and silent download detection
4. **Test cancellation** during different download phases
5. **Verify progress updates** are appearing in the UI

## Additional Notes

- The timeout is configurable via Settings (default 300 seconds / 5 minutes)
- Silent download detection triggers after 30 seconds of no output
- All log files now use UTF-8 encoding with Windows line endings (CRLF)
- Process cancellation uses graceful termination first, then forced kill if needed
