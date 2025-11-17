#include "YouTubeCacher.h"

// Global DPI manager instance
DPIManager* g_dpiManager = NULL;

// Initialize DPI manager
DPIManager* CreateDPIManager(void) {
    DPIManager* manager = (DPIManager*)malloc(sizeof(DPIManager));
    if (!manager) {
        return NULL;
    }
    
    manager->mainWindow = NULL;
    manager->dialogs = NULL;
    manager->dialogCount = 0;
    manager->dialogCapacity = 0;
    
    InitializeCriticalSection(&manager->lock);
    
    return manager;
}

// Destroy DPI manager
void DestroyDPIManager(DPIManager* manager) {
    if (!manager) {
        return;
    }
    
    EnterCriticalSection(&manager->lock);
    
    // Free main window context
    if (manager->mainWindow) {
        free(manager->mainWindow);
        manager->mainWindow = NULL;
    }
    
    // Free dialog contexts
    if (manager->dialogs) {
        for (int i = 0; i < manager->dialogCount; i++) {
            if (manager->dialogs[i]) {
                free(manager->dialogs[i]);
            }
        }
        free(manager->dialogs);
        manager->dialogs = NULL;
    }
    
    LeaveCriticalSection(&manager->lock);
    DeleteCriticalSection(&manager->lock);
    
    free(manager);
}

// Register window for DPI management
DPIContext* RegisterWindowForDPI(DPIManager* manager, HWND hwnd) {
    if (!manager || !hwnd) {
        return NULL;
    }
    
    EnterCriticalSection(&manager->lock);
    
    // Create new DPI context
    DPIContext* context = (DPIContext*)malloc(sizeof(DPIContext));
    if (!context) {
        LeaveCriticalSection(&manager->lock);
        return NULL;
    }
    
    context->hwnd = hwnd;
    context->currentDpi = GetWindowDPI(hwnd);
    context->baseDpi = 96;
    context->scaleFactor = (double)context->currentDpi / 96.0;
    
    // Get window rect in logical coordinates
    RECT physicalRect;
    GetWindowRect(hwnd, &physicalRect);
    PhysicalRectToLogical(&physicalRect, &context->logicalRect, context->currentDpi);
    
    // Check if this is the main window or a dialog
    HWND parent = GetParent(hwnd);
    if (!parent || parent == GetDesktopWindow()) {
        // This is a top-level window, treat as main window if not already set
        if (!manager->mainWindow) {
            manager->mainWindow = context;
        } else {
            // Add to dialogs array
            if (manager->dialogCount >= manager->dialogCapacity) {
                int newCapacity = manager->dialogCapacity == 0 ? 4 : manager->dialogCapacity * 2;
                DPIContext** newDialogs = (DPIContext**)realloc(manager->dialogs, 
                                                                newCapacity * sizeof(DPIContext*));
                if (!newDialogs) {
                    free(context);
                    LeaveCriticalSection(&manager->lock);
                    return NULL;
                }
                manager->dialogs = newDialogs;
                manager->dialogCapacity = newCapacity;
            }
            manager->dialogs[manager->dialogCount++] = context;
        }
    } else {
        // Add to dialogs array
        if (manager->dialogCount >= manager->dialogCapacity) {
            int newCapacity = manager->dialogCapacity == 0 ? 4 : manager->dialogCapacity * 2;
            DPIContext** newDialogs = (DPIContext**)realloc(manager->dialogs, 
                                                            newCapacity * sizeof(DPIContext*));
            if (!newDialogs) {
                free(context);
                LeaveCriticalSection(&manager->lock);
                return NULL;
            }
            manager->dialogs = newDialogs;
            manager->dialogCapacity = newCapacity;
        }
        manager->dialogs[manager->dialogCount++] = context;
    }
    
    LeaveCriticalSection(&manager->lock);
    return context;
}

// Unregister window from DPI management
void UnregisterWindowForDPI(DPIManager* manager, HWND hwnd) {
    if (!manager || !hwnd) {
        return;
    }
    
    EnterCriticalSection(&manager->lock);
    
    // Check if it's the main window
    if (manager->mainWindow && manager->mainWindow->hwnd == hwnd) {
        free(manager->mainWindow);
        manager->mainWindow = NULL;
        LeaveCriticalSection(&manager->lock);
        return;
    }
    
    // Search in dialogs array
    for (int i = 0; i < manager->dialogCount; i++) {
        if (manager->dialogs[i] && manager->dialogs[i]->hwnd == hwnd) {
            free(manager->dialogs[i]);
            
            // Shift remaining elements
            for (int j = i; j < manager->dialogCount - 1; j++) {
                manager->dialogs[j] = manager->dialogs[j + 1];
            }
            manager->dialogCount--;
            break;
        }
    }
    
    LeaveCriticalSection(&manager->lock);
}

// Get DPI context for window
DPIContext* GetDPIContext(DPIManager* manager, HWND hwnd) {
    if (!manager || !hwnd) {
        return NULL;
    }
    
    EnterCriticalSection(&manager->lock);
    
    // Check main window
    if (manager->mainWindow && manager->mainWindow->hwnd == hwnd) {
        DPIContext* context = manager->mainWindow;
        LeaveCriticalSection(&manager->lock);
        return context;
    }
    
    // Search in dialogs
    for (int i = 0; i < manager->dialogCount; i++) {
        if (manager->dialogs[i] && manager->dialogs[i]->hwnd == hwnd) {
            DPIContext* context = manager->dialogs[i];
            LeaveCriticalSection(&manager->lock);
            return context;
        }
    }
    
    LeaveCriticalSection(&manager->lock);
    return NULL;
}

// Get DPI for window (enhanced version with fallbacks)
int GetWindowDPI(HWND hwnd) {
    if (!hwnd) {
        return 96;  // Default DPI
    }
    
    // Try GetDpiForWindow (Windows 10 1607+)
    typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    
    if (hUser32) {
        GetDpiForWindowFunc pGetDpiForWindow = 
            (GetDpiForWindowFunc)(void*)GetProcAddress(hUser32, "GetDpiForWindow");
        
        if (pGetDpiForWindow) {
            UINT dpi = pGetDpiForWindow(hwnd);
            if (dpi > 0) {
                return (int)dpi;
            }
        }
    }
    
    // Fall back to monitor DPI
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (hMonitor) {
        int monitorDpi = GetMonitorDPI(hMonitor);
        if (monitorDpi > 0) {
            return monitorDpi;
        }
    }
    
    // Fall back to system DPI
    HDC hdc = GetDC(NULL);
    if (hdc) {
        int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(NULL, hdc);
        return dpi;
    }
    
    return 96;  // Ultimate fallback
}

// Get DPI for monitor
int GetMonitorDPI(HMONITOR hMonitor) {
    if (!hMonitor) {
        return 96;
    }
    
    // Try GetDpiForMonitor (Windows 8.1+)
    typedef HRESULT (WINAPI *GetDpiForMonitorFunc)(HMONITOR, int, UINT*, UINT*);
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    
    if (hShcore) {
        GetDpiForMonitorFunc pGetDpiForMonitor = 
            (GetDpiForMonitorFunc)(void*)GetProcAddress(hShcore, "GetDpiForMonitor");
        
        if (pGetDpiForMonitor) {
            UINT dpiX = 0, dpiY = 0;
            // MDT_EFFECTIVE_DPI = 0
            if (SUCCEEDED(pGetDpiForMonitor(hMonitor, 0, &dpiX, &dpiY))) {
                FreeLibrary(hShcore);
                return (int)dpiX;
            }
        }
        FreeLibrary(hShcore);
    }
    
    // Fall back to system DPI
    HDC hdc = GetDC(NULL);
    if (hdc) {
        int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(NULL, hdc);
        return dpi;
    }
    
    return 96;
}

// Get DPI for point on screen
int GetDPIForPoint(POINT pt) {
    HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    return GetMonitorDPI(hMonitor);
}

// Get scale factor for window
double GetWindowScaleFactor(HWND hwnd) {
    int dpi = GetWindowDPI(hwnd);
    return (double)dpi / 96.0;
}

// Convert logical to physical coordinates
int LogicalToPhysical(int logical, int dpi) {
    return MulDiv(logical, dpi, 96);
}

// Convert physical to logical coordinates
int PhysicalToLogical(int physical, int dpi) {
    return MulDiv(physical, 96, dpi);
}

// Convert logical rect to physical rect
void LogicalRectToPhysical(const RECT* logical, RECT* physical, int dpi) {
    if (!logical || !physical) {
        return;
    }
    
    physical->left = LogicalToPhysical(logical->left, dpi);
    physical->top = LogicalToPhysical(logical->top, dpi);
    physical->right = LogicalToPhysical(logical->right, dpi);
    physical->bottom = LogicalToPhysical(logical->bottom, dpi);
}

// Convert physical rect to logical rect
void PhysicalRectToLogical(const RECT* physical, RECT* logical, int dpi) {
    if (!physical || !logical) {
        return;
    }
    
    logical->left = PhysicalToLogical(physical->left, dpi);
    logical->top = PhysicalToLogical(physical->top, dpi);
    logical->right = PhysicalToLogical(physical->right, dpi);
    logical->bottom = PhysicalToLogical(physical->bottom, dpi);
}

// Scale value for DPI (enhanced version)
int ScaleValueForDPI(int value, int dpi) {
    return MulDiv(value, dpi, 96);
}

// Scale value for DPI with floating point precision
double ScaleValueForDPIFloat(double value, int dpi) {
    return value * ((double)dpi / 96.0);
}
