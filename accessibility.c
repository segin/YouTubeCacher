#include "YouTubeCacher.h"
#include "accessibility.h"

// Set accessible name and description for a control
void SetControlAccessibility(HWND hwnd, const wchar_t* name, const wchar_t* description)
{
    if (!hwnd) {
        return;
    }
    
    // Set accessible name using window text if provided
    if (name) {
        SetWindowTextW(hwnd, name);
    }
    
    // For accessible descriptions, we would typically use IAccessible COM interface
    // However, for basic accessibility support, setting the window text is sufficient
    // for most screen readers to announce the control properly
    
    // If description is provided and different from name, we could store it
    // in window properties for advanced accessibility features
    if (description && description[0] != L'\0') {
        // Store description as window property for potential future use
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
