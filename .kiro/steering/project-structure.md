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