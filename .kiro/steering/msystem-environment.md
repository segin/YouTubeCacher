---
inclusion: always
---

# MSYS2 Environment Management

## Environment Requirements

When executing shell commands, ensure the MSYSTEM environment variable is set to `MINGW32` for consistent build and development environment.

## Command Execution Protocol

Before executing any shell commands:

1. Check the current value of MSYSTEM environment variable
2. If MSYSTEM is not set to `MINGW32`, then:
   - Set `MSYSTEM=MINGW32`
   - Execute `exec bash -l` to reload the shell with the correct environment

This ensures all development tools, compilers, and libraries are available from the MINGW32 toolchain, providing consistent behavior across all command executions.

## Implementation

```bash
# Check current MSYSTEM
if [ "$MSYSTEM" != "MINGW32" ]; then
    export MSYSTEM=MINGW32
    exec bash -l
fi
```

This steering rule applies to all command executions to maintain environment consistency.