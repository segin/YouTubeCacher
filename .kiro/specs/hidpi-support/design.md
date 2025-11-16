# Design Document: HiDPI Support

## Overview

This design document outlines comprehensive HiDPI (High Dots Per Inch) support for YouTubeCacher to ensure proper display on high-resolution monitors and seamless scaling across different DPI settings. The application currently has basic DPI scaling implemented with `GetDpiForWindowSafe` and `ScaleForDpi` functions used in dialogs. This design extends that foundation to support per-monitor DPI awareness v2, dynamic DPI changes, proper icon scaling, and comprehensive testing across multiple DPI settings.

## Architecture

### Current System Analysis

**Existing Implementation**:
1. **DPI Awareness**: Basic system-wide DPI awareness via `SetProcessDPIAware()`
2. **DPI Detection**: `GetDpiForWindowSafe()` function with fallback to system DPI
3. **Scaling Function**: `ScaleForDpi()` using `MulDiv` for proportional scaling
4. **Dialog Scaling**: Dialogs use DPI-aware layout calculations
5. **Base DPI**: All measurements defined at 96 DPI baseline

**Current Limitations**:
- System DPI awareness only (not per-monitor v2)
- No `WM_DPICHANGED` handling for dynamic DPI changes
- No icon scaling for different DPI settings
- Window position/size not saved in logical coordinates
- No comprehensive testing at multiple DPI settings
- Main window may not scale properly (only dialogs tested)

### Enhanced Architecture

We will implement a comprehensive HiDPI system with:

1. **Per-Monitor DPI Awareness v2**: Support different DPI on each monitor
2. **Dynamic DPI Handling**: Respond to DPI changes when moving between monitors
3. **Centralized DPI System**: Unified DPI management for all windows
4. **Icon Scaling**: Load appropriate icon sizes for current DPI
5. **Font Scaling**: Proper font size scaling with DPI
6. **Layout Preservation**: Save/restore window geometry in logical coordinates

## Components and Interfaces

### 1. DPI Awareness System

#### 1.1 Application Manifest

**File**: `YouTubeCacher.manifest` (already exists, needs update)

The project already has a manifest embedded via the resource file (`YouTubeCacher.rc`). We need to update the existing manifest to add per-monitor DPI awareness.

**Current manifest** has:
```xml
<dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>
```

**Update to**:
```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity
    version="1.0.0.0"
    processorArchitecture="*"
    name="YouTubeCacher"
    type="win32"
  />
  <description>YouTube Cacher - Video Download Manager</description>
  
  <!-- Enable Windows XP+ Visual Styles -->
  <dependency>
    <dependentAssembly>
      <assemblyIdentity
        type="win32"
        name="Microsoft.Windows.Common-Controls"
        version="6.0.0.0"
        processorArchitecture="*"
        publicKeyToken="6595b64144ccf1df"
        language="*"
      />
    </dependentAssembly>
  </dependency>
  
  <!-- Application settings -->
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <!-- DPI Awareness: Per-Monitor V2 (Windows 10 1703+) with fallback -->
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">permonitorv2</dpiAwareness>
      <!-- Long Path Awareness (Windows 10 version 1607+) -->
      <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
    </windowsSettings>
  </application>
  
  <!-- Security settings -->
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v2">
    <security>
      <requestedPrivileges xmlns="urn:schemas-microsoft-com:asm.v3">
        <requestedExecutionLevel level="asInvoker" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
</assembly>
```

**Integration**: The manifest is already embedded via `YouTubeCacher.rc` using `RT_MANIFEST` resource type. The GNU toolchain (windres) automatically embeds it during resource compilation. No Makefile changes needed.

#### 1.2 DPI Awareness Initialization

**Replace in main.c**:
```c
// Current: SetProcessDPIAware();

// New: Enhanced DPI awareness with fallback
void InitializeDPIAwareness(void) {
    // Try Per-Monitor V2 (Windows 10 1703+)
    typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    
    if (hUser32) {
        SetProcessDpiAwarenessContextFunc pSetProcessDpiAwarenessContext = 
            (SetProcessDpiAwarenessContextFunc)(void*)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        
        if (pSetProcessDpiAwarenessContext) {
            // Try Per-Monitor V2 first
            if (pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                return;
            }
            // Fall back to Per-Monitor V1
            if (pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
                return;
            }
        }
    }
    
    // Try Windows 8.1 API
    typedef HRESULT (WINAPI *SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        SetProcessDpiAwarenessFunc pSetProcessDpiAwareness = 
            (SetProcessDpiAwarenessFunc)(void*)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        
        if (pSetProcessDpiAwareness) {
            pSetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
            FreeLibrary(hShcore);
            return;
        }
        FreeLibrary(hShcore);
    }
    
    // Fall back to Vista/7 system DPI awareness
    SetProcessDPIAware();
}
```

### 2. Centralized DPI Management

#### 2.1 DPI Context Structure

**New in YouTubeCacher.h**:
```c
// DPI context for a window
typedef struct {
    HWND hwnd;
    int currentDpi;
    int baseDpi;  // Always 96
    double scaleFactor;
    RECT logicalRect;  // Window rect in logical coordinates
} DPIContext;

// Global DPI manager
typedef struct {
    DPIContext* mainWindow;
    DPIContext** dialogs;
    int dialogCount;
    int dialogCapacity;
} DPIManager;
```

#### 2.2 DPI Management Functions

**New file: dpi.c**:
```c
// Initialize DPI manager
DPIManager* CreateDPIManager(void);
void DestroyDPIManager(DPIManager* manager);

// Register/unregister windows for DPI management
DPIContext* RegisterWindowForDPI(DPIManager* manager, HWND hwnd);
void UnregisterWindowForDPI(DPIManager* manager, HWND hwnd);

// Get DPI for window (enhanced version)
int GetWindowDPI(HWND hwnd);

// Get scale factor for window
double GetWindowScaleFactor(HWND hwnd);

// Convert between logical and physical coordinates
int LogicalToPhysical(int logical, int dpi);
int PhysicalToLogical(int physical, int dpi);
void LogicalRectToPhysical(const RECT* logical, RECT* physical, int dpi);
void PhysicalRectToLogical(const RECT* physical, RECT* logical, int dpi);

// Scale value for DPI (enhanced version of existing ScaleForDpi)
int ScaleValueForDPI(int value, int dpi);
double ScaleValueForDPIFloat(double value, int dpi);

// Get DPI for monitor
int GetMonitorDPI(HMONITOR hMonitor);

// Get DPI for point on screen
int GetDPIForPoint(POINT pt);
```

**Implementation Notes**:
- `GetWindowDPI`: Use `GetDpiForWindow` (Windows 10 1607+) with fallback
- `GetMonitorDPI`: Use `GetDpiForMonitor` (Windows 8.1+) with fallback
- Store DPI contexts in global manager for easy access
- Thread-safe access using existing thread safety system

### 3. Dynamic DPI Change Handling

#### 3.1 WM_DPICHANGED Message Handler

**Add to MainWindowProc and dialog procedures**:
```c
case WM_DPICHANGED: {
    // Get new DPI from wParam
    int newDpi = HIWORD(wParam);
    
    // Get suggested window rect from lParam
    RECT* suggestedRect = (RECT*)lParam;
    
    // Update DPI context
    DPIContext* context = GetDPIContext(hwnd);
    if (context) {
        int oldDpi = context->currentDpi;
        context->currentDpi = newDpi;
        context->scaleFactor = (double)newDpi / 96.0;
        
        // Rescale all UI elements
        RescaleWindowForDPI(hwnd, oldDpi, newDpi);
        
        // Apply suggested window position and size
        SetWindowPos(hwnd, NULL,
                    suggestedRect->left,
                    suggestedRect->top,
                    suggestedRect->right - suggestedRect->left,
                    suggestedRect->bottom - suggestedRect->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    return 0;
}
```

#### 3.2 Window Rescaling Function

```c
void RescaleWindowForDPI(HWND hwnd, int oldDpi, int newDpi) {
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
```

### 4. Font Scaling System

#### 4.1 Font Management

**New structures**:
```c
typedef struct {
    HFONT hFont;
    int pointSize;  // Logical point size
    int dpi;        // DPI this font was created for
    LOGFONTW logFont;
} ScalableFont;

typedef struct {
    ScalableFont* fonts;
    int count;
    int capacity;
} FontManager;
```

**Functions**:
```c
// Create font manager
FontManager* CreateFontManager(void);
void DestroyFontManager(FontManager* manager);

// Create scalable font
ScalableFont* CreateScalableFont(const wchar_t* faceName, int pointSize, int weight, int dpi);
void DestroyScalableFont(ScalableFont* font);

// Get font for DPI
HFONT GetFontForDPI(ScalableFont* font, int dpi);

// Rescale all fonts in window
void RescaleFontsForDPI(HWND hwnd, int dpi);

// Apply font to control
void SetControlFont(HWND hwnd, ScalableFont* font, int dpi);
```

**Implementation**:
```c
HFONT GetFontForDPI(ScalableFont* font, int dpi) {
    if (!font) return NULL;
    
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
```

### 5. Icon Scaling System

#### 5.1 Icon Resource Management

**Icon Sizes to Provide**:
- 16x16 (96 DPI, 100% scaling)
- 20x20 (120 DPI, 125% scaling)
- 24x24 (144 DPI, 150% scaling)
- 32x32 (192 DPI, 200% scaling)
- 48x48 (288 DPI, 300% scaling)

**New structures**:
```c
typedef struct {
    int size;
    HICON hIcon;
} IconSize;

typedef struct {
    int resourceId;
    IconSize* sizes;
    int sizeCount;
} ScalableIcon;

typedef struct {
    ScalableIcon* icons;
    int count;
} IconManager;
```

**Functions**:
```c
// Create icon manager
IconManager* CreateIconManager(void);
void DestroyIconManager(IconManager* manager);

// Load icon at appropriate size for DPI
HICON LoadIconForDPI(int resourceId, int dpi);

// Get icon size for DPI
int GetIconSizeForDPI(int baseSizeLogical, int dpi);

// Reload all icons in window for new DPI
void ReloadIconsForDPI(HWND hwnd, int dpi);

// Set icon on control with DPI awareness
void SetControlIcon(HWND hwnd, int resourceId, int dpi);
```

**Implementation**:
```c
HICON LoadIconForDPI(int resourceId, int dpi) {
    // Calculate desired icon size
    int desiredSize = ScaleValueForDPI(16, dpi);  // Base size 16x16
    
    // Available sizes in resource
    int availableSizes[] = {16, 20, 24, 32, 48};
    int sizeCount = sizeof(availableSizes) / sizeof(availableSizes[0]);
    
    // Find closest size (prefer larger)
    int bestSize = availableSizes[0];
    for (int i = 0; i < sizeCount; i++) {
        if (availableSizes[i] >= desiredSize) {
            bestSize = availableSizes[i];
            break;
        }
        bestSize = availableSizes[i];
    }
    
    // Load icon at best size
    return (HICON)LoadImageW(GetModuleHandleW(NULL),
                            MAKEINTRESOURCEW(resourceId),
                            IMAGE_ICON,
                            bestSize, bestSize,
                            LR_DEFAULTCOLOR);
}
```

### 6. Window Position and Size Persistence

#### 6.1 Logical Coordinate Storage

**Enhanced registry functions**:
```c
// Save window position in logical coordinates
BOOL SaveWindowPositionLogical(HWND hwnd, const wchar_t* keyName);

// Restore window position from logical coordinates
BOOL RestoreWindowPositionLogical(HWND hwnd, const wchar_t* keyName);

// Convert saved position to current DPI
void AdjustWindowRectForDPI(RECT* rect, int savedDpi, int currentDpi);
```

**Implementation**:
```c
BOOL SaveWindowPositionLogical(HWND hwnd, const wchar_t* keyName) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    
    // Get current DPI
    int dpi = GetWindowDPI(hwnd);
    
    // Convert to logical coordinates (96 DPI)
    RECT logicalRect;
    PhysicalRectToLogical(&rect, &logicalRect, dpi);
    
    // Save logical coordinates and DPI
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\YouTubeCacher\\WindowPos",
                       0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"Left", 0, REG_DWORD, (BYTE*)&logicalRect.left, sizeof(DWORD));
        RegSetValueExW(hKey, L"Top", 0, REG_DWORD, (BYTE*)&logicalRect.top, sizeof(DWORD));
        RegSetValueExW(hKey, L"Right", 0, REG_DWORD, (BYTE*)&logicalRect.right, sizeof(DWORD));
        RegSetValueExW(hKey, L"Bottom", 0, REG_DWORD, (BYTE*)&logicalRect.bottom, sizeof(DWORD));
        
        int baseDpi = 96;
        RegSetValueExW(hKey, L"DPI", 0, REG_DWORD, (BYTE*)&baseDpi, sizeof(DWORD));
        
        RegCloseKey(hKey);
        return TRUE;
    }
    
    return FALSE;
}

BOOL RestoreWindowPositionLogical(HWND hwnd, const wchar_t* keyName) {
    RECT logicalRect;
    int savedDpi = 96;
    
    // Load logical coordinates
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\YouTubeCacher\\WindowPos",
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD);
        RegQueryValueExW(hKey, L"Left", NULL, NULL, (BYTE*)&logicalRect.left, &size);
        RegQueryValueExW(hKey, L"Top", NULL, NULL, (BYTE*)&logicalRect.top, &size);
        RegQueryValueExW(hKey, L"Right", NULL, NULL, (BYTE*)&logicalRect.right, &size);
        RegQueryValueExW(hKey, L"Bottom", NULL, NULL, (BYTE*)&logicalRect.bottom, &size);
        RegQueryValueExW(hKey, L"DPI", NULL, NULL, (BYTE*)&savedDpi, &size);
        RegCloseKey(hKey);
        
        // Get current DPI for window's monitor
        int currentDpi = GetWindowDPI(hwnd);
        
        // Convert to physical coordinates
        RECT physicalRect;
        LogicalRectToPhysical(&logicalRect, &physicalRect, currentDpi);
        
        // Ensure window is on screen
        EnsureWindowOnScreen(&physicalRect);
        
        // Apply position
        SetWindowPos(hwnd, NULL,
                    physicalRect.left,
                    physicalRect.top,
                    physicalRect.right - physicalRect.left,
                    physicalRect.bottom - physicalRect.top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
        
        return TRUE;
    }
    
    return FALSE;
}
```

#### 6.2 Screen Bounds Checking

```c
void EnsureWindowOnScreen(RECT* rect) {
    // Get work area of monitor containing the rect
    HMONITOR hMonitor = MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hMonitor, &mi);
    
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    
    // Adjust if off screen
    if (rect->left < mi.rcWork.left) {
        rect->left = mi.rcWork.left;
        rect->right = rect->left + width;
    }
    if (rect->top < mi.rcWork.top) {
        rect->top = mi.rcWork.top;
        rect->bottom = rect->top + height;
    }
    if (rect->right > mi.rcWork.right) {
        rect->right = mi.rcWork.right;
        rect->left = rect->right - width;
    }
    if (rect->bottom > mi.rcWork.bottom) {
        rect->bottom = mi.rcWork.bottom;
        rect->top = rect->bottom - height;
    }
    
    // If still off screen (window too large), position at top-left
    if (rect->left < mi.rcWork.left) rect->left = mi.rcWork.left;
    if (rect->top < mi.rcWork.top) rect->top = mi.rcWork.top;
}
```

### 7. Main Window DPI Support

#### 7.1 Main Window Scaling

**Update main window creation**:
```c
// In WM_CREATE or dialog initialization
case WM_CREATE: {
    // Register window for DPI management
    DPIContext* context = RegisterWindowForDPI(g_dpiManager, hwnd);
    
    // Get initial DPI
    int dpi = GetWindowDPI(hwnd);
    
    // Scale all controls for initial DPI
    RescaleWindowForDPI(hwnd, 96, dpi);
    
    // Load icons at appropriate size
    ReloadIconsForDPI(hwnd, dpi);
    
    // Create fonts at appropriate size
    CreateWindowFonts(hwnd, dpi);
    
    return 0;
}
```

#### 7.2 Control Scaling

**Scale all controls in main window**:
```c
void ScaleMainWindowControls(HWND hwnd, int dpi) {
    // Define base dimensions at 96 DPI
    const int BASE_MARGIN = 10;
    const int BASE_BUTTON_HEIGHT = 23;
    const int BASE_EDIT_HEIGHT = 20;
    const int BASE_LABEL_HEIGHT = 15;
    
    // Scale dimensions
    int margin = ScaleValueForDPI(BASE_MARGIN, dpi);
    int buttonHeight = ScaleValueForDPI(BASE_BUTTON_HEIGHT, dpi);
    int editHeight = ScaleValueForDPI(BASE_EDIT_HEIGHT, dpi);
    int labelHeight = ScaleValueForDPI(BASE_LABEL_HEIGHT, dpi);
    
    // Get client area
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    
    // Position controls using scaled dimensions
    // (Similar to existing dialog layout code)
    
    // Example for URL input field
    SetWindowPos(GetDlgItem(hwnd, IDC_TEXT_FIELD),
                NULL,
                margin,
                margin,
                clientWidth - 2 * margin,
                editHeight,
                SWP_NOZORDER);
    
    // ... position other controls ...
}
```

## Data Models

### DPI Context

```c
typedef struct {
    HWND hwnd;
    int currentDpi;
    int baseDpi;  // Always 96
    double scaleFactor;
    RECT logicalRect;
    FontManager* fontManager;
    IconManager* iconManager;
} DPIContext;
```

### Global DPI Manager

```c
typedef struct {
    DPIContext* mainWindow;
    DPIContext** dialogs;
    int dialogCount;
    int dialogCapacity;
    CRITICAL_SECTION lock;  // Thread safety
} DPIManager;

// Global instance
extern DPIManager* g_dpiManager;
```

## Error Handling

### DPI API Failures

- **GetDpiForWindow fails**: Fall back to system DPI
- **GetDpiForMonitor fails**: Fall back to system DPI
- **SetProcessDpiAwarenessContext fails**: Fall back to older APIs
- **Font creation fails**: Use system default font
- **Icon loading fails**: Use default icon

### Resource Allocation

- **Font manager creation fails**: Continue without font scaling
- **Icon manager creation fails**: Continue without icon scaling
- **DPI context creation fails**: Log error, continue with system DPI

### Window Positioning

- **Saved position off-screen**: Adjust to nearest monitor
- **Saved position invalid**: Use default centered position
- **Registry access fails**: Use default position

## Testing Strategy

### DPI Setting Tests

1. **96 DPI (100% scaling)**:
   - Standard DPI baseline
   - Verify all controls are properly sized
   - Verify fonts are readable
   - Verify icons are sharp

2. **120 DPI (125% scaling)**:
   - Common laptop setting
   - Verify proportional scaling
   - Verify no clipping or overlap
   - Verify text remains readable

3. **144 DPI (150% scaling)**:
   - Common high-DPI setting
   - Verify all UI elements scale correctly
   - Verify layout remains balanced
   - Verify icons are appropriate size

4. **192 DPI (200% scaling)**:
   - High-resolution display
   - Verify crisp rendering
   - Verify no pixelation
   - Verify window fits on screen

5. **Custom DPI settings**:
   - Test 110%, 175%, 225% scaling
   - Verify fractional scaling works
   - Verify rounding doesn't cause issues

### Multi-Monitor Tests

1. **Same DPI monitors**:
   - Move window between monitors
   - Verify no visual changes
   - Verify position is maintained

2. **Different DPI monitors**:
   - Move window from 96 DPI to 144 DPI
   - Verify window rescales correctly
   - Verify `WM_DPICHANGED` is handled
   - Verify all controls rescale

3. **Monitor configuration changes**:
   - Change DPI while app is running
   - Verify app responds to change
   - Verify layout remains correct

### Dynamic DPI Change Tests

1. **Window movement**:
   - Drag window between monitors with different DPI
   - Verify smooth transition
   - Verify no flickering
   - Verify controls remain functional

2. **System DPI change**:
   - Change system DPI in Windows settings
   - Restart application
   - Verify new DPI is detected
   - Verify layout is correct

3. **Per-monitor DPI change**:
   - Change DPI of one monitor
   - Move window to that monitor
   - Verify rescaling occurs
   - Verify saved position is correct

### Font Scaling Tests

1. **Font sizes**:
   - Verify fonts scale proportionally
   - Verify text remains readable at all DPI
   - Verify no text clipping
   - Verify line spacing is correct

2. **Font rendering**:
   - Verify ClearType rendering
   - Verify no blurry text
   - Verify bold/italic styles work
   - Verify Unicode characters render

### Icon Scaling Tests

1. **Icon sizes**:
   - Verify correct icon size loaded for each DPI
   - Verify icons are sharp (not scaled up from small)
   - Verify icons fit in controls
   - Verify icon alignment

2. **Icon resources**:
   - Verify all required icon sizes exist
   - Verify fallback to nearest size works
   - Verify high-quality scaling when needed

### Window Position Tests

1. **Save and restore**:
   - Save window position at 96 DPI
   - Restore at 144 DPI
   - Verify position is correct
   - Verify size is proportional

2. **Off-screen handling**:
   - Save position on secondary monitor
   - Disconnect monitor
   - Restore position
   - Verify window appears on primary monitor

3. **Multi-monitor scenarios**:
   - Save position on high-DPI monitor
   - Restore on low-DPI monitor
   - Verify window is visible and usable

## Implementation Notes

### Windows API Compatibility

- **Windows 10 1703+**: Per-Monitor V2 DPI awareness
- **Windows 10 1607+**: `GetDpiForWindow` API
- **Windows 8.1+**: Per-Monitor V1 DPI awareness, `GetDpiForMonitor`
- **Windows Vista/7**: System DPI awareness only
- **Fallback**: Always provide fallback to older APIs

### Performance Considerations

- **DPI queries**: Cache DPI values, don't query repeatedly
- **Font creation**: Create fonts on-demand, cache for reuse
- **Icon loading**: Load icons once per DPI, cache
- **Layout calculations**: Minimize recalculations
- **WM_DPICHANGED**: Batch updates, single redraw

### Memory Management

- **Font cleanup**: Delete old fonts when creating new ones
- **Icon cleanup**: Destroy old icons when loading new ones
- **DPI context**: Free when window is destroyed
- **Manager cleanup**: Clean up on application exit

### Thread Safety

- **DPI manager**: Protect with critical section
- **Font manager**: Thread-safe access
- **Icon manager**: Thread-safe access
- **Registry access**: Serialize writes

## Migration Path

### Phase 1: Foundation
1. Create DPI management infrastructure (dpi.c)
2. Implement per-monitor DPI awareness initialization
3. Add application manifest with DPI awareness
4. Test basic DPI detection

### Phase 2: Dynamic DPI Handling
1. Implement `WM_DPICHANGED` handler
2. Implement window rescaling function
3. Test window movement between monitors
4. Verify layout preservation

### Phase 3: Font Scaling
1. Implement font manager
2. Create scalable fonts for all UI text
3. Implement font rescaling on DPI change
4. Test font rendering at multiple DPI

### Phase 4: Icon Scaling
1. Create icon resources at multiple sizes
2. Implement icon manager
3. Implement icon loading for DPI
4. Test icon quality at multiple DPI

### Phase 5: Main Window Integration
1. Update main window to use DPI system
2. Scale all main window controls
3. Implement window position persistence
4. Test main window at multiple DPI

### Phase 6: Dialog Integration
1. Update existing dialogs to use new DPI system
2. Ensure dialogs handle `WM_DPICHANGED`
3. Test all dialogs at multiple DPI
4. Verify dialog scaling on monitor changes

### Phase 7: Comprehensive Testing
1. Test at all standard DPI settings
2. Test multi-monitor configurations
3. Test dynamic DPI changes
4. Performance testing
5. Regression testing

## Performance Considerations

- **DPI queries**: < 1ms per query (cached)
- **Font creation**: < 10ms per font
- **Icon loading**: < 5ms per icon
- **Layout recalculation**: < 50ms for full window
- **WM_DPICHANGED handling**: < 100ms total
- **Memory overhead**: ~10KB per window context

## Security Considerations

- **Registry access**: Validate all registry data
- **Buffer sizes**: Use DPI-scaled buffer sizes
- **Integer overflow**: Check scaling calculations
- **Resource loading**: Validate resource IDs
- **Monitor enumeration**: Handle invalid monitors gracefully
