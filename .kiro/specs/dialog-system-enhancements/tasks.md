# Implementation Plan: Dialog System Enhancements

## Overview
This implementation plan breaks down the dialog system enhancements into discrete, manageable coding tasks. Each task builds incrementally on previous work, focusing on accessibility, keyboard navigation, and reusable UI components while maintaining native Windows look-and-feel.

## Tasks

- [x] 1. Implement accessibility foundation




  - Create accessibility support functions for screen reader integration
  - Implement high contrast mode detection and handling
  - Add accessible names to existing unified dialog controls
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 4.1, 4.2, 4.3, 4.4_

- [x] 1.1 Create accessibility helper functions





  - Write `SetControlAccessibility()` function to set accessible names and descriptions
  - Write `NotifyAccessibilityStateChange()` function for screen reader notifications
  - Write `IsScreenReaderActive()` function to detect active screen readers
  - Add functions to new `accessibility.c` file with corresponding `accessibility.h` header
  - _Requirements: 1.1, 1.2, 1.3_


- [x] 1.2 Implement high contrast mode support

  - Write `IsHighContrastMode()` function using `SystemParametersInfoW`
  - Write `GetHighContrastColor()` function to retrieve system colors
  - Write `ApplyHighContrastColors()` function to update dialog colors
  - Handle `WM_SYSCOLORCHANGE` message in dialog procedures
  - _Requirements: 4.1, 4.2, 4.3, 4.4_

- [x] 1.3 Add accessible names to unified dialog


  - Update `UnifiedDialogProc` in `dialogs.c` to set accessible names in `WM_INITDIALOG`
  - Set accessible names for icon, message, buttons, and tab controls
  - Add accessible descriptions for controls where purpose isn't obvious
  - Test with Windows Narrator to verify announcements
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

- [x] 2. Implement keyboard navigation system





  - Add tab order management for dialog controls
  - Implement accelerator key system with conflict detection
  - Add focus management and visual focus indicators

  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 3.1, 3.2, 3.3, 3.4, 3.5_

- [x] 2.1 Create tab order management system

  - Define `TabOrderEntry` and `TabOrderConfig` structures in `YouTubeCacher.h`
  - Write `SetDialogTabOrder()` function to configure tab order
  - Write `CalculateTabOrder()` function for automatic logical ordering
  - Write `FreeTabOrderConfig()` cleanup function
  - Add to new `keyboard.c` file with corresponding `keyboard.h` header
  - _Requirements: 2.1, 2.2, 2.3, 2.4_


- [x] 2.2 Implement accelerator key system

  - Add accelerator text fields to `UnifiedDialogConfig` structure
  - Write `ValidateAcceleratorKeys()` function to check for conflicts
  - Write `GetAcceleratorChar()` function to extract accelerator from label
  - Write `HasAcceleratorConflict()` function to detect duplicates
  - Update dialog button creation to use ampersand notation
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_


- [x] 2.3 Add default accelerator keys to unified dialog

  - Update unified dialog to use "&Details >>" for Details button (Alt+D)
  - Update unified dialog to use "&Copy" for Copy button (Alt+C)
  - Update unified dialog to use "&OK" for OK button (Alt+O)
  - Ensure Escape key closes dialog (already implemented, verify)
  - Ensure Enter key activates default button (already implemented, verify)
  - _Requirements: 2.5, 2.6, 2.7, 3.1, 3.2, 3.3, 3.4, 3.5_


- [x] 2.4 Implement focus management

  - Write `SetInitialDialogFocus()` function to set focus to logical first control
  - Write `GetNextFocusableControl()` function for custom focus navigation
  - Write `EnsureFocusVisible()` function to ensure focus indicators are visible
  - Update `WM_INITDIALOG` handlers to set proper initial focus
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 7.1, 7.2, 7.3, 7.4, 7.5_

- [x] 3. Create reusable UI component system




  - Implement base component architecture
  - Create file browser component
  - Create folder browser component
  - Create labeled text input component
  - Create button row component
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7_


- [x] 3.1 Implement base component architecture

  - Define `UIComponent` base structure in `YouTubeCacher.h`
  - Define component function pointer types (init, destroy, validate, getValue, setValue)
  - Create new `components.c` file with corresponding `components.h` header
  - Write component registry functions (create, register, unregister, destroy)
  - _Requirements: 6.1, 6.6, 6.7_


- [x] 3.2 Create file browser component

  - Define `FileBrowserComponent` structure
  - Write `CreateFileBrowser()` function to create label, edit, and button controls
  - Write `DestroyFileBrowser()` function for cleanup
  - Write `ValidateFileBrowser()` function to check file exists
  - Write `GetFileBrowserPath()` and `SetFileBrowserPath()` functions
  - Implement browse button handler using `GetOpenFileNameW`
  - _Requirements: 6.1, 6.2, 6.6, 6.7_


- [x] 3.3 Create folder browser component

  - Define `FolderBrowserComponent` structure
  - Write `CreateFolderBrowser()` function to create label, edit, and button controls
  - Write `DestroyFolderBrowser()` function for cleanup
  - Write `ValidateFolderBrowser()` function to check folder exists
  - Write `GetFolderBrowserPath()` and `SetFolderBrowserPath()` functions
  - Implement browse button handler using `SHBrowseForFolderW`
  - _Requirements: 6.1, 6.2, 6.6, 6.7_


- [x] 3.4 Create labeled text input component

  - Define `LabeledTextInput` structure with validation support
  - Define `ValidationType` enum (none, required, numeric, path, URL, custom)
  - Write `CreateLabeledTextInput()` function to create label, edit, and error controls
  - Write `DestroyLabeledTextInput()` function for cleanup
  - Write `ValidateLabeledTextInput()` function with built-in validation types
  - Write `GetTextInputValue()` and `SetTextInputValue()` functions
  - Write `SetCustomValidator()` function for custom validation
  - Implement visual error feedback (red border, error message)
  - _Requirements: 6.1, 6.3, 6.6, 6.7, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_

- [x] 3.5 Create button row component


  - Define `ButtonRowComponent` structure and `ButtonLayout` enum
  - Define `ButtonConfig` structure for custom button configurations
  - Write `CreateButtonRow()` function for standard layouts (OK, OK/Cancel, etc.)
  - Write `CreateCustomButtonRow()` function for custom button sets
  - Write `DestroyButtonRow()` function for cleanup
  - Implement proper button spacing and alignment (right-aligned, 6px spacing)
  - _Requirements: 6.1, 6.5, 6.6, 6.7_

- [x] 4. Implement validation framework





  - Create validation system for component validation
  - Add visual feedback for validation errors
  - Integrate validation with dialog submission
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_


- [x] 4.1 Create validation system

  - Define `ValidationResult` and `ValidationSummary` structures in `YouTubeCacher.h`
  - Write `ValidateDialog()` function to validate all components
  - Write `ShowValidationErrors()` function to display errors near controls
  - Write `FreeValidationSummary()` cleanup function
  - Write `ValidateComponent()` function for single component validation
  - Add to `components.c` file
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_



- [x] 4.2 Add visual validation feedback
  - Implement red border drawing for invalid controls
  - Implement error message display near invalid controls
  - Use `WM_PAINT` handler for custom border drawing
  - Use static text control with red color for error messages
  - _Requirements: 8.4, 8.5_



- [x] 4.3 Integrate validation with dialogs
  - Update dialog OK/Apply button handlers to call validation
  - Prevent dialog close if validation fails
  - Set focus to first invalid control on validation failure
  - Show validation error summary if multiple errors
  - _Requirements: 8.5, 8.6_

- [x] 5. Update existing dialogs with enhancements





  - Apply accessibility features to all existing dialogs
  - Add keyboard navigation to all existing dialogs
  - Update Settings dialog to use reusable components
  - _Requirements: All requirements_

- [x] 5.1 Update unified dialog with accessibility


  - Apply all accessibility features to `UnifiedDialogProc`
  - Set accessible names for all controls
  - Add screen reader notifications for state changes
  - Ensure high contrast mode support
  - Test with Windows Narrator and NVDA
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 4.1, 4.2, 4.3, 4.4_

- [x] 5.2 Update unified dialog with keyboard navigation


  - Configure proper tab order for all controls
  - Add accelerator keys to all buttons
  - Ensure focus indicators are visible
  - Test keyboard-only navigation
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 3.1, 3.2, 3.3, 3.4, 3.5, 7.1, 7.2, 7.3, 7.4, 7.5_


- [x] 5.3 Update Settings dialog with reusable components




  - Replace yt-dlp path controls with `FileBrowserComponent`
  - Replace download folder controls with `FolderBrowserComponent`
  - Add validation to all input fields
  - Test component functionality in Settings dialog
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_



- [ ] 5.4 Update About dialog with enhancements
  - Add accessible names to all controls
  - Configure tab order
  - Add accelerator keys
  - Test accessibility and keyboard navigation
  - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 3.3, 3.4, 3.5_

- [ ] 6. Update Makefile and build system
  - Add new source files to Makefile
  - Update dependency tracking
  - Ensure clean builds
  - _Requirements: All requirements_

- [ ] 6.1 Add new source files to Makefile
  - Add `accessibility.c` to SOURCES
  - Add `keyboard.c` to SOURCES
  - Add `components.c` to SOURCES
  - Update object file lists
  - _Requirements: All requirements_

- [ ] 6.2 Update dependency tracking
  - Add dependencies for `accessibility.o` (accessibility.c, accessibility.h, YouTubeCacher.h)
  - Add dependencies for `keyboard.o` (keyboard.c, keyboard.h, YouTubeCacher.h)
  - Add dependencies for `components.o` (components.c, components.h, YouTubeCacher.h)
  - Update existing dependencies if needed
  - _Requirements: All requirements_

- [ ]* 7. Comprehensive testing and validation
  - Test accessibility with screen readers
  - Test keyboard navigation
  - Test reusable components
  - Test validation framework
  - Test high contrast mode
  - _Requirements: All requirements_

- [ ]* 7.1 Test accessibility features
  - Test with Windows Narrator
  - Test with NVDA screen reader
  - Verify all controls are announced
  - Verify state changes are announced
  - Test high contrast mode (black, white, high contrast #1, high contrast #2)
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 4.1, 4.2, 4.3, 4.4_

- [ ]* 7.2 Test keyboard navigation
  - Test tab order in all dialogs
  - Test all accelerator keys
  - Test Escape and Enter keys
  - Test focus indicators visibility
  - Test keyboard-only workflow (no mouse)
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 3.1, 3.2, 3.3, 3.4, 3.5, 7.1, 7.2, 7.3, 7.4, 7.5_

- [ ]* 7.3 Test reusable components
  - Test file browser component (browse, validate, get/set path)
  - Test folder browser component (browse, validate, get/set path)
  - Test labeled text input (all validation types, error display)
  - Test button row component (all layouts, custom configurations)
  - Test component cleanup (no memory leaks)
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7_

- [ ]* 7.4 Test validation framework
  - Test validation with valid data
  - Test validation with invalid data
  - Test validation error display
  - Test focus movement to invalid control
  - Test validation summary for multiple errors
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_

- [ ]* 7.5 Test HiDPI scaling
  - Test all components at 96 DPI (100%)
  - Test all components at 120 DPI (125%)
  - Test all components at 144 DPI (150%)
  - Test all components at 192 DPI (200%)
  - Verify proper scaling and layout
  - _Requirements: All requirements_
