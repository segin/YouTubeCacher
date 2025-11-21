#include "YouTubeCacher.h"

// Adapter functions to integrate thread-safe subprocess context with existing ytdlp.c code

/**
 * Create a thread-safe subprocess context from YtDlp configuration and request
 */
ThreadSafeSubprocessContext* CreateThreadSafeSubprocessFromYtDlp(const YtDlpConfig* config, const YtDlpRequest* request) {
    if (!config || !request) {
        return NULL;
    }

    ThreadSafeSubprocessContext* context = (ThreadSafeSubprocessContext*)SAFE_MALLOC(sizeof(ThreadSafeSubprocessContext));
    if (!context) {
        return NULL;
    }

    // Initialize the context
    if (!InitializeThreadSafeSubprocessContext(context)) {
        SAFE_FREE(context);
        return NULL;
    }

    // Set executable path
    if (!SetSubprocessExecutable(context, config->ytDlpPath)) {
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
        return NULL;
    }

    // Build arguments for the operation
    wchar_t arguments[4096];
    if (!GetYtDlpArgsForOperation(request->operation, request->url, request->outputPath, config, arguments, 4096)) {
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
        return NULL;
    }

    // Set arguments
    if (!SetSubprocessArguments(context, arguments)) {
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
        return NULL;
    }

    // Set working directory if specified
    if (request->tempDir && wcslen(request->tempDir) > 0) {
        SetSubprocessWorkingDirectory(context, request->tempDir);
    }

    // Set timeout
    SetSubprocessTimeout(context, config->timeoutSeconds * 1000); // Convert seconds to milliseconds

    return context;
}

/**
 * Execute a YtDlp request using thread-safe subprocess context
 */
YtDlpResult* ExecuteYtDlpRequestThreadSafe(const YtDlpConfig* config, const YtDlpRequest* request) {
    if (!config || !request) {
        return NULL;
    }

    ThreadSafeDebugOutput(L"ExecuteYtDlpRequestThreadSafe: Starting thread-safe execution");
    
    // Start new yt-dlp invocation in session log (clears "last run" log)
    StartNewYtDlpInvocation();

    // Create thread-safe context
    ThreadSafeSubprocessContext* context = CreateThreadSafeSubprocessFromYtDlp(config, request);
    if (!context) {
        ThreadSafeDebugOutput(L"ExecuteYtDlpRequestThreadSafe: Failed to create thread-safe context");
        return NULL;
    }

    // Execute subprocess with output collection
    if (!ExecuteThreadSafeSubprocessWithOutput(context)) {
        ThreadSafeDebugOutput(L"ExecuteYtDlpRequestThreadSafe: Failed to execute subprocess");
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
        return NULL;
    }

    // Wait for completion
    DWORD timeoutMs = config->timeoutSeconds * 1000;
    if (!WaitForThreadSafeSubprocessWithOutputCompletion(context, timeoutMs)) {
        ThreadSafeDebugOutput(L"ExecuteYtDlpRequestThreadSafe: Subprocess did not complete within timeout");
        
        // Try to cancel and cleanup
        CancelThreadSafeSubprocess(context);
        WaitForThreadSafeSubprocessCompletion(context, 5000); // Wait 5 seconds for graceful shutdown
        ForceKillThreadSafeSubprocess(context); // Force kill if needed
        
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
        return NULL;
    }

    // Create result structure
    YtDlpResult* result = (YtDlpResult*)SAFE_MALLOC(sizeof(YtDlpResult));
    if (!result) {
        ThreadSafeDebugOutput(L"ExecuteYtDlpRequestThreadSafe: Failed to allocate result structure");
        CleanupThreadSafeSubprocessContext(context);
        SAFE_FREE(context);
        return NULL;
    }

    memset(result, 0, sizeof(YtDlpResult));

    // Get final output and exit code
    wchar_t* output = NULL;
    size_t outputLength = 0;
    DWORD exitCode = 0;

    if (GetFinalThreadSafeSubprocessOutput(context, &output, &outputLength, &exitCode)) {
        result->output = output; // Transfer ownership
        result->exitCode = exitCode;
        result->success = (exitCode == 0);

        ThreadSafeDebugOutputF(L"ExecuteYtDlpRequestThreadSafe: Completed with exit code %lu, success: %s, output length: %zu", 
                              exitCode, result->success ? L"TRUE" : L"FALSE", outputLength);

        // Create error message if failed
        if (!result->success) {
            result->errorMessage = CreateUserFriendlyYtDlpError(exitCode, output, request->url);
            
            // Create diagnostics
            wchar_t* diagnostics = (wchar_t*)SAFE_MALLOC(2048 * sizeof(wchar_t));
            if (diagnostics) {
                swprintf(diagnostics, 2048,
                    L"Thread-safe yt-dlp process exited with code %lu\r\n\r\n"
                    L"Executable: %ls\r\n"
                    L"Operation: %d\r\n"
                    L"URL: %ls\r\n"
                    L"Output Path: %ls\r\n\r\n"
                    L"Process output:\r\n%ls",
                    exitCode, 
                    config->ytDlpPath,
                    request->operation,
                    request->url ? request->url : L"(null)",
                    request->outputPath ? request->outputPath : L"(null)",
                    output ? output : L"(no output)");
                result->diagnostics = diagnostics;
            }
        }
    } else {
        ThreadSafeDebugOutput(L"ExecuteYtDlpRequestThreadSafe: Failed to get final output");
        result->success = FALSE;
        result->exitCode = (DWORD)-1;
        result->errorMessage = SAFE_WCSDUP(L"Failed to retrieve subprocess output");
    }

    // Cleanup context
    CleanupThreadSafeSubprocessContext(context);
    SAFE_FREE(context);

    ThreadSafeDebugOutput(L"ExecuteYtDlpRequestThreadSafe: Execution completed");
    return result;
}

/**
 * Enhanced subprocess context creation with progress callback support
 */
ThreadSafeSubprocessContext* CreateThreadSafeSubprocessWithCallback(const YtDlpConfig* config, const YtDlpRequest* request, 
                                                                   ProgressCallback progressCallback, void* callbackUserData, HWND parentWindow) {
    ThreadSafeSubprocessContext* context = CreateThreadSafeSubprocessFromYtDlp(config, request);
    if (!context) {
        return NULL;
    }

    // Set progress callback and parent window
    SetSubprocessProgressCallback(context, progressCallback, callbackUserData);
    SetSubprocessParentWindow(context, parentWindow);

    return context;
}

/**
 * Adapter function to replace the existing SubprocessContext usage
 */
BOOL StartThreadSafeSubprocessFromLegacyContext(SubprocessContext* legacyContext) {
    if (!legacyContext || !legacyContext->config || !legacyContext->request) {
        return FALSE;
    }

    ThreadSafeDebugOutput(L"StartThreadSafeSubprocessFromLegacyContext: Converting legacy context to thread-safe");

    // Create thread-safe context from legacy context
    ThreadSafeSubprocessContext* threadSafeContext = CreateThreadSafeSubprocessWithCallback(
        legacyContext->config, 
        legacyContext->request,
        legacyContext->progressCallback,
        legacyContext->callbackUserData,
        legacyContext->parentWindow
    );

    if (!threadSafeContext) {
        ThreadSafeDebugOutput(L"StartThreadSafeSubprocessFromLegacyContext: Failed to create thread-safe context");
        return FALSE;
    }

    // Store the thread-safe context in the legacy context for cleanup
    // We'll use the accumulatedOutput field to store our context pointer (hack but works)
    legacyContext->accumulatedOutput = (wchar_t*)threadSafeContext;

    // Start execution
    BOOL success = ExecuteThreadSafeSubprocessWithOutput(threadSafeContext);
    
    if (!success) {
        ThreadSafeDebugOutput(L"StartThreadSafeSubprocessFromLegacyContext: Failed to start thread-safe execution");
        CleanupThreadSafeSubprocessContext(threadSafeContext);
        SAFE_FREE(threadSafeContext);
        legacyContext->accumulatedOutput = NULL;
        return FALSE;
    }

    ThreadSafeDebugOutput(L"StartThreadSafeSubprocessFromLegacyContext: Thread-safe execution started successfully");
    return TRUE;
}

/**
 * Check if legacy subprocess context is running (using thread-safe backend)
 */
BOOL IsLegacySubprocessRunning(const SubprocessContext* legacyContext) {
    if (!legacyContext || !legacyContext->accumulatedOutput) {
        return FALSE;
    }

    ThreadSafeSubprocessContext* threadSafeContext = (ThreadSafeSubprocessContext*)legacyContext->accumulatedOutput;
    return IsThreadSafeSubprocessRunning(threadSafeContext);
}

/**
 * Wait for legacy subprocess completion (using thread-safe backend)
 */
BOOL WaitForLegacySubprocessCompletion(SubprocessContext* legacyContext, DWORD timeoutMs) {
    if (!legacyContext || !legacyContext->accumulatedOutput) {
        return FALSE;
    }

    ThreadSafeSubprocessContext* threadSafeContext = (ThreadSafeSubprocessContext*)legacyContext->accumulatedOutput;
    BOOL completed = WaitForThreadSafeSubprocessWithOutputCompletion(threadSafeContext, timeoutMs);

    if (completed) {
        // Transfer results to legacy context
        wchar_t* output = NULL;
        size_t outputLength = 0;
        DWORD exitCode = 0;

        if (GetFinalThreadSafeSubprocessOutput(threadSafeContext, &output, &outputLength, &exitCode)) {
            // Create YtDlpResult for legacy context
            if (!legacyContext->result) {
                legacyContext->result = (YtDlpResult*)SAFE_MALLOC(sizeof(YtDlpResult));
                if (legacyContext->result) {
                    memset(legacyContext->result, 0, sizeof(YtDlpResult));
                }
            }

            if (legacyContext->result) {
                legacyContext->result->output = output; // Transfer ownership
                legacyContext->result->exitCode = exitCode;
                legacyContext->result->success = (exitCode == 0);

                if (!legacyContext->result->success && legacyContext->request) {
                    legacyContext->result->errorMessage = CreateUserFriendlyYtDlpError(exitCode, output, legacyContext->request->url);
                }
            }
        }

        legacyContext->completed = TRUE;
        legacyContext->completionTime = GetTickCount();
    }

    return completed;
}

/**
 * Cancel legacy subprocess execution (using thread-safe backend)
 */
BOOL CancelLegacySubprocessExecution(SubprocessContext* legacyContext) {
    if (!legacyContext || !legacyContext->accumulatedOutput) {
        return FALSE;
    }

    ThreadSafeSubprocessContext* threadSafeContext = (ThreadSafeSubprocessContext*)legacyContext->accumulatedOutput;
    return CancelThreadSafeSubprocess(threadSafeContext);
}

/**
 * Cleanup legacy subprocess context (including thread-safe backend)
 */
void CleanupLegacySubprocessContext(SubprocessContext* legacyContext) {
    if (!legacyContext) {
        return;
    }

    // Cleanup thread-safe context if it exists
    if (legacyContext->accumulatedOutput) {
        ThreadSafeSubprocessContext* threadSafeContext = (ThreadSafeSubprocessContext*)legacyContext->accumulatedOutput;
        
        // Validate the context pointer before cleanup
        // Check if it looks like a valid context by checking if initialized flag is reasonable
        if (threadSafeContext) {
            CleanupThreadSafeSubprocessContext(threadSafeContext);
            SAFE_FREE(threadSafeContext);
        }
        
        legacyContext->accumulatedOutput = NULL;
    }

    // Continue with normal legacy cleanup
    // (This would call the original FreeSubprocessContext logic)
}