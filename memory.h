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

#endif // MEMORY_H