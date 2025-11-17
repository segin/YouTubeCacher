#ifndef DPI_H
#define DPI_H

#include <windows.h>

// DPI management function prototypes

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

#endif // DPI_H
