---
inclusion: always
---

# MSYS2 Environment Management

## Environment Requirements

The development environment uses MSYS2/MINGW32 toolchain. The MSYSTEM environment variable should already be properly configured in the shell environment.

## Command Execution Protocol

Execute shell commands normally without modifying environment variables. The MSYSTEM environment is managed by the shell initialization and does not need to be set before each command.

Only set MSYSTEM manually if there are specific issues with toolchain detection, and only when actually needed for troubleshooting.