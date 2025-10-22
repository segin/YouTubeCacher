# Design Document

## Overview

This design addresses the memory management concerns in YouTubeCacher by implementing systematic resource management patterns, memory leak detection, and wrapper functions for common allocation scenarios. The current implementation uses manual memory management throughout with many malloc()/free() calls, _wcsdup() operations, and complex error paths that can lead to memory leaks.

The design focuses on creating a robust memory management system that provides RAII-style patterns, automatic cleanup, leak detection, and memory pools while maintaining compatibility with the existing codebase.

## Architecture

### Current Memory Management Analysis

The existing codebase shows these memory management patterns:
- **Direct malloc/free**: Used in ~30+ locations for structures like YtDlpRequest, ProgressDialog, CacheEntry
- **String duplication**: Heavy use of _wcsdup() for string management (~25+ locations)
- **Complex cleanup**: Manual cleanup in error paths with potential for leaks
- **Array allocations**: Dynamic arrays for tracked files, messages, subtitle files
- **Windows API memory**: CoTaskMemFree() for Windows API allocated memory
- **No leak detection**: No systematic tracking of allocations/deallocations

### Proposed Memory Management Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                Memory Manager (memory.c)                   │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │   Allocation    │  │   Leak Detection│  │   Memory    │ │
│  │   Wrappers      │  │   & Tracking    │  │   Pools     │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                RAII Resource Management                     │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │   Auto String   │  │   Auto Array    │  │   Auto      │ │
│  │   Management    │  │   Management    │  │   Cleanup   │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                Error-Safe Allocation                        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │   Safe Malloc   │  │   Safe Realloc  │  │   Bulk      │ │
│  │   & Free        │  │   & Resize      │  │   Cleanup   │ │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## Components and Interfaces

### 1. Core Memory Manager (memory.c/memory.h)

**Purpose**: Central memory allocation and tracking system

**Interface**:
```c
// Memory manager initialization and cleanup
BOOL InitializeMemoryManager(void);
void CleanupMemoryManager(void);

// Safe allocation functions
void* SafeMalloc(size_t size, const char* file, int line);
void* SafeCalloc(size_t count, size_t size, const char* file, int line);
void* SafeRealloc(void* ptr, size_t size, const char* file, int line);
void SafeFree(void* ptr, const char* file, int line);

// Convenience macros for automatic file/line tracking
#define SAFE_MALLOC(size) SafeMalloc(size, __FILE__, __LINE__)
#define SAFE_CALLOC(count, size) SafeCalloc(count, size, __FILE__, __LINE__)
#define SAFE_REALLOC(ptr, size) SafeRealloc(ptr, size, __FILE__, __LINE__)
#define SAFE_FREE(ptr) SafeFree(ptr, __FILE__, __LINE__)

// Memory leak detection
typedef struct {
    void* address;
    size_t size;
    const char* file;
    int line;
    DWORD threadId;
    SYSTEMTIME allocTime;
} AllocationInfo;

BOOL EnableLeakDetection(BOOL enable);
void DumpMemoryLeaks(void);
size_t GetCurrentMemoryUsage(void);
int GetActiveAllocationCount(void);
```

### 2. String Management (memory.c)

**Purpose**: Safe string allocation and management

**Interface**:
```c
// Safe string functions
wchar_t* SafeWcsDup(const wchar_t* str, const char* file, int line);
wchar_t* SafeWcsNDup(const wchar_t* str, size_t maxLen, const char* file, int line);
wchar_t* SafeWcsConcat(const wchar_t* str1, const wchar_t* str2, const char* file, int line);
BOOL SafeWcsReplace(wchar_t** target, const wchar_t* newValue, const char* file, int line);

// Convenience macros
#define SAFE_WCSDUP(str) SafeWcsDup(str, __FILE__, __LINE__)
#define SAFE_WCSNDUP(str, len) SafeWcsNDup(str, len, __FILE__, __LINE__)
#define SAFE_WCSCONCAT(str1, str2) SafeWcsConcat(str1, str2, __FILE__, __LINE__)
#define SAFE_WCSREPLACE(target, newValue) SafeWcsReplace(target, newValue, __FILE__, __LINE__)

// String builder for efficient concatenation
typedef struct {
    wchar_t* buffer;
    size_t capacity;
    size_t length;
} StringBuilder;

StringBuilder* CreateStringBuilder(size_t initialCapacity);
BOOL AppendToStringBuilder(StringBuilder* sb, const wchar_t* str);
BOOL AppendFormatToStringBuilder(StringBuilder* sb, const wchar_t* format, ...);
wchar_t* FinalizeStringBuilder(StringBuilder* sb); // Returns ownership of buffer
void FreeStringBuilder(StringBuilder* sb);
```

### 3. RAII Resource Management (memory.c)

**Purpose**: Automatic resource cleanup using RAII patterns

**Interface**:
```c
// RAII structure wrapper
typedef struct {
    void* resource;
    void (*cleanup)(void* resource);
    const char* file;
    int line;
} AutoResource;

// RAII macros for automatic cleanup
#define AUTO_RESOURCE(type, cleanup_func) \
    __attribute__((cleanup(AutoResourceCleanup))) AutoResource

#define INIT_AUTO_RESOURCE(resource, cleanup_func) \
    { resource, cleanup_func, __FILE__, __LINE__ }

void AutoResourceCleanup(AutoResource* autoRes);

// Specific RAII wrappers for common types
typedef struct {
    wchar_t* str;
} AutoString;

typedef struct {
    void** array;
    size_t count;
    void (*elementCleanup)(void*);
} AutoArray;

#define AUTO_STRING __attribute__((cleanup(AutoStringCleanup))) AutoString
#define AUTO_ARRAY __attribute__((cleanup(AutoArrayCleanup))) AutoArray

void AutoStringCleanup(AutoString* autoStr);
void AutoArrayCleanup(AutoArray* autoArray);
```

### 4. Memory Pools (memory.c)

**Purpose**: Efficient allocation for frequently used objects

**Interface**:
```c
// Memory pool management
typedef struct MemoryPool MemoryPool;

MemoryPool* CreateMemoryPool(size_t objectSize, size_t initialCount);
void* AllocateFromPool(MemoryPool* pool);
void ReturnToPool(MemoryPool* pool, void* object);
void DestroyMemoryPool(MemoryPool* pool);

// Pre-defined pools for common objects
extern MemoryPool* g_stringPool;        // For small strings (< 256 chars)
extern MemoryPool* g_cacheEntryPool;    // For CacheEntry structures
extern MemoryPool* g_requestPool;       // For YtDlpRequest structures

// Pool initialization/cleanup
BOOL InitializeMemoryPools(void);
void CleanupMemoryPools(void);
```

### 5. Error-Safe Allocation Patterns (memory.c)

**Purpose**: Allocation patterns that handle errors gracefully

**Interface**:
```c
// Multi-allocation with automatic cleanup on failure
typedef struct {
    void** allocations;
    size_t count;
    size_t capacity;
} AllocationSet;

AllocationSet* CreateAllocationSet(void);
BOOL AddToAllocationSet(AllocationSet* set, void* ptr);
void CommitAllocationSet(AllocationSet* set); // Transfers ownership
void RollbackAllocationSet(AllocationSet* set); // Frees all allocations
void FreeAllocationSet(AllocationSet* set);

// Bulk operations
typedef struct {
    void** pointers;
    size_t count;
} BulkCleanup;

void BulkFree(BulkCleanup* cleanup);
void AddToBulkCleanup(BulkCleanup* cleanup, void* ptr);
```

## Data Models

### Memory Tracking Structure

```c
typedef struct {
    // Allocation tracking
    AllocationInfo* allocations;
    size_t allocationCount;
    size_t allocationCapacity;
    
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
} MemoryManager;
```

### Pool Structure

```c
typedef struct MemoryPool {
    void* memory;           // Pre-allocated memory block
    void** freeList;        // Stack of free objects
    size_t objectSize;      // Size of each object
    size_t totalObjects;    // Total objects in pool
    size_t freeCount;       // Number of free objects
    CRITICAL_SECTION lock;  // Thread safety
} MemoryPool;
```

### RAII Integration

```c
// Integration with existing structures
typedef struct {
    YtDlpRequest base;
    AutoResource autoRes;
} AutoYtDlpRequest;

typedef struct {
    CacheEntry base;
    AutoResource autoRes;
} AutoCacheEntry;

// Factory functions for RAII objects
AutoYtDlpRequest* CreateAutoYtDlpRequest(YtDlpOperation operation, const wchar_t* url, const wchar_t* outputPath);
AutoCacheEntry* CreateAutoCacheEntry(const wchar_t* videoId);
```

## Error Handling

### Memory Error Classification

1. **Allocation Failures**: Out of memory, invalid size requests
2. **Double Free**: Attempting to free already freed memory
3. **Use After Free**: Accessing freed memory
4. **Memory Leaks**: Allocated memory not freed
5. **Buffer Overruns**: Writing beyond allocated boundaries

### Error Recovery Strategies

```c
typedef enum {
    MEMORY_ERROR_ALLOCATION_FAILED,
    MEMORY_ERROR_DOUBLE_FREE,
    MEMORY_ERROR_USE_AFTER_FREE,
    MEMORY_ERROR_BUFFER_OVERRUN,
    MEMORY_ERROR_LEAK_DETECTED
} MemoryErrorType;

typedef struct {
    MemoryErrorType type;
    void* address;
    size_t size;
    const char* file;
    int line;
    wchar_t* description;
} MemoryError;

// Error handling callback
typedef void (*MemoryErrorCallback)(const MemoryError* error);
void SetMemoryErrorCallback(MemoryErrorCallback callback);
```

## Testing Strategy

### Unit Testing Approach

1. **Allocation/Deallocation**: Test all wrapper functions
2. **Leak Detection**: Verify leak detection accuracy
3. **RAII Patterns**: Test automatic cleanup
4. **Memory Pools**: Test pool allocation/deallocation
5. **Error Scenarios**: Test out-of-memory handling

### Integration Testing

1. **Existing Code Migration**: Test gradual migration of existing allocations
2. **Performance Impact**: Measure overhead of tracking
3. **Thread Safety**: Test concurrent allocations
4. **Error Recovery**: Test cleanup in error scenarios

### Memory Testing Tools

```c
// Test utilities
void EnableMemoryTestMode(BOOL enable);
void SimulateOutOfMemory(BOOL enable);
void InjectAllocationFailure(int failureCount);
MemoryStatistics* GetMemoryStatistics(void);
```

## Implementation Notes

### Migration Strategy

1. **Phase 1**: Implement core memory manager and basic wrappers
2. **Phase 2**: Add leak detection and tracking
3. **Phase 3**: Implement RAII patterns for new code
4. **Phase 4**: Gradually migrate existing allocations
5. **Phase 5**: Add memory pools for performance optimization
6. **Phase 6**: Full integration and testing

### Backward Compatibility

- Existing malloc/free calls continue to work during migration
- Gradual replacement with safe wrappers
- Optional leak detection (disabled by default in release builds)
- No changes to external interfaces

### Performance Considerations

- **Debug vs Release**: Full tracking in debug, minimal overhead in release
- **Memory Pools**: Reduce allocation overhead for frequent operations
- **Lazy Initialization**: Initialize tracking structures only when needed
- **Efficient Data Structures**: Use hash tables for fast allocation lookup

### Build Integration

```makefile
# Debug build with full memory tracking
CFLAGS_DEBUG = -DMEMORY_DEBUG -DLEAK_DETECTION

# Release build with minimal overhead
CFLAGS_RELEASE = -DNDEBUG -DMEMORY_RELEASE

# Memory manager object
memory.o: memory.c memory.h
	$(CC) $(CFLAGS) -c memory.c -o memory.o

# Update main objects to depend on memory.h
main.o: main.c memory.h
cache.o: cache.c memory.h
parser.o: parser.c memory.h
```

### Security Considerations

- **Buffer Overflow Protection**: Size validation in all allocations
- **Use-After-Free Detection**: Mark freed memory in debug builds
- **Information Leakage**: Clear sensitive data before freeing
- **Integer Overflow**: Validate size calculations before allocation

This design provides a comprehensive memory management system that addresses all current issues while maintaining compatibility and providing a path for gradual migration of the existing codebase.