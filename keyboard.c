#include "YouTubeCacher.h"

// Tab order management implementation

/**
 * SetDialogTabOrder - Configure tab order for dialog controls
 * 
 * @param hDlg: Dialog window handle
 * @param config: Tab order configuration
 */
void SetDialogTabOrder(HWND hDlg, const TabOrderConfig* config) {
    if (!hDlg || !config || !config->entries || config->count <= 0) {
        return;
    }
    
    // Sort entries by tab order (bubble sort for simplicity)
    TabOrderEntry* sorted = (TabOrderEntry*)SAFE_MALLOC(config->count * sizeof(TabOrderEntry));
    if (!sorted) {
        return;
    }
    
    memcpy(sorted, config->entries, config->count * sizeof(TabOrderEntry));
    
    // Simple bubble sort by tabOrder
    for (int i = 0; i < config->count - 1; i++) {
        for (int j = 0; j < config->count - i - 1; j++) {
            if (sorted[j].tabOrder > sorted[j + 1].tabOrder) {
                TabOrderEntry temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    // Set tab order using SetWindowPos with proper Z-order
    HWND hPrevious = HWND_TOP;
    for (int i = 0; i < config->count; i++) {
        HWND hControl = GetDlgItem(hDlg, sorted[i].controlId);
        if (hControl) {
            // Set or remove WS_TABSTOP style
            LONG style = GetWindowLong(hControl, GWL_STYLE);
            if (sorted[i].isTabStop) {
                style |= WS_TABSTOP;
            } else {
                style &= ~WS_TABSTOP;
            }
            SetWindowLong(hControl, GWL_STYLE, style);
            
            // Set Z-order for tab navigation
            SetWindowPos(hControl, hPrevious, 0, 0, 0, 0, 
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            hPrevious = hControl;
        }
    }
    
    SAFE_FREE(sorted);
}

/**
 * CalculateTabOrder - Automatically calculate logical tab order
 * 
 * @param hDlg: Dialog window handle
 * @return: Allocated TabOrderConfig (caller must free with FreeTabOrderConfig)
 */
TabOrderConfig* CalculateTabOrder(HWND hDlg) {
    if (!hDlg) {
        return NULL;
    }
    
    // Count controls with WS_TABSTOP style
    int controlCount = 0;
    HWND hControl = GetWindow(hDlg, GW_CHILD);
    while (hControl) {
        LONG style = GetWindowLong(hControl, GWL_STYLE);
        if (style & WS_TABSTOP) {
            controlCount++;
        }
        hControl = GetWindow(hControl, GW_HWNDNEXT);
    }
    
    if (controlCount == 0) {
        return NULL;
    }
    
    // Allocate configuration
    TabOrderConfig* config = (TabOrderConfig*)SAFE_MALLOC(sizeof(TabOrderConfig));
    if (!config) {
        return NULL;
    }
    
    config->entries = (TabOrderEntry*)SAFE_MALLOC(controlCount * sizeof(TabOrderEntry));
    if (!config->entries) {
        SAFE_FREE(config);
        return NULL;
    }
    config->count = controlCount;
    
    // Collect controls with their positions
    typedef struct {
        HWND hwnd;
        int controlId;
        RECT rect;
        BOOL isTabStop;
    } ControlInfo;
    
    ControlInfo* controls = (ControlInfo*)SAFE_MALLOC(controlCount * sizeof(ControlInfo));
    if (!controls) {
        SAFE_FREE(config->entries);
        SAFE_FREE(config);
        return NULL;
    }
    
    int index = 0;
    hControl = GetWindow(hDlg, GW_CHILD);
    while (hControl && index < controlCount) {
        LONG style = GetWindowLong(hControl, GWL_STYLE);
        if (style & WS_TABSTOP) {
            controls[index].hwnd = hControl;
            controls[index].controlId = GetDlgCtrlID(hControl);
            GetWindowRect(hControl, &controls[index].rect);
            controls[index].isTabStop = TRUE;
            index++;
        }
        hControl = GetWindow(hControl, GW_HWNDNEXT);
    }
    
    // Sort by position: top-to-bottom, left-to-right
    for (int i = 0; i < controlCount - 1; i++) {
        for (int j = 0; j < controlCount - i - 1; j++) {
            BOOL shouldSwap = FALSE;
            
            // Compare Y positions first (top-to-bottom)
            if (controls[j].rect.top > controls[j + 1].rect.top + 10) {
                shouldSwap = TRUE;
            } else if (abs(controls[j].rect.top - controls[j + 1].rect.top) <= 10) {
                // If roughly same Y position, compare X (left-to-right)
                if (controls[j].rect.left > controls[j + 1].rect.left) {
                    shouldSwap = TRUE;
                }
            }
            
            if (shouldSwap) {
                ControlInfo temp = controls[j];
                controls[j] = controls[j + 1];
                controls[j + 1] = temp;
            }
        }
    }
    
    // Build tab order configuration
    for (int i = 0; i < controlCount; i++) {
        config->entries[i].controlId = controls[i].controlId;
        config->entries[i].tabOrder = i;
        config->entries[i].isTabStop = controls[i].isTabStop;
    }
    
    SAFE_FREE(controls);
    return config;
}

/**
 * FreeTabOrderConfig - Free tab order configuration
 * 
 * @param config: Configuration to free
 */
void FreeTabOrderConfig(TabOrderConfig* config) {
    if (config) {
        if (config->entries) {
            SAFE_FREE(config->entries);
        }
        SAFE_FREE(config);
    }
}

// Accelerator key implementation

/**
 * GetAcceleratorChar - Extract accelerator character from label
 * 
 * @param label: Button or control label text
 * @return: Accelerator character (uppercase) or 0 if none
 */
wchar_t GetAcceleratorChar(const wchar_t* label) {
    if (!label) {
        return 0;
    }
    
    // Find ampersand that's not doubled (&&)
    const wchar_t* p = label;
    while (*p) {
        if (*p == L'&') {
            if (*(p + 1) == L'&') {
                // Escaped ampersand, skip both
                p += 2;
            } else if (*(p + 1) != L'\0') {
                // Found accelerator
                return towupper(*(p + 1));
            } else {
                // Ampersand at end of string
                return 0;
            }
        }
        p++;
    }
    
    return 0;
}

/**
 * HasAcceleratorConflict - Check if accelerator conflicts with existing controls
 * 
 * @param hDlg: Dialog window handle
 * @param accelChar: Accelerator character to check
 * @return: TRUE if conflict exists
 */
BOOL HasAcceleratorConflict(HWND hDlg, wchar_t accelChar) {
    if (!hDlg || accelChar == 0) {
        return FALSE;
    }
    
    accelChar = towupper(accelChar);
    int conflictCount = 0;
    
    // Enumerate all child controls
    HWND hControl = GetWindow(hDlg, GW_CHILD);
    while (hControl) {
        wchar_t text[256];
        if (GetWindowTextW(hControl, text, 256) > 0) {
            wchar_t controlAccel = GetAcceleratorChar(text);
            if (controlAccel != 0 && towupper(controlAccel) == accelChar) {
                conflictCount++;
                if (conflictCount > 1) {
                    return TRUE;
                }
            }
        }
        hControl = GetWindow(hControl, GW_HWNDNEXT);
    }
    
    return FALSE;
}

/**
 * ValidateAcceleratorKeys - Check for accelerator key conflicts in dialog
 * 
 * @param hDlg: Dialog window handle
 * @return: TRUE if no conflicts, FALSE if conflicts exist
 */
BOOL ValidateAcceleratorKeys(HWND hDlg) {
    if (!hDlg) {
        return FALSE;
    }
    
    // Collect all accelerator keys
    wchar_t accelerators[256];
    int accelCount = 0;
    
    HWND hControl = GetWindow(hDlg, GW_CHILD);
    while (hControl) {
        wchar_t text[256];
        if (GetWindowTextW(hControl, text, 256) > 0) {
            wchar_t accel = GetAcceleratorChar(text);
            if (accel != 0) {
                accel = towupper(accel);
                
                // Check for duplicate
                for (int i = 0; i < accelCount; i++) {
                    if (accelerators[i] == accel) {
                        // Conflict found
                        wchar_t debugMsg[256];
                        swprintf(debugMsg, 256, L"[Keyboard] Accelerator key conflict detected: %c\r\n", accel);
                        ThreadSafeDebugOutput(debugMsg);
                        return FALSE;
                    }
                }
                
                if (accelCount < 256) {
                    accelerators[accelCount++] = accel;
                }
            }
        }
        hControl = GetWindow(hControl, GW_HWNDNEXT);
    }
    
    return TRUE;
}

// Focus management implementation

/**
 * SetInitialDialogFocus - Set focus to logical first control
 * 
 * @param hDlg: Dialog window handle
 */
void SetInitialDialogFocus(HWND hDlg) {
    if (!hDlg) {
        return;
    }
    
    // Find first control with WS_TABSTOP style
    HWND hControl = GetWindow(hDlg, GW_CHILD);
    HWND hFirstTabStop = NULL;
    
    while (hControl) {
        LONG style = GetWindowLong(hControl, GWL_STYLE);
        if ((style & WS_TABSTOP) && (style & WS_VISIBLE) && IsWindowEnabled(hControl)) {
            hFirstTabStop = hControl;
            break;
        }
        hControl = GetWindow(hControl, GW_HWNDNEXT);
    }
    
    if (hFirstTabStop) {
        SetFocus(hFirstTabStop);
    }
}

/**
 * GetNextFocusableControl - Get next or previous focusable control
 * 
 * @param hDlg: Dialog window handle
 * @param hCurrent: Current control with focus
 * @param forward: TRUE for next, FALSE for previous
 * @return: Next focusable control or NULL
 */
HWND GetNextFocusableControl(HWND hDlg, HWND hCurrent, BOOL forward) {
    if (!hDlg) {
        return NULL;
    }
    
    HWND hControl = hCurrent ? hCurrent : GetWindow(hDlg, GW_CHILD);
    if (!hControl) {
        return NULL;
    }
    
    // Start searching from next/previous control
    hControl = GetWindow(hControl, forward ? GW_HWNDNEXT : GW_HWNDPREV);
    
    // Search for focusable control
    HWND hStart = hControl;
    while (hControl) {
        LONG style = GetWindowLong(hControl, GWL_STYLE);
        if ((style & WS_TABSTOP) && (style & WS_VISIBLE) && IsWindowEnabled(hControl)) {
            return hControl;
        }
        hControl = GetWindow(hControl, forward ? GW_HWNDNEXT : GW_HWNDPREV);
        
        // Wrap around
        if (!hControl) {
            hControl = GetWindow(hDlg, forward ? GW_CHILD : GW_CHILD);
            // Find last child if going backward
            if (!forward && hControl) {
                HWND hLast = hControl;
                while (GetWindow(hLast, GW_HWNDNEXT)) {
                    hLast = GetWindow(hLast, GW_HWNDNEXT);
                }
                hControl = hLast;
            }
        }
        
        // Prevent infinite loop
        if (hControl == hStart) {
            break;
        }
    }
    
    return NULL;
}

/**
 * EnsureFocusVisible - Ensure focus indicator is visible
 * 
 * @param hwnd: Control window handle
 */
void EnsureFocusVisible(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    
    // Send WM_UPDATEUISTATE to ensure focus rectangles are shown
    SendMessage(hwnd, WM_UPDATEUISTATE, 
                MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);
    
    // Invalidate control to force redraw
    InvalidateRect(hwnd, NULL, TRUE);
}
