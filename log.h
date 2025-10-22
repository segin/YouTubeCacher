#ifndef LOG_H
#define LOG_H

#include <windows.h>

// Logging function declarations
void WriteToLogfile(const wchar_t* message);
void WriteSessionStartToLogfile(void);
void WriteSessionEndToLogfile(const wchar_t* reason);
void DebugOutput(const wchar_t* message);

#endif // LOG_H