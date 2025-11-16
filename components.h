#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <windows.h>

// Component registry functions
ComponentRegistry* CreateComponentRegistry(void);
void RegisterComponent(ComponentRegistry* registry, UIComponent* component);
void UnregisterComponent(ComponentRegistry* registry, UIComponent* component);
void DestroyComponentRegistry(ComponentRegistry* registry);

// File browser component functions
FileBrowserComponent* CreateFileBrowser(HWND parent, int x, int y, int width, 
                                        const wchar_t* label, const wchar_t* filter, int controlId);
void DestroyFileBrowser(FileBrowserComponent* component);
BOOL ValidateFileBrowser(FileBrowserComponent* component, wchar_t* errorMsg, size_t errorMsgSize);
const wchar_t* GetFileBrowserPath(FileBrowserComponent* component);
void SetFileBrowserPath(FileBrowserComponent* component, const wchar_t* path);
BOOL HandleFileBrowserCommand(FileBrowserComponent* component, WPARAM wParam, LPARAM lParam);

// Folder browser component functions
FolderBrowserComponent* CreateFolderBrowser(HWND parent, int x, int y, int width,
                                            const wchar_t* label, int controlId);
void DestroyFolderBrowser(FolderBrowserComponent* component);
BOOL ValidateFolderBrowser(FolderBrowserComponent* component, wchar_t* errorMsg, size_t errorMsgSize);
const wchar_t* GetFolderBrowserPath(FolderBrowserComponent* component);
void SetFolderBrowserPath(FolderBrowserComponent* component, const wchar_t* path);
BOOL HandleFolderBrowserCommand(FolderBrowserComponent* component, WPARAM wParam, LPARAM lParam);

// Labeled text input component functions
LabeledTextInput* CreateLabeledTextInput(HWND parent, int x, int y, int width,
                                         const wchar_t* label, ValidationType validation, int controlId);
void DestroyLabeledTextInput(LabeledTextInput* component);
BOOL ValidateLabeledTextInput(LabeledTextInput* component, wchar_t* errorMsg, size_t errorMsgSize);
const wchar_t* GetTextInputValue(LabeledTextInput* component);
void SetTextInputValue(LabeledTextInput* component, const wchar_t* value);
void SetCustomValidator(LabeledTextInput* component, CustomValidationFunc validator);

#endif // COMPONENTS_H
