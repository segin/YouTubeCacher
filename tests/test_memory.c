#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
#include <wctype.h>

// Intercept standard functions
static int g_malloc_fail = 0;
void* mock_malloc(size_t size) {
    if (g_malloc_fail) return NULL;
    return malloc(size);
}
void* mock_realloc(void* ptr, size_t size) {
    if (g_malloc_fail) return NULL;
    return realloc(ptr, size);
}
void* mock_calloc(size_t nmemb, size_t size) {
    if (g_malloc_fail) return NULL;
    return calloc(nmemb, size);
}

#undef malloc
#define malloc mock_malloc
#undef realloc
#define realloc mock_realloc
#undef calloc
#define calloc mock_calloc

// Define guard to prevent inclusion of real YouTubeCacher.h
#define YOUTUBECACHER_H

// Mock constants from error.h
#define YTC_SEVERITY_ERROR 2
#define YTC_ERROR_MEMORY_ALLOCATION 1001

// Mock external functions and variables
typedef struct {
    int initialized;
} ErrorHandler;
ErrorHandler g_ErrorHandler = { 1 };

int g_ReportError_called = 0;
void ReportError(int severity, int code, const wchar_t* function, const wchar_t* file, int line, const wchar_t* message) {
    (void)severity; (void)code; (void)function; (void)file; (void)line; (void)message;
    g_ReportError_called++;
}

void ThreadSafeDebugOutputF(const wchar_t* format, ...) { (void)format; }
int AddRecoveryStrategy(void* handler, int code, int (*func)(const void*), const wchar_t* desc) {
    (void)handler; (void)code; (void)func; (void)desc;
    return 1;
}

// Mock missing Windows API
void GetSystemTime(void* st) { memset(st, 0, 16); } // 16 is enough for SYSTEMTIME

// Use the mock_windows.h
#include "mock_windows.h"

// Provide stubs for missing types used in memory.c but NOT defined in memory.h
// memory.h defines AutoResource, AutoYtDlpRequest, etc.
// But it might not define CacheEntry or YtDlpRequest if they are from other headers.
typedef struct { int dummy; } CacheEntry;
typedef struct { int dummy; } YtDlpRequest;

// Include memory.h
#include "../memory.h"

// Provide macros that memory.c expects but are usually in error.h
#ifndef SAFE_ALLOC
#define SAFE_ALLOC(ptr, size, label) do { (ptr) = malloc(size); if (!(ptr)) goto label; } while(0)
#endif
#ifndef SAFE_MALLOC_OR_CLEANUP
#define SAFE_MALLOC_OR_CLEANUP(ptr, size, label) do { (ptr) = SafeMalloc(size, __FILE__, __LINE__); if (!(ptr)) goto label; } while(0)
#endif
#ifndef SAFE_CALLOC
#define SAFE_CALLOC(nmemb, size) SafeCalloc(nmemb, size, __FILE__, __LINE__)
#endif
#ifndef CREATE_ALLOCATION_SET
#define CREATE_ALLOCATION_SET() CreateAllocationSet(__FILE__, __LINE__)
#endif
#ifndef SAFE_WCSDUP_AND_ADD
#define SAFE_WCSDUP_AND_ADD(set, str) SafeWcsdupAndAdd(set, str)
#endif
#ifndef SAFE_ALLOC_AND_ADD
#define SAFE_ALLOC_AND_ADD(set, size) SafeAllocAndAdd(set, size)
#endif

// Stubs for functions used in memory.c but not needed for SafeMalloc tests
static inline wchar_t* SafeWcsdupAndAdd(void* set, const wchar_t* str) { (void)set; (void)str; return NULL; }
static inline void* SafeAllocAndAdd(void* set, size_t size) { (void)set; (void)size; return NULL; }

// Forward declarations for functions defined in memory.c
BOOL InitializeMemoryPools(void);
void CleanupMemoryPools(void);
BOOL ValidateAllocationIntegrity(void* address);
void ReportMemoryError(MemoryErrorType type, void* address, size_t size, const char* file, int line, const wchar_t* description);

// Include memory.c
#include "../memory.c"

// Test cases
void test_safe_malloc_success() {
    printf("Running test_safe_malloc_success...\n");
    g_malloc_fail = 0;
    void* ptr = SafeMalloc(100, "test.c", 10);
    assert(ptr != NULL);
    SafeFree(ptr, "test.c", 11);
    printf("Passed!\n");
}

void test_safe_malloc_zero() {
    printf("Running test_safe_malloc_zero...\n");
    void* ptr = SafeMalloc(0, "test.c", 10);
    assert(ptr == NULL);
    printf("Passed!\n");
}

void test_safe_malloc_failure() {
    printf("Running test_safe_malloc_failure...\n");
    g_malloc_fail = 1;
    g_ReportError_called = 0;
    void* ptr = SafeMalloc(100, "test.c", 10);
    assert(ptr == NULL);
    assert(g_ReportError_called > 0);
    g_malloc_fail = 0;
    printf("Passed!\n");
}

void test_safe_malloc_tracking() {
    printf("Running test_safe_malloc_tracking...\n");
    if (g_memoryManager.initialized) CleanupMemoryManager();
    InitializeMemoryManager();
    EnableLeakDetection(TRUE);
    size_t initial_usage = GetCurrentMemoryUsage();
    int initial_count = GetActiveAllocationCount();
    void* ptr = SafeMalloc(100, "test.c", 10);
    assert(ptr != NULL);
    assert(GetCurrentMemoryUsage() == initial_usage + 100);
    assert(GetActiveAllocationCount() == initial_count + 1);
    SafeFree(ptr, "test.c", 20);
    assert(GetCurrentMemoryUsage() == initial_usage);
    assert(GetActiveAllocationCount() == initial_count);
    CleanupMemoryManager();
    printf("Passed!\n");
}

int main() {
    test_safe_malloc_success();
    test_safe_malloc_zero();
    test_safe_malloc_failure();
    test_safe_malloc_tracking();
    printf("All SafeMalloc tests passed!\n");
    return 0;
}
