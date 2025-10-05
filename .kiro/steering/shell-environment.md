---
inclusion: always
---

# Shell Environment Guidelines

## Interactive Shell Requirements

The development environment uses **Bash** as the interactive shell, not PowerShell or CMD.

## Command Syntax

All shell commands and examples should use Bash syntax:

### Environment Variables
```bash
# Setting variables
export VARIABLE_NAME=value

# Checking variables
if [ "$VARIABLE" = "value" ]; then
    echo "Variable matches"
fi
```

### File Operations
```bash
# List files
ls -la

# Copy files
cp source.txt destination.txt

# Remove files
rm file.txt

# Remove directories
rm -rf directory/

# Create directories
mkdir -p path/to/directory
```

### Command Chaining
```bash
# Sequential execution
command1 && command2

# Pipe operations
command1 | command2

# Background execution
command &
```

## MSYS2 Integration

When working in the MSYS2/MINGW32 environment:
- Use Unix-style paths (`/c/Users/...` instead of `C:\Users\...`)
- Leverage Bash built-ins and standard Unix utilities
- Maintain MSYSTEM=MINGW32 for consistent toolchain access

## Avoid PowerShell Semantics

Do not use PowerShell-specific syntax such as:
- `Get-ChildItem` (use `ls` instead)
- `Remove-Item` (use `rm` instead)
- `Copy-Item` (use `cp` instead)
- PowerShell cmdlet naming conventions
- PowerShell variable syntax (`$env:VARIABLE`)

This ensures consistency with the Bash-based development environment.