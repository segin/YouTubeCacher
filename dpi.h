#ifndef DPI_H
#define DPI_H

#include <windows.h>

// DPI management function prototypes

// Initialize DPI awareness with fallbacks
void InitializeDPIAwareness(void);

// Initialize DPI manager
DPIManager* CreateDPIManager(void);
void DestroyDPIManager(DPIManager* manager);

// Register/unregister windows for DPI management
DPIContext* RegisterWindowForDPI(DPIManager* manager, HWND hwnd);
void UnregisterWindowForDPI(DPIManager* manager, HWND hwnd);

// Get DPI context for window
DPIContext* GetDPIContext(DPIManager* manager, HWND hwnd);

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

// Dynamic DPI change handling
void RescaleWindowForDPI(HWND hwnd, int oldDpi, int newDpi);
void RescaleFontsForDPI(HWND hwnd, int dpi);
void ReloadIconsForDPI(HWND hwnd, int dpi);

// Font management functions
FontManager* CreateFontManager(void);
void DestroyFontManager(FontManager* manager);
ScalableFont* CreateScalableFont(const wchar_t* faceName, int pointSize, int weight, int dpi);
void DestroyScalableFont(ScalableFont* font);
HFONT GetFontForDPI(ScalableFont* font, int dpi);
void SetControlFont(HWND hwnd, ScalableFont* font, int dpi);
BOOL AddFontToManager(FontManager* manager, ScalableFont* font);
ScalableFont* CreateAndRegisterFont(HWND hwnd, const wchar_t* faceName, int pointSize, int weight);

// Icon management functions
IconManager* CreateIconManager(void);
void DestroyIconManager(IconManager* manager);
HICON LoadIconForDPI(int resourceId, int dpi);
int GetIconSizeForDPI(int baseSizeLogical, int dpi);
void SetControlIcon(HWND hwnd, int resourceId, int dpi);
ScalableIcon* CreateScalableIcon(int resourceId);
void DestroyScalableIcon(ScalableIcon* icon);
BOOL AddIconToManager(IconManager* manager, ScalableIcon* icon);

// Helper functions for icon integration with windows
void SetControlIconWithTracking(HWND hwnd, int resourceId, int dpi);
BOOL InitializeIconControl(HWND hDialog, int controlId, int resourceId);

// Window position persistence functions
BOOL SaveWindowPositionLogical(HWND hwnd, const wchar_t* keyName);
BOOL RestoreWindowPositionLogical(HWND hwnd, const wchar_t* keyName);
void EnsureWindowOnScreen(RECT* rect);

#endif // DPI_H
