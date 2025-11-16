#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <windows.h>

// Tab order management functions
void SetDialogTabOrder(HWND hDlg, const TabOrderConfig* config);
TabOrderConfig* CalculateTabOrder(HWND hDlg);
void FreeTabOrderConfig(TabOrderConfig* config);

// Accelerator key functions
BOOL ValidateAcceleratorKeys(HWND hDlg);
wchar_t GetAcceleratorChar(const wchar_t* label);
BOOL HasAcceleratorConflict(HWND hDlg, wchar_t accelChar);

// Focus management functions
void SetInitialDialogFocus(HWND hDlg);
HWND GetNextFocusableControl(HWND hDlg, HWND hCurrent, BOOL forward);
void EnsureFocusVisible(HWND hwnd);

#endif // KEYBOARD_H
