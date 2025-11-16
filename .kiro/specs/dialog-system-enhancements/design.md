# Design Document: Dialog System Enhancements

## Overview

This design document outlines enhancements to the YouTubeCacher unified dialog system to improve accessibility, keyboard navigation, and usability. The current system provides a solid foundation with `UnifiedDialogConfig` and dynamic layout capabilities. We will extend this system to support comprehensive accessibility features, proper keyboard navigation, and reusable UI components while maintaining native Windows look-and-feel.

## Architecture

### Current System Analysis

The existing dialog system consists of:

1. **UnifiedDialogConfig**: Configuration structure for dialog creation
2. **UnifiedDialogProc**: Single dialog procedure handling all dialog types
3. **IDD_UNIFIED_DIALOG**: Single dialog resource with dynamic layout
4. **Dynamic Sizing**: HiDPI-aware layout calculations following Microsoft UI Guidelines
5. **Tab System**: Expandable details with up to 3 tabs

**Strengths**:
- Unified approach reduces code duplication
- HiDPI support with proper DPI scaling
- Dynamic layout following Windows UI standards
- Native Windows controls

**Areas for Enhancement**:
- Limited accessibility support (no screen reader annotations)
- Incomplete keyboard navigation (missing accelerator keys)
- No tab order management
- No validation framework for reusable components

### Enhanced Architecture

We will extend the current system with:

1. **Accessibility Layer**: Screen reader support and high contrast mode handling
2. **Keyboard Navigation System**: Tab order management and accelerator keys
3. **Reusable Component Library**: Common UI patterns as configurable components
4. **Validation Framework**: Input validation with visual feedback

## Components and Interfaces

### 1. Accessibility Support

#### 1.1 Screen Reader Integration

**New Functions**:
```c
// Set accessible name and description for a control
void SetControlAccessibility(HWND hwnd, const wchar_t* name, const wchar_t* description);

// Notify screen readers of state changes
void NotifyAccessibilityStateChange(HWND hwnd, DWORD event);

// Check if screen reader is active
BOOL IsScreenReaderActive(void);
```

**Implementation Approach**:
- Use `IAccessible` COM interface for screen reader support
- Leverage `NotifyWinEvent` for state change notifications
- Set `WS_EX_CONTROLPARENT` style for proper accessibility tree
- Use `SetWindowTextW` for accessible names on controls

**Integration Points**:
- `WM_INITDIALOG`: Set accessible names for all controls
- Button/checkbox state changes: Notify screen readers
- Tab changes: Announce new tab content

#### 1.2 High Contrast Mode Support

**New Functions**:
```c
// Check if high contrast mode is enabled
BOOL IsHighContrastMode(void);

// Get system color for high contrast
COLORREF GetHighContrastColor(int colorType);

// Update dialog colors for high contrast
void ApplyHighContrastColors(HWND hDlg);
```

**Implementation Approach**:
- Use `SystemParametersInfoW` with `SPI_GETHIGHCONTRAST`
- Handle `WM_SYSCOLORCHANGE` message to detect theme changes
- Use `GetSysColor` for all color values (already native controls)
- No custom drawing needed - native controls handle this automatically

### 2. Keyboard Navigation System

#### 2.1 Tab Order Management

**New Structure**:
```c
typedef struct {
    int controlId;
    int tabOrder;
    BOOL isTabStop;
} TabOrderEntry;

typedef struct {
    TabOrderEntry* entries;
    int count;
} TabOrderConfig;
```

**New Functions**:
```c
// Set tab order for dialog controls
void SetDialogTabOrder(HWND hDlg, const TabOrderConfig* config);

// Automatically calculate logical tab order
TabOrderConfig* CalculateTabOrder(HWND hDlg);

// Free tab order configuration
void FreeTabOrderConfig(TabOrderConfig* config);
```

**Implementation Approach**:
- Use `SetWindowPos` with `HWND_TOP` and proper Z-order
- Set `WS_TABSTOP` style for focusable controls
- Remove `WS_TABSTOP` from static controls
- Ensure logical flow: top-to-bottom, left-to-right

#### 2.2 Accelerator Key System

**Enhanced UnifiedDialogConfig**:
```c
typedef struct {
    // ... existing fields ...
    
    // Accelerator key definitions (optional)
    const wchar_t* detailsButtonText;  // e.g., "&Details >>" for Alt+D
    const wchar_t* copyButtonText;     // e.g., "&Copy" for Alt+C
    const wchar_t* okButtonText;       // e.g., "&OK" for Alt+O
} UnifiedDialogConfig;
```

**New Functions**:
```c
// Validate accelerator keys don't conflict
BOOL ValidateAcceleratorKeys(HWND hDlg);

// Extract accelerator character from label
wchar_t GetAcceleratorChar(const wchar_t* label);

// Check for accelerator conflicts
BOOL HasAcceleratorConflict(HWND hDlg, wchar_t accelChar);
```

**Implementation Approach**:
- Use ampersand (&) in button text for accelerators
- Windows automatically handles Alt+Key processing
- Validate no duplicate accelerators in same dialog
- Default accelerators: D=Details, C=Copy, O=OK, Esc=Cancel

#### 2.3 Focus Management

**New Functions**:
```c
// Set initial focus to logical first control
void SetInitialDialogFocus(HWND hDlg);

// Get next/previous focusable control
HWND GetNextFocusableControl(HWND hDlg, HWND hCurrent, BOOL forward);

// Ensure focus indicator is visible
void EnsureFocusVisible(HWND hwnd);
```

**Implementation Approach**:
- Return FALSE from `WM_INITDIALOG` to set custom focus
- Use `SetFocus` to set initial focus to OK button
- Handle `WM_NEXTDLGCTL` for custom focus navigation
- Ensure focus rectangles are drawn (native controls handle this)

### 3. Reusable UI Components

#### 3.1 Component Architecture

**Base Component Structure**:
```c
typedef struct UIComponent UIComponent;

typedef void (*ComponentInitFunc)(UIComponent* component, HWND parent, int x, int y);
typedef void (*ComponentDestroyFunc)(UIComponent* component);
typedef BOOL (*ComponentValidateFunc)(UIComponent* component, wchar_t* errorMsg, size_t errorMsgSize);
typedef void (*ComponentGetValueFunc)(UIComponent* component, void* value);
typedef void (*ComponentSetValueFunc)(UIComponent* component, const void* value);

struct UIComponent {
    HWND hwndContainer;
    HWND* childControls;
    int childCount;
    
    ComponentInitFunc init;
    ComponentDestroyFunc destroy;
    ComponentValidateFunc validate;
    ComponentGetValueFunc getValue;
    ComponentSetValueFunc setValue;
    
    void* userData;
};
```

#### 3.2 File Browser Component

**Structure**:
```c
typedef struct {
    UIComponent base;
    HWND hwndLabel;
    HWND hwndEdit;
    HWND hwndButton;
    wchar_t* label;
    wchar_t* filter;  // e.g., L"Executables\0*.exe\0All Files\0*.*\0"
    wchar_t* currentPath;
} FileBrowserComponent;
```

**Functions**:
```c
FileBrowserComponent* CreateFileBrowser(HWND parent, int x, int y, int width, 
                                        const wchar_t* label, const wchar_t* filter);
void DestroyFileBrowser(FileBrowserComponent* component);
BOOL ValidateFileBrowser(FileBrowserComponent* component, wchar_t* errorMsg, size_t errorMsgSize);
const wchar_t* GetFileBrowserPath(FileBrowserComponent* component);
void SetFileBrowserPath(FileBrowserComponent* component, const wchar_t* path);
```

**Layout**:
```
[Label Text                    ]
[Text Input Field      ] [Browse...]
```

**Implementation Details**:
- Label: Static text control with `SS_LEFT` style
- Edit: Edit control with `ES_AUTOHSCROLL` and `ES_READONLY` styles
- Button: Push button with "Browse..." text
- Button click: Opens `GetOpenFileNameW` dialog
- Validation: Check file exists using `GetFileAttributesW`

#### 3.3 Folder Browser Component

**Structure**:
```c
typedef struct {
    UIComponent base;
    HWND hwndLabel;
    HWND hwndEdit;
    HWND hwndButton;
    wchar_t* label;
    wchar_t* currentPath;
} FolderBrowserComponent;
```

**Functions**:
```c
FolderBrowserComponent* CreateFolderBrowser(HWND parent, int x, int y, int width,
                                            const wchar_t* label);
void DestroyFolderBrowser(FolderBrowserComponent* component);
BOOL ValidateFolderBrowser(FolderBrowserComponent* component, wchar_t* errorMsg, size_t errorMsgSize);
const wchar_t* GetFolderBrowserPath(FolderBrowserComponent* component);
void SetFolderBrowserPath(FolderBrowserComponent* component, const wchar_t* path);
```

**Implementation Details**:
- Similar to FileBrowserComponent
- Button click: Opens `SHBrowseForFolderW` dialog
- Validation: Check directory exists using `GetFileAttributesW` with `FILE_ATTRIBUTE_DIRECTORY`

#### 3.4 Labeled Text Input Component

**Structure**:
```c
typedef enum {
    VALIDATION_NONE,
    VALIDATION_REQUIRED,
    VALIDATION_NUMERIC,
    VALIDATION_PATH,
    VALIDATION_URL,
    VALIDATION_CUSTOM
} ValidationType;

typedef BOOL (*CustomValidationFunc)(const wchar_t* value, wchar_t* errorMsg, size_t errorMsgSize);

typedef struct {
    UIComponent base;
    HWND hwndLabel;
    HWND hwndEdit;
    HWND hwndError;
    wchar_t* label;
    ValidationType validationType;
    CustomValidationFunc customValidator;
    BOOL isRequired;
} LabeledTextInput;
```

**Functions**:
```c
LabeledTextInput* CreateLabeledTextInput(HWND parent, int x, int y, int width,
                                         const wchar_t* label, ValidationType validation);
void DestroyLabeledTextInput(LabeledTextInput* component);
BOOL ValidateLabeledTextInput(LabeledTextInput* component, wchar_t* errorMsg, size_t errorMsgSize);
const wchar_t* GetTextInputValue(LabeledTextInput* component);
void SetTextInputValue(LabeledTextInput* component, const wchar_t* value);
void SetCustomValidator(LabeledTextInput* component, CustomValidationFunc validator);
```

**Layout**:
```
[Label Text                    ]
[Text Input Field              ]
[Error message if validation fails]
```

**Implementation Details**:
- Error label: Static text with red color (using `SetTextColor` in `WM_CTLCOLORSTATIC`)
- Validation on focus loss (`WM_KILLFOCUS`)
- Visual feedback: Red border on error (custom `WM_PAINT` handler)

#### 3.5 Button Row Component

**Structure**:
```c
typedef enum {
    BUTTON_LAYOUT_OK,
    BUTTON_LAYOUT_OK_CANCEL,
    BUTTON_LAYOUT_YES_NO,
    BUTTON_LAYOUT_YES_NO_CANCEL,
    BUTTON_LAYOUT_CUSTOM
} ButtonLayout;

typedef struct {
    const wchar_t* text;
    int commandId;
    BOOL isDefault;
} ButtonConfig;

typedef struct {
    UIComponent base;
    HWND* buttons;
    int buttonCount;
    ButtonLayout layout;
} ButtonRowComponent;
```

**Functions**:
```c
ButtonRowComponent* CreateButtonRow(HWND parent, int x, int y, int width, ButtonLayout layout);
ButtonRowComponent* CreateCustomButtonRow(HWND parent, int x, int y, int width,
                                          const ButtonConfig* buttons, int count);
void DestroyButtonRow(ButtonRowComponent* component);
```

**Layout**:
```
[Button 1] [Button 2] [Button 3]  (right-aligned)
```

**Implementation Details**:
- Buttons right-aligned with 6px spacing (Microsoft standard)
- Default button has `BS_DEFPUSHBUTTON` style
- Standard button width: 75px (50 DLU)
- Standard button height: 23px (14 DLU)

### 4. Validation Framework

#### 4.1 Validation System

**Structure**:
```c
typedef struct {
    UIComponent* component;
    BOOL isValid;
    wchar_t errorMessage[256];
} ValidationResult;

typedef struct {
    ValidationResult* results;
    int count;
    BOOL allValid;
} ValidationSummary;
```

**Functions**:
```c
// Validate all components in a dialog
ValidationSummary* ValidateDialog(UIComponent** components, int count);

// Show validation errors
void ShowValidationErrors(HWND hDlg, ValidationSummary* summary);

// Free validation summary
void FreeValidationSummary(ValidationSummary* summary);

// Validate single component
BOOL ValidateComponent(UIComponent* component, wchar_t* errorMsg, size_t errorMsgSize);
```

**Implementation Approach**:
- Call validation on dialog OK/Apply button
- Prevent dialog close if validation fails
- Show error message near invalid control
- Set focus to first invalid control

## Data Models

### Enhanced Dialog Configuration

```c
typedef struct {
    UnifiedDialogType dialogType;
    const wchar_t* title;
    const wchar_t* message;
    const wchar_t* details;
    
    // Tab configuration
    const wchar_t* tab1_name;
    const wchar_t* tab2_content;
    const wchar_t* tab2_name;
    const wchar_t* tab3_content;
    const wchar_t* tab3_name;
    
    // Button configuration
    BOOL showDetailsButton;
    BOOL showCopyButton;
    const wchar_t* detailsButtonText;  // With accelerator
    const wchar_t* copyButtonText;     // With accelerator
    const wchar_t* okButtonText;       // With accelerator
    
    // Accessibility configuration
    const wchar_t* accessibleDescription;
    BOOL announceToScreenReader;
    
    // Tab order configuration (optional)
    TabOrderConfig* tabOrder;
} UnifiedDialogConfig;
```

### Component Registry

```c
typedef struct {
    UIComponent** components;
    int count;
    int capacity;
} ComponentRegistry;

// Global registry for dialog components
ComponentRegistry* CreateComponentRegistry(void);
void RegisterComponent(ComponentRegistry* registry, UIComponent* component);
void UnregisterComponent(ComponentRegistry* registry, UIComponent* component);
void DestroyComponentRegistry(ComponentRegistry* registry);
```

## Error Handling

### Validation Errors

- **Display**: Show error message near invalid control
- **Focus**: Set focus to first invalid control
- **Visual**: Red border around invalid control
- **Accessibility**: Announce error to screen readers

### Component Creation Errors

- **Memory Allocation**: Return NULL on failure, caller checks
- **Control Creation**: Clean up partial components on failure
- **Logging**: Log component creation failures for debugging

### Accessibility Errors

- **Screen Reader Unavailable**: Gracefully degrade, continue without
- **COM Initialization**: Log warning, continue without accessibility features
- **Notification Failures**: Log but don't block UI operations

## Testing Strategy

### Accessibility Testing

1. **Screen Reader Testing**:
   - Test with NVDA (free, open-source)
   - Test with Windows Narrator
   - Verify all controls are announced
   - Verify state changes are announced

2. **High Contrast Testing**:
   - Enable Windows High Contrast mode
   - Verify all text is readable
   - Verify focus indicators are visible
   - Test all high contrast themes (black, white, etc.)

3. **Keyboard Navigation Testing**:
   - Tab through all controls
   - Verify tab order is logical
   - Test all accelerator keys
   - Test Escape and Enter keys
   - Verify focus indicators are visible

### Component Testing

1. **File Browser Component**:
   - Test browse button opens file dialog
   - Test path validation (file exists)
   - Test path validation (file doesn't exist)
   - Test empty path validation
   - Test HiDPI scaling

2. **Folder Browser Component**:
   - Test browse button opens folder dialog
   - Test path validation (folder exists)
   - Test path validation (folder doesn't exist)
   - Test empty path validation
   - Test HiDPI scaling

3. **Labeled Text Input**:
   - Test required field validation
   - Test numeric validation
   - Test path validation
   - Test URL validation
   - Test custom validation
   - Test error message display
   - Test HiDPI scaling

4. **Button Row Component**:
   - Test all standard layouts
   - Test custom button configurations
   - Test default button behavior
   - Test button spacing
   - Test HiDPI scaling

### Integration Testing

1. **Dialog Creation**:
   - Create dialog with all component types
   - Verify layout is correct
   - Verify tab order is logical
   - Verify accelerator keys work

2. **Validation Flow**:
   - Submit dialog with invalid data
   - Verify validation errors are shown
   - Verify focus moves to invalid control
   - Fix errors and resubmit
   - Verify dialog closes on valid data

3. **DPI Changes**:
   - Create dialog at 96 DPI
   - Move to monitor with 144 DPI
   - Verify all components scale correctly
   - Verify layout remains correct

## Implementation Notes

### Windows API Usage

- **Accessibility**: Use `IAccessible` COM interface and `NotifyWinEvent`
- **High Contrast**: Use `SystemParametersInfoW` with `SPI_GETHIGHCONTRAST`
- **Tab Order**: Use `SetWindowPos` with proper Z-order
- **Accelerators**: Use ampersand (&) in button text
- **Focus**: Use `SetFocus` and handle `WM_NEXTDLGCTL`
- **Validation**: Custom logic, no specific Windows API

### HiDPI Considerations

- All component dimensions must use `ScaleForDpi` function
- All spacing and margins must scale with DPI
- Font sizes must scale with DPI
- Components must handle `WM_DPICHANGED` message
- Use existing `GetDpiForWindowSafe` function

### Memory Management

- All components allocate memory with `SAFE_MALLOC`
- All components free memory with `SAFE_FREE`
- Component registry manages component lifecycle
- Caller responsible for destroying components
- No memory leaks on dialog close

### Thread Safety

- Dialog system runs on main UI thread only
- No thread safety concerns for dialog operations
- Components are not thread-safe (not needed)
- Validation runs synchronously on UI thread

## Migration Path

### Phase 1: Accessibility Foundation
1. Implement screen reader support functions
2. Add accessible names to existing dialogs
3. Implement high contrast mode detection
4. Test with screen readers

### Phase 2: Keyboard Navigation
1. Implement tab order management
2. Add accelerator keys to existing dialogs
3. Implement focus management
4. Test keyboard-only navigation

### Phase 3: Reusable Components
1. Implement base component architecture
2. Implement file browser component
3. Implement folder browser component
4. Implement labeled text input component
5. Implement button row component

### Phase 4: Validation Framework
1. Implement validation system
2. Integrate with components
3. Add visual feedback for errors
4. Test validation flows

### Phase 5: Integration and Testing
1. Update existing dialogs to use new features
2. Comprehensive accessibility testing
3. Comprehensive keyboard navigation testing
4. HiDPI testing at multiple scales
5. Performance testing

## Performance Considerations

- **Component Creation**: Minimal overhead, native controls
- **Validation**: Synchronous, fast for typical inputs
- **Accessibility**: Minimal overhead when screen reader not active
- **Memory**: Small per-component overhead (< 1KB each)
- **Layout Calculations**: Already optimized in existing system

## Security Considerations

- **Path Validation**: Validate all file/folder paths before use
- **Input Sanitization**: Validate all text inputs
- **Buffer Overflows**: Use safe string functions (`wcsncpy`, `swprintf` with size)
- **COM Security**: Use proper COM initialization and cleanup
- **No Injection**: No command execution from user input in components
