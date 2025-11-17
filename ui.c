#include "YouTubeCacher.h"

// Subclass procedure for text field to detect paste operations
LRESULT CALLBACK TextFieldSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PASTE:
            // User is manually pasting - set flag
            SetManualPasteFlag(TRUE);
            break;
            
        case WM_KEYDOWN:
            // Check for Ctrl+V
            if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                SetManualPasteFlag(TRUE);
            }
            break;
    }
    
    // Call original window procedure
    return CallWindowProc(GetOriginalTextFieldProc(), hwnd, uMsg, wParam, lParam);
}

// Function to update debug control visibility
void UpdateDebugControlVisibility(HWND hDlg) {
    BOOL enableDebug, enableLogfile;
    GetDebugState(&enableDebug, &enableLogfile);
    int showState = enableDebug ? SW_SHOW : SW_HIDE;
    
    // Show/hide Add button
    ShowWindow(GetDlgItem(hDlg, IDC_BUTTON1), showState);
    
    // Show/hide color buttons
    ShowWindow(GetDlgItem(hDlg, IDC_COLOR_GREEN), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_COLOR_TEAL), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_COLOR_BLUE), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_COLOR_WHITE), showState);
    
    // Show/hide debug test buttons
    ShowWindow(GetDlgItem(hDlg, IDC_DEBUG_TEST_INFO), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_DEBUG_TEST_WARNING), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_DEBUG_TEST_ERROR), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_DEBUG_TEST_SUCCESS), showState);
}

void CheckClipboardForYouTubeURL(HWND hDlg) {
    // Check if autopaste is enabled
    if (!GetAutopasteState()) {
        return; // Autopaste is disabled
    }
    
    // Only check clipboard if text field is empty
    wchar_t currentText[MAX_BUFFER_SIZE];
    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, currentText, MAX_BUFFER_SIZE);
    
    if (wcslen(currentText) == 0) {
        if (OpenClipboard(hDlg)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData != NULL) {
                wchar_t* clipText = (wchar_t*)GlobalLock(hData);
                if (clipText != NULL && IsYouTubeURL(clipText)) {
                    SetProgrammaticChangeFlag(TRUE);
                    SetDlgItemTextW(hDlg, IDC_TEXT_FIELD, clipText);
                    SetCurrentBrush(GetBrush(BRUSH_LIGHT_GREEN));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    SetProgrammaticChangeFlag(FALSE);
                }
                GlobalUnlock(hData);
            }
            CloseClipboard();
        }
    }
}

// Update the video info UI fields with title and duration
void UpdateVideoInfoUI(HWND hDlg, const wchar_t* title, const wchar_t* duration) {
    if (!hDlg) return;
    
    // Update video title field
    if (title && wcslen(title) > 0) {
        // Debug: Log the title string being set
        wchar_t debugMsg[1024];
        swprintf(debugMsg, 1024, L"YouTubeCacher: Setting title in UI: %ls (length: %zu)\n", title, wcslen(title));
        OutputDebugStringW(debugMsg);
        
        // Debug: Log individual characters
        OutputDebugStringW(L"YouTubeCacher: Title character codes: ");
        for (size_t i = 0; i < min(wcslen(title), 20); i++) {
            swprintf(debugMsg, 1024, L"U+%04X ", (unsigned int)title[i]);
            OutputDebugStringW(debugMsg);
        }
        OutputDebugStringW(L"\n");
        
        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, title);
    } else {
        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, L"Title not available");
    }
    
    // Update video duration field
    if (duration && wcslen(duration) > 0) {
        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, duration);
    } else {
        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, L"Unknown");
    }
    
    // Force redraw of the updated fields
    InvalidateRect(GetDlgItem(hDlg, IDC_VIDEO_TITLE), NULL, TRUE);
    InvalidateRect(GetDlgItem(hDlg, IDC_VIDEO_DURATION), NULL, TRUE);
}

// Enable/disable UI controls during download operations
void SetDownloadUIState(HWND hDlg, BOOL isDownloading) {
    if (!hDlg) return;
    
    // Disable/enable URL input field
    EnableWindow(GetDlgItem(hDlg, IDC_TEXT_FIELD), !isDownloading);
    
    // Disable/enable Get Info button
    EnableWindow(GetDlgItem(hDlg, IDC_GETINFO_BTN), !isDownloading);
    
    // Change Download button text and keep it enabled
    HWND hDownloadBtn = GetDlgItem(hDlg, IDC_DOWNLOAD_BTN);
    if (hDownloadBtn) {
        if (isDownloading) {
            SetWindowTextW(hDownloadBtn, L"Cancel");
        } else {
            SetWindowTextW(hDownloadBtn, L"Download");
        }
        // Keep the button enabled so user can cancel
        EnableWindow(hDownloadBtn, TRUE);
    }
    
    // Update global state
    SetDownloadingState(isDownloading);
}

// Update the main window's progress bar instead of using separate dialogs
void UpdateMainProgressBar(HWND hDlg, int percentage, const wchar_t* status) {
    if (!hDlg) return;
    
    // Update the progress bar
    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
    if (hProgressBar) {
        // Check if we're in marquee mode
        LONG style = GetWindowLongW(hProgressBar, GWL_STYLE);
        BOOL isMarquee = (style & PBS_MARQUEE) != 0;
        
        // If we have a valid percentage (> 0) and we're in marquee mode, switch to normal mode
        if (percentage > 0 && isMarquee) {
            SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
            SetWindowLongW(hProgressBar, GWL_STYLE, style & ~PBS_MARQUEE);
            SendMessageW(hProgressBar, PBM_SETPOS, percentage, 0);
        }
        // If we have a valid percentage and we're not in marquee mode, just update position
        else if (percentage > 0 && !isMarquee) {
            SendMessageW(hProgressBar, PBM_SETPOS, percentage, 0);
        }
        // If percentage is 0 or negative, don't interfere with marquee mode
        // (Let the caller explicitly manage marquee mode)
        else if (!isMarquee) {
            SendMessageW(hProgressBar, PBM_SETPOS, percentage, 0);
        }
        
        // Enable/show the progress bar during operations
        EnableWindow(hProgressBar, TRUE);
        ShowWindow(hProgressBar, SW_SHOW);
    }
    
    // Update the progress text if we have it
    HWND hProgressText = GetDlgItem(hDlg, IDC_PROGRESS_TEXT);
    if (hProgressText && status) {
        SetWindowTextW(hProgressText, status);
    }
    
    // Force UI update
    UpdateWindow(hDlg);
}

// Show or hide the main window's progress bar
void ShowMainProgressBar(HWND hDlg, BOOL show) {
    if (!hDlg) return;
    
    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
    if (hProgressBar) {
        ShowWindow(hProgressBar, show ? SW_SHOW : SW_HIDE);
        EnableWindow(hProgressBar, show);
        
        if (!show) {
            // Stop marquee and reset progress bar when hiding
            LONG style = GetWindowLongW(hProgressBar, GWL_STYLE);
            if (style & PBS_MARQUEE) {
                SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
                SetWindowLongW(hProgressBar, GWL_STYLE, style & ~PBS_MARQUEE);
            }
            SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);
        }
    }
    
    // Clear progress text when hiding
    HWND hProgressText = GetDlgItem(hDlg, IDC_PROGRESS_TEXT);
    if (hProgressText) {
        SetWindowTextW(hProgressText, show ? L"" : L"");
    }
}

// Set progress bar to marquee mode for indefinite operations (only if not already marqueeing)
void SetProgressBarMarquee(HWND hDlg, BOOL enable) {
    if (!hDlg) return;
    
    HWND hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_BAR);
    if (!hProgressBar) return;
    
    LONG style = GetWindowLongW(hProgressBar, GWL_STYLE);
    BOOL isCurrentlyMarquee = (style & PBS_MARQUEE) != 0;
    
    if (enable && !isCurrentlyMarquee) {
        // Enable marquee mode
        SetWindowLongW(hProgressBar, GWL_STYLE, style | PBS_MARQUEE);
        SendMessageW(hProgressBar, PBM_SETMARQUEE, TRUE, 50); // 50ms animation speed
        DebugOutput(L"YouTubeCacher: Progress bar set to marquee mode");
    } else if (!enable && isCurrentlyMarquee) {
        // Disable marquee mode
        SendMessageW(hProgressBar, PBM_SETMARQUEE, FALSE, 0);
        SetWindowLongW(hProgressBar, GWL_STYLE, style & ~PBS_MARQUEE);
        SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);
        DebugOutput(L"YouTubeCacher: Progress bar marquee mode disabled");
    }
    // If already in the requested state, do nothing (don't reset)
}

// Function to calculate minimum window dimensions based on DPI and content requirements
void CalculateMinimumWindowSize(int* minWidth, int* minHeight, double dpiScaleX, double dpiScaleY) {
    if (!minWidth || !minHeight) return;
    
    // Base measurements in logical units (96 DPI)
    const int BASE_MARGIN = 10;
    const int BASE_WINDOW_MARGIN = 10;
    const int BASE_BUTTON_WIDTH = 78;
    const int BASE_TEXT_HEIGHT = 22;
    const int BASE_LABEL_HEIGHT = 16;
    const int BASE_PROGRESS_HEIGHT = 16;
    const int BASE_GROUP_TITLE_HEIGHT = 18;
    const int BASE_LIST_MIN_HEIGHT = 100;  // Minimum height for the offline videos list
    const int BASE_SIDE_BUTTON_HEIGHT = 32;
    
    // Scale measurements to current DPI
    int margin = (int)(BASE_MARGIN * dpiScaleX);
    int windowMargin = (int)(BASE_WINDOW_MARGIN * dpiScaleX);
    int buttonWidth = (int)(BASE_BUTTON_WIDTH * dpiScaleX);
    int textHeight = (int)(BASE_TEXT_HEIGHT * dpiScaleY);
    int labelHeight = (int)(BASE_LABEL_HEIGHT * dpiScaleY);
    int progressHeight = (int)(BASE_PROGRESS_HEIGHT * dpiScaleY);
    int groupTitleHeight = (int)(BASE_GROUP_TITLE_HEIGHT * dpiScaleY);
    int listMinHeight = (int)(BASE_LIST_MIN_HEIGHT * dpiScaleY);
    int sideButtonHeight = (int)(BASE_SIDE_BUTTON_HEIGHT * dpiScaleY);
    
    // Calculate minimum width requirements
    // Logic: Window margins (20) + text field min width (200) + gap (10) + button width (78) + margin (10) = 318
    int minTextFieldWidth = (int)(200 * dpiScaleX);  // Minimum usable text field width
    int minContentWidth = minTextFieldWidth + margin + buttonWidth + margin;  // Content within group
    int totalMinWidth = (2 * windowMargin) + minContentWidth + (2 * margin);  // Add group margins
    
    // Ensure minimum for UI elements like labels
    int minUIWidth = (int)(400 * dpiScaleX);  // Absolute minimum for UI readability
    *minWidth = (totalMinWidth > minUIWidth) ? totalMinWidth : minUIWidth;
    
    // Calculate minimum height requirements with detailed breakdown
    
    // DOWNLOAD GROUP HEIGHT CALCULATION (130px at 96 DPI) - TWO-LINE VIDEO INFO:
    // - Group title area: 18px (group box border + "Download video" text)
    // - Top margin: 10px (breathing room after title)
    // - URL input row: 22px (standard edit control height)
    // - Reduced margin: 8px (3/4 × 10px for spacing before progress bar)
    // - Progress bar: 16px (standard progress control height, restored to original position)
    // - Reduced margin: 8px (space before video info section)
    // - Video title line: 16px (title text, truncates before "Get Info" button)
    // - Small margin: 6px (1/2 × 10px between title and duration lines)
    // - Duration + status line: 16px (duration + download progress on same line)
    // - Bottom margin: 10px (separation from offline videos group)
    int downloadGroupHeight = groupTitleHeight +           // 18px: group title space
                             margin +                      // 10px: top margin
                             textHeight +                  // 22px: URL field
                             (margin * 3/4) +             //  8px: reduced margin
                             progressHeight +              // 16px: progress bar
                             (margin * 3/4) +             //  8px: reduced margin
                             labelHeight +                 // 16px: video title line
                             (margin / 2) +               //  6px: small margin
                             labelHeight +                 // 16px: duration + status line
                             margin;                       // 10px: bottom margin
                                                          // Total: 130px
    
    // OFFLINE VIDEOS GROUP MINIMUM HEIGHT CALCULATION (159px at 96 DPI):
    // - Group title area: 18px (group box border + "Offline videos" text)
    // - Small margin: 5px (1/2 × 10px for tight spacing after title)
    // - Status labels row: 16px (shows "Status: Ready" and "Items: 0")
    // - Standard margin: 10px (separation before main list content)
    // - Minimum list height: 100px (enough to display 4-5 video entries comfortably)
    // - Bottom margin: 10px (space from window bottom edge)
    int offlineGroupMinHeight = groupTitleHeight +         // 18px: group title space
                               (margin / 2) +             //  5px: small top margin
                               labelHeight +               // 16px: status labels
                               margin +                    // 10px: standard margin
                               listMinHeight +             // 100px: minimum list space
                               margin;                     // 10px: bottom margin
                                                          // Total: 159px
    
    // SIDE BUTTON SPACE VALIDATION:
    // Ensure offline group can accommodate side buttons (Play/Delete)
    // - Two buttons: 2 × 32px = 64px
    // - Spacing between: 5px (1/2 × 10px)
    // - Total button area needed: 69px
    // Since our list minimum (100px) > button area (69px), we have adequate space
    int minSideButtonSpace = (2 * sideButtonHeight) + (margin / 2);  // 64 + 5 = 69px
    if (offlineGroupMinHeight < (groupTitleHeight + (margin / 2) + labelHeight + margin + minSideButtonSpace + margin)) {
        // This should never trigger with our current calculations, but provides safety
        offlineGroupMinHeight = groupTitleHeight + (margin / 2) + labelHeight + margin + minSideButtonSpace + margin;
    }
    
    // TOTAL WINDOW HEIGHT CALCULATION:
    // - Top window margin: 10px (space from window top edge)
    // - Download group: 136px (calculated above)
    // - Inter-group margin: 10px (visual separation between main sections)
    // - Offline group: 159px (calculated above)
    // - Bottom window margin: 10px (space from window bottom edge)
    // - Content subtotal: 325px
    // - Window chrome: ~60px (title bar, menu bar, window borders - OS dependent)
    // - Total minimum: ~385px
    *minHeight = windowMargin +                    // 10px: top margin
                downloadGroupHeight +              // 136px: download section
                margin +                           // 10px: inter-group spacing
                offlineGroupMinHeight +            // 159px: offline section
                windowMargin;                      // 10px: bottom margin
                                                  // Subtotal: 325px
    
    // Add window chrome space (title bar, menu, borders)
    // This varies by Windows version and theme, but ~60px is typical
    *minHeight += (int)(60 * dpiScaleY);          // 60px: window chrome
                                                  // Final total: ~385px at 96 DPI
}

// Function to calculate optimal default window dimensions based on DPI and content requirements
void CalculateDefaultWindowSize(int* defaultWidth, int* defaultHeight, double dpiScaleX, double dpiScaleY) {
    if (!defaultWidth || !defaultHeight) return;
    
    // Start with minimum size as baseline
    CalculateMinimumWindowSize(defaultWidth, defaultHeight, dpiScaleX, dpiScaleY);
    
    // Add comfortable extra space for better user experience
    
    // Width reasoning:
    // - Keep minimum width as default - user can resize window if needed for long titles
    // - No extra horizontal space required since video info wraps appropriately
    // - Focus on vertical space for better video list viewing instead
    int extraWidth = (int)(50 * dpiScaleX);   // Add minimal 50px for slightly more comfortable text field
    *defaultWidth += extraWidth;
    
    // Height reasoning:
    // - Minimum gives us ~100px list height (shows ~3-4 items)
    // - For default, target ~200px list height (shows ~8-10 items comfortably)
    // - Users typically have multiple downloaded videos, need to see more at once
    int extraHeight = (int)(120 * dpiScaleY);  // Add 120px for more comfortable list viewing
    *defaultHeight += extraHeight;
    
    // Ensure we don't exceed reasonable screen space (80% of typical small screen)
    int maxReasonableWidth = (int)(1090 * dpiScaleX);   // 80% of 1366px width
    int maxReasonableHeight = (int)(614 * dpiScaleY);   // 80% of 768px height
    
    if (*defaultWidth > maxReasonableWidth) {
        *defaultWidth = maxReasonableWidth;
    }
    if (*defaultHeight > maxReasonableHeight) {
        *defaultHeight = maxReasonableHeight;
    }
}

// Apply modern Windows theming to dialog and its controls
void ApplyModernThemeToDialog(HWND hDlg) {
    if (!hDlg) return;
    
    // Load UxTheme library for theming functions
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!hUxTheme) return;
    
    // Define constants that might not be available in older headers
    #ifndef ETDT_ENABLETAB
    #define ETDT_ENABLETAB 0x00000006
    #endif
    #ifndef ETDT_USETABTEXTURE
    #define ETDT_USETABTEXTURE 0x00000004
    #endif
    
    // Function pointers for theming APIs
    typedef BOOL (WINAPI *EnableThemeDialogTextureFunc)(HWND, DWORD);
    typedef HRESULT (WINAPI *SetWindowThemeFunc)(HWND, LPCWSTR, LPCWSTR);
    typedef BOOL (WINAPI *IsThemeActiveFunc)(void);
    typedef BOOL (WINAPI *IsAppThemedFunc)(void);
    
    EnableThemeDialogTextureFunc EnableThemeDialogTexture = 
        (EnableThemeDialogTextureFunc)(void*)GetProcAddress(hUxTheme, "EnableThemeDialogTexture");
    SetWindowThemeFunc SetWindowTheme = 
        (SetWindowThemeFunc)(void*)GetProcAddress(hUxTheme, "SetWindowTheme");
    IsThemeActiveFunc IsThemeActive = 
        (IsThemeActiveFunc)(void*)GetProcAddress(hUxTheme, "IsThemeActive");
    IsAppThemedFunc IsAppThemed = 
        (IsAppThemedFunc)(void*)GetProcAddress(hUxTheme, "IsAppThemed");
    
    // Only apply theming if themes are active and app is themed
    if (IsThemeActive && IsThemeActive() && IsAppThemed && IsAppThemed()) {
        // Enable dialog texture with both tab texture and tab support
        if (EnableThemeDialogTexture) {
            EnableThemeDialogTexture(hDlg, ETDT_ENABLETAB | ETDT_USETABTEXTURE);
        }
        
        // Apply modern theme to specific control types
        if (SetWindowTheme) {
            // Theme the dialog itself first
            SetWindowTheme(hDlg, L"Explorer", NULL);
            
            // Find and theme all child controls recursively
            HWND hChild = GetWindow(hDlg, GW_CHILD);
            while (hChild) {
                wchar_t className[256];
                if (GetClassNameW(hChild, className, 256)) {
                    BOOL themed = FALSE;
                    
                    // Apply specific themes based on control type
                    if (wcscmp(className, L"Button") == 0) {
                        // Check if it's a group box (different styling)
                        LONG style = GetWindowLong(hChild, GWL_STYLE);
                        if ((style & BS_GROUPBOX) == BS_GROUPBOX) {
                            SetWindowTheme(hChild, L"Explorer", NULL);
                        } else {
                            SetWindowTheme(hChild, L"Explorer", NULL);
                        }
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"Edit") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ListBox") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ComboBox") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"msctls_progress32") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"SysTabControl32") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"Static") == 0) {
                        // For group boxes and other static controls
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    else if (wcscmp(className, L"ScrollBar") == 0) {
                        SetWindowTheme(hChild, L"Explorer", NULL);
                        themed = TRUE;
                    }
                    
                    // Force redraw if we applied theming
                    if (themed) {
                        InvalidateRect(hChild, NULL, TRUE);
                        UpdateWindow(hChild);
                    }
                }
                hChild = GetWindow(hChild, GW_HWNDNEXT);
            }
        }
        
        // Force redraw of the entire dialog
        InvalidateRect(hDlg, NULL, TRUE);
        UpdateWindow(hDlg);
    }
    
    FreeLibrary(hUxTheme);
}

// Apply theming with a slight delay to ensure all controls are ready
void ApplyDelayedTheming(HWND hDlg) {
    if (!hDlg) return;
    
    // Use a timer to apply theming after the dialog is fully initialized
    SetTimer(hDlg, 9999, 100, NULL); // 100ms delay
}

// Force visual styles activation using multiple methods
void ForceVisualStylesActivation(void) {
    // Method 1: Try to activate visual styles through UxTheme
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme) {
        typedef BOOL (WINAPI *SetThemeAppPropertiesFunc)(DWORD);
        typedef HRESULT (WINAPI *EnableThemingFunc)(BOOL);
        
        SetThemeAppPropertiesFunc SetThemeAppProperties = 
            (SetThemeAppPropertiesFunc)(void*)GetProcAddress(hUxTheme, "SetThemeAppProperties");
        EnableThemingFunc EnableTheming = 
            (EnableThemingFunc)(void*)GetProcAddress(hUxTheme, "EnableTheming");
        
        if (SetThemeAppProperties) {
            // Enable all theming properties
            SetThemeAppProperties(0x7); // STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT
        }
        
        if (EnableTheming) {
            EnableTheming(TRUE);
        }
        
        FreeLibrary(hUxTheme);
    }
    
    // Method 2: Re-initialize Common Controls with explicit visual styles
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | 
                 ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
}

// Create a dialog with explicit theming support
HWND CreateThemedDialog(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc) {
    // Ensure visual styles are active before creating dialog
    ForceVisualStylesActivation();
    
    // Create the dialog
    HWND hDlg = CreateDialogW(hInstance, lpTemplate, hWndParent, lpDialogFunc);
    
    if (hDlg) {
        // Apply theming immediately after creation
        ApplyModernThemeToDialog(hDlg);
        
        // Show the dialog
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);
        
        // Apply theming again after showing (sometimes needed)
        ApplyDelayedTheming(hDlg);
    }
    
    return hDlg;
}

void ResizeControls(HWND hDlg) {
    RECT rect;
    GetClientRect(hDlg, &rect);
    
    // Get DPI for this window to scale all measurements appropriately
    HDC hdc = GetDC(hDlg);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hDlg, hdc);
    
    // Calculate DPI scaling factors (96 DPI = 100% scaling)
    double scaleX = (double)dpiX / 96.0;
    double scaleY = (double)dpiY / 96.0;
    
    // Base measurements in logical units (96 DPI)
    const int BASE_MARGIN = 10;           // Standard margin between elements
    const int BASE_WINDOW_MARGIN = 10;    // Margin from window edges
    const int BASE_BUTTON_WIDTH = 78;     // Standard button width
    const int BASE_BUTTON_HEIGHT = 26;    // Standard button height

    const int BASE_TEXT_HEIGHT = 22;      // Text field height
    const int BASE_LABEL_HEIGHT = 16;     // Label height
    const int BASE_PROGRESS_HEIGHT = 16;  // Progress bar height
    const int BASE_GROUP_TITLE_HEIGHT = 18; // Group box title area height
    
    // Scale all measurements to current DPI
    int margin = (int)(BASE_MARGIN * scaleX);
    int windowMargin = (int)(BASE_WINDOW_MARGIN * scaleX);
    int buttonWidth = (int)(BASE_BUTTON_WIDTH * scaleX);
    int buttonHeight = (int)(BASE_BUTTON_HEIGHT * scaleY);

    int textHeight = (int)(BASE_TEXT_HEIGHT * scaleY);
    int labelHeight = (int)(BASE_LABEL_HEIGHT * scaleY);
    int progressHeight = (int)(BASE_PROGRESS_HEIGHT * scaleY);
    int groupTitleHeight = (int)(BASE_GROUP_TITLE_HEIGHT * scaleY);
    
    // Calculate available space
    int clientWidth = rect.right - rect.left;
    int clientHeight = rect.bottom - rect.top;
    
    // Calculate Download video group dimensions
    // Two-line video info layout: URL field + progress bar + title line + duration line
    // Vertical layout: Group title (18) + margin (10) + URL row (22) + margin (8) + 
    //                  progress bar (16) + margin (8) + title line (16) + small margin (6) +
    //                  duration line (16) + bottom margin (10) = 130 total
    int downloadGroupHeight = groupTitleHeight + margin + textHeight + (margin * 3/4) + 
                             progressHeight + (margin * 3/4) + labelHeight + (margin / 2) + 
                             labelHeight + margin;
    
    // Position Download video group box
    int downloadGroupX = windowMargin;
    int downloadGroupY = windowMargin;
    int downloadGroupWidth = clientWidth - (2 * windowMargin);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_GROUP), NULL, 
                downloadGroupX, downloadGroupY, downloadGroupWidth, downloadGroupHeight, SWP_NOZORDER);
    
    // Calculate button positions (right-aligned within group) - restore original logic
    int buttonX = downloadGroupX + downloadGroupWidth - buttonWidth - margin;
    int availableTextWidth = buttonX - downloadGroupX - (3 * margin); // Space for text controls
    
    // Position controls within Download video group
    int currentY = downloadGroupY + groupTitleHeight + margin;
    
    // URL row - restore original position
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL1), NULL, 
                downloadGroupX + margin, currentY + 2, (int)(30 * scaleX), labelHeight, SWP_NOZORDER);
    
    int urlFieldX = downloadGroupX + margin + (int)(35 * scaleX);
    int urlFieldWidth = availableTextWidth - (int)(35 * scaleX);
    SetWindowPos(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, 
                urlFieldX, currentY, urlFieldWidth, textHeight, SWP_NOZORDER);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_DOWNLOAD_BTN), NULL, 
                buttonX, currentY - 1, buttonWidth, buttonHeight, SWP_NOZORDER);
    
    currentY += textHeight + (margin * 3/4);
    
    // Progress bar row - restore to original position (right after URL field)
    SetWindowPos(GetDlgItem(hDlg, IDC_PROGRESS_BAR), NULL, 
                urlFieldX, currentY, urlFieldWidth, progressHeight, SWP_NOZORDER);
    
    SetWindowPos(GetDlgItem(hDlg, IDC_GETINFO_BTN), NULL, 
                buttonX, currentY - 1, buttonWidth, buttonHeight, SWP_NOZORDER);
    
    currentY += progressHeight + (margin * 3/4);
    
    // CORRECTED: Two-line video info layout below progress bar
    
    // LINE 1: Video title (truncates to account for "Get Info" button)
    // Mathematical reasoning: availableTextWidth already accounts for button space
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_TITLE_LABEL), NULL, 
                downloadGroupX + margin, currentY, (int)(35 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Title text width = availableTextWidth - label width (35px)
    // This ensures title truncates BEFORE hitting the "Get Info" button
    int titleTextWidth = availableTextWidth - (int)(35 * scaleX);
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_TITLE), NULL, 
                urlFieldX, currentY, titleTextWidth, labelHeight, SWP_NOZORDER);
    
    // Move to next line for duration + download status
    currentY += labelHeight + (margin / 2);  // 6px spacing between lines
    
    // LINE 2: Duration + Download Status (on same line, below title)
    // Duration label: "Duration:" (50px width)
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_DURATION_LABEL), NULL, 
                downloadGroupX + margin, currentY, (int)(50 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Duration value: "5:23" format (60px width - enough for "99:59:59")
    int durationValueX = downloadGroupX + margin + (int)(55 * scaleX);  // After label + 5px gap
    SetWindowPos(GetDlgItem(hDlg, IDC_VIDEO_DURATION), NULL, 
                durationValueX, currentY, (int)(60 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Download status: positioned after duration with 10px gap, uses remaining width
    int statusX = durationValueX + (int)(60 * scaleX) + (int)(10 * scaleX);  // After duration + gap
    int statusWidth = (downloadGroupX + downloadGroupWidth - margin) - statusX;
    SetWindowPos(GetDlgItem(hDlg, IDC_PROGRESS_TEXT), NULL, 
                statusX, currentY, statusWidth, labelHeight, SWP_NOZORDER);
    
    // Position color indicator buttons (hidden by default, positioned for future use)
    // These will be positioned relative to the Add button later in the layout code
    
    // Calculate Offline videos group position and size
    int offlineGroupY = downloadGroupY + downloadGroupHeight + margin;
    int offlineGroupHeight = clientHeight - offlineGroupY - windowMargin;
    
    // Ensure minimum height for offline group
    if (offlineGroupHeight < (int)(100 * scaleY)) {
        offlineGroupHeight = (int)(100 * scaleY);
    }
    
    SetWindowPos(GetDlgItem(hDlg, IDC_OFFLINE_GROUP), NULL, 
                downloadGroupX, offlineGroupY, downloadGroupWidth, offlineGroupHeight, SWP_NOZORDER);
    
    // Position controls within Offline videos group
    int offlineContentY = offlineGroupY + groupTitleHeight + (margin / 2);
    
    // Status labels
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL2), NULL, 
                downloadGroupX + margin, offlineContentY, (int)(150 * scaleX), labelHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_LABEL3), NULL, 
                downloadGroupX + margin + (int)(160 * scaleX), offlineContentY, (int)(100 * scaleX), labelHeight, SWP_NOZORDER);
    
    // Calculate listbox and side buttons
    int listY = offlineContentY + labelHeight + margin;
    int listHeight = offlineGroupY + offlineGroupHeight - listY - margin;
    int sideButtonX = downloadGroupX + downloadGroupWidth - buttonWidth - margin;
    int listWidth = sideButtonX - downloadGroupX - (2 * margin);
    
    // Ensure minimum dimensions
    if (listWidth < (int)(200 * scaleX)) {
        listWidth = (int)(200 * scaleX);
    }
    if (listHeight < (int)(50 * scaleY)) {
        listHeight = (int)(50 * scaleY);
    }
    
    SetWindowPos(GetDlgItem(hDlg, IDC_LIST), NULL, 
                downloadGroupX + margin, listY, listWidth, listHeight, SWP_NOZORDER);
    
    // Resize ListView columns
    ResizeCacheListViewColumns(GetDlgItem(hDlg, IDC_LIST), listWidth);
    
    // Side buttons (Play, Delete, and Add)
    int sideButtonHeight = (int)(32 * scaleY);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON2), NULL, 
                sideButtonX, listY, buttonWidth, sideButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON3), NULL, 
                sideButtonX, listY + sideButtonHeight + (margin / 2), buttonWidth, sideButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON1), NULL, 
                sideButtonX, listY + (sideButtonHeight + (margin / 2)) * 2, buttonWidth, sideButtonHeight, SWP_NOZORDER);
    
    // Position colored buttons in 2x2 grid below Add button
    int addButtonY = listY + (sideButtonHeight + (margin / 2)) * 2;
    int colorButtonStartY = addButtonY + sideButtonHeight + (margin / 2);
    int colorButtonWidth = (int)(36 * scaleX);
    int colorButtonHeight = (int)(20 * scaleY);
    int colorButtonSpacing = (int)(6 * scaleX); // Small gap between buttons
    
    // Top row: Green and Teal
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_GREEN), NULL, 
                sideButtonX, colorButtonStartY, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_TEAL), NULL, 
                sideButtonX + colorButtonWidth + colorButtonSpacing, colorButtonStartY, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    
    // Bottom row: Blue and White
    int colorButtonRow2Y = colorButtonStartY + colorButtonHeight + (int)(4 * scaleY);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_BLUE), NULL, 
                sideButtonX, colorButtonRow2Y, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_COLOR_WHITE), NULL, 
                sideButtonX + colorButtonWidth + colorButtonSpacing, colorButtonRow2Y, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    
    // Debug test buttons in 2x2 grid below color buttons (only visible when debugging enabled)
    int debugButtonStartY = colorButtonRow2Y + colorButtonHeight + (int)(4 * scaleY);
    
    // Top row: Info and Warning
    SetWindowPos(GetDlgItem(hDlg, IDC_DEBUG_TEST_INFO), NULL, 
                sideButtonX, debugButtonStartY, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_DEBUG_TEST_WARNING), NULL, 
                sideButtonX + colorButtonWidth + colorButtonSpacing, debugButtonStartY, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    
    // Bottom row: Error and Success
    int debugButtonRow2Y = debugButtonStartY + colorButtonHeight + (int)(4 * scaleY);
    SetWindowPos(GetDlgItem(hDlg, IDC_DEBUG_TEST_ERROR), NULL, 
                sideButtonX, debugButtonRow2Y, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_DEBUG_TEST_SUCCESS), NULL, 
                sideButtonX + colorButtonWidth + colorButtonSpacing, debugButtonRow2Y, colorButtonWidth, colorButtonHeight, SWP_NOZORDER);
}

// Settings dialog procedure
// Settings dialog component storage
typedef struct {
    FileBrowserComponent* ytdlpBrowser;
    FolderBrowserComponent* downloadFolderBrowser;
    FileBrowserComponent* playerBrowser;
    ComponentRegistry* registry;
} SettingsDialogComponents;

INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static SettingsDialogComponents* components = NULL;
    
    switch (message) {
        case WM_INITDIALOG: {
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            // Create component registry
            components = (SettingsDialogComponents*)SAFE_MALLOC(sizeof(SettingsDialogComponents));
            if (!components) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            memset(components, 0, sizeof(SettingsDialogComponents));
            
            components->registry = CreateComponentRegistry();
            if (!components->registry) {
                SAFE_FREE(components);
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            
            // Store components pointer for later access
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)components);
            
            // Get positions of existing resource-based controls to place components at same location
            RECT ytdlpRect, folderRect, playerRect;
            GetWindowRect(GetDlgItem(hDlg, IDC_YTDLP_PATH), &ytdlpRect);
            GetWindowRect(GetDlgItem(hDlg, IDC_FOLDER_PATH), &folderRect);
            GetWindowRect(GetDlgItem(hDlg, IDC_PLAYER_PATH), &playerRect);
            
            // Convert to client coordinates
            POINT ytdlpPt = {ytdlpRect.left, ytdlpRect.top};
            POINT folderPt = {folderRect.left, folderRect.top};
            POINT playerPt = {playerRect.left, playerRect.top};
            ScreenToClient(hDlg, &ytdlpPt);
            ScreenToClient(hDlg, &folderPt);
            ScreenToClient(hDlg, &playerPt);
            
            // Hide existing resource-based controls (we'll use components instead)
            ShowWindow(GetDlgItem(hDlg, IDC_YTDLP_PATH), SW_HIDE);
            ShowWindow(GetDlgItem(hDlg, IDC_YTDLP_BROWSE), SW_HIDE);
            ShowWindow(GetDlgItem(hDlg, IDC_FOLDER_PATH), SW_HIDE);
            ShowWindow(GetDlgItem(hDlg, IDC_FOLDER_BROWSE), SW_HIDE);
            ShowWindow(GetDlgItem(hDlg, IDC_PLAYER_PATH), SW_HIDE);
            ShowWindow(GetDlgItem(hDlg, IDC_PLAYER_BROWSE), SW_HIDE);
            
            // Create reusable components at the positions of the hidden controls
            // Adjust Y position to account for label above edit control
            int labelHeight = 14;
            int labelSpacing = 3;
            int componentWidth = 350;
            
            // yt-dlp path browser component
            components->ytdlpBrowser = CreateFileBrowser(
                hDlg, ytdlpPt.x, ytdlpPt.y - labelHeight - labelSpacing, componentWidth,
                L"yt-dlp Executable Path:",
                L"Executable Files\0*.exe;*.cmd;*.bat;*.py;*.ps1\0All Files\0*.*\0",
                IDC_YTDLP_PATH
            );
            if (components->ytdlpBrowser) {
                RegisterComponent(components->registry, (UIComponent*)components->ytdlpBrowser);
            }
            
            // Download folder browser component
            components->downloadFolderBrowser = CreateFolderBrowser(
                hDlg, folderPt.x, folderPt.y - labelHeight - labelSpacing, componentWidth,
                L"Download Folder:",
                IDC_FOLDER_PATH
            );
            if (components->downloadFolderBrowser) {
                RegisterComponent(components->registry, (UIComponent*)components->downloadFolderBrowser);
            }
            
            // Media player path browser component
            components->playerBrowser = CreateFileBrowser(
                hDlg, playerPt.x, playerPt.y - labelHeight - labelSpacing, componentWidth,
                L"Media Player Path:",
                L"Executable Files\0*.exe\0All Files\0*.*\0",
                IDC_PLAYER_PATH
            );
            if (components->playerBrowser) {
                RegisterComponent(components->registry, (UIComponent*)components->playerBrowser);
            }
            
            // Load settings from registry and set component values
            wchar_t ytdlpPath[MAX_EXTENDED_PATH] = {0};
            wchar_t downloadPath[MAX_EXTENDED_PATH] = {0};
            wchar_t playerPath[MAX_EXTENDED_PATH] = {0};
            
            // Read from registry using existing functions
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                DWORD dataSize = sizeof(ytdlpPath);
                RegQueryValueExW(hKey, REG_YTDLP_PATH, NULL, NULL, (LPBYTE)ytdlpPath, &dataSize);
                
                dataSize = sizeof(downloadPath);
                RegQueryValueExW(hKey, REG_DOWNLOAD_PATH, NULL, NULL, (LPBYTE)downloadPath, &dataSize);
                
                dataSize = sizeof(playerPath);
                RegQueryValueExW(hKey, REG_PLAYER_PATH, NULL, NULL, (LPBYTE)playerPath, &dataSize);
                
                RegCloseKey(hKey);
            }
            
            // Set default values if empty
            if (wcslen(ytdlpPath) == 0) {
                GetDefaultYtDlpPath(ytdlpPath, MAX_EXTENDED_PATH);
            }
            if (wcslen(downloadPath) == 0) {
                GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
            }
            
            if (components->ytdlpBrowser) {
                SetFileBrowserPath(components->ytdlpBrowser, ytdlpPath);
            }
            if (components->downloadFolderBrowser) {
                SetFolderBrowserPath(components->downloadFolderBrowser, downloadPath);
            }
            if (components->playerBrowser) {
                SetFileBrowserPath(components->playerBrowser, playerPath);
            }
            
            // Load other settings (checkboxes, etc.) using existing LoadSettings function
            // But we need to load checkboxes manually since we're not using the old controls
            if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                DWORD enableDebug = 0, enableLogfile = 0, enableAutopaste = 0;
                DWORD dataSize = sizeof(DWORD);
                
                RegQueryValueExW(hKey, REG_ENABLE_DEBUG, NULL, NULL, (LPBYTE)&enableDebug, &dataSize);
                RegQueryValueExW(hKey, REG_ENABLE_LOGFILE, NULL, NULL, (LPBYTE)&enableLogfile, &dataSize);
                RegQueryValueExW(hKey, REG_ENABLE_AUTOPASTE, NULL, NULL, (LPBYTE)&enableAutopaste, &dataSize);
                
                CheckDlgButton(hDlg, IDC_ENABLE_DEBUG, enableDebug ? BST_CHECKED : BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_ENABLE_LOGFILE, enableLogfile ? BST_CHECKED : BST_UNCHECKED);
                CheckDlgButton(hDlg, IDC_ENABLE_AUTOPASTE, enableAutopaste ? BST_CHECKED : BST_UNCHECKED);
                
                RegCloseKey(hKey);
            }
            
            // Set accessible names for controls
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ENABLE_DEBUG), L"Enable debug mode", L"Show debug information in the main window");
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ENABLE_LOGFILE), L"Enable log file", L"Write debug information to a log file");
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ENABLE_AUTOPASTE), L"Enable auto-paste", L"Automatically paste URLs from clipboard");
            SetControlAccessibility(GetDlgItem(hDlg, IDOK), L"OK", L"Save settings and close dialog");
            SetControlAccessibility(GetDlgItem(hDlg, IDCANCEL), L"Cancel", L"Close dialog without saving");
            
            // Configure tab order
            TabOrderConfig tabConfig;
            TabOrderEntry entries[6];
            entries[0].controlId = IDC_YTDLP_PATH + 1; // Edit control of first component
            entries[0].tabOrder = 0;
            entries[0].isTabStop = TRUE;
            entries[1].controlId = IDC_FOLDER_PATH + 1; // Edit control of second component
            entries[1].tabOrder = 1;
            entries[1].isTabStop = TRUE;
            entries[2].controlId = IDC_PLAYER_PATH + 1; // Edit control of third component
            entries[2].tabOrder = 2;
            entries[2].isTabStop = TRUE;
            entries[3].controlId = IDC_ENABLE_DEBUG;
            entries[3].tabOrder = 3;
            entries[3].isTabStop = TRUE;
            entries[4].controlId = IDOK;
            entries[4].tabOrder = 4;
            entries[4].isTabStop = TRUE;
            entries[5].controlId = IDCANCEL;
            entries[5].tabOrder = 5;
            entries[5].isTabStop = TRUE;
            
            tabConfig.entries = entries;
            tabConfig.count = 6;
            SetDialogTabOrder(hDlg, &tabConfig);
            
            // Apply DPI-aware positioning (similar to error dialog)
            HWND hParent = GetParent(hDlg);
            if (hParent) {
                RECT parentRect, dialogRect;
                GetWindowRect(hDlg, &dialogRect);
                GetWindowRect(hParent, &parentRect);
                
                int dialogWidth = dialogRect.right - dialogRect.left;
                int dialogHeight = dialogRect.bottom - dialogRect.top;
                
                // Get monitor-specific work area for HiDPI awareness
                HMONITOR hMonitor = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                GetMonitorInfoW(hMonitor, &mi);
                RECT screenRect = mi.rcWork;
                
                // Center on parent window
                int x = parentRect.left + (parentRect.right - parentRect.left - dialogWidth) / 2;
                int y = parentRect.top + (parentRect.bottom - parentRect.top - dialogHeight) / 2;
                
                // Adjust if any edge would be off screen
                if (x < screenRect.left) x = screenRect.left;
                if (y < screenRect.top) y = screenRect.top;
                if (x + dialogWidth > screenRect.right) x = screenRect.right - dialogWidth;
                if (y + dialogHeight > screenRect.bottom) y = screenRect.bottom - dialogHeight;
                
                SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            
            // Set initial focus
            SetInitialDialogFocus(hDlg);
            
            return FALSE; // Allow custom focus setting
        }
            
        case WM_COMMAND: {
            // Handle component browse button clicks
            if (components) {
                if (components->ytdlpBrowser && HandleFileBrowserCommand(components->ytdlpBrowser, wParam, lParam)) {
                    return TRUE;
                }
                if (components->downloadFolderBrowser && HandleFolderBrowserCommand(components->downloadFolderBrowser, wParam, lParam)) {
                    return TRUE;
                }
                if (components->playerBrowser && HandleFileBrowserCommand(components->playerBrowser, wParam, lParam)) {
                    return TRUE;
                }
            }
            
            switch (LOWORD(wParam)) {
                case IDC_ENABLE_DEBUG:
                    // Handle debug checkbox change - update visibility immediately
                    if (HIWORD(wParam) == BN_CLICKED) {
                        BOOL enableDebug = (IsDlgButtonChecked(hDlg, IDC_ENABLE_DEBUG) == BST_CHECKED);
                        BOOL currentEnableDebug, currentEnableLogfile;
                        GetDebugState(&currentEnableDebug, &currentEnableLogfile);
                        SetDebugState(enableDebug, currentEnableLogfile);
                        
                        // Find the main dialog window and update debug control visibility
                        HWND hMainDlg = GetParent(hDlg);
                        if (hMainDlg) {
                            UpdateDebugControlVisibility(hMainDlg);
                        }
                    }
                    return TRUE;
                
                case IDOK: {
                    if (!components) {
                        EndDialog(hDlg, IDCANCEL);
                        return TRUE;
                    }
                    
                    // Validate all components before saving
                    UIComponent* componentArray[3];
                    int componentCount = 0;
                    
                    if (components->ytdlpBrowser) {
                        componentArray[componentCount++] = (UIComponent*)components->ytdlpBrowser;
                    }
                    if (components->downloadFolderBrowser) {
                        componentArray[componentCount++] = (UIComponent*)components->downloadFolderBrowser;
                    }
                    if (components->playerBrowser) {
                        componentArray[componentCount++] = (UIComponent*)components->playerBrowser;
                    }
                    
                    // Validate all components
                    ComponentValidationSummary* validationSummary = ValidateDialog(componentArray, componentCount);
                    if (validationSummary && !validationSummary->allValid) {
                        // Show validation errors
                        ShowValidationErrors(hDlg, validationSummary);
                        FreeValidationSummary(validationSummary);
                        return TRUE;
                    }
                    if (validationSummary) {
                        FreeValidationSummary(validationSummary);
                    }
                    
                    // Get values from components and save to registry
                    const wchar_t* ytdlpPath = GetFileBrowserPath(components->ytdlpBrowser);
                    const wchar_t* downloadPath = GetFolderBrowserPath(components->downloadFolderBrowser);
                    const wchar_t* playerPath = GetFileBrowserPath(components->playerBrowser);
                    
                    // Save string settings to registry
                    HKEY hKey;
                    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                        if (ytdlpPath && wcslen(ytdlpPath) > 0) {
                            RegSetValueExW(hKey, REG_YTDLP_PATH, 0, REG_SZ, (const BYTE*)ytdlpPath, (DWORD)((wcslen(ytdlpPath) + 1) * sizeof(wchar_t)));
                        }
                        if (downloadPath && wcslen(downloadPath) > 0) {
                            RegSetValueExW(hKey, REG_DOWNLOAD_PATH, 0, REG_SZ, (const BYTE*)downloadPath, (DWORD)((wcslen(downloadPath) + 1) * sizeof(wchar_t)));
                        }
                        if (playerPath && wcslen(playerPath) > 0) {
                            RegSetValueExW(hKey, REG_PLAYER_PATH, 0, REG_SZ, (const BYTE*)playerPath, (DWORD)((wcslen(playerPath) + 1) * sizeof(wchar_t)));
                        }
                        
                        // Save checkbox settings
                        BOOL enableDebug = (IsDlgButtonChecked(hDlg, IDC_ENABLE_DEBUG) == BST_CHECKED);
                        BOOL enableLogfile = (IsDlgButtonChecked(hDlg, IDC_ENABLE_LOGFILE) == BST_CHECKED);
                        BOOL enableAutopaste = (IsDlgButtonChecked(hDlg, IDC_ENABLE_AUTOPASTE) == BST_CHECKED);
                        
                        DWORD debugValue = enableDebug ? 1 : 0;
                        DWORD logfileValue = enableLogfile ? 1 : 0;
                        DWORD autopasteValue = enableAutopaste ? 1 : 0;
                        
                        RegSetValueExW(hKey, REG_ENABLE_DEBUG, 0, REG_DWORD, (const BYTE*)&debugValue, sizeof(DWORD));
                        RegSetValueExW(hKey, REG_ENABLE_LOGFILE, 0, REG_DWORD, (const BYTE*)&logfileValue, sizeof(DWORD));
                        RegSetValueExW(hKey, REG_ENABLE_AUTOPASTE, 0, REG_DWORD, (const BYTE*)&autopasteValue, sizeof(DWORD));
                        
                        RegCloseKey(hKey);
                    }
                    
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                    
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
        }
        
        case WM_DPICHANGED: {
            // Get new DPI from wParam
            int newDpi = HIWORD(wParam);
            
            // Get suggested window rect from lParam
            RECT* suggestedRect = (RECT*)lParam;
            
            // Update DPI context
            DPIContext* context = GetDPIContext(g_dpiManager, hDlg);
            if (context) {
                int oldDpi = context->currentDpi;
                context->currentDpi = newDpi;
                context->scaleFactor = (double)newDpi / 96.0;
                
                // Rescale all UI elements
                RescaleWindowForDPI(hDlg, oldDpi, newDpi);
                
                // Apply suggested window position and size
                SetWindowPos(hDlg, NULL,
                            suggestedRect->left,
                            suggestedRect->top,
                            suggestedRect->right - suggestedRect->left,
                            suggestedRect->bottom - suggestedRect->top,
                            SWP_NOZORDER | SWP_NOACTIVATE);
            }
            
            return 0;
        }
        
        case WM_SYSCOLORCHANGE:
            // System colors changed (including high contrast mode changes)
            // Apply high contrast colors if needed
            ApplyHighContrastColors(hDlg);
            return TRUE;
        
        case WM_DESTROY:
            // Clean up components
            if (components) {
                if (components->registry) {
                    DestroyComponentRegistry(components->registry);
                }
                SAFE_FREE(components);
                components = NULL;
            }
            break;
    }
    return FALSE;
}

// Progress dialog procedure
INT_PTR CALLBACK ProgressDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static ProgressDialog* pProgress = NULL;
    
    switch (message) {
        case WM_INITDIALOG: {
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            // Store the ProgressDialog pointer passed via lParam
            pProgress = (ProgressDialog*)lParam;
            if (pProgress) {
                pProgress->hDialog = hDlg;
                pProgress->hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS_PROGRESS);
                pProgress->hStatusText = GetDlgItem(hDlg, IDC_PROGRESS_STATUS);
                pProgress->hCancelButton = GetDlgItem(hDlg, IDC_PROGRESS_CANCEL);
                pProgress->cancelled = FALSE;
                
                // Initialize progress bar
                SendMessageW(pProgress->hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
                SendMessageW(pProgress->hProgressBar, PBM_SETPOS, 0, 0);
                
                // Center the dialog with HiDPI awareness
                HWND hParent = GetParent(hDlg);
                RECT parentRect, dialogRect;
                
                GetWindowRect(hDlg, &dialogRect);
                int dialogWidth = dialogRect.right - dialogRect.left;
                int dialogHeight = dialogRect.bottom - dialogRect.top;
                
                // Get monitor-specific work area for HiDPI awareness
                HMONITOR hMonitor;
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                
                if (hParent && GetWindowRect(hParent, &parentRect)) {
                    hMonitor = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
                } else {
                    hMonitor = MonitorFromWindow(hDlg, MONITOR_DEFAULTTONEAREST);
                }
                
                GetMonitorInfoW(hMonitor, &mi);
                RECT screenRect = mi.rcWork;
                
                int x, y;
                
                if (hParent && GetWindowRect(hParent, &parentRect)) {
                    // Center on parent window
                    x = parentRect.left + (parentRect.right - parentRect.left - dialogWidth) / 2;
                    y = parentRect.top + (parentRect.bottom - parentRect.top - dialogHeight) / 2;
                } else {
                    // Center on screen if no parent
                    x = screenRect.left + (screenRect.right - screenRect.left - dialogWidth) / 2;
                    y = screenRect.top + (screenRect.bottom - screenRect.top - dialogHeight) / 2;
                }
                
                // Adjust if any edge would be off screen
                if (x < screenRect.left) x = screenRect.left;
                if (y < screenRect.top) y = screenRect.top;
                if (x + dialogWidth > screenRect.right) x = screenRect.right - dialogWidth;
                if (y + dialogHeight > screenRect.bottom) y = screenRect.bottom - dialogHeight;
                
                SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return TRUE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_PROGRESS_CANCEL:
                    if (pProgress) {
                        pProgress->cancelled = TRUE;
                    }
                    return TRUE;
            }
            break;
        
        case WM_SYSCOLORCHANGE:
            // System colors changed (including high contrast mode changes)
            // Apply high contrast colors if needed
            ApplyHighContrastColors(hDlg);
            return TRUE;
            
        case WM_CLOSE:
            if (pProgress) {
                pProgress->cancelled = TRUE;
            }
            return TRUE;
    }
    return FALSE;
}

// Dialog procedure function
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            // Initialize brushes for text field colors (created on-demand in ApplicationState)
            SetCurrentBrush(GetBrush(BRUSH_WHITE));
               
            // Initialize ListView with columns first
            HWND hListView = GetDlgItem(hDlg, IDC_LIST);
            InitializeCacheListView(hListView);
            
            // Load debug settings from registry
            wchar_t buffer[MAX_EXTENDED_PATH];
            BOOL enableDebug = FALSE, enableLogfile = FALSE, enableAutopaste = TRUE;
            
            if (LoadSettingFromRegistry(REG_ENABLE_DEBUG, buffer, MAX_EXTENDED_PATH)) {
                enableDebug = (wcscmp(buffer, L"1") == 0);
            }
            if (LoadSettingFromRegistry(REG_ENABLE_LOGFILE, buffer, MAX_EXTENDED_PATH)) {
                enableLogfile = (wcscmp(buffer, L"1") == 0);
            }
            if (LoadSettingFromRegistry(REG_ENABLE_AUTOPASTE, buffer, MAX_EXTENDED_PATH)) {
                enableAutopaste = (wcscmp(buffer, L"1") == 0);
            }
            
            // Set the state using the application state functions
            SetDebugState(enableDebug, enableLogfile);
            SetAutopasteState(enableAutopaste);
            
            // Write session start marker to logfile if logging is enabled
            WriteSessionStartToLogfile();
            
            // Now initialize cache manager after logging is enabled
            wchar_t downloadPath[MAX_EXTENDED_PATH];
            if (!LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
            }
            
            if (InitializeCacheManager(GetCacheManager(), downloadPath)) {
                // Scan for existing videos in download folder
                ScanDownloadFolderForVideos(GetCacheManager(), downloadPath);
                
                // Refresh the UI with cached videos
                RefreshCacheList(hListView, GetCacheManager());
                UpdateCacheListStatus(hDlg, GetCacheManager());
            } else {
                // Initialize dialog controls with defaults if cache fails
                SetDlgItemTextW(hDlg, IDC_LABEL2, L"Status: Cache initialization failed");
                SetDlgItemTextW(hDlg, IDC_LABEL3, L"Items: 0");
            }
            
            // Initialize cached video metadata
            InitializeCachedMetadata(GetCachedVideoMetadata());
            
            // Update debug control visibility
            UpdateDebugControlVisibility(hDlg);
            
            // Check command line first, then clipboard
            const wchar_t* cmdURL = GetCommandLineURL();
            if (wcslen(cmdURL) > 0) {
                SetProgrammaticChangeFlag(TRUE);
                SetDlgItemTextW(hDlg, IDC_TEXT_FIELD, cmdURL);
                SetCurrentBrush(GetBrush(BRUSH_LIGHT_TEAL));
                InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                SetProgrammaticChangeFlag(FALSE);
            } else {
                // Check clipboard for YouTube URL
                CheckClipboardForYouTubeURL(hDlg);
            }
            
            // Set focus to the text field for immediate typing
            SetFocus(GetDlgItem(hDlg, IDC_TEXT_FIELD));
            
            // Subclass the text field to detect paste operations
            HWND hTextField = GetDlgItem(hDlg, IDC_TEXT_FIELD);
            WNDPROC originalProc = (WNDPROC)SetWindowLongPtr(hTextField, GWLP_WNDPROC, (LONG_PTR)TextFieldSubclassProc);
            SetOriginalTextFieldProc(originalProc);
            
            // Calculate and set optimal default window size based on DPI
            HDC hdc = GetDC(hDlg);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hDlg, hdc);
            
            double scaleX = (double)dpiX / 96.0;
            double scaleY = (double)dpiY / 96.0;
            
            int defaultWidth, defaultHeight;
            CalculateDefaultWindowSize(&defaultWidth, &defaultHeight, scaleX, scaleY);
            
            // Set the calculated default window size
            SetWindowPos(hDlg, NULL, 0, 0, defaultWidth, defaultHeight, SWP_NOMOVE | SWP_NOZORDER);
            
            return FALSE; // Return FALSE since we set focus manually
        }
            
        case WM_SIZE:
            ResizeControls(hDlg);
            return TRUE;
            
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            
            // Get DPI for dynamic minimum size calculation
            HDC hdc = GetDC(hDlg);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hDlg, hdc);
            
            // Calculate DPI scaling factors (96 DPI = 100% scaling)
            double scaleX = (double)dpiX / 96.0;
            double scaleY = (double)dpiY / 96.0;
            
            // Calculate minimum window size based on content requirements
            int minWidth, minHeight;
            CalculateMinimumWindowSize(&minWidth, &minHeight, scaleX, scaleY);
            
            mmi->ptMinTrackSize.x = minWidth;
            mmi->ptMinTrackSize.y = minHeight;
            return 0;
        }
        
        case WM_DPICHANGED: {
            // Get new DPI from wParam
            int newDpi = HIWORD(wParam);
            
            // Get suggested window rect from lParam
            RECT* suggestedRect = (RECT*)lParam;
            
            // Update DPI context
            DPIContext* context = GetDPIContext(g_dpiManager, hDlg);
            if (context) {
                int oldDpi = context->currentDpi;
                context->currentDpi = newDpi;
                context->scaleFactor = (double)newDpi / 96.0;
                
                // Rescale all UI elements
                RescaleWindowForDPI(hDlg, oldDpi, newDpi);
                
                // Apply suggested window position and size
                SetWindowPos(hDlg, NULL,
                            suggestedRect->left,
                            suggestedRect->top,
                            suggestedRect->right - suggestedRect->left,
                            suggestedRect->bottom - suggestedRect->top,
                            SWP_NOZORDER | SWP_NOACTIVATE);
            }
            
            return 0;
        }
        
        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE) {
                // Window is being activated, check clipboard
                CheckClipboardForYouTubeURL(hDlg);
            }
            break;    
        
        case WM_CTLCOLOREDIT:
            if ((HWND)lParam == GetDlgItem(hDlg, IDC_TEXT_FIELD)) {
                HDC hdc = (HDC)wParam;
                // Set the background color based on current brush
                HBRUSH currentBrush = GetCurrentBrush();
                if (currentBrush == GetBrush(BRUSH_LIGHT_GREEN)) {
                    SetBkColor(hdc, COLOR_LIGHT_GREEN);
                } else if (currentBrush == GetBrush(BRUSH_LIGHT_BLUE)) {
                    SetBkColor(hdc, COLOR_LIGHT_BLUE);
                } else if (currentBrush == GetBrush(BRUSH_LIGHT_TEAL)) {
                    SetBkColor(hdc, COLOR_LIGHT_TEAL);
                } else {
                    SetBkColor(hdc, COLOR_WHITE);
                }
                return (INT_PTR)currentBrush;
            }
            break;
            
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlType == ODT_BUTTON) {
                HBRUSH hBrush = NULL;
                COLORREF color = RGB(255, 255, 255); // Default white
                
                // Determine color based on button ID
                switch (lpDrawItem->CtlID) {
                    case IDC_COLOR_GREEN:
                        color = COLOR_LIGHT_GREEN;
                        break;
                    case IDC_COLOR_TEAL:
                        color = COLOR_LIGHT_TEAL;
                        break;
                    case IDC_COLOR_BLUE:
                        color = COLOR_LIGHT_BLUE;
                        break;
                    case IDC_COLOR_WHITE:
                        color = COLOR_WHITE;
                        break;
                }
                
                // Create brush and fill the button
                hBrush = CreateSolidBrush(color);
                if (hBrush) {
                    FillRect(lpDrawItem->hDC, &lpDrawItem->rcItem, hBrush);
                    DeleteObject(hBrush);
                }
                
                // Draw button border
                if (lpDrawItem->itemState & ODS_SELECTED) {
                    DrawEdge(lpDrawItem->hDC, &lpDrawItem->rcItem, EDGE_SUNKEN, BF_RECT);
                } else {
                    DrawEdge(lpDrawItem->hDC, &lpDrawItem->rcItem, EDGE_RAISED, BF_RECT);
                }
                
                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_EDIT_SELECTALL: {
                    HWND hFocus = GetFocus();
                    if (hFocus == GetDlgItem(hDlg, IDC_TEXT_FIELD)) {
                        SendMessage(hFocus, EM_SETSEL, 0, -1);
                    }
                    return TRUE;
                }
                    
                case ID_FILE_SETTINGS:
                    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SETTINGS), hDlg, SettingsDialogProc);
                    return TRUE;
                    
                case ID_FILE_EXIT:
                    DestroyWindow(hDlg);
                    return TRUE;
                    
                case ID_HELP_INSTALL_YTDLP:
                    InstallYtDlpWithWinget(hDlg);
                    return TRUE;
                    
                case ID_HELP_VIEW_LOG:
                    // TODO: Implement View Log dialog
                    SHOW_ERROR_DIALOG(hDlg, YTC_SEVERITY_INFO, YTC_ERROR_SUCCESS, 
                                     L"View Log functionality will be implemented in a future update.\r\n\r\n"
                                     L"This feature is planned for the next release.");
                    return TRUE;
                    
                case ID_HELP_ABOUT:
                    ShowAboutDialog(hDlg);
                    return TRUE;
                    
                case IDC_TEXT_FIELD:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // Skip processing if this is a programmatic change
                        if (GetProgrammaticChangeFlag()) {
                            break;
                        }
                        
                        // Clear cached metadata when URL changes
                        FreeCachedMetadata(GetCachedVideoMetadata());
                        
                        // Get current text
                        wchar_t buffer[MAX_BUFFER_SIZE];
                        GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, buffer, MAX_BUFFER_SIZE);
                        
                        // Handle different user input scenarios
                        HBRUSH currentBrush = GetCurrentBrush();
                        if (currentBrush == GetBrush(BRUSH_LIGHT_GREEN)) {
                            // User has modified the autopasted content - return to white
                            SetCurrentBrush(GetBrush(BRUSH_WHITE));
                        } else if (currentBrush == GetBrush(BRUSH_LIGHT_BLUE)) {
                            // User is editing manually pasted content - return to white
                            SetCurrentBrush(GetBrush(BRUSH_WHITE));
                        } else if (GetManualPasteFlag() && IsYouTubeURL(buffer)) {
                            // Manual paste of YouTube URL - set to light blue
                            SetCurrentBrush(GetBrush(BRUSH_LIGHT_BLUE));
                            SetManualPasteFlag(FALSE); // Reset flag after use
                        } else if (GetManualPasteFlag()) {
                            // Manual paste of non-YouTube content - keep white but reset flag
                            SetManualPasteFlag(FALSE);
                        }
                        // Note: Light teal (command line) is preserved during editing
                        // Regular typing in white background stays white
                        
                        InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    }
                    break;    
            case IDC_DOWNLOAD_BTN: {
                    // Check if we're currently downloading (button shows "Cancel")
                    if (IsDownloadActive()) {
                        // Cancel the active download
                        if (CancelActiveDownload()) {
                            // Clear download state and reset UI
                            ClearActiveDownload();
                            SetDownloadUIState(hDlg, FALSE);
                            UpdateMainProgressBar(hDlg, 0, L"Download cancelled");
                            
                            // Hide progress bar after a brief delay
                            Sleep(1000);
                            ShowMainProgressBar(hDlg, FALSE);
                            
                            DebugOutput(L"YouTubeCacher: Download cancelled by user");
                        } else {
                            // Failed to cancel - show error
                            UnifiedDialogConfig config = {0};
                            config.dialogType = UNIFIED_DIALOG_ERROR;
                            config.title = L"Cancel Failed";
                            config.message = L"Unable to cancel the download process.";
                            config.details = L"The download process could not be terminated. It may have already completed or encountered an error.";
                            config.tab1_name = L"Details";
                            config.showDetailsButton = TRUE;
                            config.showCopyButton = FALSE;
                            
                            ShowUnifiedDialog(hDlg, &config);
                        }
                        break;
                    }
                    
                    // Not downloading - handle normal download start
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    if (wcslen(url) == 0) {
                        // Use flexible dialog system with contextual content
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_INFO;
                        config.title = L"Enter YouTube URL";
                        config.message = L"Please enter a YouTube URL in the field above to start downloading.";
                        config.details = L"YouTubeCacher downloads videos from YouTube and saves them to your computer. "
                                        L"A valid YouTube URL is required to begin the download process.";
                        config.tab1_name = L"About Download";
                        config.tab2_content = L"How to download a video:\r\n\r\n"
                                            L"1. Copy a YouTube URL from your browser\r\n"
                                            L"   Example: https://www.youtube.com/watch?v=dQw4w9WgXcQ\r\n\r\n"
                                            L"2. Paste it into the URL field at the top\r\n\r\n"
                                            L"3. Click 'Download' to start the process\r\n\r\n"
                                            L"Supported URL formats:\r\n"
                                            L"• youtube.com/watch?v=VIDEO_ID\r\n"
                                            L"• youtu.be/VIDEO_ID\r\n"
                                            L"• youtube.com/shorts/VIDEO_ID\r\n"
                                            L"• YouTube playlist URLs";
                        config.tab2_name = L"Instructions";
                        config.tab3_content = L"Download Features:\r\n\r\n"
                                            L"• Automatic video quality selection\r\n"
                                            L"• Progress tracking with real-time updates\r\n"
                                            L"• Subtitle download when available\r\n"
                                            L"• Automatic file organization\r\n"
                                            L"• Cache management for easy access\r\n\r\n"
                                            L"Downloaded videos are saved to your configured download folder and added to the cache list for easy playback.";
                        config.tab3_name = L"Features";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                        break;
                    }
                    
                    // Check if we already have cached metadata for this URL
                    if (IsCachedMetadataValid(GetCachedVideoMetadata(), url)) {
                        // Use cached metadata to pre-populate UI fields
                        VideoMetadata metadata;
                        if (GetCachedMetadata(GetCachedVideoMetadata(), &metadata)) {
                            if (metadata.title) {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, metadata.title);
                            }
                            if (metadata.duration) {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, metadata.duration);
                            }
                            FreeVideoMetadata(&metadata);
                        }
                        
                        // Start download with cached metadata
                        if (!StartUnifiedDownload(hDlg, url)) {
                            ShowConfigurationError(hDlg, L"Failed to start download. Please check your yt-dlp configuration.");
                        }
                    } else {
                        // No cached metadata - get info asynchronously first, then download
                        // Show progress bar and disable UI for the info retrieval phase
                        ShowMainProgressBar(hDlg, TRUE);
                        SetProgressBarMarquee(hDlg, TRUE);
                        UpdateMainProgressBar(hDlg, -1, L"Getting video information...");
                        SetDownloadUIState(hDlg, TRUE);  // Change button to "Cancel" during metadata retrieval
                        
                        // Set flag to indicate we want to download after getting info
                        SetDownloadAfterInfoFlag(TRUE);
                        
                        // Start asynchronous metadata retrieval
                        if (!StartNonBlockingGetInfo(hDlg, url, GetCachedVideoMetadata())) {
                            // Failed to start async get info
                            SetDownloadAfterInfoFlag(FALSE);
                            SetProgressBarMarquee(hDlg, FALSE);
                            ShowMainProgressBar(hDlg, FALSE);
                            ClearActiveDownload();
                            SetDownloadUIState(hDlg, FALSE);  // Re-enable UI controls on failure
                            ShowConfigurationError(hDlg, L"Failed to start video information retrieval. Please check your yt-dlp configuration.");
                        }
                        // Note: The actual download will be triggered by the WM_USER + 103 message handler
                        // when the async get info completes successfully
                    }
                    break;
                }
                

                    
                case IDC_GETINFO_BTN: {
                    // Check if download is in progress
                    if (GetDownloadingState()) {
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_WARNING;
                        config.title = L"Download in Progress";
                        config.message = L"Please wait for the current download to complete before getting video information.";
                        config.details = L"A video download is currently in progress. Only one operation can run at a time to prevent conflicts.";
                        config.tab1_name = L"Details";
                        config.tab2_content = L"To get video information:\r\n\r\n"
                                            L"1. Wait for the current download to finish\r\n"
                                            L"2. The progress bar will disappear when complete\r\n"
                                            L"3. Then try the Get Info button again\r\n\r\n"
                                            L"You can also cancel the current download if needed.";
                        config.tab2_name = L"How to Fix";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                        break;
                    }
                    
                    // Get URL from text field
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    // Validate URL is provided
                    if (wcslen(url) == 0) {
                        // Use flexible dialog system with contextual content
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_INFO;
                        config.title = L"Enter YouTube URL";
                        config.message = L"Please enter a YouTube URL in the field above to get video information.";
                        config.details = L"The Get Info feature retrieves video metadata (title, duration, etc.) without downloading the video. "
                                        L"This is useful for previewing content before deciding to download.";
                        config.tab1_name = L"About Get Info";
                        config.tab2_content = L"How to use Get Info:\r\n\r\n"
                                            L"1. Copy a YouTube URL from your browser\r\n"
                                            L"   Example: https://www.youtube.com/watch?v=dQw4w9WgXcQ\r\n\r\n"
                                            L"2. Paste it into the URL field at the top\r\n\r\n"
                                            L"3. Click 'Get Info' to retrieve video details\r\n\r\n"
                                            L"Supported URL formats:\r\n"
                                            L"• youtube.com/watch?v=VIDEO_ID\r\n"
                                            L"• youtu.be/VIDEO_ID\r\n"
                                            L"• youtube.com/shorts/VIDEO_ID";
                        config.tab2_name = L"Instructions";
                        config.tab3_content = L"Get Info Benefits:\r\n\r\n"
                                            L"• Preview video title and duration\r\n"
                                            L"• Verify the URL is valid before downloading\r\n"
                                            L"• Check if video is available in your region\r\n"
                                            L"• Fast operation - no video data is downloaded\r\n\r\n"
                                            L"This helps you make informed decisions about which videos to download.";
                        config.tab3_name = L"Benefits";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                        break;
                    }
                    
                    // Check if we already have cached data for this URL
                    if (IsCachedMetadataValid(GetCachedVideoMetadata(), url)) {
                        VideoMetadata metadata;
                        if (GetCachedMetadata(GetCachedVideoMetadata(), &metadata)) {
                            // Update UI with cached information
                            if (metadata.title) {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, metadata.title);
                            } else {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, L"Unknown Title");
                            }
                            
                            if (metadata.duration) {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, metadata.duration);
                            } else {
                                SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, L"Unknown");
                            }
                            
                            // Show progress briefly to indicate completion
                            ShowMainProgressBar(hDlg, TRUE);
                            UpdateMainProgressBar(hDlg, 100, L"Video information (cached)");
                            
                            FreeVideoMetadata(&metadata);
                            break;
                        }
                    }
                    
                    // Show progress bar with indeterminate animation
                    ShowMainProgressBar(hDlg, TRUE);
                    // Set progress bar to marquee (indeterminate) style for indefinite operation
                    SetProgressBarMarquee(hDlg, TRUE);
                    UpdateMainProgressBar(hDlg, -1, L"Getting video information...");
                    
                    // Start non-blocking Get Info operation with enhanced error reporting
                    OperationResult* result = StartNonBlockingGetInfoEx(hDlg, url, GetCachedVideoMetadata());
                    if (!result || !result->success) {
                        // Failed to start operation
                        SetProgressBarMarquee(hDlg, FALSE);
                        UpdateMainProgressBar(hDlg, 0, L"Failed to start operation");
                        
                        if (result && result->errorInfo) {
                            // Show detailed error information
                            ShowDetailedError(hDlg, result->errorInfo);
                        } else {
                            // Fallback to generic error
                            UnifiedDialogConfig config = {0};
                            config.dialogType = UNIFIED_DIALOG_ERROR;
                            config.title = L"Operation Failed";
                            config.message = L"Could not start video information retrieval. Please try again.";
                            config.details = L"An unexpected error occurred while attempting to retrieve video information. The operation could not be started.";
                            config.tab1_name = L"Details";
                            config.tab2_content = L"Try these steps:\r\n\r\n"
                                                L"1. Check your internet connection\r\n"
                                                L"2. Verify the YouTube URL is valid\r\n"
                                                L"3. Ensure yt-dlp is properly installed\r\n"
                                                L"4. Try restarting the application\r\n\r\n"
                                                L"If the problem persists, check the application logs for more details.";
                            config.tab2_name = L"Troubleshooting";
                            config.showDetailsButton = TRUE;
                            config.showCopyButton = TRUE;
                            
                            ShowUnifiedDialog(hDlg, &config);
                        }
                    }
                    
                    if (result) {
                        FreeOperationResult(result);
                    }
                    
                    break;
                }  
              case IDC_BUTTON2: { // Play button
                    HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                    wchar_t* selectedVideoId = GetSelectedVideoId(hListView);
                    
                    if (!selectedVideoId) {
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_WARNING;
                        config.title = L"No Selection";
                        config.message = L"Please select a video from the list to play.";
                        config.details = L"No video is currently selected in the offline videos list. You must select a video before playing it.";
                        config.tab1_name = L"Details";
                        config.tab2_content = L"To play a video:\r\n\r\n"
                                            L"1. Click on a video in the offline videos list\r\n"
                                            L"2. The selected video will be highlighted\r\n"
                                            L"3. Click the Play button\r\n\r\n"
                                            L"Make sure you have downloaded videos first, or the list will be empty.";
                        config.tab2_name = L"How to Fix";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                        break;
                    }
                    
                    // Get player path from settings
                    wchar_t playerPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_PLAYER_PATH, playerPath, MAX_EXTENDED_PATH)) {
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_WARNING;
                        config.title = L"Player Not Configured";
                        config.message = L"Please configure a media player in File > Settings.";
                        config.details = L"No media player has been configured. A media player is required to play downloaded videos.";
                        config.tab1_name = L"Details";
                        config.tab2_content = L"To configure a media player:\r\n\r\n"
                                            L"1. Go to File > Settings\r\n"
                                            L"2. Click the browse button next to 'Media player'\r\n"
                                            L"3. Select your preferred video player (e.g., VLC, Windows Media Player)\r\n"
                                            L"4. Click OK to save\r\n\r\n"
                                            L"Popular media players:\r\n"
                                            L"• VLC Media Player (recommended)\r\n"
                                            L"• Windows Media Player\r\n"
                                            L"• PotPlayer\r\n"
                                            L"• MPC-HC";
                        config.tab2_name = L"How to Fix";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                        break;
                    }
                    
                    // Validate player exists
                    DWORD attributes = GetFileAttributesW(playerPath);
                    if (attributes == INVALID_FILE_ATTRIBUTES) {
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_ERROR;
                        config.title = L"Player Not Found";
                        config.message = L"The configured media player was not found. Please check the path in Settings.";
                        config.details = L"The media player executable file could not be found at the configured path. It may have been moved, deleted, or uninstalled.";
                        config.tab1_name = L"Details";
                        config.tab2_content = L"To fix this issue:\n\n"
                                            L"1. Go to File > Settings\n"
                                            L"2. Check the media player path\n"
                                            L"3. Browse to the correct location of your media player\n"
                                            L"4. Or install a new media player if it was uninstalled\n\n"
                                            L"Common media player locations:\n"
                                            L"• VLC: C:\\Program Files\\VideoLAN\\VLC\\vlc.exe\n"
                                            L"• Windows Media Player: C:\\Program Files\\Windows Media Player\\wmplayer.exe";
                        config.tab2_name = L"How to Fix";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                        break;
                    }
                    
                    // Play the video (no success popup)
                    if (!PlayCacheEntry(GetCacheManager(), selectedVideoId, playerPath)) {
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_ERROR;
                        config.title = L"Playback Failed";
                        config.message = L"Failed to launch the video. The file may have been moved or deleted.";
                        config.details = L"The video file could not be opened by the media player. This usually happens when the file has been moved, deleted, or corrupted.";
                        config.tab1_name = L"Details";
                        config.tab2_content = L"Possible solutions:\n\n"
                                            L"1. Check if the video file still exists\n"
                                            L"2. Try downloading the video again\n"
                                            L"3. Verify your media player is working correctly\n"
                                            L"4. Check if the file is being used by another program\n\n"
                                            L"The cache list will be refreshed to remove any invalid entries.";
                        config.tab2_name = L"Troubleshooting";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                        // Refresh cache to remove invalid entries
                        RefreshCacheList(hListView, GetCacheManager());
                        UpdateCacheListStatus(hDlg, GetCacheManager());
                    }
                    break;
                }
                    
                case IDC_BUTTON3: { // Delete button
                    HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                    int selectedCount = 0;
                    wchar_t** selectedVideoIds = GetSelectedVideoIds(hListView, &selectedCount);
                    
                    if (!selectedVideoIds || selectedCount == 0) {
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_WARNING;
                        config.title = L"No Selection";
                        config.message = L"Please select one or more videos from the list to delete.";
                        config.details = L"No videos are currently selected in the offline videos list. You must select at least one video before deleting.";
                        config.tab1_name = L"Details";
                        config.tab2_content = L"To delete videos:\n\n"
                                            L"1. Click on one or more videos in the list\n"
                                            L"2. Hold Ctrl while clicking to select multiple videos\n"
                                            L"3. Selected videos will be highlighted\n"
                                            L"4. Click the Delete button\n\n"
                                            L"Warning: Deleted videos cannot be recovered and will need to be downloaded again.";
                        config.tab2_name = L"How to Delete";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                        break;
                    }
                    
                    // Build confirmation message
                    wchar_t confirmMsg[1024];
                    if (selectedCount == 1) {
                        // Single video - show title if available
                        CacheEntry* entry = FindCacheEntry(GetCacheManager(), selectedVideoIds[0]);
                        if (entry && entry->title) {
                            swprintf(confirmMsg, 1024, 
                                    L"Are you sure you want to delete \"%ls\"?\r\n\r\n"
                                    L"This will permanently delete the video file and any associated subtitle files.",
                                    entry->title);
                        } else {
                            wcscpy(confirmMsg, L"Are you sure you want to delete the selected video?\r\n\r\n"
                                             L"This will permanently delete the video file and any associated subtitle files.");
                        }
                    } else {
                        // Multiple videos
                        swprintf(confirmMsg, 1024, 
                                L"Are you sure you want to delete %d selected videos?\r\n\r\n"
                                L"This will permanently delete all video files and any associated subtitle files.",
                                selectedCount);
                    }
                    
                    // Confirm deletion - using MessageBox for YES/NO confirmation
                    // Note: UnifiedDialog system doesn't support confirmation dialogs yet
                    int result = MessageBoxW(hDlg, confirmMsg, L"Confirm Delete", 
                                           MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                    
                    if (result == IDYES) {
                        // Log the start of delete operation
                        wchar_t logMsg[512];
                        if (selectedCount == 1) {
                            CacheEntry* entry = FindCacheEntry(GetCacheManager(), selectedVideoIds[0]);
                            if (entry && entry->title) {
                                swprintf(logMsg, 512, L"Starting delete operation for video: %ls (ID: %ls)", 
                                        entry->title, selectedVideoIds[0]);
                            } else {
                                swprintf(logMsg, 512, L"Starting delete operation for video ID: %ls", selectedVideoIds[0]);
                            }
                        } else {
                            swprintf(logMsg, 512, L"Starting delete operation for %d selected videos", selectedCount);
                        }
                        WriteToLogfile(logMsg);
                        
                        // Delete all selected videos
                        int totalErrors = 0;
                        int totalSuccessful = 0;
                        wchar_t* combinedErrorDetails = NULL;
                        size_t combinedErrorSize = 0;
                        
                        for (int i = 0; i < selectedCount; i++) {
                            DeleteResult* deleteResult = DeleteCacheEntryFilesDetailed(GetCacheManager(), selectedVideoIds[i]);
                            
                            if (deleteResult) {
                                if (deleteResult->errorCount == 0) {
                                    totalSuccessful++;
                                } else {
                                    totalErrors += deleteResult->errorCount;
                                    
                                    // Combine error details
                                    wchar_t* errorDetails = FormatDeleteErrorDetails(deleteResult);
                                    if (errorDetails) {
                                        size_t errorLen = wcslen(errorDetails);
                                        size_t newSize = combinedErrorSize + errorLen + 100; // Extra space for headers
                                        
                                        wchar_t* newCombined = (wchar_t*)SAFE_REALLOC(combinedErrorDetails, newSize * sizeof(wchar_t));
                                        if (newCombined) {
                                            combinedErrorDetails = newCombined;
                                            
                                            if (combinedErrorSize == 0) {
                                                wcscpy(combinedErrorDetails, L"Multiple Delete Operation Results:\n");
                                                wcscpy(combinedErrorDetails + wcslen(combinedErrorDetails), L"=====================================\n\n");
                                            }
                                            
                                            // Add video identifier
                                            CacheEntry* entry = FindCacheEntry(GetCacheManager(), selectedVideoIds[i]);
                                            if (entry && entry->title) {
                                                swprintf(combinedErrorDetails + wcslen(combinedErrorDetails), 
                                                        newSize - wcslen(combinedErrorDetails),
                                                        L"Video: %ls\n", entry->title);
                                            } else {
                                                swprintf(combinedErrorDetails + wcslen(combinedErrorDetails), 
                                                        newSize - wcslen(combinedErrorDetails),
                                                        L"Video ID: %ls\n", selectedVideoIds[i]);
                                            }
                                            
                                            wcscat(combinedErrorDetails, errorDetails);
                                            wcscat(combinedErrorDetails, L"\n");
                                            
                                            combinedErrorSize = wcslen(combinedErrorDetails);
                                        }
                                        
                                        SAFE_FREE(errorDetails);
                                    }
                                }
                                
                                FreeDeleteResult(deleteResult);
                            }
                        }
                        
                        // Show results
                        if (totalErrors == 0) {
                            // All deletions successful (no success popup)
                        } else {
                            // Some deletions failed - show combined error details
                            if (combinedErrorDetails) {
                                // Add summary at the beginning
                                wchar_t summary[256];
                                swprintf(summary, 256, 
                                        L"Summary: %d videos processed, %d successful, %d failed\n\n",
                                        selectedCount, totalSuccessful, selectedCount - totalSuccessful);
                                
                                size_t summaryLen = wcslen(summary);
                                size_t totalLen = summaryLen + wcslen(combinedErrorDetails) + 1;
                                wchar_t* finalDetails = (wchar_t*)SAFE_MALLOC(totalLen * sizeof(wchar_t));
                                
                                if (finalDetails) {
                                    wcscpy(finalDetails, summary);
                                    wcscat(finalDetails, combinedErrorDetails);
                                    
                                    UnifiedDialogConfig config = {0};
                                    config.dialogType = UNIFIED_DIALOG_ERROR;
                                    config.title = L"Multiple Delete Failed";
                                    config.message = L"Some files failed to delete. They may be in use or you may not have permission.";
                                    config.details = finalDetails;
                                    config.tab1_name = L"Details";
                                    config.tab2_content = L"Check if files are currently open in another application or if you have sufficient permissions.";
                                    config.tab2_name = L"Diagnosis";
                                    config.tab3_content = L"• Close any applications that might be using the files\n"
                                                        L"• Run as administrator if permission is denied\n"
                                                        L"• Check if files are read-only or protected\n"
                                                        L"• Restart the application and try again";
                                    config.tab3_name = L"Solutions";
                                    config.showDetailsButton = TRUE;
                                    config.showCopyButton = TRUE;
                                    
                                    ShowUnifiedDialog(hDlg, &config);
                                    
                                    SAFE_FREE(finalDetails);
                                }
                            } else {
                                UnifiedDialogConfig config = {0};
                                config.dialogType = UNIFIED_DIALOG_ERROR;
                                config.title = L"Delete Failed";
                                config.message = L"Failed to delete some or all files. They may be in use or you may not have permission.";
                                config.details = L"One or more video files could not be deleted. This can happen if files are currently open in a media player or if you don't have sufficient permissions.";
                                config.tab1_name = L"Details";
                                config.tab2_content = L"To resolve this issue:\n\n"
                                                    L"1. Close any media players that might be using the files\n"
                                                    L"2. Make sure no other programs have the files open\n"
                                                    L"3. Check that you have permission to delete files in the download folder\n"
                                                    L"4. Try running the application as administrator\n"
                                                    L"5. Wait a moment and try deleting again\n\n"
                                                    L"Some files may have been successfully deleted even if this error appeared.";
                                config.tab2_name = L"How to Fix";
                                config.showDetailsButton = TRUE;
                                config.showCopyButton = FALSE;
                                
                                ShowUnifiedDialog(hDlg, &config);
                            }
                        }
                        
                        if (combinedErrorDetails) {
                            SAFE_FREE(combinedErrorDetails);
                        }
                        
                        // Log delete operation summary
                        wchar_t summaryMsg[256];
                        swprintf(summaryMsg, 256, L"Delete operation completed: %d videos processed, %d successful, %d failed", 
                                selectedCount, totalSuccessful, selectedCount - totalSuccessful);
                        WriteToLogfile(summaryMsg);
                        
                        // Refresh the list to show current state
                        RefreshCacheList(hListView, GetCacheManager());
                        UpdateCacheListStatus(hDlg, GetCacheManager());
                    }
                    
                    // Clean up selected video IDs
                    FreeSelectedVideoIds(selectedVideoIds, selectedCount);
                    break;
                }          
      case IDC_BUTTON1: { // Add button (for debugging)
                    // Get download path
                    wchar_t downloadPath[MAX_EXTENDED_PATH];
                    if (!LoadSettingFromRegistry(REG_DOWNLOAD_PATH, downloadPath, MAX_EXTENDED_PATH)) {
                        GetDefaultDownloadPath(downloadPath, MAX_EXTENDED_PATH);
                    }
                    
                    // Create download directory if it doesn't exist
                    CreateDownloadDirectoryIfNeeded(downloadPath);
                    
                    // Add dummy video (no success popup)
                    if (AddDummyVideo(GetCacheManager(), downloadPath)) {
                        // Refresh the list
                        HWND hListView = GetDlgItem(hDlg, IDC_LIST);
                        RefreshCacheList(hListView, GetCacheManager());
                        UpdateCacheListStatus(hDlg, GetCacheManager());
                    } else {
                        UnifiedDialogConfig config = {0};
                        config.dialogType = UNIFIED_DIALOG_ERROR;
                        config.title = L"Add Failed";
                        config.message = L"Failed to add dummy video to cache.";
                        config.details = L"The test video entry could not be added to the cache database. This may indicate a problem with the cache system.";
                        config.tab1_name = L"Details";
                        config.tab2_content = L"This is a development/testing feature. If you're seeing this error:\n\n"
                                            L"1. Check that the download folder is accessible\n"
                                            L"2. Verify you have write permissions to the cache database\n"
                                            L"3. Try restarting the application\n"
                                            L"4. Check available disk space\n\n"
                                            L"This error doesn't affect normal video downloading functionality.";
                        config.tab2_name = L"Information";
                        config.showDetailsButton = TRUE;
                        config.showCopyButton = FALSE;
                        
                        ShowUnifiedDialog(hDlg, &config);
                    }
                    break;
                }
                    
                case IDC_COLOR_GREEN:
                    SetCurrentBrush(GetBrush(BRUSH_LIGHT_GREEN));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_TEAL:
                    SetCurrentBrush(GetBrush(BRUSH_LIGHT_TEAL));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_BLUE:
                    SetCurrentBrush(GetBrush(BRUSH_LIGHT_BLUE));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_COLOR_WHITE:
                    SetCurrentBrush(GetBrush(BRUSH_WHITE));
                    InvalidateRect(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, TRUE);
                    break;
                    
                case IDC_DEBUG_TEST_INFO: {
                    // Test Info dialog using new error dialog system
                    SHOW_ERROR_DIALOG(hDlg, YTC_SEVERITY_INFO, YTC_ERROR_SUCCESS,
                                     L"This is a test information dialog to verify the new error dialog system is working correctly.\r\n\r\n"
                                     L"This tests the INFO severity mapping to UNIFIED_DIALOG_INFO type with proper visual styling.");
                    break;
                }
                
                case IDC_DEBUG_TEST_WARNING: {
                    // Test Warning dialog using new error dialog system
                    SHOW_ERROR_DIALOG(hDlg, YTC_SEVERITY_WARNING, YTC_ERROR_CONFIGURATION,
                                     L"This is a test warning dialog to verify warning notifications are displayed correctly.\r\n\r\n"
                                     L"This tests the WARNING severity mapping to UNIFIED_DIALOG_WARNING type with proper visual styling.");
                    break;
                }
                
                case IDC_DEBUG_TEST_ERROR: {
                    // Test Error dialog using new error dialog system
                    SHOW_ERROR_DIALOG(hDlg, YTC_SEVERITY_ERROR, YTC_ERROR_VALIDATION_FAILED,
                                     L"This is a test error dialog to verify error reporting is working correctly.\r\n\r\n"
                                     L"This tests the ERROR severity mapping to UNIFIED_DIALOG_ERROR type with proper visual styling and technical details.");
                    break;
                }
                
                case IDC_DEBUG_TEST_SUCCESS: {
                    // Test Success dialog
                    UnifiedDialogConfig config = {0};
                    config.dialogType = UNIFIED_DIALOG_SUCCESS;
                    config.title = L"Test Success Dialog";
                    config.message = L"This is a test success dialog to verify success notifications are displayed correctly.";
                    config.details = L"Success dialogs are used to confirm:\r\n\r\n"
                                   L"• Successful completion of operations\r\n"
                                   L"• Positive outcomes that users should know about\r\n"
                                   L"• Configuration changes that were applied\r\n"
                                   L"• Downloads or installations that completed\r\n\r\n"
                                   L"They use a green checkmark icon and provide positive feedback to users.";
                    config.tab1_name = L"Details";
                    config.tab2_content = L"Success scenarios in YouTube Cacher:\r\n\r\n"
                                        L"• Video downloaded successfully\r\n"
                                        L"• yt-dlp installation completed\r\n"
                                        L"• Settings saved and applied\r\n"
                                        L"• Cache refresh completed\r\n"
                                        L"• Video information retrieved\r\n\r\n"
                                        L"Success dialogs help users understand that their actions were completed successfully.";
                    config.tab2_name = L"Examples";
                    config.showDetailsButton = TRUE;
                    config.showCopyButton = FALSE;
                    
                    ShowUnifiedDialog(hDlg, &config);
                    break;
                }
                    
                case IDCANCEL:
                    DestroyWindow(hDlg);
                    return TRUE;
            }
            break;
            
        case WM_SHOWWINDOW:
            // Apply delayed theming when window is shown
            if (wParam) { // Window is being shown
                ApplyDelayedTheming(hDlg);
            }
            return FALSE;
            
        case WM_TIMER:
            // Handle delayed theming timer
            if (wParam == 9999) {
                KillTimer(hDlg, 9999);
                ApplyModernThemeToDialog(hDlg);
                return TRUE;
            }
            return FALSE;
        
        case WM_SYSCOLORCHANGE:
            // System colors changed (including high contrast mode changes)
            // Apply high contrast colors if needed
            ApplyHighContrastColors(hDlg);
            return TRUE;
            
        case WM_CLOSE:
            // Restore original text field window procedure
            WNDPROC originalProc = GetOriginalTextFieldProc();
            if (originalProc) {
                HWND hTextField = GetDlgItem(hDlg, IDC_TEXT_FIELD);
                if (hTextField) {
                    SetWindowLongPtr(hTextField, GWLP_WNDPROC, (LONG_PTR)originalProc);
                }
                // Clear the stored procedure to prevent double-restoration
                SetOriginalTextFieldProc(NULL);
            }
            
            // Just destroy the window - cleanup will happen in WM_DESTROY
            DestroyWindow(hDlg);
            return TRUE;
            
        case WM_USER + 100: {
            // Handle thread-safe progress updates from worker threads
            int percentage = (int)wParam;
            const wchar_t* status = (const wchar_t*)lParam;
            UpdateMainProgressBar(hDlg, percentage, status);
            return TRUE;
        }
        
        case WM_USER + 101: {
            // Handle video info thread completion
            VideoInfoThreadData* data = (VideoInfoThreadData*)lParam;
            if (data) {
                if (data->success) {
                    UpdateMainProgressBar(hDlg, 90, L"Updating interface...");
                    
                    // Update the UI with the retrieved information
                    UpdateVideoInfoUI(hDlg, data->title, data->duration);
                    
                    UpdateMainProgressBar(hDlg, 100, L"Video information retrieved successfully");
                    // Keep progress bar visible - don't hide it
                    // Success dialog removed - info is displayed in the UI fields
                } else {
                    UpdateMainProgressBar(hDlg, 0, L"Failed to retrieve video information");
                    // Keep progress bar visible to show failure status
                    
                    // Clear any existing video info
                    UpdateVideoInfoUI(hDlg, L"", L"");
                    
                    UnifiedDialogConfig config = {0};
                    config.dialogType = UNIFIED_DIALOG_ERROR;
                    config.title = L"Information Retrieval Failed";
                    config.message = L"Could not retrieve video information from the provided URL.";
                    config.details = L"The video information could not be retrieved. This may be due to network issues, an invalid URL, or the video being unavailable.";
                    config.tab1_name = L"Details";
                    config.tab2_content = L"Please check:\n\n"
                                        L"• The URL is valid and accessible\n"
                                        L"• yt-dlp is properly installed and configured\n"
                                        L"• You have an internet connection\n"
                                        L"• The video is not private or restricted\n"
                                        L"• The video hasn't been deleted or made unavailable\n\n"
                                        L"If the URL works in your web browser but not here, try:\n"
                                        L"• Updating yt-dlp to the latest version\n"
                                        L"• Checking if YouTube has changed their API\n"
                                        L"• Waiting a few minutes and trying again";
                    config.tab2_name = L"Troubleshooting";
                    config.showDetailsButton = TRUE;
                    config.showCopyButton = TRUE;
                    
                    ShowUnifiedDialog(hDlg, &config);
                }
                
                // Cleanup thread data
                if (data->hThread) {
                    CloseHandle(data->hThread);
                }
                SAFE_FREE(data);
            }
            return TRUE;
        }
        
        case WM_USER + 103: {
            // Handle enhanced metadata retrieval completion with detailed error reporting
            BOOL success = (BOOL)wParam;
            VideoMetadata* metadata = (VideoMetadata*)lParam;
            
            // Stop progress animation
            SetProgressBarMarquee(hDlg, FALSE);
            
            if (success && metadata && metadata->success) {
                UpdateMainProgressBar(hDlg, 90, L"Updating interface...");
                
                // Update the UI with the retrieved information
                UpdateVideoInfoUI(hDlg, metadata->title, metadata->duration);
                
                UpdateMainProgressBar(hDlg, 100, L"Video information retrieved successfully");
            } else {
                UpdateMainProgressBar(hDlg, 0, L"Failed to retrieve video information");
                
                // Clear any existing video info
                UpdateVideoInfoUI(hDlg, L"", L"");
                
                // Create detailed error information based on what we know
                DetailedErrorInfo* errorInfo = NULL;
                
                if (!metadata) {
                    errorInfo = CreateDetailedErrorInfo(
                        ERROR_TYPE_MEMORY_ALLOCATION, 0,
                        L"Video Information Retrieval", L"Metadata structure is NULL");
                } else {
                    // Try to determine the specific error type
                    ErrorType errorType = ERROR_TYPE_YTDLP_EXECUTION;
                    wchar_t contextBuffer[512];
                    
                    // Get the URL from the edit control for context
                    HWND hEdit = GetDlgItem(hDlg, IDC_TEXT_FIELD);
                    wchar_t url[MAX_URL_LENGTH];
                    GetWindowTextW(hEdit, url, MAX_URL_LENGTH);
                    
                    swprintf(contextBuffer, 512, L"URL: %ls", url);
                    
                    // Check if it's a URL validation issue
                    if (!IsYouTubeURL(url)) {
                        errorType = ERROR_TYPE_URL_INVALID;
                    }
                    
                    errorInfo = CreateDetailedErrorInfo(
                        errorType, 1, // Exit code 1 for general yt-dlp failure
                        L"Video Information Retrieval", contextBuffer);
                }
                
                if (errorInfo) {
                    ShowDetailedError(hDlg, errorInfo);
                    FreeDetailedErrorInfo(errorInfo);
                } else {
                    // Fallback to generic error
                    UnifiedDialogConfig config = {0};
                    config.dialogType = UNIFIED_DIALOG_ERROR;
                    config.title = L"Information Retrieval Failed";
                    config.message = L"Could not retrieve video information. Please try again.";
                    config.details = L"An unexpected error occurred while retrieving video information. The specific cause could not be determined.";
                    config.tab1_name = L"Details";
                    config.tab2_content = L"Try these steps:\n\n"
                                        L"1. Check your internet connection\n"
                                        L"2. Verify the YouTube URL is correct\n"
                                        L"3. Make sure yt-dlp is installed and working\n"
                                        L"4. Try a different YouTube video\n"
                                        L"5. Restart the application\n\n"
                                        L"If the problem persists, there may be a temporary issue with YouTube or yt-dlp.";
                    config.tab2_name = L"Troubleshooting";
                    config.showDetailsButton = TRUE;
                    config.showCopyButton = TRUE;
                    
                    ShowUnifiedDialog(hDlg, &config);
                }
            }
            
            // Check if we need to start download after getting info
            BOOL shouldDownload = GetDownloadAfterInfoFlag();
            if (shouldDownload) {
                SetDownloadAfterInfoFlag(FALSE);  // Clear the flag
                
                if (success && metadata && metadata->success) {
                    // Successfully got metadata, now start download
                    wchar_t url[MAX_URL_LENGTH];
                    GetDlgItemTextW(hDlg, IDC_TEXT_FIELD, url, MAX_URL_LENGTH);
                    
                    UpdateMainProgressBar(hDlg, -1, L"Starting download...");
                    
                    if (!StartUnifiedDownload(hDlg, url)) {
                        // Failed to start download
                        SetProgressBarMarquee(hDlg, FALSE);
                        ShowMainProgressBar(hDlg, FALSE);
                        ClearActiveDownload();
                        SetDownloadUIState(hDlg, FALSE);  // Re-enable UI controls
                        ShowConfigurationError(hDlg, L"Failed to start download. Please check your yt-dlp configuration.");
                    }
                    // Note: UI state will be managed by StartUnifiedDownload and the download process
                } else {
                    // Failed to get metadata, can't download
                    ShowMainProgressBar(hDlg, FALSE);
                    ClearActiveDownload();
                    SetDownloadUIState(hDlg, FALSE);  // Re-enable UI controls
                }
            } else {
                // This was just a Get Info operation, hide progress bar and re-enable UI
                ShowMainProgressBar(hDlg, FALSE);
                ClearActiveDownload();
                SetDownloadUIState(hDlg, FALSE);  // Re-enable UI controls
            }
            
            // Cleanup metadata (it was allocated by the worker thread)
            if (metadata) {
                FreeVideoMetadata(metadata);
                SAFE_FREE(metadata);
            }
            
            return TRUE;
        }
        

        
        case WM_DOWNLOAD_COMPLETE: {
            OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE message received\n");
            
            // Handle download completion
            YtDlpResult* result = (YtDlpResult*)wParam;
            NonBlockingDownloadContext* downloadContext = (NonBlockingDownloadContext*)lParam;
            
            if (!result) {
                OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - NULL result\n");
                return TRUE;
            }
            
            if (!downloadContext) {
                OutputDebugStringW(L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - NULL downloadContext\n");
                return TRUE;
            }
            
            wchar_t debugMsg[256];
            swprintf(debugMsg, 256, L"YouTubeCacher: WM_DOWNLOAD_COMPLETE - success=%d, exitCode=%d\n", 
                    result->success, result->exitCode);
            OutputDebugStringW(debugMsg);
            
            HandleDownloadCompletion(hDlg, result, downloadContext);
            return TRUE;
        }
        
        case WM_UNIFIED_DOWNLOAD_UPDATE: {
            int updateType = (int)wParam;
            LPARAM data = lParam;
            
            switch (updateType) {
                case 1: { // Update video title
                    wchar_t* title = (wchar_t*)data;
                    if (title) {
                        // Debug: Log the title received via message
                        wchar_t debugMsg[1024];
                        swprintf(debugMsg, 1024, L"YouTubeCacher: Received title via message: %ls (length: %zu)\n", title, wcslen(title));
                        OutputDebugStringW(debugMsg);
                        
                        // Debug: Log individual characters
                        OutputDebugStringW(L"YouTubeCacher: Message title character codes: ");
                        for (size_t i = 0; i < min(wcslen(title), 20); i++) {
                            swprintf(debugMsg, 1024, L"U+%04X ", (unsigned int)title[i]);
                            OutputDebugStringW(debugMsg);
                        }
                        OutputDebugStringW(L"\n");
                        
                        SetDlgItemTextW(hDlg, IDC_VIDEO_TITLE, title);
                        SAFE_FREE(title);
                    }
                    break;
                }
                case 2: { // Update video duration
                    wchar_t* duration = (wchar_t*)data;
                    if (duration) {
                        SetDlgItemTextW(hDlg, IDC_VIDEO_DURATION, duration);
                        SAFE_FREE(duration);
                    }
                    break;
                }
                case 3: { // Update progress percentage
                    int percentage = (int)data;
                    if (percentage == -1) {
                        // Indeterminate progress - use marquee mode
                        SetProgressBarMarquee(hDlg, TRUE);
                    } else {
                        // Determinate progress - use percentage mode
                        UpdateMainProgressBar(hDlg, percentage, NULL);
                    }
                    break;
                }
                case 4: { // Start marquee
                    SetProgressBarMarquee(hDlg, TRUE);
                    break;
                }
                case 5: { // Update status text
                    wchar_t* status = (wchar_t*)data;
                    if (status) {
                        SetDlgItemTextW(hDlg, IDC_PROGRESS_TEXT, status);
                        SAFE_FREE(status);
                    }
                    break;
                }
                case 6: { // Stop marquee
                    SetProgressBarMarquee(hDlg, FALSE);
                    break;
                }
                case 7: { // Download failed
                    UpdateMainProgressBar(hDlg, 0, L"Download failed");
                    ClearActiveDownload();
                    SetDownloadUIState(hDlg, FALSE);
                    Sleep(500);
                    ShowMainProgressBar(hDlg, FALSE);
                    break;
                }
            }
            return TRUE;
        }
            
        case WM_DESTROY: {
            // Write session end marker for clean shutdown
            WriteSessionEndToLogfile(L"Clean program shutdown");
            
            // Restore original text field window procedure FIRST to avoid issues with third-party hooks
            // (This is a safety check in case WM_CLOSE wasn't called)
            WNDPROC origProc = GetOriginalTextFieldProc();
            if (origProc) {
                HWND hTextField = GetDlgItem(hDlg, IDC_TEXT_FIELD);
                if (hTextField) {
                    SetWindowLongPtr(hTextField, GWLP_WNDPROC, (LONG_PTR)origProc);
                }
                SetOriginalTextFieldProc(NULL);
            }
            
            // Clean up ListView item data
            CleanupListViewItemData(GetDlgItem(hDlg, IDC_LIST));
            
            // Clean up application state (this includes cache manager cleanup)
            ApplicationState* state = GetApplicationState();
            if (state) {
                CleanupApplicationState(state);
            }
            
            // Explicitly destroy all child windows to prevent third-party hook interference
            // This ensures all window procedures are properly unwound before final cleanup
            HWND hChild = GetWindow(hDlg, GW_CHILD);
            while (hChild) {
                HWND hNext = GetWindow(hChild, GW_HWNDNEXT);
                DestroyWindow(hChild);
                hChild = hNext;
            }
            
            // Process any remaining messages to ensure clean shutdown
            MSG msg;
            while (PeekMessageW(&msg, hDlg, 0, 0, PM_REMOVE)) {
                // Discard messages for this window
            }
            
            // Hide the window to reduce third-party hook activity
            ShowWindow(hDlg, SW_HIDE);
            
            PostQuitMessage(0);
            
            // WORKAROUND: Third-party hooks (NetSight) crash during final window destruction
            // After all cleanup is complete, exit immediately to avoid hook interference
            // This is safe because all application state has been cleaned up above
            ExitProcess(0);
            
            return TRUE;
        }
    }
    return FALSE;
}