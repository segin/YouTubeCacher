#include "YouTubeCacher.h"

// Define _countof if not available
#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

// HiDPI helper functions
static int ScaleForDpi(int value, int dpi) {
    return MulDiv(value, dpi, 96); // 96 is the standard DPI
}



// Dynamic dialog layout and sizing with proper icon-text alignment
static void CalculateOptimalDialogSize(HWND hDlg, const wchar_t* message, int* width, int* height) {
    if (!message || !width || !height) return;
    
    // Get DPI for proper scaling
    int dpi = GetWindowDPI(hDlg);
    
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
    int dpi = GetWindowDPI(hDlg);
    
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
    
    // Use unified dialog control IDs for all dialog types
    int messageId = IDC_UNIFIED_MESSAGE;
    int iconId = IDC_UNIFIED_ICON;
    int detailsButtonId = IDC_UNIFIED_DETAILS_BTN;
    int copyButtonId = IDC_UNIFIED_COPY_BTN;
    int okButtonId = IDC_UNIFIED_OK_BTN;
    
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
    L"Details",
    L"Information", 
    L"Summary"
};

// Unified dialog function - single entry point for all dialog types
INT_PTR ShowUnifiedDialog(HWND parent, const UnifiedDialogConfig* config) {
    if (!config) return IDCANCEL;
    
    // Store config in a way the dialog procedure can access it
    return DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_UNIFIED_DIALOG), 
                          parent, UnifiedDialogProc, (LPARAM)config);
}

// Unified Dialog Procedure - handles all dialog types with single resource
INT_PTR CALLBACK UnifiedDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static const UnifiedDialogConfig* config = NULL;
    static BOOL isExpanded = FALSE;
    
    switch (message) {
        case WM_INITDIALOG: {
            config = (const UnifiedDialogConfig*)lParam;
            if (!config) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            
            // Set dialog title and message
            SetWindowTextW(hDlg, config->title ? config->title : L"Information");
            SetDlgItemTextW(hDlg, IDC_UNIFIED_MESSAGE, config->message ? config->message : L"No message");
            
            // Set appropriate icon based on dialog type
            LPCWSTR iconResource;
            switch (config->dialogType) {
                case UNIFIED_DIALOG_ERROR:
                    iconResource = IDI_ERROR;
                    break;
                case UNIFIED_DIALOG_WARNING:
                    iconResource = IDI_WARNING;
                    break;
                case UNIFIED_DIALOG_SUCCESS:
                    iconResource = IDI_INFORMATION;
                    break;
                case UNIFIED_DIALOG_INFO:
                default:
                    iconResource = IDI_INFORMATION;
                    break;
            }
            
            HICON hIcon = LoadIconW(NULL, iconResource);
            if (hIcon) {
                SendDlgItemMessageW(hDlg, IDC_UNIFIED_ICON, STM_SETICON, (WPARAM)hIcon, 0);
            }
            
            // Set up tabs if details are provided
            if (config->details || config->tab2_content || config->tab3_content) {
                HWND hTabControl = GetDlgItem(hDlg, IDC_UNIFIED_TAB_CONTROL);
                if (hTabControl) {
                    TCITEMW tie;
                    tie.mask = TCIF_TEXT;
                    
                    // Always add first tab (Details)
                    tie.pszText = (wchar_t*)(config->tab1_name ? config->tab1_name : L"Details");
                    TabCtrl_InsertItem(hTabControl, 0, &tie);
                    
                    // Add second tab if content exists
                    if (config->tab2_content) {
                        tie.pszText = (wchar_t*)(config->tab2_name ? config->tab2_name : L"Information");
                        TabCtrl_InsertItem(hTabControl, 1, &tie);
                    }
                    
                    // Add third tab if content exists
                    if (config->tab3_content) {
                        tie.pszText = (wchar_t*)(config->tab3_name ? config->tab3_name : L"Additional");
                        TabCtrl_InsertItem(hTabControl, 2, &tie);
                    }
                    
                    TabCtrl_SetCurSel(hTabControl, 0);
                    
                    // Set initial tab content
                    SetDlgItemTextW(hDlg, IDC_UNIFIED_TAB1_TEXT, config->details ? config->details : L"No details available");
                    if (config->tab2_content) {
                        SetDlgItemTextW(hDlg, IDC_UNIFIED_TAB2_TEXT, config->tab2_content);
                    }
                    if (config->tab3_content) {
                        SetDlgItemTextW(hDlg, IDC_UNIFIED_TAB3_TEXT, config->tab3_content);
                    }
                }
            }
            
            // Set button text with accelerator keys
            const wchar_t* detailsText = config->detailsButtonText ? config->detailsButtonText : L"&Details >>";
            const wchar_t* copyText = config->copyButtonText ? config->copyButtonText : L"&Copy";
            const wchar_t* okText = config->okButtonText ? config->okButtonText : L"&OK";
            
            SetWindowTextW(GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN), detailsText);
            SetWindowTextW(GetDlgItem(hDlg, IDC_UNIFIED_COPY_BTN), copyText);
            SetWindowTextW(GetDlgItem(hDlg, IDC_UNIFIED_OK_BTN), okText);
            
            // Show/hide buttons based on config
            if (!config->showDetailsButton || (!config->details && !config->tab2_content && !config->tab3_content)) {
                ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN), SW_HIDE);
            }
            
            if (!config->showCopyButton) {
                ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_COPY_BTN), SW_HIDE);
            }
            
            // Set accessible names for screen readers
            HWND hIconCtrl = GetDlgItem(hDlg, IDC_UNIFIED_ICON);
            HWND hMessageCtrl = GetDlgItem(hDlg, IDC_UNIFIED_MESSAGE);
            HWND hDetailsBtnCtrl = GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN);
            HWND hCopyBtnCtrl = GetDlgItem(hDlg, IDC_UNIFIED_COPY_BTN);
            HWND hOkBtnCtrl = GetDlgItem(hDlg, IDC_UNIFIED_OK_BTN);
            HWND hTabCtrl = GetDlgItem(hDlg, IDC_UNIFIED_TAB_CONTROL);
            
            // Set accessible name for icon based on dialog type
            const wchar_t* iconDescription = NULL;
            switch (config->dialogType) {
                case UNIFIED_DIALOG_ERROR:
                    iconDescription = L"Error icon";
                    break;
                case UNIFIED_DIALOG_WARNING:
                    iconDescription = L"Warning icon";
                    break;
                case UNIFIED_DIALOG_SUCCESS:
                    iconDescription = L"Success icon";
                    break;
                case UNIFIED_DIALOG_INFO:
                default:
                    iconDescription = L"Information icon";
                    break;
            }
            SetControlAccessibility(hIconCtrl, iconDescription, NULL);
            
            // Set accessible name for message text
            SetControlAccessibility(hMessageCtrl, L"Message", NULL);
            
            // Set accessible names for buttons (text is already set by button creation)
            SetControlAccessibility(hDetailsBtnCtrl, L"Details", L"Show or hide additional details");
            SetControlAccessibility(hCopyBtnCtrl, L"Copy", L"Copy message to clipboard");
            SetControlAccessibility(hOkBtnCtrl, L"OK", L"Close dialog");
            
            // Set accessible name for tab control
            if (hTabCtrl && IsWindowVisible(hTabCtrl)) {
                SetControlAccessibility(hTabCtrl, L"Details tabs", L"Additional information organized in tabs");
            }
            
            // Set accessible names for tab content controls
            HWND hTab1Text = GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT);
            HWND hTab2Text = GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT);
            HWND hTab3Text = GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT);
            
            if (hTab1Text) {
                const wchar_t* tab1Name = config->tab1_name ? config->tab1_name : L"Details";
                SetControlAccessibility(hTab1Text, tab1Name, L"Detailed information content");
            }
            if (hTab2Text && config->tab2_content) {
                const wchar_t* tab2Name = config->tab2_name ? config->tab2_name : L"Information";
                SetControlAccessibility(hTab2Text, tab2Name, L"Additional information content");
            }
            if (hTab3Text && config->tab3_content) {
                const wchar_t* tab3Name = config->tab3_name ? config->tab3_name : L"Additional";
                SetControlAccessibility(hTab3Text, tab3Name, L"Additional content");
            }
            
            // Notify screen readers that dialog is ready
            if (IsScreenReaderActive()) {
                NotifyAccessibilityStateChange(hDlg, EVENT_OBJECT_SHOW);
                // Announce the dialog message to screen readers
                if (config->message) {
                    NotifyAccessibilityStateChange(hMessageCtrl, EVENT_OBJECT_NAMECHANGE);
                }
            }
            
            // Configure tab order for keyboard navigation
            // Tab order: Details button -> Copy button -> OK button -> Tab control (if visible)
            TabOrderConfig tabConfig;
            TabOrderEntry entries[4];
            int entryCount = 0;
            
            // Details button (if visible)
            if (config->showDetailsButton && (config->details || config->tab2_content || config->tab3_content)) {
                entries[entryCount].controlId = IDC_UNIFIED_DETAILS_BTN;
                entries[entryCount].tabOrder = entryCount;
                entries[entryCount].isTabStop = TRUE;
                entryCount++;
            }
            
            // Copy button (if visible)
            if (config->showCopyButton) {
                entries[entryCount].controlId = IDC_UNIFIED_COPY_BTN;
                entries[entryCount].tabOrder = entryCount;
                entries[entryCount].isTabStop = TRUE;
                entryCount++;
            }
            
            // OK button (always visible)
            entries[entryCount].controlId = IDC_UNIFIED_OK_BTN;
            entries[entryCount].tabOrder = entryCount;
            entries[entryCount].isTabStop = TRUE;
            entryCount++;
            
            // Tab control (if visible and has content)
            if (config->details || config->tab2_content || config->tab3_content) {
                entries[entryCount].controlId = IDC_UNIFIED_TAB_CONTROL;
                entries[entryCount].tabOrder = entryCount;
                entries[entryCount].isTabStop = TRUE;
                entryCount++;
            }
            
            tabConfig.entries = entries;
            tabConfig.count = entryCount;
            SetDialogTabOrder(hDlg, &tabConfig);
            
            // Validate accelerator keys to ensure no conflicts
            ValidateAcceleratorKeys(hDlg);
            
            // Start in collapsed state
            ResizeUnifiedDialog(hDlg, FALSE);
            
            // Set initial focus to OK button
            SetInitialDialogFocus(hDlg);
            
            // Return FALSE to allow our custom focus setting
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_UNIFIED_DETAILS_BTN:
                    isExpanded = !isExpanded;
                    ResizeUnifiedDialog(hDlg, isExpanded);
                    if (isExpanded) {
                        ShowUnifiedDialogTab(hDlg, TabCtrl_GetCurSel(GetDlgItem(hDlg, IDC_UNIFIED_TAB_CONTROL)));
                    }
                    
                    // Notify screen readers of state change
                    if (IsScreenReaderActive()) {
                        HWND hDetailsBtn = GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN);
                        HWND hTabCtrl = GetDlgItem(hDlg, IDC_UNIFIED_TAB_CONTROL);
                        
                        // Announce button state change
                        NotifyAccessibilityStateChange(hDetailsBtn, EVENT_OBJECT_STATECHANGE);
                        
                        // Announce tab control visibility change
                        if (hTabCtrl) {
                            NotifyAccessibilityStateChange(hTabCtrl, isExpanded ? EVENT_OBJECT_SHOW : EVENT_OBJECT_HIDE);
                        }
                        
                        // If expanded, announce the current tab content
                        if (isExpanded) {
                            int currentTab = TabCtrl_GetCurSel(hTabCtrl);
                            HWND hTabText = NULL;
                            switch (currentTab) {
                                case 0: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT); break;
                                case 1: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT); break;
                                case 2: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT); break;
                            }
                            if (hTabText) {
                                NotifyAccessibilityStateChange(hTabText, EVENT_OBJECT_FOCUS);
                            }
                        }
                    }
                    return TRUE;
                    
                case IDC_UNIFIED_COPY_BTN:
                    CopyUnifiedDialogToClipboard(config);
                    return TRUE;
                    
                case IDC_UNIFIED_OK_BTN:
                case IDOK:
                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            break;
            
        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->code == TCN_SELCHANGE && pnmh->idFrom == IDC_UNIFIED_TAB_CONTROL) {
                int selectedTab = TabCtrl_GetCurSel(pnmh->hwndFrom);
                ShowUnifiedDialogTab(hDlg, selectedTab);
                
                // Notify screen readers of tab change
                if (IsScreenReaderActive()) {
                    // Announce tab control selection change
                    NotifyAccessibilityStateChange(pnmh->hwndFrom, EVENT_OBJECT_SELECTION);
                    
                    // Announce the newly visible tab content
                    HWND hTabText = NULL;
                    switch (selectedTab) {
                        case 0: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT); break;
                        case 1: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT); break;
                        case 2: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT); break;
                    }
                    if (hTabText) {
                        NotifyAccessibilityStateChange(hTabText, EVENT_OBJECT_SHOW);
                    }
                }
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
                
                // Resize dialog to maintain proper layout
                ResizeUnifiedDialog(hDlg, isExpanded);
                
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
        
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }
    
    return FALSE;
}

// Helper function to resize unified dialog
void ResizeUnifiedDialog(HWND hDlg, BOOL expanded) {
    // Get DPI for this window
    int dpi = GetWindowDPI(hDlg);
    
    // ============================================================================
    // Microsoft Windows UI Guidelines - Modern Dialog Standards
    // ============================================================================
    // Based on Windows 10/11 dialog standards:
    // - Edge margins: 11px (7 DLU converted to pixels)
    // - Button size: 75px wide × 23px high (standard push button)
    // - Button spacing: 6px between adjacent buttons
    // - Control spacing: 10px between icon and text
    // - Icon to text gap: 10px
    // - Bottom margin: 11px from button bottom to dialog edge
    //
    // These are PIXEL values at 96 DPI, scaled appropriately for HiDPI.
    // ============================================================================
    
    // Using modern Windows dialog standards with proper sizing
    int margin = ScaleForDpi(11, dpi);           // 11px edge margins (7 DLU standard)
    int iconSize = ScaleForDpi(32, dpi);         // Standard system icon (32×32)
    int buttonHeight = ScaleForDpi(23, dpi);     // 23px button height (14 DLU standard)
    int buttonWidth = ScaleForDpi(75, dpi);      // 75px button width (50 DLU standard)
    int buttonGap = ScaleForDpi(6, dpi);         // 6px between buttons
    int controlSpacing = ScaleForDpi(10, dpi);   // 10px between icon and text
    int groupSpacing = ScaleForDpi(10, dpi);     // 10px between content and buttons
    
    // Get current message text for sizing
    wchar_t messageText[1024];
    GetDlgItemTextW(hDlg, IDC_UNIFIED_MESSAGE, messageText, 1024);
    
    // Calculate text metrics
    HDC hdc = GetDC(hDlg);
    HFONT hFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
    HFONT hOldFont = NULL;
    if (hFont) hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    // Calculate dialog dimensions
    int minWidth = ScaleForDpi(320, dpi);
    int maxWidth = ScaleForDpi(520, dpi);
    int iconGap = controlSpacing;
    int availableTextWidth = maxWidth - margin - iconSize - iconGap - margin;
    
    RECT textRect = {0, 0, availableTextWidth, 0};
    int textHeight = DrawTextW(hdc, messageText, -1, &textRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    
    int requiredWidth = margin + iconSize + iconGap + textRect.right + margin;
    int dialogWidth = max(minWidth, min(maxWidth, requiredWidth));
    
    int finalTextWidth = dialogWidth - margin - iconSize - iconGap - margin;
    RECT finalTextRect = {0, 0, finalTextWidth, 0};
    textHeight = DrawTextW(hdc, messageText, -1, &finalTextRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    
    // ============================================================================
    // Position elements per Microsoft UI Guidelines
    // ============================================================================
    // Layout: [margin][icon][controlSpacing][message text][margin]
    //         [margin][content area...                    ][margin]
    //         [margin][groupSpacing]
    //         [margin][buttons...                         ][margin]
    // ============================================================================
    
    int iconX = margin;
    int iconY = margin;
    
    // Icon-text vertical alignment:
    // - If text is shorter than icon: vertically center text with icon
    // - If text is taller than icon: vertically center icon with text
    int messageX = iconX + iconSize + controlSpacing;
    int messageY;
    int messageWidth = finalTextWidth;
    int messageHeight = textHeight;
    
    if (textHeight <= iconSize) {
        // Text is shorter: center text vertically with icon
        messageY = iconY + (iconSize - textHeight) / 2;
    } else {
        // Text is taller: keep text at margin, adjust icon to center with text
        messageY = iconY;
        iconY = iconY + (textHeight - iconSize) / 2;
    }
    
    // ============================================================================
    // Calculate dialog height per Microsoft UI Guidelines
    // ============================================================================
    // Vertical layout:
    // [margin]              - 7 DLU top margin
    // [content area]        - icon and/or message (whichever is taller)
    // [groupSpacing]        - 7 DLU between content and buttons
    // [buttons]             - 14 DLU button height
    // [margin]              - 7 DLU bottom margin
    // 
    // When expanded, add:
    // [groupSpacing]        - 7 DLU between buttons and tab
    // [tab control]         - variable height
    // [margin]              - 7 DLU bottom margin (replaces previous margin)
    // ============================================================================
    
    // Content area height = max of icon height or message height
    int contentHeight = max(iconSize, messageHeight);
    
    // Button Y position = margin + content + groupSpacing
    int buttonY = margin + contentHeight + groupSpacing;
    
    // CLIENT AREA HEIGHT CALCULATION (not including window frame)
    // Collapsed: margin + content + groupSpacing + buttons + margin
    int collapsedClientHeight = margin + contentHeight + groupSpacing + buttonHeight + margin;
    
    // Expanded: margin + content + groupSpacing + buttons + groupSpacing + tab + margin
    int tabHeight = ScaleForDpi(200, dpi);
    int expandedClientHeight = margin + contentHeight + groupSpacing + buttonHeight + groupSpacing + tabHeight + margin;
    
    // ============================================================================
    // Convert CLIENT AREA to WINDOW SIZE
    // ============================================================================
    // Microsoft UI Guidelines: Use AdjustWindowRectEx to account for:
    // - Title bar (SM_CYCAPTION)
    // - Dialog frame borders (SM_CYDLGFRAME for non-resizable dialogs)
    // - Menu bar (if present)
    // ============================================================================
    
    RECT clientRect = {0, 0, dialogWidth, expanded ? expandedClientHeight : collapsedClientHeight};
    DWORD style = GetWindowLongW(hDlg, GWL_STYLE);
    DWORD exStyle = GetWindowLongW(hDlg, GWL_EXSTYLE);
    
    // AdjustWindowRectEx calculates window size from client size
    AdjustWindowRectEx(&clientRect, style, FALSE, exStyle);
    
    int finalHeight = clientRect.bottom - clientRect.top;
    
    // Position dialog on screen
    RECT rect;
    GetWindowRect(hDlg, &rect);
    int currentX = rect.left;
    int currentY = rect.top;
    
    // Adjust position if dialog would go off screen
    HMONITOR hMonitor = MonitorFromWindow(hDlg, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hMonitor, &mi);
    RECT screenRect = mi.rcWork;
    
    if (currentX < screenRect.left) currentX = screenRect.left;
    if (currentY < screenRect.top) currentY = screenRect.top;
    if (currentX + dialogWidth > screenRect.right) currentX = screenRect.right - dialogWidth;
    if (currentY + finalHeight > screenRect.bottom) currentY = screenRect.bottom - finalHeight;
    
    // Apply positioning
    SetWindowPos(hDlg, NULL, currentX, currentY, dialogWidth, finalHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    
    // Position controls
    SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_ICON), NULL, iconX, iconY, iconSize, iconSize, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_MESSAGE), NULL, messageX, messageY, messageWidth, messageHeight, SWP_NOZORDER);
    
    // Position buttons
    int detailsX = margin;
    int okX = dialogWidth - margin - buttonWidth;
    int copyX = okX - buttonGap - buttonWidth;
    
    SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN), NULL, detailsX, buttonY, buttonWidth, buttonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_COPY_BTN), NULL, copyX, buttonY, buttonWidth, buttonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_OK_BTN), NULL, okX, buttonY, buttonWidth, buttonHeight, SWP_NOZORDER);
    
    // Position expanded controls using proper calculations
    // Microsoft UI Guidelines: 7 DLU spacing between button group and tab control group
    if (expanded) {
        int tabY = buttonY + buttonHeight + groupSpacing;  // 7 DLU below buttons
        int tabWidth = dialogWidth - 2 * margin;
        
        SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_TAB_CONTROL), NULL, margin, tabY, tabWidth, tabHeight, SWP_NOZORDER);
        
        // Tab control internal padding: 3px on sides, 24px for tab header
        int tabInternalMargin = ScaleForDpi(3, dpi);
        int tabHeaderHeight = ScaleForDpi(24, dpi);
        int textX = margin + tabInternalMargin;
        int textY = tabY + tabHeaderHeight;
        int textW = tabWidth - (2 * tabInternalMargin);
        int textH = tabHeight - tabHeaderHeight - tabInternalMargin;  // Account for header + bottom padding
        
        SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT), NULL, textX, textY, textW, textH, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT), NULL, textX, textY, textW, textH, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT), NULL, textX, textY, textW, textH, SWP_NOZORDER);
    }
    
    // Show/hide expanded controls
    int showState = expanded ? SW_SHOW : SW_HIDE;
    ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB_CONTROL), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT), showState);
    ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT), showState);
    
    // Update details button text (preserve accelerator key)
    SetWindowTextW(GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN), expanded ? L"<< &Details" : L"&Details >>");
    
    if (hOldFont) SelectObject(hdc, hOldFont);
    ReleaseDC(hDlg, hdc);
}

// Helper function to show specific tab
void ShowUnifiedDialogTab(HWND hDlg, int tabIndex) {
    // Hide all text controls
    ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT), SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT), SW_HIDE);
    ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT), SW_HIDE);
    
    // Show selected tab
    switch (tabIndex) {
        case 0:
            ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT), SW_SHOW);
            break;
        case 1:
            ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT), SW_SHOW);
            break;
        case 2:
            ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT), SW_SHOW);
            break;
    }
}

// Helper function to copy dialog content to clipboard
BOOL CopyUnifiedDialogToClipboard(const UnifiedDialogConfig* config) {
    if (!config) return FALSE;
    
    size_t totalSize = 1024;
    if (config->title) totalSize += wcslen(config->title);
    if (config->message) totalSize += wcslen(config->message);
    if (config->details) totalSize += wcslen(config->details);
    if (config->tab2_content) totalSize += wcslen(config->tab2_content);
    if (config->tab3_content) totalSize += wcslen(config->tab3_content);
    
    wchar_t* clipboardText = (wchar_t*)SAFE_MALLOC(totalSize * sizeof(wchar_t));
    if (!clipboardText) return FALSE;
    
    const wchar_t* typeStr;
    switch (config->dialogType) {
        case UNIFIED_DIALOG_ERROR: typeStr = L"ERROR"; break;
        case UNIFIED_DIALOG_WARNING: typeStr = L"WARNING"; break;
        case UNIFIED_DIALOG_SUCCESS: typeStr = L"SUCCESS"; break;
        case UNIFIED_DIALOG_INFO: 
        default: typeStr = L"INFORMATION"; break;
    }
    
    swprintf(clipboardText, totalSize,
        L"=== %ls REPORT ===\r\n"
        L"Title: %ls\r\n"
        L"Message: %ls\r\n\r\n"
        L"=== %ls ===\r\n%ls\r\n\r\n"
        L"=== %ls ===\r\n%ls\r\n\r\n"
        L"=== %ls ===\r\n%ls\r\n",
        typeStr,
        config->title ? config->title : L"No title",
        config->message ? config->message : L"No message",
        config->tab1_name ? config->tab1_name : L"DETAILS",
        config->details ? config->details : L"No details available",
        config->tab2_name ? config->tab2_name : L"INFORMATION",
        config->tab2_content ? config->tab2_content : L"No additional information",
        config->tab3_name ? config->tab3_name : L"ADDITIONAL",
        config->tab3_content ? config->tab3_content : L"No additional content");
    
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
    
    SAFE_FREE(clipboardText);
    return success;
}



// Resize error dialog for expanded/collapsed state
void ResizeErrorDialog(HWND hDlg, BOOL expanded) {
    // Get DPI for this window
    int dpi = GetWindowDPI(hDlg);
    
    // Get all control handles at the start
    HWND hIcon = GetDlgItem(hDlg, IDC_UNIFIED_ICON);
    HWND hMessage = GetDlgItem(hDlg, IDC_UNIFIED_MESSAGE);
    HWND hDetailsBtn = GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN);
    HWND hCopyBtn = GetDlgItem(hDlg, IDC_UNIFIED_COPY_BTN);
    HWND hOkBtn = GetDlgItem(hDlg, IDC_UNIFIED_OK_BTN);
    HWND hTabControl = GetDlgItem(hDlg, IDC_UNIFIED_TAB_CONTROL);
    HWND hDetailsText = GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT);
    HWND hDiagText = GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT);
    HWND hSolutionText = GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT);
    
    // Get the current message text for dynamic sizing
    wchar_t messageText[1024];
    GetDlgItemTextW(hDlg, IDC_UNIFIED_MESSAGE, messageText, 1024);
    
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
    
    // Use calculated height without artificial minimum - let content determine size
    // Only ensure we have enough space for the actual content
    
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
    
    // Update details button text (preserve accelerator key)
    if (hDetailsBtn) {
        SetWindowTextW(hDetailsBtn, expanded ? L"<< &Details" : L"&Details >>");
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
    
    // Always add the Details tab
    tie.pszText = (wchar_t*)SUCCESS_TAB_NAMES[0];
    TabCtrl_InsertItem(hTabControl, 0, &tie);
    
    TabCtrl_SetCurSel(hTabControl, 0);
}

// Initialize success dialog tabs with all tabs (for full dialogs)
void InitializeFullSuccessDialogTabs(HWND hTabControl) {
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
    HWND hDetailsText = GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT);
    HWND hDiagText = GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT);
    HWND hSolutionText = GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT);
    
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
    
    wchar_t* clipboardText = (wchar_t*)SAFE_MALLOC(totalSize * sizeof(wchar_t));
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
    
    SAFE_FREE(clipboardText);
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
            
            // Use unified control IDs for all dialog types
            int tabControlId = IDC_UNIFIED_TAB_CONTROL;
            int messageId = IDC_UNIFIED_MESSAGE;
            int iconId = IDC_UNIFIED_ICON;
            int detailsTextId = IDC_UNIFIED_TAB1_TEXT;
            int diagTextId = IDC_UNIFIED_TAB2_TEXT;
            int solutionTextId = IDC_UNIFIED_TAB3_TEXT;
            
            // Set appropriate icon based on dialog type
            LPCWSTR iconResource;
            switch (errorDialog->dialogType) {
                case DIALOG_TYPE_SUCCESS:
                    iconResource = IDI_INFORMATION;
                    break;
                case DIALOG_TYPE_ERROR:
                    iconResource = IDI_ERROR;
                    break;
                default:
                    iconResource = IDI_WARNING;
                    break;
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
                // Check if we have diagnostics and solutions to determine tab setup
                if (errorDialog->diagnostics && errorDialog->solutions) {
                    InitializeFullSuccessDialogTabs(errorDialog->hTabControl);
                } else {
                    InitializeSuccessDialogTabs(errorDialog->hTabControl);
                }
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
            HWND hMessage = GetDlgItem(hDlg, IDC_UNIFIED_MESSAGE);
            if (hMessage) {
                int dpi = GetWindowDPI(hDlg);
                int iconSpace = ScaleForDpi(50, dpi);
                int margin = ScaleForDpi(10, dpi);
                int messageWidth = optimalWidth - iconSpace - margin;
                int messageHeight = optimalHeight - ScaleForDpi(60, dpi);
                
                SetWindowPos(hMessage, NULL, iconSpace, margin, messageWidth, messageHeight, 
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            
            // Set accessible names for all controls
            HWND hIconCtrl = GetDlgItem(hDlg, iconId);
            HWND hMessageCtrl = GetDlgItem(hDlg, messageId);
            HWND hDetailsBtnCtrl = GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN);
            HWND hCopyBtnCtrl = GetDlgItem(hDlg, IDC_UNIFIED_COPY_BTN);
            HWND hOkBtnCtrl = GetDlgItem(hDlg, IDC_UNIFIED_OK_BTN);
            HWND hTabCtrlCtrl = errorDialog->hTabControl;
            
            // Set accessible name for icon based on dialog type
            const wchar_t* iconDescription = NULL;
            switch (errorDialog->dialogType) {
                case DIALOG_TYPE_ERROR:
                    iconDescription = L"Error icon";
                    break;
                case DIALOG_TYPE_WARNING:
                    iconDescription = L"Warning icon";
                    break;
                case DIALOG_TYPE_SUCCESS:
                    iconDescription = L"Success icon";
                    break;
                default:
                    iconDescription = L"Information icon";
                    break;
            }
            SetControlAccessibility(hIconCtrl, iconDescription, NULL);
            
            // Set accessible name for message text
            SetControlAccessibility(hMessageCtrl, L"Message", NULL);
            
            // Set accessible names for buttons
            SetControlAccessibility(hDetailsBtnCtrl, L"Details", L"Show or hide additional details");
            SetControlAccessibility(hCopyBtnCtrl, L"Copy", L"Copy message to clipboard");
            SetControlAccessibility(hOkBtnCtrl, L"OK", L"Close dialog");
            
            // Set accessible name for tab control
            if (hTabCtrlCtrl) {
                SetControlAccessibility(hTabCtrlCtrl, L"Details tabs", L"Additional information organized in tabs");
            }
            
            // Set accessible names for tab content controls
            HWND hTab1TextCtrl = GetDlgItem(hDlg, detailsTextId);
            HWND hTab2TextCtrl = GetDlgItem(hDlg, diagTextId);
            HWND hTab3TextCtrl = GetDlgItem(hDlg, solutionTextId);
            
            if (hTab1TextCtrl) {
                const wchar_t* tab1Name = errorDialog->dialogType == DIALOG_TYPE_SUCCESS ? L"Details" : L"Error Details";
                SetControlAccessibility(hTab1TextCtrl, tab1Name, L"Detailed information content");
            }
            if (hTab2TextCtrl) {
                const wchar_t* tab2Name = errorDialog->dialogType == DIALOG_TYPE_SUCCESS ? L"Information" : L"Diagnostics";
                SetControlAccessibility(hTab2TextCtrl, tab2Name, L"Diagnostic information content");
            }
            if (hTab3TextCtrl) {
                const wchar_t* tab3Name = errorDialog->dialogType == DIALOG_TYPE_SUCCESS ? L"Summary" : L"Solutions";
                SetControlAccessibility(hTab3TextCtrl, tab3Name, L"Solutions and recommendations");
            }
            
            // Notify screen readers that dialog is ready
            if (IsScreenReaderActive()) {
                NotifyAccessibilityStateChange(hDlg, EVENT_OBJECT_SHOW);
                // Announce the dialog message to screen readers
                if (errorDialog->message) {
                    NotifyAccessibilityStateChange(hMessageCtrl, EVENT_OBJECT_NAMECHANGE);
                }
            }
            
            // Configure tab order for keyboard navigation
            // Tab order: Details button -> Copy button -> OK button -> Tab control (if visible)
            TabOrderConfig tabConfig;
            TabOrderEntry entries[4];
            int entryCount = 0;
            
            // Details button (if visible)
            BOOL hasDetails = errorDialog->details || errorDialog->diagnostics || errorDialog->solutions;
            if (hasDetails) {
                entries[entryCount].controlId = IDC_UNIFIED_DETAILS_BTN;
                entries[entryCount].tabOrder = entryCount;
                entries[entryCount].isTabStop = TRUE;
                entryCount++;
            }
            
            // Copy button (always visible)
            entries[entryCount].controlId = IDC_UNIFIED_COPY_BTN;
            entries[entryCount].tabOrder = entryCount;
            entries[entryCount].isTabStop = TRUE;
            entryCount++;
            
            // OK button (always visible)
            entries[entryCount].controlId = IDC_UNIFIED_OK_BTN;
            entries[entryCount].tabOrder = entryCount;
            entries[entryCount].isTabStop = TRUE;
            entryCount++;
            
            // Tab control (if visible and has content)
            if (hasDetails) {
                entries[entryCount].controlId = IDC_UNIFIED_TAB_CONTROL;
                entries[entryCount].tabOrder = entryCount;
                entries[entryCount].isTabStop = TRUE;
                entryCount++;
            }
            
            tabConfig.entries = entries;
            tabConfig.count = entryCount;
            SetDialogTabOrder(hDlg, &tabConfig);
            
            // Validate accelerator keys to ensure no conflicts
            ValidateAcceleratorKeys(hDlg);
            
            // Hide Details button and related controls for success dialogs if no additional content
            if (errorDialog->dialogType == DIALOG_TYPE_SUCCESS && !errorDialog->details && !errorDialog->diagnostics && !errorDialog->solutions) {
                ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB_CONTROL), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT), SW_HIDE);
            }
            
            // Start in collapsed state (this will trigger ResizeErrorDialog)
            ResizeErrorDialog(hDlg, FALSE);
            
            // Set initial focus
            SetInitialDialogFocus(hDlg);
            
            // Return FALSE to allow our custom focus setting
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_UNIFIED_DETAILS_BTN:
                    if (errorDialog) {
                        errorDialog->isExpanded = !errorDialog->isExpanded;
                        ResizeErrorDialog(hDlg, errorDialog->isExpanded);
                        if (errorDialog->isExpanded) {
                            ShowErrorDialogTab(hDlg, TabCtrl_GetCurSel(errorDialog->hTabControl));
                        }
                        
                        // Notify screen readers of state change
                        if (IsScreenReaderActive()) {
                            HWND hDetailsBtn = GetDlgItem(hDlg, IDC_UNIFIED_DETAILS_BTN);
                            
                            // Announce button state change
                            NotifyAccessibilityStateChange(hDetailsBtn, EVENT_OBJECT_STATECHANGE);
                            
                            // Announce tab control visibility change
                            if (errorDialog->hTabControl) {
                                NotifyAccessibilityStateChange(errorDialog->hTabControl, 
                                    errorDialog->isExpanded ? EVENT_OBJECT_SHOW : EVENT_OBJECT_HIDE);
                            }
                            
                            // If expanded, announce the current tab content
                            if (errorDialog->isExpanded) {
                                int currentTab = TabCtrl_GetCurSel(errorDialog->hTabControl);
                                HWND hTabText = NULL;
                                switch (currentTab) {
                                    case 0: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT); break;
                                    case 1: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT); break;
                                    case 2: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT); break;
                                }
                                if (hTabText) {
                                    NotifyAccessibilityStateChange(hTabText, EVENT_OBJECT_FOCUS);
                                }
                            }
                        }
                    }
                    return TRUE;
                    
                case IDC_UNIFIED_COPY_BTN:
                    if (errorDialog) {
                        if (CopyErrorInfoToClipboard(errorDialog)) {
                            SHOW_ERROR_DIALOG(hDlg, YTC_SEVERITY_INFO, YTC_ERROR_SUCCESS, 
                                             L"Information copied to clipboard successfully.\r\n\r\n"
                                             L"The error details are now available in your clipboard.");
                        } else {
                            SHOW_ERROR_DIALOG(hDlg, YTC_SEVERITY_ERROR, YTC_ERROR_DIALOG_CREATION, 
                                             L"Failed to copy information to clipboard.\r\n\r\n"
                                             L"Please try again or manually select and copy the text.");
                        }
                    }
                    return TRUE;
                    
                case IDC_UNIFIED_OK_BTN:
                case IDOK:
                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            break;
            
        case WM_NOTIFY: {
            NMHDR* pnmh = (NMHDR*)lParam;
            if (pnmh->idFrom == IDC_UNIFIED_TAB_CONTROL && pnmh->code == TCN_SELCHANGE) {
                int selectedTab = TabCtrl_GetCurSel(errorDialog->hTabControl);
                ShowErrorDialogTab(hDlg, selectedTab);
                
                // Notify screen readers of tab change
                if (IsScreenReaderActive()) {
                    // Announce tab control selection change
                    NotifyAccessibilityStateChange(errorDialog->hTabControl, EVENT_OBJECT_SELECTION);
                    
                    // Announce the newly visible tab content
                    HWND hTabText = NULL;
                    switch (selectedTab) {
                        case 0: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB1_TEXT); break;
                        case 1: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB2_TEXT); break;
                        case 2: hTabText = GetDlgItem(hDlg, IDC_UNIFIED_TAB3_TEXT); break;
                    }
                    if (hTabText) {
                        NotifyAccessibilityStateChange(hTabText, EVENT_OBJECT_SHOW);
                    }
                }
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
                
                // Resize dialog to maintain proper layout
                if (errorDialog) {
                    ResizeErrorDialog(hDlg, errorDialog->isExpanded);
                }
                
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
        
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }
    
    return FALSE;
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
    
    UnifiedDialogConfig config = {0};
    config.dialogType = UNIFIED_DIALOG_ERROR;
    config.title = title;
    config.message = message;
    config.details = result->output ? result->output : L"No detailed output available";
    config.tab1_name = L"Output";
    config.tab2_content = result->diagnostics ? result->diagnostics : L"No diagnostic information available";
    config.tab2_name = L"Diagnostics";
    config.tab3_content = solutions;
    config.tab3_name = L"Solutions";
    config.showDetailsButton = TRUE;
    config.showCopyButton = TRUE;
    
    INT_PTR result_code = ShowUnifiedDialog(parent, &config);
    
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
    
    UnifiedDialogConfig config = {0};
    config.dialogType = UNIFIED_DIALOG_ERROR;
    config.title = title;
    config.message = message;
    config.details = validationInfo->errorDetails ? validationInfo->errorDetails : L"No detailed error information available";
    config.tab1_name = L"Details";
    config.tab2_content = L"Validation performed comprehensive checks on the yt-dlp executable and its dependencies.";
    config.tab2_name = L"Validation Info";
    config.tab3_content = solutions;
    config.tab3_name = L"Solutions";
    config.showDetailsButton = TRUE;
    config.showCopyButton = TRUE;
    
    INT_PTR result = ShowUnifiedDialog(parent, &config);
    
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
    
    UnifiedDialogConfig config = {0};
    config.dialogType = UNIFIED_DIALOG_ERROR;
    config.title = title;
    config.message = message;
    config.details = details;
    config.tab1_name = L"Details";
    config.tab2_content = L"Process creation or execution failed at the Windows API level.";
    config.tab2_name = L"Technical Info";
    config.tab3_content = solutions;
    config.tab3_name = L"Solutions";
    config.showDetailsButton = TRUE;
    config.showCopyButton = TRUE;
    
    INT_PTR result = ShowUnifiedDialog(parent, &config);
    
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
    
    UnifiedDialogConfig config = {0};
    config.dialogType = UNIFIED_DIALOG_ERROR;
    config.title = title;
    config.message = message;
    config.details = details;
    config.tab1_name = L"Details";
    config.tab2_content = L"Temporary directory creation failed. This may be due to permissions, disk space, or path length issues.";
    config.tab2_name = L"Analysis";
    config.tab3_content = solutions;
    config.tab3_name = L"Solutions";
    config.showDetailsButton = TRUE;
    config.showCopyButton = TRUE;
    
    INT_PTR result = ShowUnifiedDialog(parent, &config);
    
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
    
    UnifiedDialogConfig config = {0};
    config.dialogType = UNIFIED_DIALOG_ERROR;
    config.title = title;
    config.message = message;
    config.details = details;
    config.tab1_name = L"Details";
    config.tab2_content = L"Memory allocation failed. This may indicate low system memory or memory fragmentation.";
    config.tab2_name = L"Analysis";
    config.tab3_content = solutions;
    config.tab3_name = L"Solutions";
    config.showDetailsButton = TRUE;
    config.showCopyButton = TRUE;
    
    INT_PTR result = ShowUnifiedDialog(parent, &config);
    
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
    
    UnifiedDialogConfig config = {0};
    config.dialogType = UNIFIED_DIALOG_ERROR;
    config.title = title;
    config.message = message;
    config.details = details ? details : L"Configuration initialization failed";
    config.tab1_name = L"Details";
    config.tab2_content = L"Application configuration could not be loaded or initialized properly.";
    config.tab2_name = L"Analysis";
    config.tab3_content = solutions;
    config.tab3_name = L"Solutions";
    config.showDetailsButton = TRUE;
    config.showCopyButton = TRUE;
    
    INT_PTR result = ShowUnifiedDialog(parent, &config);
    
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
    
    UnifiedDialogConfig config = {0};
    config.dialogType = UNIFIED_DIALOG_ERROR;
    config.title = title;
    config.message = message;
    config.details = details;
    config.tab1_name = L"Details";
    config.tab2_content = L"User interface component creation failed. This may indicate system resource issues.";
    config.tab2_name = L"Analysis";
    config.tab3_content = solutions;
    config.tab3_name = L"Solutions";
    config.showDetailsButton = TRUE;
    config.showCopyButton = TRUE;
    
    INT_PTR result = ShowUnifiedDialog(parent, &config);
    
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
        char* utf8Buffer = (char*)SAFE_MALLOC(utf8Size);
        if (utf8Buffer) {
            WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
            
            DWORD bytesWritten;
            WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
            FlushFileBuffers(hLogFile);
            
            SAFE_FREE(utf8Buffer);
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
        char* utf8Buffer = (char*)SAFE_MALLOC(utf8Size);
        if (utf8Buffer) {
            WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
            
            DWORD bytesWritten;
            WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
            FlushFileBuffers(hLogFile);
            
            SAFE_FREE(utf8Buffer);
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
        char* utf8Buffer = (char*)SAFE_MALLOC(utf8Size);
        if (utf8Buffer) {
            WideCharToMultiByte(CP_UTF8, 0, logEntry, -1, utf8Buffer, utf8Size, NULL, NULL);
            
            DWORD bytesWritten;
            WriteFile(hLogFile, utf8Buffer, utf8Size - 1, &bytesWritten, NULL);
            FlushFileBuffers(hLogFile);
            
            SAFE_FREE(utf8Buffer);
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

// About Dialog Procedure - Simple GNOME-style layout
INT_PTR CALLBACK AboutDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    switch (message) {
        case WM_INITDIALOG: {
            // Set the application icon
            HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
            if (hIcon) {
                SendDlgItemMessageW(hDlg, IDC_ABOUT_ICON, STM_SETICON, (WPARAM)hIcon, 0);
            }
            
            // Set version from header
            SetDlgItemTextW(hDlg, IDC_ABOUT_VERSION, APP_VERSION);
            
            // === DYNAMIC LAYOUT CALCULATION ===
            // Using Microsoft UI Guidelines (Windows 98/2000/XP era) + GNOME font sizing
            
            // Get DPI for scaling calculations
            int dpi = GetWindowDPI(hDlg);
            
            // Microsoft UI Guidelines - convert DLU to pixels at current DPI
            // 1 DLU horizontal = (dialog_base_unit_x / 4) pixels
            // 1 DLU vertical = (dialog_base_unit_y / 8) pixels
            // Standard dialog base units at 96 DPI: 6x13 for "MS Shell Dlg"
            int baseUnitX = MulDiv(6, dpi, 96);  // ~6px at 96 DPI
            int baseUnitY = MulDiv(13, dpi, 96); // ~13px at 96 DPI
            
            // Microsoft spacing standards in DLU converted to pixels
            int dialogMargin = MulDiv(7 * baseUnitX, 1, 4);      // 7 DLU = ~11px margin
            int controlSpacing = MulDiv(4 * baseUnitX, 1, 4);    // 4 DLU = ~6px between controls
            int groupSpacing = MulDiv(7 * baseUnitX, 1, 4);      // 7 DLU = ~11px between groups
            int iconSize = ScaleForDpi(32, dpi);                 // Standard icon size
            int buttonHeight = MulDiv(14 * baseUnitY, 1, 8);     // 14 DLU = ~23px button height
            int bottomPadding = ScaleForDpi(8, dpi);             // Requested 8px bottom padding
            
            // Get device context for text measurements
            HDC hdc = GetDC(hDlg);
            if (!hdc) return TRUE;
            
            // === FONT CREATION (GNOME-style sizing) ===
            HFONT hBaseFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
            HFONT hTitleFont = NULL, hSmallFont = NULL;
            LOGFONTW baseLf;
            
            if (hBaseFont && GetObjectW(hBaseFont, sizeof(baseLf), &baseLf)) {
                // Title font: 133% size (4/3 ratio), bold - GNOME style
                LOGFONTW titleLf = baseLf;
                titleLf.lfHeight = (titleLf.lfHeight * 4) / 3;
                titleLf.lfWeight = FW_BOLD;
                hTitleFont = CreateFontIndirectW(&titleLf);
                
                // Small font: 75% size (3/4 ratio) - GNOME style  
                LOGFONTW smallLf = baseLf;
                smallLf.lfHeight = (smallLf.lfHeight * 3) / 4;
                hSmallFont = CreateFontIndirectW(&smallLf);
            }
            
            // === TEXT MEASUREMENT PHASE ===
            // Measure all text elements to determine required dialog width
            
            struct {
                const wchar_t* text;
                HFONT font;
                SIZE size;
                int controlId;
            } textElements[] = {
                {L"YouTube Cacher", hTitleFont, {0}, IDC_ABOUT_TITLE},
                {APP_VERSION, hBaseFont, {0}, IDC_ABOUT_VERSION}, 
                {L"A YouTube downloader frontend to youtube-dl and yt-dlp.", hBaseFont, {0}, IDC_ABOUT_DESCRIPTION},
                {L"YouTube Cacher on GitHub", hBaseFont, {0}, IDC_ABOUT_GITHUB_LINK},
                {L"Copyright © 2025 Kirn Gill II <segin2005@gmail.com>", hSmallFont, {0}, IDC_ABOUT_COPYRIGHT},
                {L"This program comes with absolutely no warranty.", hSmallFont, {0}, IDC_ABOUT_WARRANTY},
                {L"See the MIT License for details.", hSmallFont, {0}, IDC_ABOUT_LICENSE_LINK}
            };
            
            int maxTextWidth = 0;
            for (int i = 0; i < 7; i++) {
                HFONT hOldFont = NULL;
                if (textElements[i].font) {
                    hOldFont = (HFONT)SelectObject(hdc, textElements[i].font);
                }
                
                GetTextExtentPoint32W(hdc, textElements[i].text, (int)wcslen(textElements[i].text), &textElements[i].size);
                
                if (textElements[i].size.cx > maxTextWidth) {
                    maxTextWidth = textElements[i].size.cx;
                }
                
                if (hOldFont) {
                    SelectObject(hdc, hOldFont);
                }
            }
            
            // === DIALOG WIDTH CALCULATION ===
            // Width = left margin + max text width + right margin
            // Minimum width to accommodate icon and reasonable spacing
            int minDialogWidth = ScaleForDpi(320, dpi);
            int calculatedWidth = (2 * dialogMargin) + maxTextWidth;
            int dialogWidth = max(minDialogWidth, calculatedWidth);
            
            // === VERTICAL LAYOUT CALCULATION ===
            int currentY = dialogMargin;
            
            // 1. Icon positioning (centered horizontally)
            int iconX = (dialogWidth - iconSize) / 2;
            int iconY = currentY;
            currentY += iconSize + controlSpacing;
            
            // 2. Title positioning (centered, with title font height)
            int titleY = currentY;
            int titleHeight = textElements[0].size.cy;
            currentY += titleHeight + controlSpacing;
            
            // 3. Version positioning (centered, with base font height)  
            int versionY = currentY;
            int versionHeight = textElements[1].size.cy;
            currentY += versionHeight + groupSpacing; // Group spacing after version
            
            // 4. Description positioning (centered)
            int descY = currentY;
            int descHeight = textElements[2].size.cy;
            currentY += descHeight + controlSpacing;
            
            // 5. GitHub link positioning (centered)
            int githubY = currentY;
            int githubHeight = textElements[3].size.cy;
            currentY += githubHeight + groupSpacing; // Group spacing before copyright section
            
            // 6. Copyright positioning (centered, small font)
            int copyrightY = currentY;
            int copyrightHeight = textElements[4].size.cy;
            currentY += copyrightHeight + controlSpacing;
            
            // 7. Warranty positioning (centered, small font)
            int warrantyY = currentY;
            int warrantyHeight = textElements[5].size.cy;
            currentY += warrantyHeight + controlSpacing;
            
            // 8. License link positioning (centered, small font)
            int licenseY = currentY;
            int licenseHeight = textElements[6].size.cy;
            currentY += licenseHeight + groupSpacing; // Group spacing before button
            
            // 9. Button positioning (centered)
            int buttonWidth = ScaleForDpi(75, dpi); // Microsoft minimum button width
            int buttonX = (dialogWidth - buttonWidth) / 2;
            int buttonY = currentY;
            currentY += buttonHeight + bottomPadding; // 8px bottom padding as requested
            
            // === DIALOG HEIGHT CALCULATION ===
            int dialogHeight = currentY;
            
            // === APPLY LAYOUT ===
            // Resize dialog to calculated dimensions
            RECT windowRect;
            GetWindowRect(hDlg, &windowRect);
            
            // Calculate window frame size
            RECT clientRect;
            GetClientRect(hDlg, &clientRect);
            int frameWidth = (windowRect.right - windowRect.left) - (clientRect.right - clientRect.left);
            int frameHeight = (windowRect.bottom - windowRect.top) - (clientRect.bottom - clientRect.top);
            
            // Center dialog on screen
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);
            int windowX = (screenWidth - dialogWidth - frameWidth) / 2;
            int windowY = (screenHeight - dialogHeight - frameHeight) / 2;
            
            SetWindowPos(hDlg, NULL, windowX, windowY, 
                        dialogWidth + frameWidth, dialogHeight + frameHeight, 
                        SWP_NOZORDER | SWP_NOACTIVATE);
            
            // Position all controls
            SetWindowPos(GetDlgItem(hDlg, IDC_ABOUT_ICON), NULL, 
                        iconX, iconY, iconSize, iconSize, SWP_NOZORDER);
            
            // Center each text element horizontally
            for (int i = 0; i < 7; i++) {
                HWND hControl = GetDlgItem(hDlg, textElements[i].controlId);
                if (hControl) {
                    int textX = (dialogWidth - textElements[i].size.cx) / 2;
                    int textY;
                    
                    switch (i) {
                        case 0: textY = titleY; break;      // Title
                        case 1: textY = versionY; break;    // Version  
                        case 2: textY = descY; break;       // Description
                        case 3: textY = githubY; break;     // GitHub link
                        case 4: textY = copyrightY; break;  // Copyright
                        case 5: textY = warrantyY; break;   // Warranty
                        case 6: textY = licenseY; break;    // License link
                    }
                    
                    SetWindowPos(hControl, NULL, textX, textY, 
                                textElements[i].size.cx, textElements[i].size.cy, SWP_NOZORDER);
                    
                    // Apply fonts
                    if (textElements[i].font) {
                        SendMessageW(hControl, WM_SETFONT, (WPARAM)textElements[i].font, TRUE);
                    }
                }
            }
            
            // Position Close button
            SetWindowPos(GetDlgItem(hDlg, IDC_ABOUT_CLOSE), NULL,
                        buttonX, buttonY, buttonWidth, buttonHeight, SWP_NOZORDER);
            
            // Store fonts for cleanup
            if (hTitleFont) SetPropW(hDlg, L"TitleFont", hTitleFont);
            if (hSmallFont) SetPropW(hDlg, L"SmallFont", hSmallFont);
            
            ReleaseDC(hDlg, hdc);
            
            // === ACCESSIBILITY ENHANCEMENTS ===
            // Set accessible names for all controls
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_ICON), L"Application icon", NULL);
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_TITLE), L"Application title", NULL);
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_VERSION), L"Version number", NULL);
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_DESCRIPTION), L"Application description", NULL);
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_GITHUB_LINK), L"GitHub repository link", L"Opens the GitHub repository in your web browser");
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_COPYRIGHT), L"Copyright information", NULL);
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_WARRANTY), L"Warranty disclaimer", NULL);
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_LICENSE_LINK), L"License information link", L"Opens the MIT License in your web browser");
            SetControlAccessibility(GetDlgItem(hDlg, IDC_ABOUT_CLOSE), L"Close", L"Close the About dialog");
            
            // === KEYBOARD NAVIGATION ENHANCEMENTS ===
            // Configure tab order for focusable controls
            TabOrderConfig tabConfig;
            TabOrderEntry entries[3];
            
            // Only the links and close button should be in tab order
            entries[0].controlId = IDC_ABOUT_GITHUB_LINK;
            entries[0].tabOrder = 0;
            entries[0].isTabStop = TRUE;
            
            entries[1].controlId = IDC_ABOUT_LICENSE_LINK;
            entries[1].tabOrder = 1;
            entries[1].isTabStop = TRUE;
            
            entries[2].controlId = IDC_ABOUT_CLOSE;
            entries[2].tabOrder = 2;
            entries[2].isTabStop = TRUE;
            
            tabConfig.entries = entries;
            tabConfig.count = 3;
            SetDialogTabOrder(hDlg, &tabConfig);
            
            // Set initial focus to Close button
            SetInitialDialogFocus(hDlg);
            
            return FALSE; // Allow custom focus setting
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_ABOUT_CLOSE:
                case IDOK:
                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            break;
            
        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->code == NM_CLICK || pnmh->code == NM_RETURN) {
                // Handle SysLink clicks for both GitHub and MIT License links
                if (pnmh->idFrom == IDC_ABOUT_GITHUB_LINK || pnmh->idFrom == IDC_ABOUT_LICENSE_LINK) {
                    PNMLINK pNMLink = (PNMLINK)lParam;
                    ShellExecuteW(hDlg, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
                }
            }
            break;
        }
        
        case WM_DESTROY: {
            // Clean up custom fonts
            HFONT hTitleFont = (HFONT)GetPropW(hDlg, L"TitleFont");
            if (hTitleFont) {
                DeleteObject(hTitleFont);
                RemovePropW(hDlg, L"TitleFont");
            }
            
            HFONT hSmallFont = (HFONT)GetPropW(hDlg, L"SmallFont");
            if (hSmallFont) {
                DeleteObject(hSmallFont);
                RemovePropW(hDlg, L"SmallFont");
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
        
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }
    
    return FALSE;
}

// Show About Dialog
void ShowAboutDialog(HWND parent) {
    DialogBoxW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_ABOUT_DIALOG), parent, AboutDialogProc);
}