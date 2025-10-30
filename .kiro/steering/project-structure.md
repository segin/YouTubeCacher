---
inclusion: always
---

# YouTube Cacher Project Structure

## Overview

This project follows a modular C/Windows application structure with clear separation of concerns and centralized configuration management.

## File Organization

### Header Files

- **`YouTubeCacher.h`** - Master project header containing:
  - Application constants (name, version, buffer sizes)
  - Window sizing and layout constants
  - Color definitions for UI elements
  - Function prototypes
  - Global variable extern declarations

- **`resource.h`** - Resource definitions shared between C code and resource script:
  - Control IDs (IDC_*)
  - Dialog and menu resource IDs (IDD_*, IDR_*)
  - Menu command IDs (ID_*)

- **`uri.h`** - URI validation module interface

### Source Files

- **`main.c`** - Main application logic:
  - Window procedure and dialog handling
  - UI event processing
  - Application initialization and cleanup

- **`uri.c`** - URI validation functionality:
  - YouTube URL detection and validation

### Resource Files

- **`YouTubeCacher.rc`** - Windows resource script:
  - Dialog definitions
  - Menu definitions
  - Accelerator tables

### Build System

- **`Makefile`** - Build configuration with:
  - Compiler flags including `-Wall -Wextra -Werror`
  - Source file dependencies
  - Resource compilation
  - Clean targets

## Design Principles

1. **Desktop Utility Focus**: YouTubeCacher is a desktop utility application, not a high-availability enterprise backend. All designs, architectures, and implementations should be appropriate for a desktop utility - prioritizing simplicity, maintainability, and user experience over enterprise-scale features like complex thread pools, extensive monitoring, or high-availability patterns
2. **Centralized Constants**: All magic numbers and configuration values are defined in header files
3. **Modular Design**: Functionality is separated into logical modules (URI handling, main UI, etc.)
4. **Resource Sharing**: Common definitions between C code and resource script are in `resource.h`
5. **Type Safety**: Proper extern declarations and function prototypes
6. **Maintainability**: Constants can be changed in one location
7. **Unicode Support**: All code uses Unicode (UTF-16) APIs and TCHAR types for proper internationalization
8. **Master Header Inclusion**: The only header file included by `.c` files is `YouTubeCacher.h`. This master header includes all other necessary headers
9. **Version Control Discipline**: Always run `git status`, `git commit` and `git push` after every development step. Use `git diff` when `git status` shows additional staged changes
10. **GUI Application**: YouTubeCacher is a Windows GUI application that does not produce command-line output. Testing should focus on successful compilation and proper module integration rather than command-line execution

## Critical Implementation Rules

### Duration Processing Requirements
- **NEVER remove or simplify duration parsing logic without explicit approval**
- Duration processing must handle both JSON metadata and direct yt-dlp output formats
- All duration parsing functions in `parser.c` are essential and must be preserved
- Duration formatting must support both MM:SS and HH:MM:SS formats
- JSON parsing for duration must extract numeric seconds and convert to formatted time strings
- The `ParseJsonMetadataLine` function is critical for extracting video metadata

### UI/UX Patterns - STRICTLY PROHIBITED
- **NEVER create popup progress dialogs during download operations**
- **NEVER use `CreateProgressDialog`, `ProgressDialog`, or modal dialogs for download progress**
- **NEVER show blocking dialogs during background operations**
- All download progress must be shown in the main window's integrated progress controls
- Use non-blocking, in-window progress indicators only
- The `ProgressDialog` system was removed for being poor UX and must not be reintroduced
- Background operations must update the main UI asynchronously without blocking user interaction

### Forbidden Patterns
- Modal progress dialogs during downloads (removed for poor UX)
- Blocking UI during background operations
- Oversimplified duration parsing that loses functionality
- Removal of JSON metadata extraction without replacement
- Synchronous operations that freeze the main UI thread

## Build Process

The build system automatically handles dependencies between:
- C source files and headers
- Resource script and shared headers
- Object file linking

## UI Architecture

The application uses a Windows dialog-based interface with:
- Resizable layout with proper control positioning
- Menu bar with keyboard accelerators
- Color-coded input states for different URL sources
- Clipboard integration for automatic URL detection