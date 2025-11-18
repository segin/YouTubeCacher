#include "YouTubeCacher.h"

// Global DPI manager instance
DPIManager* g_dpiManager = NULL;

// Initialize DPI awareness with fallbacks for different Windows versions
void InitializeDPIAwareness(void) {
    // Try Per-Monitor V2 (Windows 10 1703+)
    typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    
    if (hUser32) {
        SetProcessDpiAwarenessContextFunc pSetProcessDpiAwarenessContext = 
            (SetProcessDpiAwarenessContextFunc)(void*)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        
        if (pSetProcessDpiAwarenessContext) {
            // Try Per-Monitor V2 first
            if (pSetProcessDpiAwarenessContext((DPI_AWARENESS_CONTEXT)-4)) {  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
                return;
            }
            // Fall back to Per-Monitor V1
            if (pSetProcessDpiAwarenessContext((DPI_AWARENESS_CONTEXT)-3)) {  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE
                return;
            }
        }
    }
    
    // Try Windows 8.1 API
    typedef HRESULT (WINAPI *SetProcessDpiAwarenessFunc)(int);
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        SetProcessDpiAwarenessFunc pSetProcessDpiAwareness = 
            (SetProcessDpiAwarenessFunc)(void*)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        
        if (pSetProcessDpiAwareness) {
            // PROCESS_PER_MONITOR_DPI_AWARE = 2
            pSetProcessDpiAwareness(2);
            FreeLibrary(hShcore);
            return;
        }
        FreeLibrary(hShcore);
    }
    
    // Fall back to Vista/7 system DPI awareness
    SetProcessDPIAware();
}

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
        if (manager->mainWindow->fontManager) {
            DestroyFontManager(manager->mainWindow->fontManager);
        }
        free(manager->mainWindow);
        manager->mainWindow = NULL;
    }
    
    // Free dialog contexts
    if (manager->dialogs) {
        for (int i = 0; i < manager->dialogCount; i++) {
            if (manager->dialogs[i]) {
                if (manager->dialogs[i]->fontManager) {
                    DestroyFontManager(manager->dialogs[i]->fontManager);
                }
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
    context->fontManager = CreateFontManager();
    
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
        if (manager->mainWindow->fontManager) {
            DestroyFontManager(manager->mainWindow->fontManager);
        }
        free(manager->mainWindow);
        manager->mainWindow = NULL;
        LeaveCriticalSection(&manager->lock);
        return;
    }
    
    // Search in dialogs array
    for (int i = 0; i < manager->dialogCount; i++) {
        if (manager->dialogs[i] && manager->dialogs[i]->hwnd == hwnd) {
            if (manager->dialogs[i]->fontManager) {
                DestroyFontManager(manager->dialogs[i]->fontManager);
            }
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

// Rescale window for DPI change
void RescaleWindowForDPI(HWND hwnd, int oldDpi, int newDpi) {
    if (!hwnd || oldDpi <= 0 || newDpi <= 0 || oldDpi == newDpi) {
        return;
    }
    
    // Calculate scale ratio
    double scaleRatio = (double)newDpi / (double)oldDpi;
    
    // Rescale all child controls
    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        RECT rect;
        GetWindowRect(hChild, &rect);
        MapWindowPoints(HWND_DESKTOP, hwnd, (POINT*)&rect, 2);
        
        // Scale position and size
        int newX = (int)(rect.left * scaleRatio);
        int newY = (int)(rect.top * scaleRatio);
        int newWidth = (int)((rect.right - rect.left) * scaleRatio);
        int newHeight = (int)((rect.bottom - rect.top) * scaleRatio);
        
        SetWindowPos(hChild, NULL, newX, newY, newWidth, newHeight,
                    SWP_NOZORDER | SWP_NOACTIVATE);
        
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
    
    // Rescale fonts
    RescaleFontsForDPI(hwnd, newDpi);
    
    // Reload icons at new size
    ReloadIconsForDPI(hwnd, newDpi);
    
    // Force redraw
    InvalidateRect(hwnd, NULL, TRUE);
}

// Create font manager
FontManager* CreateFontManager(void) {
    FontManager* manager = (FontManager*)malloc(sizeof(FontManager));
    if (!manager) {
        return NULL;
    }
    
    manager->fonts = NULL;
    manager->count = 0;
    manager->capacity = 0;
    
    return manager;
}

// Destroy font manager
void DestroyFontManager(FontManager* manager) {
    if (!manager) {
        return;
    }
    
    // Destroy all fonts
    for (int i = 0; i < manager->count; i++) {
        if (manager->fonts[i]) {
            DestroyScalableFont(manager->fonts[i]);
        }
    }
    
    // Free fonts array
    if (manager->fonts) {
        free(manager->fonts);
    }
    
    free(manager);
}

// Create scalable font
ScalableFont* CreateScalableFont(const wchar_t* faceName, int pointSize, int weight, int dpi) {
    if (!faceName || pointSize <= 0 || dpi <= 0) {
        return NULL;
    }
    
    ScalableFont* font = (ScalableFont*)malloc(sizeof(ScalableFont));
    if (!font) {
        return NULL;
    }
    
    // Initialize LOGFONT structure
    ZeroMemory(&font->logFont, sizeof(LOGFONTW));
    font->logFont.lfHeight = -MulDiv(pointSize, dpi, 72);
    font->logFont.lfWeight = weight;
    font->logFont.lfCharSet = DEFAULT_CHARSET;
    font->logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    font->logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    font->logFont.lfQuality = CLEARTYPE_QUALITY;
    font->logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcsncpy(font->logFont.lfFaceName, faceName, LF_FACESIZE - 1);
    font->logFont.lfFaceName[LF_FACESIZE - 1] = L'\0';
    
    // Create the font
    font->hFont = CreateFontIndirectW(&font->logFont);
    if (!font->hFont) {
        free(font);
        return NULL;
    }
    
    font->pointSize = pointSize;
    font->dpi = dpi;
    
    return font;
}

// Destroy scalable font
void DestroyScalableFont(ScalableFont* font) {
    if (!font) {
        return;
    }
    
    if (font->hFont) {
        DeleteObject(font->hFont);
    }
    
    free(font);
}

// Get font for DPI (creates new font if DPI changed)
HFONT GetFontForDPI(ScalableFont* font, int dpi) {
    if (!font || dpi <= 0) {
        return NULL;
    }
    
    // If DPI matches, return existing font
    if (font->dpi == dpi) {
        return font->hFont;
    }
    
    // Create new font for new DPI
    LOGFONTW lf = font->logFont;
    lf.lfHeight = -MulDiv(font->pointSize, dpi, 72);
    
    HFONT hNewFont = CreateFontIndirectW(&lf);
    if (hNewFont) {
        // Delete old font and replace
        if (font->hFont) {
            DeleteObject(font->hFont);
        }
        font->hFont = hNewFont;
        font->dpi = dpi;
    }
    
    return font->hFont;
}

// Set control font
void SetControlFont(HWND hwnd, ScalableFont* font, int dpi) {
    if (!hwnd || !font) {
        return;
    }
    
    HFONT hFont = GetFontForDPI(font, dpi);
    if (hFont) {
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
}

// Add font to manager
BOOL AddFontToManager(FontManager* manager, ScalableFont* font) {
    if (!manager || !font) {
        return FALSE;
    }
    
    // Expand array if needed
    if (manager->count >= manager->capacity) {
        int newCapacity = manager->capacity == 0 ? 4 : manager->capacity * 2;
        ScalableFont** newFonts = (ScalableFont**)realloc(manager->fonts, 
                                                           newCapacity * sizeof(ScalableFont*));
        if (!newFonts) {
            return FALSE;
        }
        manager->fonts = newFonts;
        manager->capacity = newCapacity;
    }
    
    manager->fonts[manager->count++] = font;
    return TRUE;
}

// Create and register font with window's font manager
ScalableFont* CreateAndRegisterFont(HWND hwnd, const wchar_t* faceName, int pointSize, int weight) {
    if (!hwnd || !faceName || !g_dpiManager) {
        return NULL;
    }
    
    // Get DPI context for window
    DPIContext* context = GetDPIContext(g_dpiManager, hwnd);
    if (!context || !context->fontManager) {
        return NULL;
    }
    
    // Create font at current DPI
    ScalableFont* font = CreateScalableFont(faceName, pointSize, weight, context->currentDpi);
    if (!font) {
        return NULL;
    }
    
    // Add to font manager
    if (!AddFontToManager(context->fontManager, font)) {
        DestroyScalableFont(font);
        return NULL;
    }
    
    return font;
}

// Rescale fonts for DPI
void RescaleFontsForDPI(HWND hwnd, int dpi) {
    if (!hwnd || dpi <= 0) {
        return;
    }
    
    // Get DPI context for this window
    DPIContext* context = GetDPIContext(g_dpiManager, hwnd);
    if (!context || !context->fontManager) {
        return;
    }
    
    // Update all fonts in the font manager for the new DPI
    for (int i = 0; i < context->fontManager->count; i++) {
        ScalableFont* font = context->fontManager->fonts[i];
        if (font) {
            GetFontForDPI(font, dpi);
        }
    }
    
    // Enumerate all child controls and update their fonts
    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        // Get the current font of the control
        HFONT hCurrentFont = (HFONT)SendMessageW(hChild, WM_GETFONT, 0, 0);
        
        // Find matching scalable font in font manager
        for (int i = 0; i < context->fontManager->count; i++) {
            ScalableFont* font = context->fontManager->fonts[i];
            if (font && font->hFont == hCurrentFont) {
                // Update this control with the rescaled font
                SetControlFont(hChild, font, dpi);
                break;
            }
        }
        
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

// Create icon manager
IconManager* CreateIconManager(void) {
    IconManager* manager = (IconManager*)malloc(sizeof(IconManager));
    if (!manager) {
        return NULL;
    }
    
    manager->icons = NULL;
    manager->count = 0;
    manager->capacity = 0;
    
    return manager;
}

// Destroy icon manager
void DestroyIconManager(IconManager* manager) {
    if (!manager) {
        return;
    }
    
    // Destroy all icons
    for (int i = 0; i < manager->count; i++) {
        if (manager->icons[i]) {
            DestroyScalableIcon(manager->icons[i]);
        }
    }
    
    // Free icons array
    if (manager->icons) {
        free(manager->icons);
    }
    
    free(manager);
}

// Create scalable icon
ScalableIcon* CreateScalableIcon(int resourceId) {
    if (resourceId <= 0) {
        return NULL;
    }
    
    ScalableIcon* icon = (ScalableIcon*)malloc(sizeof(ScalableIcon));
    if (!icon) {
        return NULL;
    }
    
    icon->resourceId = resourceId;
    icon->sizes = NULL;
    icon->sizeCount = 0;
    icon->sizeCapacity = 0;
    
    return icon;
}

// Destroy scalable icon
void DestroyScalableIcon(ScalableIcon* icon) {
    if (!icon) {
        return;
    }
    
    // Destroy all icon handles
    for (int i = 0; i < icon->sizeCount; i++) {
        if (icon->sizes[i].hIcon) {
            DestroyIcon(icon->sizes[i].hIcon);
        }
    }
    
    // Free sizes array
    if (icon->sizes) {
        free(icon->sizes);
    }
    
    free(icon);
}

// Add icon to manager
BOOL AddIconToManager(IconManager* manager, ScalableIcon* icon) {
    if (!manager || !icon) {
        return FALSE;
    }
    
    // Expand array if needed
    if (manager->count >= manager->capacity) {
        int newCapacity = manager->capacity == 0 ? 4 : manager->capacity * 2;
        ScalableIcon** newIcons = (ScalableIcon**)realloc(manager->icons, 
                                                           newCapacity * sizeof(ScalableIcon*));
        if (!newIcons) {
            return FALSE;
        }
        manager->icons = newIcons;
        manager->capacity = newCapacity;
    }
    
    manager->icons[manager->count++] = icon;
    return TRUE;
}

// Get icon size for DPI
int GetIconSizeForDPI(int baseSizeLogical, int dpi) {
    if (baseSizeLogical <= 0 || dpi <= 0) {
        return 16;  // Default icon size
    }
    
    return ScaleValueForDPI(baseSizeLogical, dpi);
}

// Load icon at appropriate size for DPI
HICON LoadIconForDPI(int resourceId, int dpi) {
    if (resourceId <= 0 || dpi <= 0) {
        return NULL;
    }
    
    // Calculate desired icon size (base size is 16x16 at 96 DPI)
    int desiredSize = GetIconSizeForDPI(16, dpi);
    
    // Available sizes in resource (standard Windows icon sizes)
    int availableSizes[] = {16, 20, 24, 32, 48, 64};
    int sizeCount = sizeof(availableSizes) / sizeof(availableSizes[0]);
    
    // Find closest size (prefer larger to avoid upscaling)
    int bestSize = availableSizes[0];
    for (int i = 0; i < sizeCount; i++) {
        if (availableSizes[i] >= desiredSize) {
            bestSize = availableSizes[i];
            break;
        }
        bestSize = availableSizes[i];
    }
    
    // Load icon at best size
    HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL),
                                     MAKEINTRESOURCEW(resourceId),
                                     IMAGE_ICON,
                                     bestSize, bestSize,
                                     LR_DEFAULTCOLOR);
    
    // If loading failed, try default size
    if (!hIcon) {
        hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL),
                                   MAKEINTRESOURCEW(resourceId),
                                   IMAGE_ICON,
                                   0, 0,  // Use default size
                                   LR_DEFAULTCOLOR);
    }
    
    return hIcon;
}

// Set icon on control with DPI awareness
void SetControlIcon(HWND hwnd, int resourceId, int dpi) {
    if (!hwnd || resourceId <= 0 || dpi <= 0) {
        return;
    }
    
    // Load icon at appropriate size
    HICON hIcon = LoadIconForDPI(resourceId, dpi);
    if (!hIcon) {
        return;
    }
    
    // Get the old icon to destroy it later
    HICON hOldIcon = (HICON)SendMessageW(hwnd, STM_GETICON, 0, 0);
    
    // Set the new icon
    SendMessageW(hwnd, STM_SETICON, (WPARAM)hIcon, 0);
    
    // Destroy the old icon if it exists
    if (hOldIcon) {
        DestroyIcon(hOldIcon);
    }
}

// Reload icons for DPI change
void ReloadIconsForDPI(HWND hwnd, int dpi) {
    if (!hwnd || dpi <= 0) {
        return;
    }
    
    // Enumerate all child controls
    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        // Get window class name to identify static controls
        wchar_t className[256];
        GetClassNameW(hChild, className, 256);
        
        // Check if this is a static control (which can display icons)
        if (_wcsicmp(className, L"Static") == 0) {
            // Get the control style
            LONG_PTR style = GetWindowLongPtrW(hChild, GWL_STYLE);
            
            // Check if it has SS_ICON style
            if ((style & SS_ICON) == SS_ICON) {
                // Get the current icon
                HICON hCurrentIcon = (HICON)SendMessageW(hChild, STM_GETICON, 0, 0);
                
                if (hCurrentIcon) {
                    // We need to determine the resource ID of the icon
                    // Since we can't easily get the resource ID from the icon handle,
                    // we'll need to store this information in the window's user data
                    // or use a different approach
                    
                    // For now, we'll just note that icons need to be reloaded
                    // The actual resource ID should be stored when the icon is first set
                    // This will be handled in task 6.5 when integrating with windows
                    
                    // Get resource ID from window data (if stored)
                    int resourceId = (int)GetWindowLongPtrW(hChild, GWLP_USERDATA);
                    if (resourceId > 0) {
                        // Reload icon at new DPI
                        SetControlIcon(hChild, resourceId, dpi);
                    }
                }
            }
        }
        
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

// Helper function to set icon on control and store resource ID for DPI changes
// This should be used instead of directly calling STM_SETICON
void SetControlIconWithTracking(HWND hwnd, int resourceId, int dpi) {
    if (!hwnd || resourceId <= 0 || dpi <= 0) {
        return;
    }
    
    // Store the resource ID in the control's user data for later DPI changes
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)resourceId);
    
    // Set the icon at the appropriate DPI
    SetControlIcon(hwnd, resourceId, dpi);
}

// Helper function to initialize icon on a static control with DPI awareness
// This is a convenience function for dialog initialization
BOOL InitializeIconControl(HWND hDialog, int controlId, int resourceId) {
    if (!hDialog || controlId <= 0 || resourceId <= 0) {
        return FALSE;
    }
    
    // Get the control handle
    HWND hControl = GetDlgItem(hDialog, controlId);
    if (!hControl) {
        return FALSE;
    }
    
    // Get current DPI for the dialog
    int dpi = GetWindowDPI(hDialog);
    
    // Set the icon with tracking
    SetControlIconWithTracking(hControl, resourceId, dpi);
    
    return TRUE;
}
