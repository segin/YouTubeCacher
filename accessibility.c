#include "YouTubeCacher.h"
#include "accessibility.h"

// Forward declarations
static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);

// Set accessible name and description for a control
void SetControlAccessibility(HWND hwnd, const wchar_t* name, const wchar_t* description)
{
    if (!hwnd) {
        return;
    }
    
    // Store accessible name as window property
    // DO NOT use SetWindowTextW here - that would overwrite the visible text!
    // Screen readers can access this property for accessibility information
    if (name) {
        SetPropW(hwnd, L"AccessibleName", (HANDLE)name);
    }
    
    // Store accessible description as window property for advanced accessibility features
    if (description && description[0] != L'\0') {
        SetPropW(hwnd, L"AccessibleDescription", (HANDLE)description);
    }
}

// Notify screen readers of state changes
void NotifyAccessibilityStateChange(HWND hwnd, DWORD event)
{
    if (!hwnd) {
        return;
    }
    
    // Use NotifyWinEvent to notify screen readers of state changes
    // Common events:
    // EVENT_OBJECT_STATECHANGE - Control state changed
    // EVENT_OBJECT_VALUECHANGE - Control value changed
    // EVENT_OBJECT_NAMECHANGE - Control name changed
    // EVENT_OBJECT_FOCUS - Control received focus
    
    // Get the control ID for the event
    LONG idObject = OBJID_CLIENT;
    LONG idChild = GetDlgCtrlID(hwnd);
    
    // Notify the system of the event
    NotifyWinEvent(event, hwnd, idObject, idChild);
}

// Check if screen reader is active
BOOL IsScreenReaderActive(void)
{
    // Check if a screen reader is running by querying system parameters
    BOOL screenReaderActive = FALSE;
    
    // Check SPI_GETSCREENREADER flag
    SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReaderActive, 0);
    
    if (screenReaderActive) {
        return TRUE;
    }
    
    // Additional check: Look for common screen reader processes
    // This provides a fallback if SPI_GETSCREENREADER is not set
    const wchar_t* screenReaderProcesses[] = {
        L"nvda.exe",        // NVDA
        L"narrator.exe",    // Windows Narrator
        L"jfw.exe",         // JAWS
        L"WindowEyes.exe",  // Window-Eyes
        L"ZoomText.exe",    // ZoomText
        NULL
    };
    
    // Take a snapshot of all processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    // Get the first process
    if (!Process32FirstW(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return FALSE;
    }
    
    // Iterate through processes
    do {
        // Check if this process matches any known screen reader
        for (int i = 0; screenReaderProcesses[i] != NULL; i++) {
            if (_wcsicmp(pe32.szExeFile, screenReaderProcesses[i]) == 0) {
                CloseHandle(hSnapshot);
                return TRUE;
            }
        }
    } while (Process32NextW(hSnapshot, &pe32));
    
    CloseHandle(hSnapshot);
    return FALSE;
}

// Check if high contrast mode is enabled
BOOL IsHighContrastMode(void)
{
    HIGHCONTRASTW hc;
    hc.cbSize = sizeof(HIGHCONTRASTW);
    
    // Query the system for high contrast settings
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRASTW), &hc, 0)) {
        // Check if the HCF_HIGHCONTRASTON flag is set
        return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
    }
    
    return FALSE;
}

// Get system color for high contrast mode
COLORREF GetHighContrastColor(int colorType)
{
    // Use GetSysColor to retrieve system colors
    // These colors automatically respect high contrast mode settings
    // Common color types:
    // COLOR_WINDOW - Window background
    // COLOR_WINDOWTEXT - Window text
    // COLOR_BTNFACE - Button face
    // COLOR_BTNTEXT - Button text
    // COLOR_HIGHLIGHT - Selected item background
    // COLOR_HIGHLIGHTTEXT - Selected item text
    // COLOR_GRAYTEXT - Disabled text
    // COLOR_WINDOWFRAME - Window frame
    
    return GetSysColor(colorType);
}

// Apply high contrast colors to a dialog
void ApplyHighContrastColors(HWND hDlg)
{
    if (!hDlg) {
        return;
    }
    
    // Check if high contrast mode is enabled
    if (!IsHighContrastMode()) {
        return;
    }
    
    // Native Windows controls automatically handle high contrast mode
    // by using system colors. However, we can force a redraw to ensure
    // all controls update their appearance immediately.
    
    // Invalidate the entire dialog to force a repaint
    InvalidateRect(hDlg, NULL, TRUE);
    
    // Enumerate all child controls and invalidate them as well
    EnumChildWindows(hDlg, EnumChildProc, 0);
    
    // Update the window to apply changes immediately
    UpdateWindow(hDlg);
}

// Callback function for EnumChildWindows to invalidate child controls
static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    (void)lParam; // Unused parameter
    
    // Invalidate each child control to force redraw
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
    
    return TRUE; // Continue enumeration
}
