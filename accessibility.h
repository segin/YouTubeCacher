#ifndef ACCESSIBILITY_H
#define ACCESSIBILITY_H

#include <windows.h>

// Accessibility function prototypes

// Set accessible name and description for a control
void SetControlAccessibility(HWND hwnd, const wchar_t* name, const wchar_t* description);

// Notify screen readers of state changes
void NotifyAccessibilityStateChange(HWND hwnd, DWORD event);

// Check if screen reader is active
BOOL IsScreenReaderActive(void);

// High contrast mode support functions

// Check if high contrast mode is enabled
BOOL IsHighContrastMode(void);

// Get system color for high contrast mode
COLORREF GetHighContrastColor(int colorType);

// Apply high contrast colors to a dialog
void ApplyHighContrastColors(HWND hDlg);

#endif // ACCESSIBILITY_H
