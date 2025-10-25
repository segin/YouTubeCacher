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

// Memory Pool System for Frequent Allocations

// Memory pool structure for efficient allocation of fixed-size objects
typedef struct MemoryPool {
    void* memory;               // Pre-allocated memory block
    void** freeList;            // Stack of free objects (array of pointers)
    size_t objectSize;          // Size of each object in bytes
    size_t totalObjects;        // Total objects in pool
    size_t freeCount;           // Number of free objects available
    size_t allocatedCount;      // Number of currently allocated objects
    CRITICAL_SECTION lock;      // Thread safety
    const char* poolName;       // Name for debugging
} MemoryPool;

// Memory pool management functions
MemoryPool* CreateMemoryPool(size_t objectSize, size_t initialCount, const char* poolName);
void* AllocateFromPool(MemoryPool* pool);
void ReturnToPool(MemoryPool* pool, void* object);
void DestroyMemoryPool(MemoryPool* pool);

// Global memory pools for common objects
extern MemoryPool* g_stringPool;        // For small strings (< 256 chars)
extern MemoryPool* g_cacheEntryPool;    // For CacheEntry structures
extern MemoryPool* g_requestPool;       // For YtDlpRequest structures

// Pool initialization and cleanup functions
BOOL InitializeMemoryPools(void);
void CleanupMemoryPools(void);

// Pool allocation macros for convenience
#define POOL_ALLOC_STRING() AllocateFromPool(g_stringPool)
#define POOL_FREE_STRING(ptr) ReturnToPool(g_stringPool, ptr)
#define POOL_ALLOC_CACHE_ENTRY() AllocateFromPool(g_cacheEntryPool)
#define POOL_FREE_CACHE_ENTRY(ptr) ReturnToPool(g_cacheEntryPool, ptr)
#define POOL_ALLOC_REQUEST() AllocateFromPool(g_requestPool)
#define POOL_FREE_REQUEST(ptr) ReturnToPool(g_requestPool, ptr)

// Pool statistics and monitoring
typedef struct {
    size_t totalPools;
    size_t totalAllocatedObjects;
    size_t totalFreeObjects;
    size_t totalMemoryUsed;
    size_t totalMemoryAllocated;
} PoolStatistics;

PoolStatistics GetPoolStatistics(void);
void DumpPoolStatistics(void);

// Test function for memory pool functionality
BOOL TestMemoryPools(void);

// Test function for error-safe allocation patterns
BOOL TestErrorSafeAllocationPatterns(void);

// Error-Safe Allocation Patterns

// Multi-allocation transaction structure for atomic allocation operations
typedef struct {
    void** allocations;         // Array of allocated pointers
    size_t count;              // Number of allocations in the set
    size_t capacity;           // Capacity of the allocations array
    const char* file;          // File where the set was created
    int line;                  // Line where the set was created
} AllocationSet;

// Bulk cleanup structure for efficient bulk deallocation
typedef struct {
    void** pointers;           // Array of pointers to free
    size_t count;             // Number of pointers in the cleanup set
    size_t capacity;          // Capacity of the pointers array
} BulkCleanup;

// AllocationSet functions for transaction-based allocation
AllocationSet* CreateAllocationSet(const char* file, int line);
BOOL AddToAllocationSet(AllocationSet* set, void* ptr);
void CommitAllocationSet(AllocationSet* set);  // Transfers ownership, clears set
void RollbackAllocationSet(AllocationSet* set); // Frees all allocations, clears set
void FreeAllocationSet(AllocationSet* set);    // Frees the set structure itself

// BulkCleanup functions for efficient bulk operations
BulkCleanup* CreateBulkCleanup(size_t initialCapacity);
BOOL AddToBulkCleanup(BulkCleanup* cleanup, void* ptr);
void BulkFree(BulkCleanup* cleanup);           // Frees all pointers and clears set
void FreeBulkCleanup(BulkCleanup* cleanup);    // Frees the cleanup structure itself

// Convenience macros for error-safe allocation patterns
#define CREATE_ALLOCATION_SET() CreateAllocationSet(__FILE__, __LINE__)

// Helper macros for common allocation patterns
#define SAFE_ALLOC_AND_ADD(set, size) \
    ({ void* _ptr = SAFE_MALLOC(size); \
       if (_ptr && !AddToAllocationSet(set, _ptr)) { \
           SAFE_FREE(_ptr); _ptr = NULL; \
       } \
       _ptr; })

#define SAFE_CALLOC_AND_ADD(set, count, size) \
    ({ void* _ptr = SAFE_CALLOC(count, size); \
       if (_ptr && !AddToAllocationSet(set, _ptr)) { \
           SAFE_FREE(_ptr); _ptr = NULL; \
       } \
       _ptr; })

#define SAFE_WCSDUP_AND_ADD(set, str) \
    ({ wchar_t* _ptr = SAFE_WCSDUP(str); \
       if (_ptr && !AddToAllocationSet(set, _ptr)) { \
           SAFE_FREE(_ptr); _ptr = NULL; \
       } \
       _ptr; })

#endif // MEMORY_H