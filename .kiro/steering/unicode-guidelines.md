# Unicode Development Guidelines

## Unicode Requirements

All code in this project must use Unicode (UTF-16) APIs and character types for proper internationalization support and compatibility with modern Windows systems.

## Implementation Rules

### Character Types (Modern Approach)
- Use `wchar_t` for wide character variables
- Use `wchar_t*` for wide string pointers  
- Use `const wchar_t*` for const wide string pointers
- Use `WCHAR` when Windows API compatibility is needed (equivalent to `wchar_t`)

### String Literals
- Use `L""` prefix for wide string literals
- Example: `L"Hello World"` instead of `"Hello World"`
- Avoid obsolete `TEXT()` and `_T()` macros

### API Functions
- Always use the explicit Unicode (W) versions of Windows APIs
- Examples:
  - `CreateWindowW` instead of `CreateWindowA` or `CreateWindow`
  - `GetWindowTextW` instead of `GetWindowTextA`
  - `MessageBoxW` instead of `MessageBoxA`
  - `GetEnvironmentVariableW` instead of `GetEnvironmentVariableA`

### String Functions
- Use `wcslen` instead of `strlen`
- Use `wcscpy` instead of `strcpy`
- Use `wcscat` instead of `strcat`
- Use `wcscmp` instead of `strcmp`
- Use `wcsncpy` instead of `strncpy`
- Use `swprintf` instead of `snprintf`

### File Operations
- Use `GetFileAttributes` (already Unicode-aware)
- Use `CreateDirectory` (already Unicode-aware)
- Use wide character versions for explicit file operations

### Conversion Functions
- Use `MultiByteToWideChar` and `WideCharToMultiByte` when interfacing with ANSI APIs
- Always specify UTF-8 (`CP_UTF8`) for multibyte conversions

## Build Configuration

The Makefile includes `-DUNICODE -D_UNICODE` flags to enable Unicode compilation throughout the project.

## Benefits

1. **Internationalization**: Proper support for all languages and character sets
2. **Modern Windows**: Better compatibility with Windows 10/11 systems
3. **Long Path Support**: Enhanced compatibility with Windows long path features
4. **Future-Proofing**: Alignment with Microsoft's recommended practices