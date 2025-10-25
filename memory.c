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
            printf("  Thread ID: %lu\n", alloc->threadId);
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