# Implementation Plan: HiDPI Support

## Overview
This implementation plan breaks down comprehensive HiDPI support into discrete, manageable coding tasks. Each task builds incrementally on the existing basic DPI scaling, adding per-monitor DPI awareness v2, dynamic DPI change handling, icon scaling, font scaling, and proper window position persistence.

## Tasks

- [x] 1. Update application manifest for per-monitor DPI awareness




  - Update `YouTubeCacher.manifest` with per-monitor DPI awareness v2 settings
  - Test manifest embedding via resource file
  - Verify DPI awareness is properly declared
  - _Requirements: 1.1, 1.2, 1.3_

- [x] 1.1 Update manifest file


  - Open `YouTubeCacher.manifest`
  - Add `<dpiAware>true/pm</dpiAware>` for backward compatibility
  - Add `<dpiAwareness>permonitorv2</dpiAwareness>` for Windows 10 1703+
  - Keep existing `longPathAware` and other settings
  - Verify XML is well-formed
  - _Requirements: 1.1, 1.2, 1.3_


- [x] 1.2 Verify manifest embedding


  - Build the application
  - Use `mt.exe` or resource viewer to verify manifest is embedded
  - Check that DPI awareness settings are present in embedded manifest
  - Test on Windows 10 to verify per-monitor awareness is active
  - _Requirements: 1.1, 1.2, 1.3_

- [x] 2. Implement centralized DPI management system





  - Create DPI context structures and management functions
  - Implement enhanced DPI detection with fallbacks
  - Create coordinate conversion functions

  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6_

- [x] 2.1 Create DPI management structures

  - Define `DPIContext` structure in `YouTubeCacher.h`
  - Define `DPIManager` structure in `YouTubeCacher.h`
  - Create new `dpi.c` file with corresponding `dpi.h` header
  - Add global `g_dpiManager` variable
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6_



- [x] 2.2 Implement DPI manager functions

  - Write `CreateDPIManager()` function to initialize manager
  - Write `DestroyDPIManager()` function for cleanup
  - Write `RegisterWindowForDPI()` function to track windows
  - Write `UnregisterWindowForDPI()` function to untrack windows
  - Write `GetDPIContext()` function to retrieve context for window

  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6_

- [x] 2.3 Implement enhanced DPI detection

  - Write `GetWindowDPI()` function using `GetDpiForWindow` with fallback
  - Write `GetMonitorDPI()` function using `GetDpiForMonitor` with fallback
  - Write `GetDPIForPoint()` function to get DPI for screen location
  - Write `GetWindowScaleFactor()` function to calculate scale factor
  - Replace existing `GetDpiForWindowSafe()` calls with new functions
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7_


- [-] 2.4 Implement coordinate conversion functions

  - Write `LogicalToPhysical()` function to convert single values
  - Write `PhysicalToLogical()` function to convert single values
  - Write `LogicalRectToPhysical()` function to convert rectangles
  - Write `PhysicalRectToLogical()` function to convert rectangles
  - Write `ScaleValueForDPI()` function (enhanced version of existing `ScaleForDpi`)
  - Write `ScaleValueForDPIFloat()` function for floating-point scaling
  - _Requirements: 7.1, 7.2, 7.3, 7.4_

- [x] 3. Implement enhanced DPI awareness initialization





  - Replace `SetProcessDPIAware()` with enhanced initialization
  - Support per-monitor DPI awareness v2 with fallbacks
  - Initialize global DPI manager
  - _Requirements: 1.1, 1.2, 1.3_


- [x] 3.1 Create enhanced DPI initialization function

  - Write `InitializeDPIAwareness()` function in `dpi.c`
  - Try `SetProcessDpiAwarenessContext` with `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`
  - Fall back to `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE` if v2 fails
  - Fall back to `SetProcessDpiAwareness` (Windows 8.1) if context fails
  - Fall back to `SetProcessDPIAware` (Windows Vista/7) as last resort
  - _Requirements: 1.1, 1.2, 1.3_



- [x] 3.2 Update main.c initialization
  - Replace `SetProcessDPIAware()` call with `InitializeDPIAwareness()`
  - Call `CreateDPIManager()` after DPI awareness initialization
  - Store global DPI manager pointer
  - Add cleanup call to `DestroyDPIManager()` on exit
  - _Requirements: 1.1, 1.2, 1.3_

- [x] 4. Implement dynamic DPI change handling




  - Add WM_DPICHANGED message handler
  - Implement window rescaling function
  - Update all window procedures to handle DPI changes
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_

- [x] 4.1 Implement WM_DPICHANGED handler


  - Add `WM_DPICHANGED` case to `MainWindowProc` in main window
  - Extract new DPI from `wParam`
  - Extract suggested window rect from `lParam`
  - Update DPI context with new DPI
  - Call rescaling function
  - Apply suggested window position and size
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_


- [x] 4.2 Implement window rescaling function

  - Write `RescaleWindowForDPI()` function in `dpi.c`
  - Calculate scale ratio between old and new DPI
  - Enumerate all child controls
  - Scale position and size of each control
  - Call font rescaling function
  - Call icon reloading function
  - Force window redraw
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_


- [x] 4.3 Add WM_DPICHANGED to dialog procedures

  - Add `WM_DPICHANGED` handler to `UnifiedDialogProc`
  - Add `WM_DPICHANGED` handler to `EnhancedErrorDialogProc`
  - Add `WM_DPICHANGED` handler to `SettingsDialogProc`
  - Add `WM_DPICHANGED` handler to `AboutDialogProc`
  - Ensure all dialogs rescale properly on DPI change
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_

- [-] 5. Implement font scaling system



  - Create font management structures
  - Implement scalable font creation
  - Add font rescaling for DPI changes
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_


- [x] 5.1 Create font management structures

  - Define `ScalableFont` structure in `YouTubeCacher.h`
  - Define `FontManager` structure in `YouTubeCacher.h`
  - Add font management functions to `dpi.c`
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_


- [x] 5.2 Implement font manager functions

  - Write `CreateFontManager()` function to initialize manager
  - Write `DestroyFontManager()` function for cleanup
  - Write `CreateScalableFont()` function to create font at base DPI
  - Write `DestroyScalableFont()` function to clean up font
  - Write `GetFontForDPI()` function to get/create font for specific DPI
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_


- [x] 5.3 Implement font rescaling

  - Write `RescaleFontsForDPI()` function to update all fonts in window
  - Write `SetControlFont()` function to apply font to control
  - Enumerate all controls and update fonts
  - Use `WM_SETFONT` message to apply fonts
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_


- [x] 5.4 Integrate font scaling with windows
  - Create scalable fonts for main window controls
  - Create scalable fonts for dialog controls
  - Update font creation in `WM_CREATE` and `WM_INITDIALOG`
  - Call font rescaling in `WM_DPICHANGED` handlers
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_

- [x] 6. Implement icon scaling system






  - Create icon management structures
  - Implement icon loading for DPI
  - Add icon reloading for DPI changes
  - Create multiple icon sizes in resources
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_



- [x] 6.1 Create icon management structures


  - Define `IconSize` structure in `YouTubeCacher.h`
  - Define `ScalableIcon` structure in `YouTubeCacher.h`
  - Define `IconManager` structure in `YouTubeCacher.h`
  - Add icon management functions to `dpi.c`
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_



- [x] 6.2 Implement icon manager functions

  - Write `CreateIconManager()` function to initialize manager
  - Write `DestroyIconManager()` function for cleanup
  - Write `LoadIconForDPI()` function to load appropriate icon size
  - Write `GetIconSizeForDPI()` function to calculate icon size
  - Write `SetControlIcon()` function to apply icon to control
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_


- [x] 6.3 Implement icon reloading

  - Write `ReloadIconsForDPI()` function to update all icons in window
  - Enumerate controls with icons (static controls with SS_ICON style)
  - Load new icon at appropriate size
  - Apply icon using `STM_SETICON` message
  - Destroy old icon to prevent leaks
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [x] 6.4 Create multiple icon sizes


  - Create 16x16 icon resources (96 DPI baseline)
  - Create 20x20 icon resources (120 DPI, 125% scaling)
  - Create 24x24 icon resources (144 DPI, 150% scaling)
  - Create 32x32 icon resources (192 DPI, 200% scaling)
  - Create 48x48 icon resources (288 DPI, 300% scaling)
  - Add icon resources to `YouTubeCacher.rc`
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_


- [x] 6.5 Integrate icon scaling with windows

  - Update icon loading in main window to use `LoadIconForDPI()`
  - Update icon loading in dialogs to use `LoadIconForDPI()`
  - Call icon reloading in `WM_DPICHANGED` handlers
  - Test icon quality at different DPI settings
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [x] 7. Implement window position and size persistence






  - Create logical coordinate storage functions
  - Implement screen bounds checking
  - Update registry functions for logical coordinates

  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_


- [x] 7.1 Implement logical coordinate storage

  - Write `SaveWindowPositionLogical()` function in `dpi.c`
  - Convert physical window rect to logical coordinates
  - Save logical coordinates to registry
  - Save base DPI (96) to registry
  - _Requirements: 9.1, 9.2, 9.3_


- [x] 7.2 Implement logical coordinate restoration

  - Write `RestoreWindowPositionLogical()` function in `dpi.c`
  - Load logical coordinates from registry
  - Convert logical coordinates to physical for current DPI
  - Apply screen bounds checking
  - Set window position and size
  - _Requirements: 9.1, 9.2, 9.4, 9.5_



- [ ] 7.3 Implement screen bounds checking
  - Write `EnsureWindowOnScreen()` function in `dpi.c`
  - Get monitor work area for window position
  - Adjust window position if off-screen
  - Ensure window is visible and usable
  - Handle disconnected monitors gracefully
  - _Requirements: 9.5_

- [x] 7.4 Update window position save/restore


  - Replace existing `SaveWindowPosition()` calls with `SaveWindowPositionLogical()`
  - Replace existing `RestoreWindowPosition()` calls with `RestoreWindowPositionLogical()`
  - Test position persistence across DPI changes
  - Test position persistence across monitor changes
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_

- [x] 8. Update main window for HiDPI support
  - Register main window with DPI manager
  - Scale all main window controls
  - Handle DPI changes in main window
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_



- [x] 8.1 Register main window for DPI management
  - Call `RegisterWindowForDPI()` in main window creation
  - Get initial DPI for main window
  - Store DPI context in window data
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7_

- [x] 8.2 Implement main window control scaling
  - Write `ScaleMainWindowControls()` function
  - Define base dimensions at 96 DPI for all controls
  - Scale dimensions using `ScaleValueForDPI()`
  - Position all controls using scaled dimensions
  - Call from `WM_CREATE` and `WM_SIZE` handlers
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_

- [x] 8.3 Update main window initialization
  - Call `ScaleMainWindowControls()` after control creation
  - Create scalable fonts for main window
  - Load icons at appropriate DPI
  - Test main window at different DPI settings
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7_

- [x] 8.4 Test main window DPI handling


  - Test main window at 96 DPI (100%)
  - Test main window at 120 DPI (125%)
  - Test main window at 144 DPI (150%)
  - Test main window at 192 DPI (200%)
  - Test moving main window between monitors with different DPI
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_

- [x] 9. Update dialogs for HiDPI support





  - Ensure all dialogs use DPI-aware scaling
  - Update existing `ScaleForDpi` calls to use new system
  - Test all dialogs at multiple DPI settings


  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_

- [x] 9.1 Update unified dialog DPI handling




  - Verify unified dialog uses new DPI functions
  - Ensure `WM_DPICHANGED` handler is present


  - Test unified dialog at multiple DPI settings
  - Test moving dialog between monitors
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_

- [x] 9.2 Update Settings dialog DPI handling


  - Register Settings dialog with DPI manager
  - Add `WM_DPICHANGED` handler
  - Ensure all controls scale properly
  - Test Settings dialog at multiple DPI settings
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_

- [ ] 9.3 Update About dialog DPI handling
  - Register About dialog with DPI manager
  - Add `WM_DPICHANGED` handler
  - Ensure all controls scale properly
  - Test About dialog at multiple DPI settings
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_

- [ ] 10. Update Makefile and build system
  - Add new source files to Makefile
  - Update dependency tracking
  - Ensure clean builds
  - _Requirements: All requirements_

- [ ] 10.1 Add dpi.c to Makefile
  - Add `dpi.c` to SOURCES variable
  - Update object file lists for 32-bit and 64-bit
  - Add dependency tracking for `dpi.o`
  - _Requirements: All requirements_

- [ ] 10.2 Update dependency tracking
  - Add dependencies for `dpi.o` (dpi.c, dpi.h, YouTubeCacher.h)
  - Update dependencies for files that include `dpi.h`
  - Update dependencies for main.c (now includes DPI initialization)
  - _Requirements: All requirements_

- [ ]* 11. Comprehensive testing and validation
  - Test at all standard DPI settings
  - Test multi-monitor configurations
  - Test dynamic DPI changes
  - Test window position persistence
  - Test font and icon scaling
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7_

- [ ]* 11.1 Test standard DPI settings
  - Test at 96 DPI (100% scaling) - verify baseline
  - Test at 120 DPI (125% scaling) - verify proportional scaling
  - Test at 144 DPI (150% scaling) - verify layout balance
  - Test at 192 DPI (200% scaling) - verify crisp rendering
  - Test at custom DPI (110%, 175%, 225%) - verify fractional scaling
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

- [ ]* 11.2 Test multi-monitor configurations
  - Test with two monitors at same DPI - verify no visual changes
  - Test with two monitors at different DPI - verify rescaling
  - Test moving window between monitors - verify smooth transition
  - Test with monitor configuration changes - verify adaptation
  - _Requirements: 8.5, 8.6, 8.7_

- [ ]* 11.3 Test dynamic DPI changes
  - Test dragging window between monitors with different DPI
  - Test changing system DPI while app is running
  - Test changing per-monitor DPI while app is running
  - Verify no flickering or visual artifacts
  - Verify all controls remain functional
  - _Requirements: 8.5, 8.6, 8.7_

- [ ]* 11.4 Test window position persistence
  - Save position at 96 DPI, restore at 144 DPI - verify correct position
  - Save position on secondary monitor, disconnect monitor, restore - verify on primary
  - Save position on high-DPI monitor, restore on low-DPI - verify usable
  - Test with multiple monitor configurations
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_

- [ ]* 11.5 Test font scaling
  - Verify fonts scale proportionally at all DPI settings
  - Verify text remains readable at all DPI
  - Verify no text clipping or overlap
  - Verify ClearType rendering quality
  - Verify bold/italic styles work correctly
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6_

- [ ]* 11.6 Test icon scaling
  - Verify correct icon size loaded for each DPI
  - Verify icons are sharp (not pixelated)
  - Verify icons fit properly in controls
  - Verify icon alignment is correct
  - Test all icon resources at multiple DPI
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [ ]* 11.7 Test all windows and dialogs
  - Test main window at all DPI settings
  - Test unified dialog at all DPI settings
  - Test Settings dialog at all DPI settings
  - Test About dialog at all DPI settings
  - Test all dialogs with DPI changes
  - _Requirements: All requirements_
