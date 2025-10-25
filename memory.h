#ifndef MEMORY_H
#define MEMORY_H

#include <windows.h>
#include <stdlib.h>
#include <stdarg.h>

// Memory allocation tracking structure
typedef struct {
    void* address;
    size_t size;
    const char* file;
    int line;
    DWORD threadId;
    SYSTEMTIME allocTime;
} AllocationInfo;

// Hash table entry for fast allocation lookup
typedef struct HashEntry {
    AllocationInfo* allocation;
    struct HashEntry* next;
} HashEntry;

// Hash table for allocation lookup
typedef struct {
    HashEntry** buckets;
    size_t bucketCount;
} AllocationHashTable;

// Memory manager global state
typedef struct {
    // Allocation tracking
    AllocationInfo* allocations;
    size_t allocationCount;
    size_t allocationCapacity;
    
    // Hash table for fast lookup
    AllocationHashTable hashTable;
    
    // Statistics
    size_t totalAllocated;
    size_t totalFreed;
    size_t peakUsage;
    size_t currentUsage;
    
    // Configuration
    BOOL leakDetectionEnabled;
    BOOL verboseLogging;
    
    // Synchronization
    CRITICAL_SECTION lock;
    BOOL initialized;
} MemoryManager;

// Core memory manager functions
BOOL InitializeMemoryManager(void);
void CleanupMemoryManager(void);

// Safe allocation functions with file/line tracking
void* SafeMalloc(size_t size, const char* file, int line);
void* SafeCalloc(size_t count, size_t size, const char* file, int line);
void* SafeRealloc(void* ptr, size_t size, const char* file, int line);
void SafeFree(void* ptr, const char* file, int line);

// Convenience macros for automatic file/line tracking
#define SAFE_MALLOC(size) SafeMalloc(size, __FILE__, __LINE__)
#define SAFE_CALLOC(count, size) SafeCalloc(count, size, __FILE__, __LINE__)
#define SAFE_REALLOC(ptr, size) SafeRealloc(ptr, size, __FILE__, __LINE__)
#define SAFE_FREE(ptr) SafeFree(ptr, __FILE__, __LINE__)

// Memory leak detection functions
BOOL EnableLeakDetection(BOOL enable);
void DumpMemoryLeaks(void);
size_t GetCurrentMemoryUsage(void);
int GetActiveAllocationCount(void);

// Safe string management functions
wchar_t* SafeWcsDup(const wchar_t* str, const char* file, int line);
wchar_t* SafeWcsNDup(const wchar_t* str, size_t maxLen, const char* file, int line);
wchar_t* SafeWcsConcat(const wchar_t* str1, const wchar_t* str2, const char* file, int line);
BOOL SafeWcsReplace(wchar_t** target, const wchar_t* newValue, const char* file, int line);

// Convenience macros for string functions
#define SAFE_WCSDUP(str) SafeWcsDup(str, __FILE__, __LINE__)
#define SAFE_WCSNDUP(str, len) SafeWcsNDup(str, len, __FILE__, __LINE__)
#define SAFE_WCSCONCAT(str1, str2) SafeWcsConcat(str1, str2, __FILE__, __LINE__)
#define SAFE_WCSREPLACE(target, newValue) SafeWcsReplace(target, newValue, __FILE__, __LINE__)

// StringBuilder structure for efficient string concatenation
typedef struct {
    wchar_t* buffer;
    size_t capacity;
    size_t length;
    const char* file;
    int line;
} StringBuilder;

// StringBuilder functions
StringBuilder* CreateStringBuilder(size_t initialCapacity, const char* file, int line);
BOOL AppendToStringBuilder(StringBuilder* sb, const wchar_t* str);
BOOL AppendFormatToStringBuilder(StringBuilder* sb, const wchar_t* format, ...);
wchar_t* FinalizeStringBuilder(StringBuilder* sb); // Returns ownership of buffer
void FreeStringBuilder(StringBuilder* sb);

// StringBuilder convenience macro
#define CREATE_STRING_BUILDER(capacity) CreateStringBuilder(capacity, __FILE__, __LINE__)

// RAII Resource Management Structures and Functions

// Generic RAII resource wrapper
typedef struct {
    void* resource;
    void (*cleanup)(void* resource);
    const char* file;
    int line;
} AutoResource;

// RAII cleanup function for AutoResource
void AutoResourceCleanup(AutoResource* autoRes);

// RAII macros for automatic cleanup using GCC cleanup attribute
#define AUTO_RESOURCE(cleanup_func) \
    __attribute__((cleanup(AutoResourceCleanup))) AutoResource

#define INIT_AUTO_RESOURCE(resource, cleanup_func) \
    { resource, cleanup_func, __FILE__, __LINE__ }

// Specific RAII wrappers for common types
typedef struct {
    wchar_t* str;
    const char* file;
    int line;
} AutoString;

typedef struct {
    void** array;
    size_t count;
    void (*elementCleanup)(void*);
    const char* file;
    int line;
} AutoArray;

// RAII cleanup functions for specific types
void AutoStringCleanup(AutoString* autoStr);
void AutoArrayCleanup(AutoArray* autoArray);

// RAII macros for automatic cleanup
#define AUTO_STRING __attribute__((cleanup(AutoStringCleanup))) AutoString
#define AUTO_ARRAY __attribute__((cleanup(AutoArrayCleanup))) AutoArray

// Factory functions for RAII-wrapped existing structures
typedef struct {
    void* request;
    AutoResource autoRes;
} AutoYtDlpRequest;

typedef struct {
    void* entry;
    AutoResource autoRes;
} AutoCacheEntry;

// Factory function declarations
AutoYtDlpRequest* CreateAutoYtDlpRequest(void* request);
AutoCacheEntry* CreateAutoCacheEntry(void* entry);

// Generic cleanup wrapper for SafeFree
void GenericSafeFreeCleanup(void* ptr);

// Cleanup functions for wrapped structures
void CleanupYtDlpRequest(void* request);
void CleanupCacheEntry(void* entry);

// Helper macros for initializing RAII structures
#define INIT_AUTO_STRING(str_ptr) \
    { str_ptr, __FILE__, __LINE__ }

#define INIT_AUTO_ARRAY(array_ptr, count_val, cleanup_func) \
    { array_ptr, count_val, cleanup_func, __FILE__, __LINE__ }

#endif // MEMORY_H