#include "YouTubeCacher.h"

// Global memory manager instance
static MemoryManager g_memoryManager = {0};

// Internal helper function declarations
static BOOL AddAllocationRecord(void* address, size_t size, const char* file, int line);
static void ExpandAllocationTable(void);
static BOOL RemoveAllocationRecord(void* address, size_t* outSize);


// Hash table function declarations
static BOOL InitializeHashTable(AllocationHashTable* table, size_t bucketCount);
static void CleanupHashTable(AllocationHashTable* table);
static size_t HashAddress(void* address, size_t bucketCount);
static BOOL AddToHashTable(AllocationHashTable* table, AllocationInfo* allocation);
static AllocationInfo* FindInHashTable(AllocationHashTable* table, void* address);
static BOOL RemoveFromHashTable(AllocationHashTable* table, void* address);

// Initial allocation table size
#define INITIAL_ALLOCATION_TABLE_SIZE 1024
#define ALLOCATION_TABLE_GROWTH_FACTOR 2

// Hash table configuration
#define INITIAL_HASH_TABLE_SIZE 1024

BOOL InitializeMemoryManager(void)
{
    if (g_memoryManager.initialized) {
        return TRUE; // Already initialized
    }

    // Initialize critical section for thread safety
    InitializeCriticalSection(&g_memoryManager.lock);

    // Allocate initial allocation tracking table
    g_memoryManager.allocations = (AllocationInfo*)malloc(
        INITIAL_ALLOCATION_TABLE_SIZE * sizeof(AllocationInfo));
    
    if (!g_memoryManager.allocations) {
        DeleteCriticalSection(&g_memoryManager.lock);
        return FALSE;
    }

    // Initialize hash table for fast lookup
    if (!InitializeHashTable(&g_memoryManager.hashTable, INITIAL_HASH_TABLE_SIZE)) {
        free(g_memoryManager.allocations);
        DeleteCriticalSection(&g_memoryManager.lock);
        return FALSE;
    }

    // Initialize memory manager state
    g_memoryManager.allocationCapacity = INITIAL_ALLOCATION_TABLE_SIZE;
    g_memoryManager.allocationCount = 0;
    g_memoryManager.totalAllocated = 0;
    g_memoryManager.totalFreed = 0;
    g_memoryManager.peakUsage = 0;
    g_memoryManager.currentUsage = 0;
    g_memoryManager.leakDetectionEnabled = TRUE;
    g_memoryManager.verboseLogging = FALSE;
    g_memoryManager.initialized = TRUE;

    // Initialize memory pools
    if (!InitializeMemoryPools()) {
        // Cleanup on failure
        CleanupHashTable(&g_memoryManager.hashTable);
        free(g_memoryManager.allocations);
        DeleteCriticalSection(&g_memoryManager.lock);
        g_memoryManager.initialized = FALSE;
        return FALSE;
    }

    return TRUE;
}

void CleanupMemoryManager(void)
{
    if (!g_memoryManager.initialized) {
        return;
    }

    EnterCriticalSection(&g_memoryManager.lock);

    // Dump any remaining leaks before cleanup
    if (g_memoryManager.leakDetectionEnabled && g_memoryManager.allocationCount > 0) {
        DumpMemoryLeaks();
    }

    // Cleanup memory pools first
    CleanupMemoryPools();

    // Cleanup hash table
    CleanupHashTable(&g_memoryManager.hashTable);

    // Free the allocation tracking table
    if (g_memoryManager.allocations) {
        free(g_memoryManager.allocations);
        g_memoryManager.allocations = NULL;
    }

    g_memoryManager.initialized = FALSE;
    
    LeaveCriticalSection(&g_memoryManager.lock);
    DeleteCriticalSection(&g_memoryManager.lock);
}

void* SafeMalloc(size_t size, const char* file, int line)
{
    if (size == 0) {
        return NULL;
    }

    void* ptr = malloc(size);
    if (!ptr) {
        return NULL;
    }

    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        
        AddAllocationRecord(ptr, size, file, line);
        g_memoryManager.totalAllocated += size;
        g_memoryManager.currentUsage += size;
        
        if (g_memoryManager.currentUsage > g_memoryManager.peakUsage) {
            g_memoryManager.peakUsage = g_memoryManager.currentUsage;
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }

    return ptr;
}

void* SafeCalloc(size_t count, size_t size, const char* file, int line)
{
    if (count == 0 || size == 0) {
        return NULL;
    }

    // Check for overflow
    if (count > SIZE_MAX / size) {
        return NULL;
    }

    size_t totalSize = count * size;
    void* ptr = calloc(count, size);
    if (!ptr) {
        return NULL;
    }

    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        
        AddAllocationRecord(ptr, totalSize, file, line);
        g_memoryManager.totalAllocated += totalSize;
        g_memoryManager.currentUsage += totalSize;
        
        if (g_memoryManager.currentUsage > g_memoryManager.peakUsage) {
            g_memoryManager.peakUsage = g_memoryManager.currentUsage;
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }

    return ptr;
}

void* SafeRealloc(void* ptr, size_t size, const char* file, int line)
{
    if (size == 0) {
        SafeFree(ptr, file, line);
        return NULL;
    }

    if (!ptr) {
        return SafeMalloc(size, file, line);
    }

    size_t oldSize = 0;
    void* oldPtr = ptr;
    
    // Get old size and remove record before realloc if tracking is enabled
    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        
        // Use hash table for fast lookup and removal
        if (!RemoveAllocationRecord(ptr, &oldSize)) {
            // Allocation not found - this might be an error
            oldSize = 0;
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }

    void* newPtr = realloc(oldPtr, size);
    if (!newPtr) {
        // Realloc failed, need to restore the old allocation record
        if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled && oldSize > 0) {
            EnterCriticalSection(&g_memoryManager.lock);
            AddAllocationRecord(oldPtr, oldSize, file, line);
            LeaveCriticalSection(&g_memoryManager.lock);
        }
        return NULL;
    }

    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        
        // Add new allocation record
        AddAllocationRecord(newPtr, size, file, line);
        
        // Update statistics
        g_memoryManager.totalAllocated += size;
        g_memoryManager.currentUsage = g_memoryManager.currentUsage - oldSize + size;
        
        if (g_memoryManager.currentUsage > g_memoryManager.peakUsage) {
            g_memoryManager.peakUsage = g_memoryManager.currentUsage;
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }

    return newPtr;
}

void SafeFree(void* ptr, const char* file, int line)
{
    // Suppress unused parameter warnings in release builds
    (void)file;
    (void)line;
    
    if (!ptr) {
        return; // Safe to free NULL pointer
    }

    size_t freedSize = 0;
    
    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        
        // Use hash table for fast lookup and removal
        if (RemoveAllocationRecord(ptr, &freedSize)) {
            g_memoryManager.totalFreed += freedSize;
            g_memoryManager.currentUsage -= freedSize;
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }

    free(ptr);
}

BOOL EnableLeakDetection(BOOL enable)
{
    if (!g_memoryManager.initialized) {
        return FALSE;
    }

    EnterCriticalSection(&g_memoryManager.lock);
    g_memoryManager.leakDetectionEnabled = enable;
    LeaveCriticalSection(&g_memoryManager.lock);
    
    return TRUE;
}

void DumpMemoryLeaks(void)
{
    if (!g_memoryManager.initialized || !g_memoryManager.leakDetectionEnabled) {
        return;
    }

    EnterCriticalSection(&g_memoryManager.lock);
    
    if (g_memoryManager.allocationCount > 0) {
        printf("=== MEMORY LEAKS DETECTED ===\n");
        printf("Total leaked allocations: %zu\n", g_memoryManager.allocationCount);
        
        size_t totalLeakedBytes = 0;
        for (size_t i = 0; i < g_memoryManager.allocationCount; i++) {
            AllocationInfo* alloc = &g_memoryManager.allocations[i];
            printf("Leak #%zu: %zu bytes at %p\n", i + 1, alloc->size, alloc->address);
            printf("  Allocated at: %s:%d\n", alloc->file ? alloc->file : "unknown", alloc->line);
            printf("  Thread ID: %u\n", (unsigned int)alloc->threadId);
            printf("  Time: %02d:%02d:%02d.%03d\n", 
                   alloc->allocTime.wHour, alloc->allocTime.wMinute, 
                   alloc->allocTime.wSecond, alloc->allocTime.wMilliseconds);
            totalLeakedBytes += alloc->size;
        }
        
        printf("Total leaked bytes: %zu\n", totalLeakedBytes);
        printf("=============================\n");
    } else {
        printf("No memory leaks detected.\n");
    }
    
    LeaveCriticalSection(&g_memoryManager.lock);
}

size_t GetCurrentMemoryUsage(void)
{
    if (!g_memoryManager.initialized) {
        return 0;
    }

    EnterCriticalSection(&g_memoryManager.lock);
    size_t usage = g_memoryManager.currentUsage;
    LeaveCriticalSection(&g_memoryManager.lock);
    
    return usage;
}

int GetActiveAllocationCount(void)
{
    if (!g_memoryManager.initialized) {
        return 0;
    }

    EnterCriticalSection(&g_memoryManager.lock);
    int count = (int)g_memoryManager.allocationCount;
    LeaveCriticalSection(&g_memoryManager.lock);
    
    return count;
}

// Internal helper functions
static BOOL AddAllocationRecord(void* address, size_t size, const char* file, int line)
{
    // Expand table if needed
    if (g_memoryManager.allocationCount >= g_memoryManager.allocationCapacity) {
        ExpandAllocationTable();
    }

    if (g_memoryManager.allocationCount >= g_memoryManager.allocationCapacity) {
        return FALSE; // Expansion failed
    }

    AllocationInfo* alloc = &g_memoryManager.allocations[g_memoryManager.allocationCount];
    alloc->address = address;
    alloc->size = size;
    alloc->file = file;
    alloc->line = line;
    alloc->threadId = GetCurrentThreadId();
    GetSystemTime(&alloc->allocTime);
    
    // Add to hash table for fast lookup
    if (!AddToHashTable(&g_memoryManager.hashTable, alloc)) {
        return FALSE; // Hash table insertion failed
    }
    
    g_memoryManager.allocationCount++;
    return TRUE;
}

static BOOL RemoveAllocationRecord(void* address, size_t* outSize)
{
    // Find allocation using hash table
    AllocationInfo* alloc = FindInHashTable(&g_memoryManager.hashTable, address);
    if (!alloc) {
        if (outSize) *outSize = 0;
        return FALSE;
    }

    // Store size for caller
    if (outSize) {
        *outSize = alloc->size;
    }

    // Remove from hash table
    if (!RemoveFromHashTable(&g_memoryManager.hashTable, address)) {
        return FALSE;
    }

    // Find in linear array and remove by swapping with last element
    for (size_t i = 0; i < g_memoryManager.allocationCount; i++) {
        if (g_memoryManager.allocations[i].address == address) {
            // Move last element to this position
            g_memoryManager.allocations[i] = 
                g_memoryManager.allocations[g_memoryManager.allocationCount - 1];
            g_memoryManager.allocationCount--;
            return TRUE;
        }
    }

    return FALSE; // Should not happen if hash table is consistent
}





static void ExpandAllocationTable(void)
{
    size_t newCapacity = g_memoryManager.allocationCapacity * ALLOCATION_TABLE_GROWTH_FACTOR;
    AllocationInfo* newTable = (AllocationInfo*)realloc(
        g_memoryManager.allocations, 
        newCapacity * sizeof(AllocationInfo));
    
    if (newTable) {
        g_memoryManager.allocations = newTable;
        g_memoryManager.allocationCapacity = newCapacity;
    }
}

// Hash table implementation functions
static BOOL InitializeHashTable(AllocationHashTable* table, size_t bucketCount)
{
    if (!table || bucketCount == 0) {
        return FALSE;
    }

    table->buckets = (HashEntry**)calloc(bucketCount, sizeof(HashEntry*));
    if (!table->buckets) {
        return FALSE;
    }

    table->bucketCount = bucketCount;
    return TRUE;
}

static void CleanupHashTable(AllocationHashTable* table)
{
    if (!table || !table->buckets) {
        return;
    }

    // Free all hash entries
    for (size_t i = 0; i < table->bucketCount; i++) {
        HashEntry* entry = table->buckets[i];
        while (entry) {
            HashEntry* next = entry->next;
            free(entry);
            entry = next;
        }
    }

    free(table->buckets);
    table->buckets = NULL;
    table->bucketCount = 0;
}

static size_t HashAddress(void* address, size_t bucketCount)
{
    // Simple hash function for pointer addresses
    uintptr_t addr = (uintptr_t)address;
    
    // Use multiplication method with golden ratio approximation
    // This helps distribute addresses more evenly across buckets
    const uintptr_t A = 2654435769UL; // 2^32 / golden ratio
    return ((addr * A) >> (32 - 10)) % bucketCount; // Use top 10 bits for hash
}

static BOOL AddToHashTable(AllocationHashTable* table, AllocationInfo* allocation)
{
    if (!table || !table->buckets || !allocation) {
        return FALSE;
    }

    size_t bucket = HashAddress(allocation->address, table->bucketCount);
    
    // Create new hash entry
    HashEntry* entry = (HashEntry*)malloc(sizeof(HashEntry));
    if (!entry) {
        return FALSE;
    }

    entry->allocation = allocation;
    entry->next = table->buckets[bucket];
    table->buckets[bucket] = entry;

    return TRUE;
}

static AllocationInfo* FindInHashTable(AllocationHashTable* table, void* address)
{
    if (!table || !table->buckets || !address) {
        return NULL;
    }

    size_t bucket = HashAddress(address, table->bucketCount);
    HashEntry* entry = table->buckets[bucket];

    while (entry) {
        if (entry->allocation && entry->allocation->address == address) {
            return entry->allocation;
        }
        entry = entry->next;
    }

    return NULL;
}

static BOOL RemoveFromHashTable(AllocationHashTable* table, void* address)
{
    if (!table || !table->buckets || !address) {
        return FALSE;
    }

    size_t bucket = HashAddress(address, table->bucketCount);
    HashEntry* entry = table->buckets[bucket];
    HashEntry* prev = NULL;

    while (entry) {
        if (entry->allocation && entry->allocation->address == address) {
            // Remove entry from linked list
            if (prev) {
                prev->next = entry->next;
            } else {
                table->buckets[bucket] = entry->next;
            }
            
            free(entry);
            return TRUE;
        }
        prev = entry;
        entry = entry->next;
    }

    return FALSE;
}

// Safe string management function implementations
wchar_t* SafeWcsDup(const wchar_t* str, const char* file, int line)
{
    if (!str) {
        return NULL;
    }

    size_t len = wcslen(str);
    size_t size = (len + 1) * sizeof(wchar_t);
    
    wchar_t* duplicate = (wchar_t*)SafeMalloc(size, file, line);
    if (!duplicate) {
        return NULL;
    }

    wcscpy(duplicate, str);
    return duplicate;
}

wchar_t* SafeWcsNDup(const wchar_t* str, size_t maxLen, const char* file, int line)
{
    if (!str) {
        return NULL;
    }

    // Find the actual length, limited by maxLen
    size_t len = 0;
    while (len < maxLen && str[len] != L'\0') {
        len++;
    }

    size_t size = (len + 1) * sizeof(wchar_t);
    
    wchar_t* duplicate = (wchar_t*)SafeMalloc(size, file, line);
    if (!duplicate) {
        return NULL;
    }

    wcsncpy(duplicate, str, len);
    duplicate[len] = L'\0'; // Ensure null termination
    return duplicate;
}

wchar_t* SafeWcsConcat(const wchar_t* str1, const wchar_t* str2, const char* file, int line)
{
    if (!str1 && !str2) {
        return NULL;
    }

    size_t len1 = str1 ? wcslen(str1) : 0;
    size_t len2 = str2 ? wcslen(str2) : 0;
    size_t totalLen = len1 + len2;
    size_t size = (totalLen + 1) * sizeof(wchar_t);

    wchar_t* result = (wchar_t*)SafeMalloc(size, file, line);
    if (!result) {
        return NULL;
    }

    result[0] = L'\0'; // Initialize as empty string

    if (str1) {
        wcscpy(result, str1);
    }
    if (str2) {
        wcscat(result, str2);
    }

    return result;
}

BOOL SafeWcsReplace(wchar_t** target, const wchar_t* newValue, const char* file, int line)
{
    if (!target) {
        return FALSE;
    }

    // Free the old string if it exists
    if (*target) {
        SafeFree(*target, file, line);
        *target = NULL;
    }

    // Duplicate the new value if provided
    if (newValue) {
        *target = SafeWcsDup(newValue, file, line);
        return (*target != NULL);
    }

    return TRUE; // Successfully set to NULL
}

// StringBuilder implementation
StringBuilder* CreateStringBuilder(size_t initialCapacity, const char* file, int line)
{
    if (initialCapacity == 0) {
        initialCapacity = 256; // Default initial capacity
    }

    StringBuilder* sb = (StringBuilder*)SafeMalloc(sizeof(StringBuilder), file, line);
    if (!sb) {
        return NULL;
    }

    sb->buffer = (wchar_t*)SafeMalloc(initialCapacity * sizeof(wchar_t), file, line);
    if (!sb->buffer) {
        SafeFree(sb, file, line);
        return NULL;
    }

    sb->capacity = initialCapacity;
    sb->length = 0;
    sb->buffer[0] = L'\0';
    sb->file = file;
    sb->line = line;

    return sb;
}

BOOL AppendToStringBuilder(StringBuilder* sb, const wchar_t* str)
{
    if (!sb || !str) {
        return FALSE;
    }

    size_t strLen = wcslen(str);
    size_t newLength = sb->length + strLen;

    // Expand buffer if necessary
    if (newLength >= sb->capacity) {
        size_t newCapacity = sb->capacity;
        while (newCapacity <= newLength) {
            newCapacity *= 2;
        }

        wchar_t* newBuffer = (wchar_t*)SafeRealloc(sb->buffer, 
            newCapacity * sizeof(wchar_t), sb->file, sb->line);
        if (!newBuffer) {
            return FALSE;
        }

        sb->buffer = newBuffer;
        sb->capacity = newCapacity;
    }

    // Append the string
    wcscat(sb->buffer, str);
    sb->length = newLength;

    return TRUE;
}

BOOL AppendFormatToStringBuilder(StringBuilder* sb, const wchar_t* format, ...)
{
    if (!sb || !format) {
        return FALSE;
    }

    va_list args;
    va_start(args, format);

    // Use a reasonable initial buffer size for formatting
    size_t formatBufferSize = 1024;
    wchar_t* formatBuffer = (wchar_t*)SafeMalloc(formatBufferSize * sizeof(wchar_t), sb->file, sb->line);
    if (!formatBuffer) {
        va_end(args);
        return FALSE;
    }

    // Try to format the string
    int written = vswprintf(formatBuffer, formatBufferSize, format, args);
    va_end(args);

    if (written < 0) {
        SafeFree(formatBuffer, sb->file, sb->line);
        return FALSE;
    }

    // Append the formatted string to the StringBuilder
    BOOL result = AppendToStringBuilder(sb, formatBuffer);
    
    SafeFree(formatBuffer, sb->file, sb->line);
    return result;
}

wchar_t* FinalizeStringBuilder(StringBuilder* sb)
{
    if (!sb) {
        return NULL;
    }

    wchar_t* result = sb->buffer;
    sb->buffer = NULL; // Transfer ownership
    sb->capacity = 0;
    sb->length = 0;

    return result;
}

void FreeStringBuilder(StringBuilder* sb)
{
    if (!sb) {
        return;
    }

    if (sb->buffer) {
        SafeFree(sb->buffer, sb->file, sb->line);
    }

    SafeFree(sb, sb->file, sb->line);
}

// RAII Resource Management Implementation

void AutoResourceCleanup(AutoResource* autoRes)
{
    if (!autoRes) {
        return;
    }

    if (autoRes->resource && autoRes->cleanup) {
        autoRes->cleanup(autoRes->resource);
        autoRes->resource = NULL;
    }
}

void AutoStringCleanup(AutoString* autoStr)
{
    if (!autoStr) {
        return;
    }

    if (autoStr->str) {
        SafeFree(autoStr->str, autoStr->file, autoStr->line);
        autoStr->str = NULL;
    }
}

void AutoArrayCleanup(AutoArray* autoArray)
{
    if (!autoArray) {
        return;
    }

    if (autoArray->array) {
        // Clean up individual elements if cleanup function is provided
        if (autoArray->elementCleanup) {
            for (size_t i = 0; i < autoArray->count; i++) {
                if (autoArray->array[i]) {
                    autoArray->elementCleanup(autoArray->array[i]);
                }
            }
        }

        // Free the array itself
        SafeFree(autoArray->array, autoArray->file, autoArray->line);
        autoArray->array = NULL;
        autoArray->count = 0;
    }
}

// Generic cleanup wrapper for SafeFree
void GenericSafeFreeCleanup(void* ptr)
{
    if (ptr) {
        SafeFree(ptr, __FILE__, __LINE__);
    }
}

// Cleanup functions for wrapped structures
void CleanupYtDlpRequest(void* request)
{
    if (!request) {
        return;
    }

    // Note: This is a placeholder implementation
    // The actual cleanup would depend on the YtDlpRequest structure
    // For now, we just free the memory assuming it's a simple allocation
    SafeFree(request, __FILE__, __LINE__);
}

void CleanupCacheEntry(void* entry)
{
    if (!entry) {
        return;
    }

    // Note: This is a placeholder implementation
    // The actual cleanup would depend on the CacheEntry structure
    // For now, we just free the memory assuming it's a simple allocation
    SafeFree(entry, __FILE__, __LINE__);
}

// Factory functions for RAII-wrapped existing structures
AutoYtDlpRequest* CreateAutoYtDlpRequest(void* request)
{
    if (!request) {
        return NULL;
    }

    AutoYtDlpRequest* autoRequest = (AutoYtDlpRequest*)SAFE_MALLOC(sizeof(AutoYtDlpRequest));
    if (!autoRequest) {
        return NULL;
    }

    autoRequest->request = request;
    autoRequest->autoRes.resource = request;
    autoRequest->autoRes.cleanup = CleanupYtDlpRequest;
    autoRequest->autoRes.file = __FILE__;
    autoRequest->autoRes.line = __LINE__;

    return autoRequest;
}

AutoCacheEntry* CreateAutoCacheEntry(void* entry)
{
    if (!entry) {
        return NULL;
    }

    AutoCacheEntry* autoEntry = (AutoCacheEntry*)SAFE_MALLOC(sizeof(AutoCacheEntry));
    if (!autoEntry) {
        return NULL;
    }

    autoEntry->entry = entry;
    autoEntry->autoRes.resource = entry;
    autoEntry->autoRes.cleanup = CleanupCacheEntry;
    autoEntry->autoRes.file = __FILE__;
    autoEntry->autoRes.line = __LINE__;

    return autoEntry;
}

// Memory Pool System Implementation

// Global memory pools
MemoryPool* g_stringPool = NULL;        // For small strings (< 256 chars)
MemoryPool* g_cacheEntryPool = NULL;    // For CacheEntry structures  
MemoryPool* g_requestPool = NULL;       // For YtDlpRequest structures

// Pool configuration constants
#define STRING_POOL_OBJECT_SIZE     (256 * sizeof(wchar_t))  // 256 wide chars
#define STRING_POOL_INITIAL_COUNT   100
#define CACHE_ENTRY_POOL_INITIAL_COUNT  50
#define REQUEST_POOL_INITIAL_COUNT  20

MemoryPool* CreateMemoryPool(size_t objectSize, size_t initialCount, const char* poolName)
{
    if (objectSize == 0 || initialCount == 0) {
        return NULL;
    }

    MemoryPool* pool = (MemoryPool*)SAFE_MALLOC(sizeof(MemoryPool));
    if (!pool) {
        return NULL;
    }

    // Initialize critical section for thread safety
    InitializeCriticalSection(&pool->lock);

    // Allocate the main memory block
    pool->memory = SAFE_MALLOC(objectSize * initialCount);
    if (!pool->memory) {
        DeleteCriticalSection(&pool->lock);
        SAFE_FREE(pool);
        return NULL;
    }

    // Allocate the free list (array of pointers to free objects)
    pool->freeList = (void**)SAFE_MALLOC(sizeof(void*) * initialCount);
    if (!pool->freeList) {
        SAFE_FREE(pool->memory);
        DeleteCriticalSection(&pool->lock);
        SAFE_FREE(pool);
        return NULL;
    }

    // Initialize pool properties
    pool->objectSize = objectSize;
    pool->totalObjects = initialCount;
    pool->freeCount = initialCount;
    pool->allocatedCount = 0;
    pool->poolName = poolName;

    // Initialize free list - all objects are initially free
    char* memoryPtr = (char*)pool->memory;
    for (size_t i = 0; i < initialCount; i++) {
        pool->freeList[i] = memoryPtr + (i * objectSize);
    }

    return pool;
}

void* AllocateFromPool(MemoryPool* pool)
{
    if (!pool) {
        return NULL;
    }

    EnterCriticalSection(&pool->lock);

    void* object = NULL;
    
    if (pool->freeCount > 0) {
        // Get object from free list (stack-like behavior - LIFO)
        pool->freeCount--;
        object = pool->freeList[pool->freeCount];
        pool->allocatedCount++;
        
        // Clear the memory for safety
        memset(object, 0, pool->objectSize);
    }

    LeaveCriticalSection(&pool->lock);

    return object;
}

void ReturnToPool(MemoryPool* pool, void* object)
{
    if (!pool || !object) {
        return;
    }

    EnterCriticalSection(&pool->lock);

    // Verify the object belongs to this pool
    char* memoryStart = (char*)pool->memory;
    char* memoryEnd = memoryStart + (pool->totalObjects * pool->objectSize);
    char* objectPtr = (char*)object;

    if (objectPtr >= memoryStart && objectPtr < memoryEnd) {
        // Check if the object is properly aligned
        size_t offset = objectPtr - memoryStart;
        if (offset % pool->objectSize == 0) {
            // Object is valid, add it back to free list
            if (pool->freeCount < pool->totalObjects) {
                pool->freeList[pool->freeCount] = object;
                pool->freeCount++;
                pool->allocatedCount--;
                
                // Clear the memory for security
                memset(object, 0, pool->objectSize);
            }
        }
    }

    LeaveCriticalSection(&pool->lock);
}

void DestroyMemoryPool(MemoryPool* pool)
{
    if (!pool) {
        return;
    }

    EnterCriticalSection(&pool->lock);

    // Free the memory block
    if (pool->memory) {
        SAFE_FREE(pool->memory);
        pool->memory = NULL;
    }

    // Free the free list
    if (pool->freeList) {
        SAFE_FREE(pool->freeList);
        pool->freeList = NULL;
    }

    LeaveCriticalSection(&pool->lock);
    DeleteCriticalSection(&pool->lock);

    // Free the pool structure itself
    SAFE_FREE(pool);
}

BOOL InitializeMemoryPools(void)
{
    // Create string pool for small strings
    g_stringPool = CreateMemoryPool(STRING_POOL_OBJECT_SIZE, STRING_POOL_INITIAL_COUNT, "StringPool");
    if (!g_stringPool) {
        return FALSE;
    }

    // Create cache entry pool
    g_cacheEntryPool = CreateMemoryPool(sizeof(CacheEntry), CACHE_ENTRY_POOL_INITIAL_COUNT, "CacheEntryPool");
    if (!g_cacheEntryPool) {
        DestroyMemoryPool(g_stringPool);
        g_stringPool = NULL;
        return FALSE;
    }

    // Create request pool  
    g_requestPool = CreateMemoryPool(sizeof(YtDlpRequest), REQUEST_POOL_INITIAL_COUNT, "RequestPool");
    if (!g_requestPool) {
        DestroyMemoryPool(g_stringPool);
        DestroyMemoryPool(g_cacheEntryPool);
        g_stringPool = NULL;
        g_cacheEntryPool = NULL;
        return FALSE;
    }

    return TRUE;
}

void CleanupMemoryPools(void)
{
    if (g_stringPool) {
        DestroyMemoryPool(g_stringPool);
        g_stringPool = NULL;
    }

    if (g_cacheEntryPool) {
        DestroyMemoryPool(g_cacheEntryPool);
        g_cacheEntryPool = NULL;
    }

    if (g_requestPool) {
        DestroyMemoryPool(g_requestPool);
        g_requestPool = NULL;
    }
}

PoolStatistics GetPoolStatistics(void)
{
    PoolStatistics stats = {0};

    if (g_stringPool) {
        EnterCriticalSection(&g_stringPool->lock);
        stats.totalPools++;
        stats.totalAllocatedObjects += g_stringPool->allocatedCount;
        stats.totalFreeObjects += g_stringPool->freeCount;
        stats.totalMemoryUsed += g_stringPool->allocatedCount * g_stringPool->objectSize;
        stats.totalMemoryAllocated += g_stringPool->totalObjects * g_stringPool->objectSize;
        LeaveCriticalSection(&g_stringPool->lock);
    }

    if (g_cacheEntryPool) {
        EnterCriticalSection(&g_cacheEntryPool->lock);
        stats.totalPools++;
        stats.totalAllocatedObjects += g_cacheEntryPool->allocatedCount;
        stats.totalFreeObjects += g_cacheEntryPool->freeCount;
        stats.totalMemoryUsed += g_cacheEntryPool->allocatedCount * g_cacheEntryPool->objectSize;
        stats.totalMemoryAllocated += g_cacheEntryPool->totalObjects * g_cacheEntryPool->objectSize;
        LeaveCriticalSection(&g_cacheEntryPool->lock);
    }

    if (g_requestPool) {
        EnterCriticalSection(&g_requestPool->lock);
        stats.totalPools++;
        stats.totalAllocatedObjects += g_requestPool->allocatedCount;
        stats.totalFreeObjects += g_requestPool->freeCount;
        stats.totalMemoryUsed += g_requestPool->allocatedCount * g_requestPool->objectSize;
        stats.totalMemoryAllocated += g_requestPool->totalObjects * g_requestPool->objectSize;
        LeaveCriticalSection(&g_requestPool->lock);
    }

    return stats;
}

void DumpPoolStatistics(void)
{
    PoolStatistics stats = GetPoolStatistics();
    
    printf("=== MEMORY POOL STATISTICS ===\n");
    printf("Total Pools: %zu\n", stats.totalPools);
    printf("Total Allocated Objects: %zu\n", stats.totalAllocatedObjects);
    printf("Total Free Objects: %zu\n", stats.totalFreeObjects);
    printf("Total Memory Used: %zu bytes\n", stats.totalMemoryUsed);
    printf("Total Memory Allocated: %zu bytes\n", stats.totalMemoryAllocated);
    
    if (stats.totalMemoryAllocated > 0) {
        double efficiency = (double)stats.totalMemoryUsed / stats.totalMemoryAllocated * 100.0;
        printf("Memory Efficiency: %.1f%%\n", efficiency);
    }

    // Individual pool statistics
    if (g_stringPool) {
        EnterCriticalSection(&g_stringPool->lock);
        printf("\nString Pool:\n");
        printf("  Object Size: %zu bytes\n", g_stringPool->objectSize);
        printf("  Total Objects: %zu\n", g_stringPool->totalObjects);
        printf("  Allocated: %zu\n", g_stringPool->allocatedCount);
        printf("  Free: %zu\n", g_stringPool->freeCount);
        LeaveCriticalSection(&g_stringPool->lock);
    }

    if (g_cacheEntryPool) {
        EnterCriticalSection(&g_cacheEntryPool->lock);
        printf("\nCache Entry Pool:\n");
        printf("  Object Size: %zu bytes\n", g_cacheEntryPool->objectSize);
        printf("  Total Objects: %zu\n", g_cacheEntryPool->totalObjects);
        printf("  Allocated: %zu\n", g_cacheEntryPool->allocatedCount);
        printf("  Free: %zu\n", g_cacheEntryPool->freeCount);
        LeaveCriticalSection(&g_cacheEntryPool->lock);
    }

    if (g_requestPool) {
        EnterCriticalSection(&g_requestPool->lock);
        printf("\nRequest Pool:\n");
        printf("  Object Size: %zu bytes\n", g_requestPool->objectSize);
        printf("  Total Objects: %zu\n", g_requestPool->totalObjects);
        printf("  Allocated: %zu\n", g_requestPool->allocatedCount);
        printf("  Free: %zu\n", g_requestPool->freeCount);
        LeaveCriticalSection(&g_requestPool->lock);
    }

    printf("==============================\n");
}

// Test function to verify memory pool functionality
BOOL TestMemoryPools(void)
{
    printf("=== TESTING MEMORY POOLS ===\n");
    
    if (!g_stringPool || !g_cacheEntryPool || !g_requestPool) {
        printf("ERROR: Memory pools not initialized\n");
        return FALSE;
    }

    // Test string pool
    printf("Testing String Pool...\n");
    wchar_t* str1 = (wchar_t*)AllocateFromPool(g_stringPool);
    wchar_t* str2 = (wchar_t*)AllocateFromPool(g_stringPool);
    
    if (!str1 || !str2) {
        printf("ERROR: Failed to allocate from string pool\n");
        return FALSE;
    }

    // Use the strings
    wcscpy(str1, L"Test String 1");
    wcscpy(str2, L"Test String 2");
    printf("Allocated strings: '%ls' and '%ls'\n", str1, str2);

    // Return to pool
    ReturnToPool(g_stringPool, str1);
    ReturnToPool(g_stringPool, str2);
    printf("Returned strings to pool\n");

    // Test cache entry pool
    printf("Testing Cache Entry Pool...\n");
    CacheEntry* entry1 = (CacheEntry*)AllocateFromPool(g_cacheEntryPool);
    CacheEntry* entry2 = (CacheEntry*)AllocateFromPool(g_cacheEntryPool);
    
    if (!entry1 || !entry2) {
        printf("ERROR: Failed to allocate from cache entry pool\n");
        return FALSE;
    }

    printf("Allocated cache entries at %p and %p\n", (void*)entry1, (void*)entry2);

    // Return to pool
    ReturnToPool(g_cacheEntryPool, entry1);
    ReturnToPool(g_cacheEntryPool, entry2);
    printf("Returned cache entries to pool\n");

    // Test request pool
    printf("Testing Request Pool...\n");
    YtDlpRequest* req1 = (YtDlpRequest*)AllocateFromPool(g_requestPool);
    YtDlpRequest* req2 = (YtDlpRequest*)AllocateFromPool(g_requestPool);
    
    if (!req1 || !req2) {
        printf("ERROR: Failed to allocate from request pool\n");
        return FALSE;
    }

    printf("Allocated requests at %p and %p\n", (void*)req1, (void*)req2);

    // Return to pool
    ReturnToPool(g_requestPool, req1);
    ReturnToPool(g_requestPool, req2);
    printf("Returned requests to pool\n");

    // Display pool statistics
    DumpPoolStatistics();

    printf("=== MEMORY POOL TESTS PASSED ===\n");
    return TRUE;
}