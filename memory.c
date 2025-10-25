#include "YouTubeCacher.h"

// Additional includes for error handling
#ifdef _WIN32
#include <dbghelp.h>
// Note: Link with dbghelp.lib in Makefile instead of using pragma
#endif

// Global memory manager instance
static MemoryManager g_memoryManager = {0};

// Global error handling state
static MemoryErrorCallback g_errorCallback = NULL;
static BOOL g_doubleFreDetectionEnabled = TRUE;
static BOOL g_useAfterFreeDetectionEnabled = TRUE;
static BOOL g_bufferOverrunDetectionEnabled = TRUE;
static CRITICAL_SECTION g_errorLock;
static BOOL g_errorSystemInitialized = FALSE;

// Freed memory tracking for use-after-free detection
typedef struct FreedMemoryInfo {
    void* address;
    size_t size;
    const char* file;
    int line;
    SYSTEMTIME freeTime;
    struct FreedMemoryInfo* next;
} FreedMemoryInfo;

static FreedMemoryInfo* g_freedMemoryList = NULL;
static size_t g_freedMemoryCount = 0;
static const size_t MAX_FREED_MEMORY_TRACKING = 1000; // Limit to prevent excessive memory usage

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

// Internal error handling function declarations
static void InitializeErrorSystem(void);
static void CleanupErrorSystem(void);
static void AddFreedMemoryRecord(void* address, size_t size, const char* file, int line);
static BOOL IsFreedMemory(void* address);
static void CleanupFreedMemoryList(void);
static void FillMemoryPattern(void* ptr, size_t size, DWORD pattern);
static BOOL CheckMemoryPattern(void* ptr, size_t size, DWORD pattern);
static void CaptureStackTrace(void** stackTrace, int* stackDepth);

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
    
    // Cleanup error handling system
    CleanupErrorSystem();
}

void* SafeMalloc(size_t size, const char* file, int line)
{
    if (size == 0) {
        return NULL;
    }

    // Initialize error system if needed
    if (!g_errorSystemInitialized) {
        InitializeErrorSystem();
    }

#ifdef MEMORY_DEBUG
    // In debug builds, allocate extra space for guard patterns
    size_t totalSize = size;
    if (g_bufferOverrunDetectionEnabled) {
        totalSize = size + (2 * GUARD_SIZE); // Guard before and after
    }
    
    void* rawPtr = malloc(totalSize);
    if (!rawPtr) {
        ReportMemoryError(MEMORY_ERROR_ALLOCATION_FAILED, NULL, size, file, line,
                         L"malloc() failed to allocate requested memory");
        return NULL;
    }
    
    void* userPtr = rawPtr;
    if (g_bufferOverrunDetectionEnabled) {
        // Set up guard patterns
        char* guardBefore = (char*)rawPtr;
        char* userArea = guardBefore + GUARD_SIZE;
        char* guardAfter = userArea + size;
        
        // Fill guard areas with pattern
        FillMemoryPattern(guardBefore, GUARD_SIZE, GUARD_PATTERN);
        FillMemoryPattern(guardAfter, GUARD_SIZE, GUARD_PATTERN);
        
        // Fill user area with uninitialized pattern
        FillMemoryPattern(userArea, size, UNINITIALIZED_PATTERN);
        
        userPtr = userArea;
    } else {
        // Fill with uninitialized pattern even without guards
        FillMemoryPattern(rawPtr, size, UNINITIALIZED_PATTERN);
    }
#else
    // In release builds, just allocate normally
    void* rawPtr = malloc(size);
    if (!rawPtr) {
        ReportMemoryError(MEMORY_ERROR_ALLOCATION_FAILED, NULL, size, file, line,
                         L"malloc() failed to allocate requested memory");
        return NULL;
    }
    void* userPtr = rawPtr;
#endif

    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        
        // Record the user pointer, not the raw pointer
        AddAllocationRecord(userPtr, size, file, line);
        g_memoryManager.totalAllocated += size;
        g_memoryManager.currentUsage += size;
        
        if (g_memoryManager.currentUsage > g_memoryManager.peakUsage) {
            g_memoryManager.peakUsage = g_memoryManager.currentUsage;
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }

    return userPtr;
}

void* SafeCalloc(size_t count, size_t size, const char* file, int line)
{
    if (count == 0 || size == 0) {
        return NULL;
    }

    // Check for overflow
    if (count > SIZE_MAX / size) {
        ReportMemoryError(MEMORY_ERROR_ALLOCATION_FAILED, NULL, count * size, file, line,
                         L"Integer overflow in calloc size calculation");
        return NULL;
    }

    // Initialize error system if needed
    if (!g_errorSystemInitialized) {
        InitializeErrorSystem();
    }

    size_t totalSize = count * size;

#ifdef MEMORY_DEBUG
    // In debug builds, allocate extra space for guard patterns
    size_t allocSize = totalSize;
    if (g_bufferOverrunDetectionEnabled) {
        allocSize = totalSize + (2 * GUARD_SIZE); // Guard before and after
    }
    
    void* rawPtr = calloc(1, allocSize); // Use calloc(1, allocSize) to zero entire block
    if (!rawPtr) {
        ReportMemoryError(MEMORY_ERROR_ALLOCATION_FAILED, NULL, totalSize, file, line,
                         L"calloc() failed to allocate requested memory");
        return NULL;
    }
    
    void* userPtr = rawPtr;
    if (g_bufferOverrunDetectionEnabled) {
        // Set up guard patterns (calloc already zeroed everything)
        char* guardBefore = (char*)rawPtr;
        char* userArea = guardBefore + GUARD_SIZE;
        char* guardAfter = userArea + totalSize;
        
        // Fill guard areas with pattern (overwriting the zeros)
        FillMemoryPattern(guardBefore, GUARD_SIZE, GUARD_PATTERN);
        FillMemoryPattern(guardAfter, GUARD_SIZE, GUARD_PATTERN);
        
        // User area remains zeroed from calloc
        userPtr = userArea;
    }
#else
    // In release builds, just allocate normally
    void* rawPtr = calloc(count, size);
    if (!rawPtr) {
        ReportMemoryError(MEMORY_ERROR_ALLOCATION_FAILED, NULL, totalSize, file, line,
                         L"calloc() failed to allocate requested memory");
        return NULL;
    }
    void* userPtr = rawPtr;
#endif

    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        
        // Record the user pointer, not the raw pointer
        AddAllocationRecord(userPtr, totalSize, file, line);
        g_memoryManager.totalAllocated += totalSize;
        g_memoryManager.currentUsage += totalSize;
        
        if (g_memoryManager.currentUsage > g_memoryManager.peakUsage) {
            g_memoryManager.peakUsage = g_memoryManager.currentUsage;
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }

    return userPtr;
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
    if (!ptr) {
        return; // Safe to free NULL pointer
    }

    // Initialize error system if needed
    if (!g_errorSystemInitialized) {
        InitializeErrorSystem();
    }

    // Check for double-free if detection is enabled
    if (g_doubleFreDetectionEnabled && IsFreedMemory(ptr)) {
        ReportMemoryError(MEMORY_ERROR_DOUBLE_FREE, ptr, 0, file, line,
                         L"Attempt to free already freed memory");
        return; // Don't actually free again
    }

    size_t freedSize = 0;
    BOOL wasTracked = FALSE;
    
    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        
        // Use hash table for fast lookup and removal
        if (RemoveAllocationRecord(ptr, &freedSize)) {
            g_memoryManager.totalFreed += freedSize;
            g_memoryManager.currentUsage -= freedSize;
            wasTracked = TRUE;
        } else {
            // Address not found in allocation table
            if (g_doubleFreDetectionEnabled) {
                LeaveCriticalSection(&g_memoryManager.lock);
                ReportMemoryError(MEMORY_ERROR_INVALID_ADDRESS, ptr, 0, file, line,
                                 L"Attempt to free untracked memory address");
                return;
            }
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }

    // Validate allocation integrity before freeing
    if (wasTracked && !ValidateAllocationIntegrity(ptr)) {
        // Error already reported by ValidateAllocationIntegrity
        return; // Don't free corrupted memory
    }

    // Add to freed memory tracking for use-after-free detection
    if (wasTracked && g_useAfterFreeDetectionEnabled) {
        EnterCriticalSection(&g_errorLock);
        AddFreedMemoryRecord(ptr, freedSize, file, line);
        LeaveCriticalSection(&g_errorLock);
    }

#ifdef MEMORY_DEBUG
    // In debug builds, we need to free the raw pointer (with guards)
    void* rawPtr = ptr;
    if (g_bufferOverrunDetectionEnabled && wasTracked) {
        rawPtr = (char*)ptr - GUARD_SIZE;
    }
    free(rawPtr);
#else
    free(ptr);
#endif
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

// Memory Error Handling and Reporting Implementation

void SetMemoryErrorCallback(MemoryErrorCallback callback)
{
    if (!g_errorSystemInitialized) {
        InitializeErrorSystem();
    }

    EnterCriticalSection(&g_errorLock);
    g_errorCallback = callback;
    LeaveCriticalSection(&g_errorLock);
}

MemoryErrorCallback GetMemoryErrorCallback(void)
{
    if (!g_errorSystemInitialized) {
        return NULL;
    }

    EnterCriticalSection(&g_errorLock);
    MemoryErrorCallback callback = g_errorCallback;
    LeaveCriticalSection(&g_errorLock);
    
    return callback;
}

void ClearMemoryErrorCallback(void)
{
    if (!g_errorSystemInitialized) {
        return;
    }

    EnterCriticalSection(&g_errorLock);
    g_errorCallback = NULL;
    LeaveCriticalSection(&g_errorLock);
}

void ReportMemoryError(MemoryErrorType type, void* address, size_t size, 
                      const char* file, int line, const wchar_t* description)
{
    if (!g_errorSystemInitialized) {
        InitializeErrorSystem();
    }

    // Create error structure
    MemoryError error = {0};
    error.type = type;
    error.address = address;
    error.size = size;
    error.file = file;
    error.line = line;
    error.threadId = GetCurrentThreadId();
    GetSystemTime(&error.errorTime);
    
    // Capture stack trace
    CaptureStackTrace(error.stackTrace, &error.stackDepth);
    
    // Duplicate description string for safety
    if (description) {
        size_t descLen = wcslen(description);
        error.description = (wchar_t*)malloc((descLen + 1) * sizeof(wchar_t));
        if (error.description) {
            wcscpy(error.description, description);
        }
    }

    // Call error callback if set
    EnterCriticalSection(&g_errorLock);
    MemoryErrorCallback callback = g_errorCallback;
    LeaveCriticalSection(&g_errorLock);

    if (callback) {
        callback(&error);
    } else {
        // Default error handling - output to debug console and log
        printf("MEMORY ERROR [%s:%d]: ", 
               file ? file : "unknown", line);
        
        switch (type) {
            case MEMORY_ERROR_ALLOCATION_FAILED:
                printf("Allocation failed for %zu bytes\n", size);
                break;
            case MEMORY_ERROR_DOUBLE_FREE:
                printf("Double free detected at address %p\n", address);
                break;
            case MEMORY_ERROR_USE_AFTER_FREE:
                printf("Use after free detected at address %p\n", address);
                break;
            case MEMORY_ERROR_BUFFER_OVERRUN:
                printf("Buffer overrun detected at address %p\n", address);
                break;
            case MEMORY_ERROR_LEAK_DETECTED:
                printf("Memory leak detected: %zu bytes at %p\n", size, address);
                break;
            case MEMORY_ERROR_INVALID_ADDRESS:
                printf("Invalid memory address: %p\n", address);
                break;
            case MEMORY_ERROR_CORRUPTION_DETECTED:
                printf("Memory corruption detected at address %p\n", address);
                break;
        }
        
        if (description) {
            wprintf(L"Description: %ls\n", description);
        }
        
        printf("Thread ID: %u, Time: %02d:%02d:%02d.%03d\n",
               (unsigned int)error.threadId,
               error.errorTime.wHour, error.errorTime.wMinute,
               error.errorTime.wSecond, error.errorTime.wMilliseconds);
    }

    // Clean up allocated description
    if (error.description) {
        free(error.description);
    }
}

BOOL EnableDoubleFreDetection(BOOL enable)
{
    if (!g_errorSystemInitialized) {
        InitializeErrorSystem();
    }

    EnterCriticalSection(&g_errorLock);
    g_doubleFreDetectionEnabled = enable;
    LeaveCriticalSection(&g_errorLock);
    
    return TRUE;
}

BOOL EnableUseAfterFreeDetection(BOOL enable)
{
    if (!g_errorSystemInitialized) {
        InitializeErrorSystem();
    }

    EnterCriticalSection(&g_errorLock);
    g_useAfterFreeDetectionEnabled = enable;
    LeaveCriticalSection(&g_errorLock);
    
    return TRUE;
}

BOOL EnableBufferOverrunDetection(BOOL enable)
{
    if (!g_errorSystemInitialized) {
        InitializeErrorSystem();
    }

    EnterCriticalSection(&g_errorLock);
    g_bufferOverrunDetectionEnabled = enable;
    LeaveCriticalSection(&g_errorLock);
    
    return TRUE;
}

BOOL IsDoubleFreDetectionEnabled(void)
{
    if (!g_errorSystemInitialized) {
        return FALSE;
    }

    EnterCriticalSection(&g_errorLock);
    BOOL enabled = g_doubleFreDetectionEnabled;
    LeaveCriticalSection(&g_errorLock);
    
    return enabled;
}

BOOL IsUseAfterFreeDetectionEnabled(void)
{
    if (!g_errorSystemInitialized) {
        return FALSE;
    }

    EnterCriticalSection(&g_errorLock);
    BOOL enabled = g_useAfterFreeDetectionEnabled;
    LeaveCriticalSection(&g_errorLock);
    
    return enabled;
}

BOOL IsBufferOverrunDetectionEnabled(void)
{
    if (!g_errorSystemInitialized) {
        return FALSE;
    }

    EnterCriticalSection(&g_errorLock);
    BOOL enabled = g_bufferOverrunDetectionEnabled;
    LeaveCriticalSection(&g_errorLock);
    
    return enabled;
}

BOOL ValidateMemoryAddress(void* address)
{
    if (!address) {
        return FALSE;
    }

    // Check if it's a freed memory address
    if (g_useAfterFreeDetectionEnabled && IsFreedMemory(address)) {
        ReportMemoryError(MEMORY_ERROR_USE_AFTER_FREE, address, 0, 
                         __FILE__, __LINE__, L"Access to freed memory detected");
        return FALSE;
    }

    // Check if it's a valid allocation
    if (g_memoryManager.initialized && g_memoryManager.leakDetectionEnabled) {
        EnterCriticalSection(&g_memoryManager.lock);
        AllocationInfo* alloc = FindInHashTable(&g_memoryManager.hashTable, address);
        LeaveCriticalSection(&g_memoryManager.lock);
        
        if (!alloc) {
            ReportMemoryError(MEMORY_ERROR_INVALID_ADDRESS, address, 0,
                             __FILE__, __LINE__, L"Address not found in allocation table");
            return FALSE;
        }
    }

    return TRUE;
}

BOOL ValidateAllocationIntegrity(void* address)
{
    if (!ValidateMemoryAddress(address)) {
        return FALSE;
    }

#ifdef MEMORY_DEBUG
    if (g_bufferOverrunDetectionEnabled && g_memoryManager.initialized) {
        EnterCriticalSection(&g_memoryManager.lock);
        AllocationInfo* alloc = FindInHashTable(&g_memoryManager.hashTable, address);
        
        if (alloc) {
            // Check guard patterns before and after the allocation
            char* allocStart = (char*)alloc->address;
            char* guardBefore = allocStart - GUARD_SIZE;
            char* guardAfter = allocStart + alloc->size;
            
            // Check if guard patterns are intact
            if (!CheckMemoryPattern(guardBefore, GUARD_SIZE, GUARD_PATTERN) ||
                !CheckMemoryPattern(guardAfter, GUARD_SIZE, GUARD_PATTERN)) {
                
                LeaveCriticalSection(&g_memoryManager.lock);
                ReportMemoryError(MEMORY_ERROR_BUFFER_OVERRUN, address, alloc->size,
                                 alloc->file, alloc->line, L"Buffer overrun detected via guard pattern");
                return FALSE;
            }
        }
        
        LeaveCriticalSection(&g_memoryManager.lock);
    }
#endif

    return TRUE;
}

void CheckForMemoryCorruption(void)
{
    if (!g_memoryManager.initialized || !g_memoryManager.leakDetectionEnabled) {
        return;
    }

    EnterCriticalSection(&g_memoryManager.lock);
    
    // Check all active allocations for corruption
    for (size_t i = 0; i < g_memoryManager.allocationCount; i++) {
        AllocationInfo* alloc = &g_memoryManager.allocations[i];
        
        if (!ValidateAllocationIntegrity(alloc->address)) {
            // Error already reported by ValidateAllocationIntegrity
            continue;
        }
    }
    
    LeaveCriticalSection(&g_memoryManager.lock);
}

// Internal error system functions
static void InitializeErrorSystem(void)
{
    if (g_errorSystemInitialized) {
        return;
    }

    InitializeCriticalSection(&g_errorLock);
    g_errorCallback = NULL;
    g_doubleFreDetectionEnabled = TRUE;
    g_useAfterFreeDetectionEnabled = TRUE;
    g_bufferOverrunDetectionEnabled = TRUE;
    g_freedMemoryList = NULL;
    g_freedMemoryCount = 0;
    g_errorSystemInitialized = TRUE;
}

static void CleanupErrorSystem(void)
{
    if (!g_errorSystemInitialized) {
        return;
    }

    EnterCriticalSection(&g_errorLock);
    
    // Clean up freed memory tracking list
    CleanupFreedMemoryList();
    
    g_errorCallback = NULL;
    g_errorSystemInitialized = FALSE;
    
    LeaveCriticalSection(&g_errorLock);
    DeleteCriticalSection(&g_errorLock);
}

static void AddFreedMemoryRecord(void* address, size_t size, const char* file, int line)
{
    if (!g_useAfterFreeDetectionEnabled || !address) {
        return;
    }

    // Limit the number of freed memory records to prevent excessive memory usage
    if (g_freedMemoryCount >= MAX_FREED_MEMORY_TRACKING) {
        // Remove oldest record
        if (g_freedMemoryList) {
            FreedMemoryInfo* oldest = g_freedMemoryList;
            FreedMemoryInfo* prev = NULL;
            
            // Find the last item (oldest)
            while (oldest->next) {
                prev = oldest;
                oldest = oldest->next;
            }
            
            // Remove it
            if (prev) {
                prev->next = NULL;
            } else {
                g_freedMemoryList = NULL;
            }
            
            free(oldest);
            g_freedMemoryCount--;
        }
    }

    // Add new freed memory record
    FreedMemoryInfo* info = (FreedMemoryInfo*)malloc(sizeof(FreedMemoryInfo));
    if (info) {
        info->address = address;
        info->size = size;
        info->file = file;
        info->line = line;
        GetSystemTime(&info->freeTime);
        info->next = g_freedMemoryList;
        g_freedMemoryList = info;
        g_freedMemoryCount++;
        
        // Fill freed memory with pattern for detection
#ifdef MEMORY_DEBUG
        FillMemoryPattern(address, size, FREED_MEMORY_PATTERN);
#endif
    }
}

static BOOL IsFreedMemory(void* address)
{
    if (!g_useAfterFreeDetectionEnabled || !address) {
        return FALSE;
    }

    FreedMemoryInfo* current = g_freedMemoryList;
    while (current) {
        if (current->address == address) {
            return TRUE;
        }
        current = current->next;
    }
    
    return FALSE;
}

static void CleanupFreedMemoryList(void)
{
    FreedMemoryInfo* current = g_freedMemoryList;
    while (current) {
        FreedMemoryInfo* next = current->next;
        free(current);
        current = next;
    }
    
    g_freedMemoryList = NULL;
    g_freedMemoryCount = 0;
}

static void FillMemoryPattern(void* ptr, size_t size, DWORD pattern)
{
    if (!ptr || size == 0) {
        return;
    }

    DWORD* dwordPtr = (DWORD*)ptr;
    size_t dwordCount = size / sizeof(DWORD);
    
    // Fill complete DWORD values
    for (size_t i = 0; i < dwordCount; i++) {
        dwordPtr[i] = pattern;
    }
    
    // Fill remaining bytes
    size_t remainingBytes = size % sizeof(DWORD);
    if (remainingBytes > 0) {
        char* bytePtr = (char*)ptr + (dwordCount * sizeof(DWORD));
        char* patternBytes = (char*)&pattern;
        
        for (size_t i = 0; i < remainingBytes; i++) {
            bytePtr[i] = patternBytes[i % sizeof(DWORD)];
        }
    }
}

static BOOL CheckMemoryPattern(void* ptr, size_t size, DWORD pattern)
{
    if (!ptr || size == 0) {
        return TRUE; // Nothing to check
    }

    DWORD* dwordPtr = (DWORD*)ptr;
    size_t dwordCount = size / sizeof(DWORD);
    
    // Check complete DWORD values
    for (size_t i = 0; i < dwordCount; i++) {
        if (dwordPtr[i] != pattern) {
            return FALSE;
        }
    }
    
    // Check remaining bytes
    size_t remainingBytes = size % sizeof(DWORD);
    if (remainingBytes > 0) {
        char* bytePtr = (char*)ptr + (dwordCount * sizeof(DWORD));
        char* patternBytes = (char*)&pattern;
        
        for (size_t i = 0; i < remainingBytes; i++) {
            if (bytePtr[i] != patternBytes[i % sizeof(DWORD)]) {
                return FALSE;
            }
        }
    }
    
    return TRUE;
}

static void CaptureStackTrace(void** stackTrace, int* stackDepth)
{
    // Initialize output parameters
    *stackDepth = 0;
    
    if (!stackTrace) {
        return;
    }

    // Clear the stack trace array
    for (int i = 0; i < 16; i++) {
        stackTrace[i] = NULL;
    }

#ifdef _WIN32
    // Use Windows API to capture stack trace
    HANDLE process = GetCurrentProcess();
    
    // Initialize symbol handler (this should ideally be done once at startup)
    static BOOL symbolsInitialized = FALSE;
    if (!symbolsInitialized) {
        SymInitialize(process, NULL, TRUE);
        symbolsInitialized = TRUE;
    }
    
    // Capture the stack trace
    *stackDepth = CaptureStackBackTrace(1, 16, stackTrace, NULL);
#else
    // For non-Windows platforms, stack trace capture would need different implementation
    // For now, just set depth to 0
    *stackDepth = 0;
#endif
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

// Error-Safe Allocation Patterns Implementation

// Initial capacity for allocation sets and bulk cleanup structures
#define INITIAL_ALLOCATION_SET_CAPACITY 16
#define INITIAL_BULK_CLEANUP_CAPACITY 32
#define ALLOCATION_SET_GROWTH_FACTOR 2

AllocationSet* CreateAllocationSet(const char* file, int line)
{
    AllocationSet* set = (AllocationSet*)SafeMalloc(sizeof(AllocationSet), file, line);
    if (!set) {
        return NULL;
    }

    set->allocations = (void**)SafeMalloc(
        INITIAL_ALLOCATION_SET_CAPACITY * sizeof(void*), file, line);
    if (!set->allocations) {
        SafeFree(set, file, line);
        return NULL;
    }

    set->count = 0;
    set->capacity = INITIAL_ALLOCATION_SET_CAPACITY;
    set->file = file;
    set->line = line;

    return set;
}

BOOL AddToAllocationSet(AllocationSet* set, void* ptr)
{
    if (!set || !ptr) {
        return FALSE;
    }

    // Expand capacity if needed
    if (set->count >= set->capacity) {
        size_t newCapacity = set->capacity * ALLOCATION_SET_GROWTH_FACTOR;
        void** newAllocations = (void**)SafeRealloc(set->allocations, 
            newCapacity * sizeof(void*), set->file, set->line);
        
        if (!newAllocations) {
            return FALSE; // Failed to expand, allocation not added
        }

        set->allocations = newAllocations;
        set->capacity = newCapacity;
    }

    // Add the pointer to the set
    set->allocations[set->count] = ptr;
    set->count++;

    return TRUE;
}

void CommitAllocationSet(AllocationSet* set)
{
    if (!set) {
        return;
    }

    // Committing transfers ownership - just clear the set without freeing
    set->count = 0;
    // Note: We don't free the allocations array to allow reuse of the set
}

void RollbackAllocationSet(AllocationSet* set)
{
    if (!set) {
        return;
    }

    // Free all allocations in the set
    for (size_t i = 0; i < set->count; i++) {
        if (set->allocations[i]) {
            SafeFree(set->allocations[i], set->file, set->line);
            set->allocations[i] = NULL;
        }
    }

    // Clear the set
    set->count = 0;
}

void FreeAllocationSet(AllocationSet* set)
{
    if (!set) {
        return;
    }

    // First rollback any uncommitted allocations
    RollbackAllocationSet(set);

    // Free the allocations array
    if (set->allocations) {
        SafeFree(set->allocations, set->file, set->line);
        set->allocations = NULL;
    }

    // Free the set structure itself
    SafeFree(set, set->file, set->line);
}

BulkCleanup* CreateBulkCleanup(size_t initialCapacity)
{
    if (initialCapacity == 0) {
        initialCapacity = INITIAL_BULK_CLEANUP_CAPACITY;
    }

    BulkCleanup* cleanup = (BulkCleanup*)SAFE_MALLOC(sizeof(BulkCleanup));
    if (!cleanup) {
        return NULL;
    }

    cleanup->pointers = (void**)SAFE_MALLOC(initialCapacity * sizeof(void*));
    if (!cleanup->pointers) {
        SAFE_FREE(cleanup);
        return NULL;
    }

    cleanup->count = 0;
    cleanup->capacity = initialCapacity;

    return cleanup;
}

BOOL AddToBulkCleanup(BulkCleanup* cleanup, void* ptr)
{
    if (!cleanup || !ptr) {
        return FALSE;
    }

    // Expand capacity if needed
    if (cleanup->count >= cleanup->capacity) {
        size_t newCapacity = cleanup->capacity * ALLOCATION_SET_GROWTH_FACTOR;
        void** newPointers = (void**)SAFE_REALLOC(cleanup->pointers, 
            newCapacity * sizeof(void*));
        
        if (!newPointers) {
            return FALSE; // Failed to expand
        }

        cleanup->pointers = newPointers;
        cleanup->capacity = newCapacity;
    }

    // Add the pointer to the cleanup set
    cleanup->pointers[cleanup->count] = ptr;
    cleanup->count++;

    return TRUE;
}

void BulkFree(BulkCleanup* cleanup)
{
    if (!cleanup) {
        return;
    }

    // Free all pointers in the cleanup set
    for (size_t i = 0; i < cleanup->count; i++) {
        if (cleanup->pointers[i]) {
            SAFE_FREE(cleanup->pointers[i]);
            cleanup->pointers[i] = NULL;
        }
    }

    // Clear the set for reuse
    cleanup->count = 0;
}

void FreeBulkCleanup(BulkCleanup* cleanup)
{
    if (!cleanup) {
        return;
    }

    // First free all remaining pointers
    BulkFree(cleanup);

    // Free the pointers array
    if (cleanup->pointers) {
        SAFE_FREE(cleanup->pointers);
        cleanup->pointers = NULL;
    }

    // Free the cleanup structure itself
    SAFE_FREE(cleanup);
}

// Test function for error-safe allocation patterns
BOOL TestErrorSafeAllocationPatterns(void)
{
    printf("=== TESTING ERROR-SAFE ALLOCATION PATTERNS ===\n");

    // Test AllocationSet functionality
    printf("Testing AllocationSet...\n");
    
    AllocationSet* set = CREATE_ALLOCATION_SET();
    if (!set) {
        printf("ERROR: Failed to create AllocationSet\n");
        return FALSE;
    }

    // Test successful allocation and commit
    wchar_t* str1 = SAFE_WCSDUP_AND_ADD(set, L"Test String 1");
    wchar_t* str2 = SAFE_WCSDUP_AND_ADD(set, L"Test String 2");
    void* buffer = SAFE_ALLOC_AND_ADD(set, 1024);

    if (!str1 || !str2 || !buffer) {
        printf("ERROR: Failed to allocate and add to set\n");
        RollbackAllocationSet(set);
        FreeAllocationSet(set);
        return FALSE;
    }

    printf("Successfully allocated %zu items in set\n", set->count);
    
    // Test commit (transfers ownership)
    CommitAllocationSet(set);
    printf("Committed allocation set (count now: %zu)\n", set->count);

    // Manually free the committed allocations
    SAFE_FREE(str1);
    SAFE_FREE(str2);
    SAFE_FREE(buffer);

    // Test rollback functionality
    printf("Testing rollback functionality...\n");
    wchar_t* str3 = SAFE_WCSDUP_AND_ADD(set, L"Test String 3");
    wchar_t* str4 = SAFE_WCSDUP_AND_ADD(set, L"Test String 4");
    
    if (!str3 || !str4) {
        printf("ERROR: Failed to allocate for rollback test\n");
        FreeAllocationSet(set);
        return FALSE;
    }

    printf("Allocated %zu items for rollback test\n", set->count);
    
    // Rollback should free all allocations
    RollbackAllocationSet(set);
    printf("Rolled back allocation set (count now: %zu)\n", set->count);

    // Clean up the set
    FreeAllocationSet(set);

    // Test BulkCleanup functionality
    printf("Testing BulkCleanup...\n");
    
    BulkCleanup* cleanup = CreateBulkCleanup(0); // Use default capacity
    if (!cleanup) {
        printf("ERROR: Failed to create BulkCleanup\n");
        return FALSE;
    }

    // Allocate several items and add to bulk cleanup
    for (int i = 0; i < 10; i++) {
        wchar_t* testStr = SAFE_MALLOC(256 * sizeof(wchar_t));
        if (testStr) {
            swprintf(testStr, 256, L"Bulk Test String %d", i);
            if (!AddToBulkCleanup(cleanup, testStr)) {
                printf("ERROR: Failed to add item %d to bulk cleanup\n", i);
                SAFE_FREE(testStr);
                FreeBulkCleanup(cleanup);
                return FALSE;
            }
        }
    }

    printf("Added %zu items to bulk cleanup\n", cleanup->count);

    // Test bulk free
    BulkFree(cleanup);
    printf("Bulk freed all items (count now: %zu)\n", cleanup->count);

    // Clean up the bulk cleanup structure
    FreeBulkCleanup(cleanup);

    printf("=== ERROR-SAFE ALLOCATION PATTERN TESTS PASSED ===\n");
    return TRUE;
}

// Test callback function for error handling tests
static int g_testErrorCount = 0;
static MemoryErrorType g_lastErrorType = MEMORY_ERROR_ALLOCATION_FAILED;

static void TestErrorCallback(const MemoryError* error)
{
    g_testErrorCount++;
    g_lastErrorType = error->type;
    printf("Test callback received error type %d at address %p\n", 
           error->type, error->address);
}

// Test function for memory error handling and reporting
BOOL TestMemoryErrorHandling(void)
{
    printf("=== TESTING MEMORY ERROR HANDLING ===\n");

    // Test error callback system
    printf("Testing error callback system...\n");
    
    // Reset test counters
    g_testErrorCount = 0;
    g_lastErrorType = MEMORY_ERROR_ALLOCATION_FAILED;
    
    SetMemoryErrorCallback(TestErrorCallback);
    
    // Test double-free detection
    printf("Testing double-free detection...\n");
    EnableDoubleFreDetection(TRUE);
    
    void* testPtr = SAFE_MALLOC(100);
    if (!testPtr) {
        printf("ERROR: Failed to allocate test memory\n");
        return FALSE;
    }
    
    SAFE_FREE(testPtr);  // First free - should be OK
    
    int oldErrorCount = g_testErrorCount;
    SAFE_FREE(testPtr);  // Second free - should trigger error
    
    if (g_testErrorCount <= oldErrorCount) {
        printf("ERROR: Double-free detection failed\n");
        return FALSE;
    }
    
    if (g_lastErrorType != MEMORY_ERROR_DOUBLE_FREE) {
        printf("ERROR: Wrong error type for double-free (got %d, expected %d)\n",
               g_lastErrorType, MEMORY_ERROR_DOUBLE_FREE);
        return FALSE;
    }
    
    printf("Double-free detection working correctly\n");

    // Test use-after-free detection
    printf("Testing use-after-free detection...\n");
    EnableUseAfterFreeDetection(TRUE);
    
    void* testPtr2 = SAFE_MALLOC(200);
    if (!testPtr2) {
        printf("ERROR: Failed to allocate test memory for use-after-free test\n");
        return FALSE;
    }
    
    SAFE_FREE(testPtr2);
    
    oldErrorCount = g_testErrorCount;
    BOOL isValid = ValidateMemoryAddress(testPtr2); // Should detect use-after-free
    
    if (isValid) {
        printf("ERROR: Use-after-free detection failed\n");
        return FALSE;
    }
    
    if (g_testErrorCount <= oldErrorCount) {
        printf("ERROR: Use-after-free error not reported\n");
        return FALSE;
    }
    
    printf("Use-after-free detection working correctly\n");

#ifdef MEMORY_DEBUG
    // Test buffer overrun detection
    printf("Testing buffer overrun detection...\n");
    EnableBufferOverrunDetection(TRUE);
    
    void* testPtr3 = SAFE_MALLOC(50);
    if (!testPtr3) {
        printf("ERROR: Failed to allocate test memory for buffer overrun test\n");
        return FALSE;
    }
    
    // Simulate buffer overrun by corrupting guard pattern
    char* guardAfter = (char*)testPtr3 + 50;
    *guardAfter = 0xFF; // Corrupt the guard pattern
    
    oldErrorCount = g_testErrorCount;
    BOOL isIntact = ValidateAllocationIntegrity(testPtr3);
    
    if (isIntact) {
        printf("ERROR: Buffer overrun detection failed\n");
        SAFE_FREE(testPtr3);
        return FALSE;
    }
    
    if (g_testErrorCount <= oldErrorCount) {
        printf("ERROR: Buffer overrun error not reported\n");
        SAFE_FREE(testPtr3);
        return FALSE;
    }
    
    printf("Buffer overrun detection working correctly\n");
    
    // Don't free testPtr3 as it's corrupted and would cause issues
#endif

    // Test error configuration functions
    printf("Testing error configuration functions...\n");
    
    EnableDoubleFreDetection(FALSE);
    if (IsDoubleFreDetectionEnabled()) {
        printf("ERROR: Double-free detection disable failed\n");
        return FALSE;
    }
    
    EnableUseAfterFreeDetection(FALSE);
    if (IsUseAfterFreeDetectionEnabled()) {
        printf("ERROR: Use-after-free detection disable failed\n");
        return FALSE;
    }
    
    EnableBufferOverrunDetection(FALSE);
    if (IsBufferOverrunDetectionEnabled()) {
        printf("ERROR: Buffer overrun detection disable failed\n");
        return FALSE;
    }
    
    // Re-enable for cleanup
    EnableDoubleFreDetection(TRUE);
    EnableUseAfterFreeDetection(TRUE);
    EnableBufferOverrunDetection(TRUE);
    
    printf("Error configuration functions working correctly\n");

    // Test memory corruption check
    printf("Testing memory corruption check...\n");
    CheckForMemoryCorruption(); // Should not crash or report errors for valid allocations
    printf("Memory corruption check completed\n");

    // Clear the test callback
    ClearMemoryErrorCallback();
    
    printf("=== MEMORY ERROR HANDLING TESTS PASSED ===\n");
    return TRUE;
}