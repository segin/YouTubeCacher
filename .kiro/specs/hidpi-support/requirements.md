# Requirements Document

## Introduction

This specification defines comprehensive HiDPI (High Dots Per Inch) support for YouTubeCacher to ensure the application displays correctly on high-resolution displays and scales properly across different DPI settings. While basic DPI scaling has been implemented, additional work is needed to test on various DPI settings, ensure all UI elements scale properly, add per-monitor DPI awareness, and optimize for different screen sizes. This will provide a crisp, properly-sized user interface on modern high-resolution displays.

## Glossary

- **HiDPI**: High Dots Per Inch displays with pixel densities higher than traditional 96 DPI screens
- **DPI**: Dots Per Inch, a measure of screen pixel density
- **DPI Scaling**: The process of adjusting UI element sizes based on screen DPI to maintain consistent physical size
- **Per-Monitor DPI Awareness**: The ability to handle different DPI settings on different monitors in multi-monitor setups
- **System DPI**: The DPI setting configured in Windows display settings (typically 96, 120, 144, 192 DPI)
- **DPI Awareness Context**: Windows API concept defining how an application handles DPI scaling
- **Logical Pixels**: Device-independent pixels used in UI layout calculations
- **Physical Pixels**: Actual screen pixels that may differ from logical pixels based on DPI scaling
- **DPI Change Event**: Windows message sent when the DPI setting changes for a window
- **Scale Factor**: The multiplier applied to UI elements (e.g., 1.0 for 96 DPI, 1.5 for 144 DPI, 2.0 for 192 DPI)

## Requirements

### Requirement 1

**User Story:** As a user with a high-resolution display, I want all UI elements to scale proportionally, so that the application is readable and usable at my display's native resolution

#### Acceptance Criteria

1. THE Application SHALL declare per-monitor DPI awareness version 2 in the application manifest
2. WHEN the Application starts, THE Application SHALL query the system DPI for the primary monitor
3. WHEN the Application creates the main window, THE Application SHALL scale all UI element dimensions based on the current DPI
4. WHEN the Application creates a dialog, THE Application SHALL scale all dialog dimensions and control sizes based on the current DPI
5. THE Application SHALL scale font sizes proportionally with DPI scaling
6. THE Application SHALL scale icon sizes proportionally with DPI scaling
7. THE Application SHALL scale spacing and padding between UI elements proportionally with DPI scaling

### Requirement 2

**User Story:** As a user with multiple monitors at different DPI settings, I want the application to display correctly when moved between monitors, so that it remains readable regardless of which monitor it's on

#### Acceptance Criteria

1. WHEN the main window is moved to a monitor with different DPI, THE Application SHALL receive a DPI change notification
2. WHEN a DPI change notification is received, THE Application SHALL recalculate all UI element dimensions for the new DPI
3. WHEN a DPI change notification is received, THE Application SHALL resize and reposition all controls in the main window
4. WHEN a DPI change notification is received, THE Application SHALL reload icons at the appropriate size for the new DPI
5. WHEN a DPI change notification is received, THE Application SHALL update font sizes for the new DPI
6. WHEN a dialog is open during a DPI change, THE Application SHALL update the dialog's scaling to match the new DPI

### Requirement 3

**User Story:** As a user, I want text to remain crisp and readable at all DPI settings, so that I can comfortably read all application content

#### Acceptance Criteria

1. THE Application SHALL use ClearType-compatible font rendering for all text
2. THE Application SHALL scale font sizes using floating-point calculations to avoid rounding errors
3. WHEN the DPI is 120 (125% scaling), THE Application SHALL render text at 125% of the base font size
4. WHEN the DPI is 144 (150% scaling), THE Application SHALL render text at 150% of the base font size
5. WHEN the DPI is 192 (200% scaling), THE Application SHALL render text at 200% of the base font size
6. THE Application SHALL ensure minimum font sizes remain readable at all DPI settings

### Requirement 4

**User Story:** As a user, I want icons and images to remain sharp at all DPI settings, so that the application looks professional on my high-resolution display

#### Acceptance Criteria

1. THE Application SHALL provide icon resources at multiple sizes (16x16, 24x24, 32x32, 48x48, 64x64)
2. WHEN loading an icon, THE Application SHALL select the icon size closest to the scaled target size
3. WHERE an exact icon size is not available, THE Application SHALL scale the nearest larger size using high-quality filtering
4. THE Application SHALL avoid scaling icons up from smaller sizes when larger sizes are available
5. WHEN the DPI changes, THE Application SHALL reload all visible icons at the appropriate size for the new DPI

### Requirement 5

**User Story:** As a user, I want the application window to fit properly on my screen at all DPI settings, so that I can see all controls without the window being too large or too small

#### Acceptance Criteria

1. THE Application SHALL calculate initial window size based on logical pixels that scale with DPI
2. WHEN the DPI is 96 (100% scaling), THE Application SHALL create a window of base dimensions
3. WHEN the DPI is 120 (125% scaling), THE Application SHALL create a window 125% of base dimensions
4. WHEN the DPI is 144 (150% scaling), THE Application SHALL create a window 150% of base dimensions
5. WHEN the DPI is 192 (200% scaling), THE Application SHALL create a window 200% of base dimensions
6. THE Application SHALL ensure the scaled window size does not exceed 90% of the monitor's work area
7. WHERE the scaled window would exceed the monitor's work area, THE Application SHALL reduce the window size to fit

### Requirement 6

**User Story:** As a user, I want all interactive controls to scale properly, so that buttons and input fields are appropriately sized for my display

#### Acceptance Criteria

1. THE Application SHALL scale button widths and heights proportionally with DPI
2. THE Application SHALL scale text input field heights proportionally with DPI
3. THE Application SHALL scale checkbox and radio button sizes proportionally with DPI
4. THE Application SHALL scale progress bar dimensions proportionally with DPI
5. THE Application SHALL scale control spacing and margins proportionally with DPI
6. THE Application SHALL ensure minimum control sizes remain usable at all DPI settings

### Requirement 7

**User Story:** As a developer, I want DPI scaling calculations centralized, so that all UI code uses consistent scaling logic

#### Acceptance Criteria

1. THE Application SHALL provide a function to convert logical pixels to physical pixels based on current DPI
2. THE Application SHALL provide a function to convert physical pixels to logical pixels based on current DPI
3. THE Application SHALL provide a function to get the current DPI scale factor
4. THE Application SHALL provide a function to scale a rectangle structure based on current DPI
5. THE Application SHALL provide a function to scale a font size based on current DPI
6. THE Application SHALL store the current DPI value in the application state for easy access

### Requirement 8

**User Story:** As a quality assurance tester, I want the application tested at common DPI settings, so that I can verify it works correctly for all users

#### Acceptance Criteria

1. THE Application SHALL be tested at 96 DPI (100% scaling, standard DPI)
2. THE Application SHALL be tested at 120 DPI (125% scaling, common laptop setting)
3. THE Application SHALL be tested at 144 DPI (150% scaling, common high-DPI setting)
4. THE Application SHALL be tested at 192 DPI (200% scaling, high-resolution display)
5. THE Application SHALL be tested with multi-monitor configurations having different DPI settings
6. THE Application SHALL be tested with DPI changes while the application is running
7. THE Application SHALL be tested with window movement between monitors with different DPI settings

### Requirement 9

**User Story:** As a user, I want the application to remember my window size and position across DPI changes, so that my preferred layout is maintained

#### Acceptance Criteria

1. WHEN the Application saves window position, THE Application SHALL save the position in logical coordinates
2. WHEN the Application restores window position, THE Application SHALL convert saved logical coordinates to physical coordinates based on current DPI
3. WHEN the Application saves window size, THE Application SHALL save the size in logical dimensions
4. WHEN the Application restores window size, THE Application SHALL convert saved logical dimensions to physical dimensions based on current DPI
5. WHERE the restored window would be off-screen due to DPI changes, THE Application SHALL adjust the position to be visible
