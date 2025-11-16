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

#endif // ACCESSIBILITY_H
