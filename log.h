#ifndef LOG_H
#define LOG_H

#include <windows.h>

// Forward declaration for ErrorContext
typedef struct ErrorContext ErrorContext;

// Logging function declarations
void WriteToLogfile(const wchar_t* message);
void WriteSessionStartToLogfile(void);
void WriteSessionEndToLogfile(const wchar_t* reason);
void DebugOutput(const wchar_t* message);

// Enhanced logging functions with error context integration
void WriteErrorContextToLogfile(const ErrorContext* context);
void DebugOutputWithContext(const ErrorContext* context);
void WriteStructuredErrorToLogfile(const wchar_t* severity, int errorCode, 
                                  const wchar_t* function, const wchar_t* file, 
                                  int line, const wchar_t* message);

// Backward compatibility macros
#define LOG_ERROR(message) DebugOutput(message)
#define LOG_INFO(message) DebugOutput(message)
#define LOG_WARNING(message) DebugOutput(message)

#endif // LOG_H