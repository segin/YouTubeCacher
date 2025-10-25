#ifndef MEMORY_H
#define MEMORY_H

#include <windows.h>
#include <stdlib.h>

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



#endif // MEMORY_H