#include "YouTubeCacher.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>

// Define _countof if not available
#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

// HiDPI helper functions
static int GetDpiForWindowSafe(HWND hwnd) {
    // Try to use GetDpiForWindow (Windows 10 1607+)
    typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
    static GetDpiForWindowFunc pGetDpiForWindow = NULL;
    static BOOL checked = FALSE;
    
    if (!checked) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            pGetDpiForWindow = (GetDpiForWindowFunc)(void*)GetProcAddress(hUser32, "GetDpiForWindow");
        }
        checked = TRUE;
    }
    
    if (pGetDpiForWindow) {
        return pGetDpiForWindow(hwnd);
    }
    
    // Fallback to system DPI
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    return dpi;
}

static int ScaleForDpi(int value, int dpi) {
    return MulDiv(value, dpi, 96); // 96 is the standard DPI
}

// Dynamic dialog layout and sizing with proper icon-text alignment
static void CalculateOptimalDialogSize(HWND hDlg, const wchar_t* message, int* width, int* height) {
    if (!message || !width || !height) return;
    
    // Get DPI for proper scaling
    int dpi = GetDpiForWindowSafe(hDlg);
    
    // Base measurements at 96 DPI
    const int BASE_ICON_SIZE = 32;           // Standard system icon size
    const int BASE_ICON_MARGIN = 10;        // Space around icon
    const int BASE_TEXT_MARGIN = 10;        // Space around text
    const int BASE_BUTTON_HEIGHT = 23;      // Standard button height
    const int BASE_BUTTON_MARGIN = 7;       // Space around buttons
    const int BASE_DETAILS_BUTTON_WIDTH = 60; // "Details >>" button width
    const int BASE_COPY_BUTTON_WIDTH = 35;  // "Copy" button width
    const int BASE_OK_BUTTON_WIDTH = 35;    // "OK" button width
    const int BASE_MIN_WIDTH = 280;         // Minimum dialog width
    const int BASE_MAX_WIDTH = 500;         // Maximum dialog width
    
    // Scale measurements to current DPI
    int iconSize = ScaleForDpi(BASE_ICON_SIZE, dpi);
    int iconMargin = ScaleForDpi(BASE_ICON_MARGIN, dpi);
    int textMargin = ScaleForDpi(BASE_TEXT_MARGIN, dpi);
    int buttonHeight = ScaleForDpi(BASE_BUTTON_HEIGHT, dpi);
    int buttonMargin = ScaleForDpi(BASE_BUTTON_MARGIN, dpi);
    int detailsButtonWidth = ScaleForDpi(BASE_DETAILS_BUTTON_WIDTH, dpi);
    int copyButtonWidth = ScaleForDpi(BASE_COPY_BUTTON_WIDTH, dpi);
    int okButtonWidth = ScaleForDpi(BASE_OK_BUTTON_WIDTH, dpi);
    int minWidth = ScaleForDpi(BASE_MIN_WIDTH, dpi);
    int maxWidth = ScaleForDpi(BASE_MAX_WIDTH, dpi);
    
    // Get device context for text measurement
    HDC hdc = GetDC(hDlg);
    if (!hdc) {
        *width = minWidth;
        *height = ScaleForDpi(120, dpi);
        return;
    }
    
    // Select dialog font for accurate measurement
    HFONT hFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
    HFONT hOldFont = NULL;
    if (hFont) {
        hOldFont = (HFONT)SelectObject(hdc, hFont);
    }
    
    // STEP 1: Create dummy single-line label to measure baseline text metrics
    RECT dummyRect = {0, 0, 1000, 0};
    int singleLineHeight = DrawTextW(hdc, L"Dummy", -1, &dummyRect, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    
    // STEP 2: Calculate available text area width
    // Layout: [margin][icon][margin][text area][margin]
    int availableTextWidth = maxWidth - (iconMargin + iconSize + iconMargin + textMargin + textMargin);
    
    // STEP 3: Measure actual message text with word wrapping
    RECT textRect = {0, 0, availableTextWidth, 0};
    int actualTextHeight = DrawTextW(hdc, message, -1, &textRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    int actualTextWidth = textRect.right;
    
    // STEP 4: Calculate icon-text alignment
    // The first line of text should be vertically centered with the icon
    int iconCenterY = iconSize / 2;
    int textFirstLineCenterY = singleLineHeight / 2;
    int textOffsetY = iconCenterY - textFirstLineCenterY;
    
    // STEP 5: Calculate total content area dimensions
    int contentWidth = iconMargin + iconSize + iconMargin + actualTextWidth + textMargin;
    int contentHeight = max(iconSize, actualTextHeight + max(0, textOffsetY));
    
    // STEP 6: Calculate button area requirements
    // Layout: [Details >>] [space] [Copy] [OK]
    int buttonAreaWidth = detailsButtonWidth + buttonMargin + copyButtonWidth + buttonMargin + okButtonWidth;
    int buttonAreaHeight = buttonHeight + (2 * buttonMargin);
    
    // STEP 7: Calculate final dialog dimensions
    int requiredWidth = max(contentWidth + (2 * iconMargin), buttonAreaWidth + (2 * iconMargin));
    int requiredHeight = iconMargin + contentHeight + buttonMargin + buttonAreaHeight + iconMargin;
    
    // STEP 8: Apply constraints and set results
    *width = max(minWidth, min(maxWidth, requiredWidth));
    *height = max(ScaleForDpi(100, dpi), requiredHeight);
    
    // Cleanup
    if (hOldFont) {
        SelectObject(hdc, hOldFont);
    }
    ReleaseDC(hDlg, hdc);
}

// Dynamically position dialog controls with proper icon-text alignment
static void PositionDialogControls(HWND hDlg, EnhancedErrorDialog* errorDialog) {
    if (!hDlg || !errorDialog || !errorDialog->message) return;
    
    // Get DPI for proper scaling
    int dpi = GetDpiForWindowSafe(hDlg);
    
    // Base measurements at 96 DPI
    const int BASE_ICON_SIZE = 32;
    const int BASE_ICON_MARGIN = 10;
    const int BASE_TEXT_MARGIN = 10;
    const int BASE_BUTTON_HEIGHT = 23;
    const int BASE_BUTTON_MARGIN = 7;
    const int BASE_DETAILS_BUTTON_WIDTH = 60;
    const int BASE_COPY_BUTTON_WIDTH = 35;
    const int BASE_OK_BUTTON_WIDTH = 35;
    
    // Scale measurements to current DPI
    int iconSize = ScaleForDpi(BASE_ICON_SIZE, dpi);
    int iconMargin = ScaleForDpi(BASE_ICON_MARGIN, dpi);
    int textMargin = ScaleForDpi(BASE_TEXT_MARGIN, dpi);
    int buttonHeight = ScaleForDpi(BASE_BUTTON_HEIGHT, dpi);
    int buttonMargin = ScaleForDpi(BASE_BUTTON_MARGIN, dpi);
    int detailsButtonWidth = ScaleForDpi(BASE_DETAILS_BUTTON_WIDTH, dpi);
    int copyButtonWidth = ScaleForDpi(BASE_COPY_BUTTON_WIDTH, dpi);
    int okButtonWidth = ScaleForDpi(BASE_OK_BUTTON_WIDTH, dpi);
    
    // Get dialog client area
    RECT dialogRect;
    GetClientRect(hDlg, &dialogRect);
    int dialogWidth = dialogRect.right - dialogRect.left;
    int dialogHeight = dialogRect.bottom - dialogRect.top;
    
    // Determine control IDs based on dialog type
    int messageId, iconId, detailsButtonId, copyButtonId, okButtonId;
    if (errorDialog->dialogType == DIALOG_TYPE_SUCCESS) {
        messageId = IDC_SUCCESS_MESSAGE;
        iconId = IDC_SUCCESS_ICON;
        detailsButtonId = IDC_SUCCESS_DETAILS_BTN;
        copyButtonId = IDC_SUCCESS_COPY_BTN;
        okButtonId = IDC_SUCCESS_OK_BTN;
    } else {
        messageId = IDC_ERROR_MESSAGE;
        iconId = IDC_ERROR_ICON;
        detailsButtonId = IDC_ERROR_DETAILS_BTN;
        copyButtonId = IDC_ERROR_COPY_BTN;
        okButtonId = IDC_ERROR_OK_BTN;
    }
    
    // Get device context for text measurement
    HDC hdc = GetDC(hDlg);
    if (!hdc) return;
    
    HFONT hFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
    HFONT hOldFont = NULL;
    if (hFont) {
        hOldFont = (HFONT)SelectObject(hdc, hFont);
    }
    
    // Measure single line height for alignment calculation
    RECT dummyRect = {0, 0, 1000, 0};
    int singleLineHeight = DrawTextW(hdc, L"Dummy", -1, &dummyRect, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    
    // Calculate text area width
    int textAreaWidth = dialogWidth - (iconMargin + iconSize + iconMargin + textMargin + textMargin);
    
    // Measure actual message text
    RECT textRect = {0, 0, textAreaWidth, 0};
    int textHeight = DrawTextW(hdc, errorDialog->message, -1, &textRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    
    // Calculate vertical alignment
    int iconCenterY = iconSize / 2;
    int textFirstLineCenterY = singleLineHeight / 2;
    int textOffsetY = max(0, iconCenterY - textFirstLineCenterY);
    
    // Position icon (left side, vertically centered in content area)
    int iconX = iconMargin;
    int iconY = iconMargin;
    SetWindowPos(GetDlgItem(hDlg, iconId), NULL, iconX, iconY, iconSize, iconSize, SWP_NOZORDER);
    
    // Position message text (aligned with icon center for first line)
    int textX = iconMargin + iconSize + iconMargin;
    int textY = iconMargin + textOffsetY;
    SetWindowPos(GetDlgItem(hDlg, messageId), NULL, textX, textY, textAreaWidth, textHeight, SWP_NOZORDER);
    
    // Calculate button positions (bottom of dialog)
    int buttonY = dialogHeight - buttonMargin - buttonHeight;
    
    // Position buttons from left to right
    int detailsButtonX = iconMargin;
    SetWindowPos(GetDlgItem(hDlg, detailsButtonId), NULL, detailsButtonX, buttonY, detailsButtonWidth, buttonHeight, SWP_NOZORDER);
    
    // Position Copy and OK buttons from right to left
    int okButtonX = dialogWidth - iconMargin - okButtonWidth;
    SetWindowPos(GetDlgItem(hDlg, okButtonId), NULL, okButtonX, buttonY, okButtonWidth, buttonHeight, SWP_NOZORDER);
    
    int copyButtonX = okButtonX - buttonMargin - copyButtonWidth;
    SetWindowPos(GetDlgItem(hDlg, copyButtonId), NULL, copyButtonX, buttonY, copyButtonWidth, buttonHeight, SWP_NOZORDER);
    
    // Cleanup
    if (hOldFont) {
        SelectObject(hdc, hOldFont);
    }
    ReleaseDC(hDlg, hdc);
}

// Tab names for the error dialog
static const wchar_t* TAB_NAMES[] = {
    L"Error Details",
    L"Diagnostics", 
    L"Solutions"
};

// Tab names for the success dialog
static const wchar_t* SUCCESS_TAB_NAMES[] = {
    L"Operation Details",
    L"Information",
    L"Summary"
};

// Enhanced error dialog creation
EnhancedErrorDialog* CreateEnhancedErrorDialog(const wchar_t* title, const wchar_t* message,
                                              const wchar_t* details, const wchar_t* diagnostics,
                                              const wchar_t* solutions, ErrorType errorType) {
    EnhancedErrorDialog* dialog = (EnhancedErrorDialog*)malloc(sizeof(EnhancedErrorDialog));
    if (!dialog) return NULL;

    // Initialize structure
    memset(dialog, 0, sizeof(EnhancedErrorDialog));
    dialog->errorType = errorType;
    dialog->dialogType = DIALOG_TYPE_ERROR; // Default to error type
    dialog->isExpanded = FALSE;

    // Allocate and copy strings
    if (title) {
        size_t len = wcslen(title) + 1;
        dialog->title = (wchar_t*)malloc(len * sizeof(wchar_t));
        if (dialog->title) wcscpy(dialog->title, title);
    }

    if (message) {
        size_t len = wcslen(message) + 1;
        dialog->message = (wchar_t*)malloc(len * sizeof(wchar_t));
        if (dialog->message) wcscpy(dialog->message, message);
    }

    if (details) {
        size_t len = wcslen(details) + 1;
        dialog->details = (wchar_t*)malloc(len * sizeof(wchar_t));
        if (dialog->details) wcscpy(dialog->details, details);
    }

    if (diagnostics) {
        size_t len = wcslen(diagnostics) + 1;
        dialog->diagnostics = (wchar_t*)malloc(len * sizeof(wchar_t));
        if (dialog->diagnostics) wcscpy(dialog->diagnostics, diagnostics);
    }

    if (solutions) {
        size_t len = wcslen(solutions) + 1;
        dialog->solutions = (wchar_t*)malloc(len * sizeof(wchar_t));
        if (dialog->solutions) wcscpy(dialog->solutions, solutions);
    }

    return dialog;
}

// Free enhanced error dialog
void FreeEnhancedErrorDialog(EnhancedErrorDialog* errorDialog) {
    if (!errorDialog) return;

    free(errorDialog->title);
    free(errorDialog->message);
    free(errorDialog->details);
    free(errorDialog->diagnostics);
    free(errorDialog->solutions);
    free(errorDialog);
}

// Resize error dialog for expanded/collapsed state
void ResizeErrorDialog(HWND hDlg, BOOL expanded) {
    // Get DPI for this window
    int dpi = GetDpiForWindowSafe(hDlg);
    
    // Get all control handles at the start
    HWND hIcon = GetDlgItem(hDlg, IDC_ERROR_ICON);
    HWND hMessage = GetDlgItem(hDlg, IDC_ERROR_MESSAGE);
    HWND hDetailsBtn = GetDlgItem(hDlg, IDC_ERROR_DETAILS_BTN);
    HWND hCopyBtn = GetDlgItem(hDlg, IDC_ERROR_COPY_BTN);
    HWND hOkBtn = GetDlgItem(hDlg, IDC_ERROR_OK_BTN);
    HWND hTabControl = GetDlgItem(hDlg, IDC_ERROR_TAB_CONTROL);
    HWND hDetailsText = GetDlgItem(hDlg, IDC_ERROR_DETAILS_TEXT);
    HWND hDiagText = GetDlgItem(hDlg, IDC_ERROR_DIAG_TEXT);
    HWND hSolutionText = GetDlgItem(hDlg, IDC_ERROR_SOLUTION_TEXT);
    
    // Get the current message text for dynamic sizing
    wchar_t messageText[1024];
    GetDlgItemTextW(hDlg, IDC_ERROR_MESSAGE, messageText, 1024);
    
    // === STEP 1: Calculate base metrics per Microsoft Win32 UI standards ===
    int margin = ScaleForDpi(11, dpi);           // 7 DLU = ~11px dialog margin standard
    int iconSize = ScaleForDpi(32, dpi);         // Standard system icon size
    int buttonWidth = ScaleForDpi(75, dpi);      // Min 50 DLU = ~75px for "Details >>" button
    int buttonHeight = ScaleForDpi(23, dpi);     // Standard 14 DLU = ~23px button height
    int smallButtonWidth = ScaleForDpi(75, dpi); // Microsoft standard: all buttons minimum 75px wide
    int buttonGap = ScaleForDpi(6, dpi);         // 4 DLU = ~6px spacing between buttons
    int controlSpacing = ScaleForDpi(6, dpi);    // 4 DLU = ~6px between related controls
    int groupSpacing = ScaleForDpi(10, dpi);     // 7 DLU = ~10px between control groups
    
    // === STEP 2: Calculate text metrics ===
    HDC hdc = GetDC(hDlg);
    HFONT hFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
    HFONT hOldFont = NULL;
    if (hFont) hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    // Get single line height for vertical centering calculations
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight;
    
    // === STEP 3: Calculate dialog width with proper margins ===
    // Get dialog type to determine appropriate dimensions
    EnhancedErrorDialog* errorDialog = (EnhancedErrorDialog*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    BOOL isSuccessDialog = (errorDialog && errorDialog->dialogType == DIALOG_TYPE_SUCCESS);
    
    int minWidth = ScaleForDpi(isSuccessDialog ? 520 : 320, dpi);
    int maxWidth = ScaleForDpi(isSuccessDialog ? 680 : 480, dpi);
    
    // Width calculation: left margin + icon + control spacing + text area + right margin
    int iconGap = controlSpacing; // 4 DLU = ~6px gap between icon and text per Win32 standards
    int availableTextWidth = maxWidth - margin - iconSize - iconGap - margin;
    
    // Measure text with word wrapping to determine required height
    RECT textRect = {0, 0, availableTextWidth, 0};
    int textHeight = DrawTextW(hdc, messageText, -1, &textRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    
    // Calculate required dialog width: margins + icon + gap + actual text width
    int requiredWidth = margin + iconSize + iconGap + textRect.right + margin;
    int dialogWidth = max(minWidth, min(maxWidth, requiredWidth));
    
    // Recalculate final text area width based on actual dialog width
    int finalTextWidth = dialogWidth - margin - iconSize - iconGap - margin;
    
    // Remeasure text with final width to get accurate height
    RECT finalTextRect = {0, 0, finalTextWidth, 0};
    textHeight = DrawTextW(hdc, messageText, -1, &finalTextRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    
    // === STEP 4: Position icon with left margin ===
    int iconX = margin;
    int iconY = margin;
    
    // === STEP 5: Position message label with proper spacing ===
    // First line of text should be vertically centered with icon center
    int iconCenterY = iconY + iconSize / 2;
    int textStartY = iconCenterY - lineHeight / 2;
    
    int messageX = iconX + iconSize + iconGap;
    int messageY = textStartY;
    int messageWidth = finalTextWidth;
    int messageHeight = textHeight;
    
    // === STEP 6: Position buttons per Win32 standards ===
    // Buttons go below content with proper group spacing (7 DLU = ~10px)
    int contentBottom = max(iconY + iconSize, messageY + messageHeight);
    int buttonY = contentBottom + groupSpacing;
    
    // For success dialogs, Details button is hidden, so position Copy and OK buttons differently
    int detailsX = margin;
    int okX, copyX;
    
    if (isSuccessDialog) {
        // Success dialog: only Copy and OK buttons visible, positioned on the right
        okX = dialogWidth - margin - smallButtonWidth;
        copyX = okX - buttonGap - smallButtonWidth;
    } else {
        // Error dialog: Details on left, Copy and OK on right
        okX = dialogWidth - margin - smallButtonWidth;
        copyX = okX - buttonGap - smallButtonWidth;
    }
    
    // === STEP 7: Calculate collapsed dialog height with Win32 standards ===
    // Height = content + group spacing + button height + dialog margin
    int collapsedHeight = buttonY + buttonHeight + margin;
    
    // Win32 minimum dialog height should accommodate all elements
    int minCollapsedHeight = ScaleForDpi(isSuccessDialog ? 150 : 120, dpi); // Larger minimum for success dialogs
    collapsedHeight = max(collapsedHeight, minCollapsedHeight);
    
    // === STEP 8: Calculate expanded height with proper margins ===
    int tabHeight = ScaleForDpi(isSuccessDialog ? 290 : 140, dpi);
    // Expanded = collapsed + margin between buttons and tabs + tab height + bottom margin
    int expandedHeight = collapsedHeight + groupSpacing + tabHeight + margin;
    
    int finalHeight = expanded ? expandedHeight : collapsedHeight;
    
    // === STEP 9: Position dialog on screen ===
    RECT rect;
    GetWindowRect(hDlg, &rect);
    int currentX = rect.left;
    int currentY = rect.top;
    
    HMONITOR hMonitor = MonitorFromWindow(hDlg, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hMonitor, &mi);
    RECT screenRect = mi.rcWork;
    
    // Adjust position if dialog would go off screen
    if (currentX < screenRect.left) currentX = screenRect.left;
    if (currentY < screenRect.top) currentY = screenRect.top;
    if (currentX + dialogWidth > screenRect.right) currentX = screenRect.right - dialogWidth;
    if (currentY + finalHeight > screenRect.bottom) currentY = screenRect.bottom - finalHeight;
    
    // === STEP 10: Apply all positioning ===
    SetWindowPos(hDlg, NULL, currentX, currentY, dialogWidth, finalHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    
    // Position icon
    if (hIcon) {
        SetWindowPos(hIcon, NULL, iconX, iconY, iconSize, iconSize, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    // Position message
    if (hMessage) {
        SetWindowPos(hMessage, NULL, messageX, messageY, messageWidth, messageHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    // Position buttons
    if (hDetailsBtn) {
        SetWindowPos(hDetailsBtn, NULL, detailsX, buttonY, buttonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    if (hCopyBtn) {
        SetWindowPos(hCopyBtn, NULL, copyX, buttonY, smallButtonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    if (hOkBtn) {
        SetWindowPos(hOkBtn, NULL, okX, buttonY, smallButtonWidth, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    // Position expanded controls if needed
    if (expanded) {
        // Tab control positioned with group spacing below buttons per Win32 standards
        int tabY = buttonY + buttonHeight + groupSpacing;
        int tabWidth = dialogWidth - 2 * margin;
        
        if (hTabControl) {
            SetWindowPos(hTabControl, NULL, margin, tabY, tabWidth, tabHeight, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        
        // Position text areas inside tab control
        int textX = margin + ScaleForDpi(5, dpi);
        int textY = tabY + ScaleForDpi(20, dpi); // Account for tab header
        int textW = tabWidth - ScaleForDpi(10, dpi);
        int textH = tabHeight - ScaleForDpi(25, dpi);
        
        if (hDetailsText) {
            SetWindowPos(hDetailsText, NULL, textX, textY, textW, textH, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        
        if (hDiagText) {
            SetWindowPos(hDiagText, NULL, textX, textY, textW, textH, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        
        if (hSolutionText) {
            SetWindowPos(hSolutionText, NULL, textX, textY, textW, textH, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    
    // Show/hide expanded controls
    int showState = expanded ? SW_SHOW : SW_HIDE;
    if (hTabControl) ShowWindow(hTabControl, showState);
    if (hDetailsText) ShowWindow(hDetailsText, showState);
    if (hDiagText) ShowWindow(hDiagText, showState);
    if (hSolutionText) ShowWindow(hSolutionText, showState);
    
    // Update details button text
    if (hDetailsBtn) {
        SetWindowTextW(hDetailsBtn, expanded ? L"<< Details" : L"Details >>");
    }
    
    // Cleanup
    if (hOldFont) SelectObject(hdc, hOldFont);
    ReleaseDC(hDlg, hdc);
}

// Initialize error dialog tabs
void InitializeErrorDialogTabs(HWND hTabControl) {
    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    
    for (int i = 0; i < 3; i++) {
        tie.pszText = (wchar_t*)TAB_NAMES[i];
        TabCtrl_InsertItem(hTabControl, i, &tie);
    }
    
    TabCtrl_SetCurSel(hTabControl, 0);
}

// Initialize success dialog tabs
void InitializeSuccessDialogTabs(HWND hTabControl) {
    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    
    for (int i = 0; i < 3; i++) {
        tie.pszText = (wchar_t*)SUCCESS_TAB_NAMES[i];
        TabCtrl_InsertItem(hTabControl, i, &tie);
    }
    
    TabCtrl_SetCurSel(hTabControl, 0);
}

// Show specific error dialog tab
void ShowErrorDialogTab(HWND hDlg, int tabIndex) {
    HWND hDetailsText = GetDlgItem(hDlg, IDC_ERROR_DETAILS_TEXT);
    HWND hDiagText = GetDlgItem(hDlg, IDC_ERROR_DIAG_TEXT);
    HWND hSolutionText = GetDlgItem(hDlg, IDC_ERROR_SOLUTION_TEXT);
    
    // Hide all text controls first
    ShowWindow(hDetailsText, SW_HIDE);
    ShowWindow(hDiagText, SW_HIDE);
    ShowWindow(hSolutionText, SW_HIDE);
    
    // Show the selected tab's text control
    switch (tabIndex) {
        case TAB_ERROR_DETAILS:
            ShowWindow(hDetailsText, SW_SHOW);
            break;
        case TAB_ERROR_DIAGNOSTICS:
            ShowWindow(hDiagText, SW_SHOW);
            break;
        case TAB_ERROR_SOLUTIONS:
            ShowWindow(hSolutionText, SW_SHOW);
            break;
    }
}

// Copy error information to clipboard
BOOL CopyErrorInfoToClipboard(const EnhancedErrorDialog* errorDialog) {
    if (!errorDialog) return FALSE;
    
    // Calculate total size needed
    size_t totalSize = 1024; // Base size for formatting
    if (errorDialog->title) totalSize += wcslen(errorDialog->title);
    if (errorDialog->message) totalSize += wcslen(errorDialog->message);
    if (errorDialog->details) totalSize += wcslen(errorDialog->details);
    if (errorDialog->diagnostics) totalSize += wcslen(errorDialog->diagnostics);
    if (errorDialog->solutions) totalSize += wcslen(errorDialog->solutions);
    
    wchar_t* clipboardText = (wchar_t*)malloc(totalSize * sizeof(wchar_t));
    if (!clipboardText) return FALSE;
    
    // Format the information based on dialog type
    if (errorDialog->dialogType == DIALOG_TYPE_SUCCESS) {
        swprintf(clipboardText, totalSize,
            L"=== SUCCESS REPORT ===\r\n"
            L"Title: %ls\r\n"
            L"Message: %ls\r\n\r\n"
            L"=== DETAILS ===\r\n%ls\r\n\r\n"
            L"=== INFORMATION ===\r\n%ls\r\n\r\n"
            L"=== SUMMARY ===\r\n%ls\r\n",
            errorDialog->title ? errorDialog->title : L"Success",
            errorDialog->message ? errorDialog->message : L"Operation completed successfully",
            errorDialog->details ? errorDialog->details : L"No additional details available",
            errorDialog->diagnostics ? errorDialog->diagnostics : L"No additional information available",
            errorDialog->solutions ? errorDialog->solutions : L"No additional summary available");
    } else {
        swprintf(clipboardText, totalSize,
            L"=== ERROR REPORT ===\r\n"
            L"Title: %ls\r\n"
            L"Message: %ls\r\n\r\n"
            L"=== ERROR DETAILS ===\r\n%ls\r\n\r\n"
            L"=== DIAGNOSTICS ===\r\n%ls\r\n\r\n"
            L"=== SOLUTIONS ===\r\n%ls\r\n",
            errorDialog->title ? errorDialog->title : L"Unknown Error",
            errorDialog->message ? errorDialog->message : L"No message available",
            errorDialog->details ? errorDialog->details : L"No details available",
            errorDialog->diagnostics ? errorDialog->diagnostics : L"No diagnostics available",
            errorDialog->solutions ? errorDialog->solutions : L"No solutions available");
    }
    
    BOOL success = FALSE;
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        
        size_t textLen = wcslen(clipboardText);
        HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, (textLen + 1) * sizeof(wchar_t));
        
        if (hClipboardData) {
            wchar_t* pchData = (wchar_t*)GlobalLock(hClipboardData);
            if (pchData) {
                wcscpy(pchData, clipboardText);
                GlobalUnlock(hClipboardData);
                
                if (SetClipboardData(CF_UNICODETEXT, hClipboardData)) {
                    success = TRUE;
                }
            }
        }
        
        CloseClipboard();
    }
    
    free(clipboardText);
    return success;
}

// Enhanced error dialog procedure
INT_PTR CALLBACK EnhancedErrorDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static EnhancedErrorDialog* errorDialog = NULL;
    
    switch (message) {
        case WM_INITDIALOG: {
            errorDialog = (EnhancedErrorDialog*)lParam;
            if (!errorDialog) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            
            // Store dialog data for access by other functions
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)errorDialog);
            
            // Apply modern Windows theming to dialog and controls
            ApplyModernThemeToDialog(hDlg);
            
            errorDialog->hDialog = hDlg;
            
            // Determine control IDs based on dialog type
            int tabControlId, messageId, iconId, detailsTextId, diagTextId, solutionTextId;
            LPCWSTR iconResource;
            
            if (errorDialog->dialogType == DIALOG_TYPE_SUCCESS) {
                tabControlId = IDC_SUCCESS_TAB_CONTROL;
                messageId = IDC_SUCCESS_MESSAGE;
                iconId = IDC_SUCCESS_ICON;
                detailsTextId = IDC_SUCCESS_DETAILS_TEXT;
                diagTextId = IDC_SUCCESS_INFO_TEXT;
                solutionTextId = IDC_SUCCESS_SUMMARY_TEXT;
                iconResource = IDI_INFORMATION; // Use information icon for success
            } else {
                tabControlId = IDC_ERROR_TAB_CONTROL;
                messageId = IDC_ERROR_MESSAGE;
                iconId = IDC_ERROR_ICON;
                detailsTextId = IDC_ERROR_DETAILS_TEXT;
                diagTextId = IDC_ERROR_DIAG_TEXT;
                solutionTextId = IDC_ERROR_SOLUTION_TEXT;
                iconResource = IDI_ERROR;
            }
            
            errorDialog->hTabControl = GetDlgItem(hDlg, tabControlId);
            
            // Set dialog title
            if (errorDialog->title) {
                SetWindowTextW(hDlg, errorDialog->title);
            }
            
            // Set message
            if (errorDialog->message) {
                SetDlgItemTextW(hDlg, messageId, errorDialog->message);
            }
            
            // Set appropriate icon
            HWND hIcon = GetDlgItem(hDlg, iconId);
            if (hIcon) {
                HICON hDialogIcon = LoadIconW(NULL, iconResource);
                SendMessageW(hIcon, STM_SETICON, (WPARAM)hDialogIcon, 0);
            }
            
            // Initialize tabs with appropriate labels
            if (errorDialog->dialogType == DIALOG_TYPE_SUCCESS) {
                InitializeSuccessDialogTabs(errorDialog->hTabControl);
            } else {
                InitializeErrorDialogTabs(errorDialog->hTabControl);
            }
            
            // Set text content for each tab
            if (errorDialog->details) {
                SetDlgItemTextW(hDlg, detailsTextId, errorDialog->details);
            }
            if (errorDialog->diagnostics) {
                SetDlgItemTextW(hDlg, diagTextId, errorDialog->diagnostics);
            }
            if (errorDialog->solutions) {
                SetDlgItemTextW(hDlg, solutionTextId, errorDialog->solutions);
            }
            
            // Calculate optimal dialog size based on message content
            int optimalWidth = 400, optimalHeight = 300; // Default values
            CalculateOptimalDialogSize(hDlg, errorDialog->message, &optimalWidth, &optimalHeight);
            
            // Resize dialog to optimal dimensions first
            SetWindowPos(hDlg, NULL, 0, 0, optimalWidth, optimalHeight, SWP_NOMOVE | SWP_NOZORDER);
            
            // Now position all controls dynamically with proper alignment
            PositionDialogControls(hDlg, errorDialog);
            
            // Center dialog on parent window with HiDPI-aware screen bounds checking
            HWND hParent = GetParent(hDlg);
            RECT parentRect;
            
            // Get screen work area for the appropriate monitor
            HMONITOR hMonitor;
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            
            if (hParent && GetWindowRect(hParent, &parentRect)) {
                // Use monitor containing parent window
                hMonitor = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
            } else {
                // Use monitor containing dialog
                hMonitor = MonitorFromWindow(hDlg, MONITOR_DEFAULTTONEAREST);
            }
            
            GetMonitorInfoW(hMonitor, &mi);
            RECT screenRect = mi.rcWork;
            
            int x, y;
            
            if (hParent && GetWindowRect(hParent, &parentRect)) {
                // Center on parent window
                x = parentRect.left + (parentRect.right - parentRect.left - optimalWidth) / 2;
                y = parentRect.top + (parentRect.bottom - parentRect.top - optimalHeight) / 2;
            } else {
                // Center on screen if no parent
                x = screenRect.left + (screenRect.right - screenRect.left - optimalWidth) / 2;
                y = screenRect.top + (screenRect.bottom - screenRect.top - optimalHeight) / 2;
            }
            
            // Adjust if any edge would be off screen
            if (x < screenRect.left) x = screenRect.left;
            if (y < screenRect.top) y = screenRect.top;
            if (x + optimalWidth > screenRect.right) x = screenRect.right - optimalWidth;
            if (y + optimalHeight > screenRect.bottom) y = screenRect.bottom - optimalHeight;
            
            // Set initial size and position
            SetWindowPos(hDlg, NULL, x, y, optimalWidth, optimalHeight, SWP_NOZORDER);
            
            // Update message control for word wrapping
            HWND hMessage = GetDlgItem(hDlg, IDC_ERROR_MESSAGE);
            if (hMessage) {
                int dpi = GetDpiForWindowSafe(hDlg);
                int iconSpace = ScaleForDpi(50, dpi);
                int margin = ScaleForDpi(10, dpi);
                int messageWidth = optimalWidth - iconSpace - margin;
                int messageHeight = optimalHeight - ScaleForDpi(60, dpi);
                
                SetWindowPos(hMessage, NULL, iconSpace, margin, messageWidth, messageHeight, 
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            
            // Hide Details button and related controls for success dialogs
            if (errorDialog->dialogType == DIALOG_TYPE_SUCCESS) {
                ShowWindow(GetDlgItem(hDlg, IDC_SUCCESS_DETAILS_BTN), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_SUCCESS_TAB_CONTROL), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_SUCCESS_DETAILS_TEXT), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_SUCCESS_INFO_TEXT), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_SUCCESS_SUMMARY_TEXT), SW_HIDE);
            }
            
            // Start in collapsed state (this will trigger ResizeErrorDialog)
            ResizeErrorDialog(hDlg, FALSE);
            
            return TRUE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_ERROR_DETAILS_BTN:
                    if (errorDialog) {
                        errorDialog->isExpanded = !errorDialog->isExpanded;
                        ResizeErrorDialog(hDlg, errorDialog->isExpanded);
                        if (errorDialog->isExpanded) {
                            ShowErrorDialogTab(hDlg, TabCtrl_GetCurSel(errorDialog->hTabControl));
                        }
                    }
                    return TRUE;
                    
                case IDC_ERROR_COPY_BTN:
                case IDC_SUCCESS_COPY_BTN:
                    if (errorDialog) {
                        if (CopyErrorInfoToClipboard(errorDialog)) {
                            MessageBoxW(hDlg, L"Information copied to clipboard.", 
                                       L"Information", MB_OK | MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hDlg, L"Failed to copy information to clipboard.", 
                                       L"Error", MB_OK | MB_ICONERROR);
                        }
                    }
                    return TRUE;
                    
                case IDC_SUCCESS_DETAILS_BTN:
                    if (errorDialog) {
                        errorDialog->isExpanded = !errorDialog->isExpanded;
                        ResizeErrorDialog(hDlg, errorDialog->isExpanded);
                        if (errorDialog->isExpanded) {
                            ShowErrorDialogTab(hDlg, TabCtrl_GetCurSel(errorDialog->hTabControl));
                        }
                    }
                    return TRUE;
                    
                case IDC_ERROR_OK_BTN:
                case IDC_SUCCESS_OK_BTN:
                case IDOK:
                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            break;
            
        case WM_NOTIFY: {
            NMHDR* pnmh = (NMHDR*)lParam;
            if (pnmh->idFrom == IDC_ERROR_TAB_CONTROL && pnmh->code == TCN_SELCHANGE) {
                int selectedTab = TabCtrl_GetCurSel(errorDialog->hTabControl);
                ShowErrorDialogTab(hDlg, selectedTab);
                return TRUE;
            }
            break;
        }
        
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }
    
    return FALSE;
}

// Show enhanced error dialog
INT_PTR ShowEnhancedErrorDialog(HWND parent, EnhancedErrorDialog* errorDialog) {
    if (!errorDialog) return IDCANCEL;
    
    // Choose the appropriate dialog resource based on dialog type
    int dialogResource = (errorDialog->dialogType == DIALOG_TYPE_SUCCESS) ? 
                        IDD_SUCCESS_DIALOG : IDD_ERROR_DIALOG;
    
    return DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(dialogResource),
                          parent, EnhancedErrorDialogProc, (LPARAM)errorDialog);
}

// Convenience function for yt-dlp errors
INT_PTR ShowYtDlpError(HWND parent, const YtDlpResult* result, const YtDlpRequest* request) {
    if (!result) return IDCANCEL;
    
    // Suppress unused parameter warning
    (void)request;
    
    wchar_t title[256];
    swprintf(title, _countof(title), L"yt-dlp Error (Exit Code: %lu)", result->exitCode);
    
    wchar_t message[512];
    if (result->errorMessage && wcslen(result->errorMessage) > 0) {
        swprintf(message, _countof(message), L"yt-dlp operation failed: %ls", result->errorMessage);
    } else {
        wcscpy(message, L"yt-dlp operation failed with an unknown error.");
    }
    
    // Analyze the error to determine type and generate solutions
    ErrorAnalysis* analysis = AnalyzeYtDlpError(result);
    
    wchar_t solutions[1024] = L"General troubleshooting steps:\r\n"
                              L"1. Check your internet connection\r\n"
                              L"2. Verify the URL is correct and accessible\r\n"
                              L"3. Try updating yt-dlp to the latest version\r\n"
                              L"4. Check available disk space";
    
    if (analysis && analysis->solution) {
        swprintf(solutions, _countof(solutions), L"%ls", analysis->solution);
    }
    
    // Log the error
    LogError(L"YtDlp", message, result->output ? result->output : L"No output available");
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title,
        message,
        result->output ? result->output : L"No detailed output available",
        result->diagnostics ? result->diagnostics : L"No diagnostic information available",
        solutions,
        analysis ? analysis->type : ERROR_TYPE_UNKNOWN
    );
    
    INT_PTR result_code = IDCANCEL;
    if (dialog) {
        result_code = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    if (analysis) {
        FreeErrorAnalysis(analysis);
    }
    
    return result_code;
}

// Convenience function for validation errors
INT_PTR ShowValidationError(HWND parent, const ValidationInfo* validationInfo) {
    if (!validationInfo) return IDCANCEL;
    
    wchar_t title[256];
    wcscpy(title, L"yt-dlp Validation Error");
    
    wchar_t message[512];
    switch (validationInfo->result) {
        case VALIDATION_NOT_FOUND:
            wcscpy(message, L"yt-dlp executable not found at the specified path.");
            break;
        case VALIDATION_NOT_EXECUTABLE:
            wcscpy(message, L"The specified file is not a valid executable.");
            break;
        case VALIDATION_MISSING_DEPENDENCIES:
            wcscpy(message, L"yt-dlp is missing required dependencies (Python runtime).");
            break;
        case VALIDATION_VERSION_INCOMPATIBLE:
            wcscpy(message, L"The yt-dlp version is incompatible with this application.");
            break;
        case VALIDATION_PERMISSION_DENIED:
            wcscpy(message, L"Permission denied when trying to access yt-dlp executable.");
            break;
        default:
            wcscpy(message, L"yt-dlp validation failed for an unknown reason.");
            break;
    }
    
    wchar_t solutions[1024];
    if (validationInfo->suggestions) {
        wcscpy(solutions, validationInfo->suggestions);
    } else {
        wcscpy(solutions, 
                 L"1. Download yt-dlp from https://github.com/yt-dlp/yt-dlp\r\n"
                 L"2. Ensure Python is installed and accessible\r\n"
                 L"3. Check file permissions and antivirus settings\r\n"
                 L"4. Update the path in Settings if yt-dlp was moved");
    }
    
    // Log the error
    LogError(L"Validation", message, validationInfo->errorDetails ? validationInfo->errorDetails : L"No details available");
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title,
        message,
        validationInfo->errorDetails ? validationInfo->errorDetails : L"No detailed error information available",
        L"Validation performed comprehensive checks on the yt-dlp executable and its dependencies.",
        solutions,
        ERROR_TYPE_DEPENDENCIES
    );
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

// Convenience function for process errors
INT_PTR ShowProcessError(HWND parent, DWORD errorCode, const wchar_t* operation) {
    wchar_t title[256];
    wcscpy(title, L"Process Error");
    
    wchar_t message[512];
    swprintf(message, _countof(message), L"Failed to %ls (Error Code: %lu)", 
              operation ? operation : L"execute operation", errorCode);
    
    wchar_t details[1024];
    LPWSTR errorText = NULL;
    if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL, errorCode, 0, (LPWSTR)&errorText, 0, NULL)) {
        swprintf(details, _countof(details), L"Windows Error: %ls", errorText);
        LocalFree(errorText);
    } else {
        swprintf(details, _countof(details), L"Windows Error Code: %lu", errorCode);
    }
    
    wchar_t solutions[1024];
    wcscpy(solutions,
             L"1. Check if the executable path is correct\r\n"
             L"2. Verify you have permission to run the program\r\n"
             L"3. Ensure the executable is not blocked by antivirus\r\n"
             L"4. Try running the application as administrator");
    
    // Log the error
    LogError(L"Process", message, details);
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title,
        message,
        details,
        L"Process creation or execution failed at the Windows API level.",
        solutions,
        ERROR_TYPE_PERMISSIONS
    );
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

// Convenience function for temporary directory errors
INT_PTR ShowTempDirError(HWND parent, const wchar_t* tempDir, DWORD errorCode) {
    wchar_t title[256];
    wcscpy(title, L"Temporary Directory Error");
    
    wchar_t message[512];
    swprintf(message, _countof(message), L"Failed to create or access temporary directory: %ls", 
              tempDir ? tempDir : L"Unknown path");
    
    wchar_t details[1024];
    LPWSTR errorText = NULL;
    if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL, errorCode, 0, (LPWSTR)&errorText, 0, NULL)) {
        swprintf(details, _countof(details), 
                   L"Path: %ls\r\nWindows Error: %ls", 
                   tempDir ? tempDir : L"Unknown", errorText);
        LocalFree(errorText);
    } else {
        swprintf(details, _countof(details), 
                   L"Path: %ls\r\nError Code: %lu", 
                   tempDir ? tempDir : L"Unknown", errorCode);
    }
    
    wchar_t solutions[1024];
    wcscpy(solutions,
             L"1. Check available disk space on the target drive\r\n"
             L"2. Verify write permissions to the directory\r\n"
             L"3. Try using a different temporary directory\r\n"
             L"4. Clear existing temporary files\r\n"
             L"5. Check if the path length exceeds Windows limits");
    
    // Log the error
    LogError(L"TempDir", message, details);
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title,
        message,
        details,
        L"Temporary directory creation failed. This may be due to permissions, disk space, or path length issues.",
        solutions,
        ERROR_TYPE_TEMP_DIR
    );
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

// Additional convenience functions for common scenarios
INT_PTR ShowMemoryError(HWND parent, const wchar_t* operation) {
    wchar_t title[256];
    wcscpy(title, L"Memory Error");
    
    wchar_t message[512];
    swprintf(message, _countof(message), L"Failed to allocate memory for %ls", 
              operation ? operation : L"operation");
    
    wchar_t details[1024];
    swprintf(details, _countof(details), 
               L"Operation: %ls\r\nError: Insufficient memory available", 
               operation ? operation : L"Unknown operation");
    
    wchar_t solutions[1024];
    wcscpy(solutions,
             L"1. Close other applications to free up memory\r\n"
             L"2. Restart the application\r\n"
             L"3. Restart your computer if the problem persists\r\n"
             L"4. Check available system memory");
    
    // Log the error
    LogError(L"Memory", message, details);
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title,
        message,
        details,
        L"Memory allocation failed. This may indicate low system memory or memory fragmentation.",
        solutions,
        ERROR_TYPE_UNKNOWN
    );
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

INT_PTR ShowConfigurationError(HWND parent, const wchar_t* details) {
    wchar_t title[256];
    wcscpy(title, L"Configuration Error");
    
    wchar_t message[512];
    wcscpy(message, L"Failed to initialize application configuration");
    
    wchar_t solutions[1024];
    wcscpy(solutions,
             L"1. Check File > Settings for correct paths\r\n"
             L"2. Verify yt-dlp is properly installed\r\n"
             L"3. Ensure all required files are accessible\r\n"
             L"4. Try resetting settings to defaults");
    
    // Log the error
    LogError(L"Configuration", message, details ? details : L"No details available");
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title,
        message,
        details ? details : L"Configuration initialization failed",
        L"Application configuration could not be loaded or initialized properly.",
        solutions,
        ERROR_TYPE_DEPENDENCIES
    );
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

INT_PTR ShowUIError(HWND parent, const wchar_t* operation) {
    wchar_t title[256];
    wcscpy(title, L"User Interface Error");
    
    wchar_t message[512];
    swprintf(message, _countof(message), L"Failed to create user interface component: %ls", 
              operation ? operation : L"unknown component");
    
    wchar_t details[1024];
    swprintf(details, _countof(details), 
               L"Component: %ls\r\nError: UI creation failed", 
               operation ? operation : L"Unknown component");
    
    wchar_t solutions[1024];
    wcscpy(solutions,
             L"1. Restart the application\r\n"
             L"2. Check system resources and close other applications\r\n"
             L"3. Verify Windows is functioning properly\r\n"
             L"4. Try running as administrator");
    
    // Log the error
    LogError(L"UI", message, details);
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title,
        message,
        details,
        L"User interface component creation failed. This may indicate system resource issues.",
        solutions,
        ERROR_TYPE_UNKNOWN
    );
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

INT_PTR ShowSuccessMessage(HWND parent, const wchar_t* title, const wchar_t* message) {
    wchar_t nextSteps[512];
    wcscpy(nextSteps, L"The operation completed successfully. You can now use the downloaded files or perform additional operations.");
    
    // Log the success
    LogInfo(L"Success", message ? message : L"Operation completed");
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title ? title : L"Success",
        message ? message : L"Operation completed successfully",
        L"The requested operation has been completed without errors.",
        L"All processes executed successfully with no issues detected.",
        nextSteps,
        ERROR_TYPE_UNKNOWN  // Will be overridden by dialogType
    );
    
    if (dialog) {
        // Set this as a success dialog
        dialog->dialogType = DIALOG_TYPE_SUCCESS;
    }
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

INT_PTR ShowWarningMessage(HWND parent, const wchar_t* title, const wchar_t* message) {
    wchar_t solutions[512];
    wcscpy(solutions, L"This is a warning message. Please review the information and take appropriate action if needed.");
    
    // Log the warning
    LogWarning(L"Warning", message ? message : L"Warning condition detected");
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title ? title : L"Warning",
        message ? message : L"A warning condition has been detected",
        L"Please review the warning information and take appropriate action.",
        L"Warning condition detected. Review and take action if necessary.",
        solutions,
        ERROR_TYPE_UNKNOWN
    );
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

INT_PTR ShowInfoMessage(HWND parent, const wchar_t* title, const wchar_t* message) {
    wchar_t solutions[512];
    wcscpy(solutions, L"This is an informational message. No action is required.");
    
    // Log the info
    LogInfo(L"Info", message ? message : L"Information message");
    
    EnhancedErrorDialog* dialog = CreateEnhancedErrorDialog(
        title ? title : L"Information",
        message ? message : L"Information",
        message ? message : L"Informational message",
        L"This is an informational message for your reference.",
        solutions,
        ERROR_TYPE_UNKNOWN
    );
    
    INT_PTR result = IDCANCEL;
    if (dialog) {
        result = ShowEnhancedErrorDialog(parent, dialog);
        FreeEnhancedErrorDialog(dialog);
    }
    
    return result;
}

// Error logging implementation
static HANDLE hLogFile = INVALID_HANDLE_VALUE;
static BOOL loggingInitialized = FALSE;

BOOL InitializeErrorLogging(void) {
    if (loggingInitialized) return TRUE;
    
    // Get the application data directory
    wchar_t logPath[MAX_EXTENDED_PATH];
    PWSTR appDataPath = NULL;
    
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &appDataPath);
    if (SUCCEEDED(hr) && appDataPath) {
        swprintf(logPath, MAX_EXTENDED_PATH, L"%ls\\YouTubeCacher", appDataPath);
        CoTaskMemFree(appDataPath);
        
        // Create directory if it doesn't exist
        CreateDirectoryW(logPath, NULL);
        
        // Append log filename
        wcscat(logPath, L"\\error.log");
        
        // Open log file for append
        hLogFile = CreateFileW(logPath, 
                              GENERIC_WRITE, 
                              FILE_SHARE_READ, 
                              NULL, 
                              OPEN_ALWAYS, 
                              FILE_ATTRIBUTE_NORMAL, 
                              NULL);
        
        if (hLogFile != INVALID_HANDLE_VALUE) {
            // Move to end of file
            SetFilePointer(hLogFile, 0, NULL, FILE_END);
            loggingInitialized = TRUE;
            
            // Write startup message
            LogInfo(L"System", L"Error logging initialized");
            return TRUE;
        }
    }
    
    return FALSE;
}

void LogError(const wchar_t* category, const wchar_t* message, const wchar_t* details) {
    if (!loggingInitialized || hLogFile == INVALID_HANDLE_VALUE) return;
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    wchar_t logEntry[2048];
    swprintf(logEntry, _countof(logEntry),
             L"[%04d-%02d-%02d %02d:%02d:%02d] ERROR [%ls] %ls\r\n"
             L"Details: %ls\r\n\r\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             category ? category : L"Unknown",
             message ? message : L"No message",
             details ? details : L"No details");
    
    // Convert to UTF-8 for file writing
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, NULL, 0, NULL, NULL);
    if (utf8Size > 0) {
        char* utf8Buffer = (char*)malloc(utf8Size);
        if (utf8Buffer) {
            WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
            
            DWORD bytesWritten;
            WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
            FlushFileBuffers(hLogFile);
            
            free(utf8Buffer);
        }
    }
}

void LogWarning(const wchar_t* category, const wchar_t* message) {
    if (!loggingInitialized || hLogFile == INVALID_HANDLE_VALUE) return;
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    wchar_t logEntry[1024];
    swprintf(logEntry, _countof(logEntry),
             L"[%04d-%02d-%02d %02d:%02d:%02d] WARNING [%ls] %ls\r\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             category ? category : L"Unknown",
             message ? message : L"No message");
    
    // Convert to UTF-8 for file writing
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, NULL, 0, NULL, NULL);
    if (utf8Size > 0) {
        char* utf8Buffer = (char*)malloc(utf8Size);
        if (utf8Buffer) {
            WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
            
            DWORD bytesWritten;
            WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
            FlushFileBuffers(hLogFile);
            
            free(utf8Buffer);
        }
    }
}

void LogInfo(const wchar_t* category, const wchar_t* message) {
    if (!loggingInitialized || hLogFile == INVALID_HANDLE_VALUE) return;
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    wchar_t logEntry[1024];
    swprintf(logEntry, _countof(logEntry),
             L"[%04d-%02d-%02d %02d:%02d:%02d] INFO [%ls] %ls\r\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             category ? category : L"Unknown",
             message ? message : L"No message");
    
    // Convert to UTF-8 for file writing
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, NULL, 0, NULL, NULL);
    if (utf8Size > 0) {
        char* utf8Buffer = (char*)malloc(utf8Size);
        if (utf8Buffer) {
            WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
            
            DWORD bytesWritten;
            WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
            FlushFileBuffers(hLogFile);
            
            free(utf8Buffer);
        }
    }
}

void CleanupErrorLogging(void) {
    if (loggingInitialized && hLogFile != INVALID_HANDLE_VALUE) {
        LogInfo(L"System", L"Error logging shutdown");
        CloseHandle(hLogFile);
        hLogFile = INVALID_HANDLE_VALUE;
        loggingInitialized = FALSE;
    }
}