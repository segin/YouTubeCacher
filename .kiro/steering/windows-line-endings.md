# Windows Line Endings Guidelines

## Line Ending Requirements

YouTubeCacher is a Windows-native and Windows-only application. All code, documentation, and text files must use Windows line endings (CRLF: `\r\n`).

## Implementation Rules

### String Literals and Output
- Always use `\r\n` for line breaks in string literals
- Never use Unix-style `\n` line endings
- Example: `L"Error message\r\nAdditional details\r\n"`

### File Operations
- When writing text files, ensure CRLF line endings
- When reading files, handle both CRLF and LF but output CRLF
- Use text mode file operations when appropriate to ensure proper line ending handling

### Code Formatting
- Source code files (.c, .h) should use CRLF line endings
- Resource files (.rc) should use CRLF line endings
- Documentation files (.md, .txt) should use CRLF line endings

### Git Configuration
- Configure Git to handle line endings appropriately for Windows development
- Use `.gitattributes` to enforce CRLF for text files if needed

## Rationale

1. **Platform Consistency**: Windows applications should follow Windows conventions
2. **User Experience**: Windows users expect CRLF line endings in text output
3. **Tool Compatibility**: Windows development tools work best with CRLF
4. **Native Behavior**: Matches Windows API and system behavior

## Validation

When reviewing code or text output:
- Verify all multi-line strings use `\r\n`
- Check that file output operations produce Windows line endings
- Ensure log files and error messages display correctly in Windows tools