#ifndef SETTINGS_H
#define SETTINGS_H

#include <windows.h>

// Registry operations
BOOL LoadSettingFromRegistry(const wchar_t* valueName, wchar_t* buffer, DWORD bufferSize);
BOOL SaveSettingToRegistry(const wchar_t* valueName, const wchar_t* value);

// Settings management
void LoadSettings(HWND hDlg);
void SaveSettings(HWND hDlg);

// Path management
void GetDefaultDownloadPath(wchar_t* path, size_t pathSize);
void GetDefaultYtDlpPath(wchar_t* path, size_t pathSize);
BOOL CreateDownloadDirectoryIfNeeded(const wchar_t* path);

// Utility functions
void FormatDuration(wchar_t* duration, size_t bufferSize);

#endif // SETTINGS_H