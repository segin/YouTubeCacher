---
inclusion: always
---

# MSYS2 Environment Management

## Environment Requirements

When executing shell commands, ensure the MSYSTEM environment variable is set to `MINGW32` for consistent build and development environment.

## Command Execution Protocol

Before executing any shell commands, unconditionally set the MSYSTEM environment variable to `MINGW32`:

```bash
export MSYSTEM=MINGW32
```

This ensures all development tools, compilers, and libraries are available from the MINGW32 toolchain, providing consistent behavior across all command executions. The setting is done unconditionally without checking the current value to guarantee the correct environment.

This steering rule applies to all command executions to maintain environment consistency.