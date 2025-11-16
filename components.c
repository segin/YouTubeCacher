#include "YouTubeCacher.h"

// Create a new component registry
ComponentRegistry* CreateComponentRegistry(void) {
    ComponentRegistry* registry = (ComponentRegistry*)SAFE_MALLOC(sizeof(ComponentRegistry));
    if (!registry) {
        return NULL;
    }
    
    registry->capacity = 10;  // Initial capacity
    registry->count = 0;
    registry->components = (UIComponent**)SAFE_MALLOC(sizeof(UIComponent*) * registry->capacity);
    
    if (!registry->components) {
        SAFE_FREE(registry);
        return NULL;
    }
    
    return registry;
}

// Register a component in the registry
void RegisterComponent(ComponentRegistry* registry, UIComponent* component) {
    if (!registry || !component) {
        return;
    }
    
    // Check if we need to expand the registry
    if (registry->count >= registry->capacity) {
        int newCapacity = registry->capacity * 2;
        UIComponent** newComponents = (UIComponent**)SAFE_MALLOC(sizeof(UIComponent*) * newCapacity);
        
        if (!newComponents) {
            return;  // Failed to expand
        }
        
        // Copy existing components
        for (int i = 0; i < registry->count; i++) {
            newComponents[i] = registry->components[i];
        }
        
        SAFE_FREE(registry->components);
        registry->components = newComponents;
        registry->capacity = newCapacity;
    }
    
    // Add the component
    registry->components[registry->count] = component;
    registry->count++;
}

// Unregister a component from the registry
void UnregisterComponent(ComponentRegistry* registry, UIComponent* component) {
    if (!registry || !component) {
        return;
    }
    
    // Find the component
    for (int i = 0; i < registry->count; i++) {
        if (registry->components[i] == component) {
            // Shift remaining components down
            for (int j = i; j < registry->count - 1; j++) {
                registry->components[j] = registry->components[j + 1];
            }
            registry->count--;
            return;
        }
    }
}

// Destroy the component registry and all registered components
void DestroyComponentRegistry(ComponentRegistry* registry) {
    if (!registry) {
        return;
    }
    
    // Destroy all registered components
    for (int i = 0; i < registry->count; i++) {
        if (registry->components[i] && registry->components[i]->destroy) {
            registry->components[i]->destroy(registry->components[i]);
        }
    }
    
    // Free the registry itself
    SAFE_FREE(registry->components);
    SAFE_FREE(registry);
}

// File browser component implementation

// Handle browse button click
static void HandleFileBrowseClick(FileBrowserComponent* component) {
    if (!component || !component->hwndEdit) {
        return;
    }
    
    OPENFILENAMEW ofn;
    wchar_t fileName[MAX_EXTENDED_PATH] = L"";
    
    // Get current path if any
    if (component->currentPath) {
        wcsncpy(fileName, component->currentPath, MAX_EXTENDED_PATH - 1);
        fileName[MAX_EXTENDED_PATH - 1] = L'\0';
    }
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetParent(component->hwndButton);
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_EXTENDED_PATH;
    ofn.lpstrFilter = component->filter ? component->filter : L"All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameW(&ofn)) {
        SetFileBrowserPath(component, fileName);
    }
}

// Handle WM_COMMAND messages for file browser component
BOOL HandleFileBrowserCommand(FileBrowserComponent* component, WPARAM wParam, LPARAM lParam) {
    (void)lParam;  // Unused parameter
    
    if (!component) {
        return FALSE;
    }
    
    int controlId = LOWORD(wParam);
    int notificationCode = HIWORD(wParam);
    
    // Check if this is the browse button being clicked
    if (controlId == (component->controlId + 2) && notificationCode == BN_CLICKED) {
        HandleFileBrowseClick(component);
        return TRUE;
    }
    
    return FALSE;
}

// Create a file browser component
FileBrowserComponent* CreateFileBrowser(HWND parent, int x, int y, int width, 
                                        const wchar_t* label, const wchar_t* filter, int controlId) {
    if (!parent || !label) {
        return NULL;
    }
    
    FileBrowserComponent* component = (FileBrowserComponent*)SAFE_MALLOC(sizeof(FileBrowserComponent));
    if (!component) {
        return NULL;
    }
    
    ZeroMemory(component, sizeof(FileBrowserComponent));
    component->controlId = controlId;
    
    // Allocate and copy label
    size_t labelLen = wcslen(label) + 1;
    component->label = (wchar_t*)SAFE_MALLOC(labelLen * sizeof(wchar_t));
    if (!component->label) {
        SAFE_FREE(component);
        return NULL;
    }
    wcscpy(component->label, label);
    
    // Allocate and copy filter if provided
    if (filter) {
        // Calculate filter string length (double-null terminated)
        size_t filterLen = 0;
        const wchar_t* p = filter;
        while (*p || *(p + 1)) {
            filterLen++;
            p++;
        }
        filterLen += 2;  // Include both null terminators
        
        component->filter = (wchar_t*)SAFE_MALLOC(filterLen * sizeof(wchar_t));
        if (component->filter) {
            memcpy(component->filter, filter, filterLen * sizeof(wchar_t));
        }
    }
    
    // Create label control
    component->hwndLabel = CreateWindowW(
        L"STATIC", label,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, width, 20,
        parent, (HMENU)(INT_PTR)controlId,
        GetModuleHandle(NULL), NULL
    );
    
    // Create edit control (read-only)
    component->hwndEdit = CreateWindowW(
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        x, y + 22, width - 85, 24,
        parent, (HMENU)(INT_PTR)(controlId + 1),
        GetModuleHandle(NULL), NULL
    );
    
    // Create browse button
    component->hwndButton = CreateWindowW(
        L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + width - 80, y + 22, 75, 24,
        parent, (HMENU)(INT_PTR)(controlId + 2),
        GetModuleHandle(NULL), NULL
    );
    
    // Set up child controls array
    component->base.childCount = 3;
    component->base.childControls = (HWND*)SAFE_MALLOC(sizeof(HWND) * 3);
    if (component->base.childControls) {
        component->base.childControls[0] = component->hwndLabel;
        component->base.childControls[1] = component->hwndEdit;
        component->base.childControls[2] = component->hwndButton;
    }
    
    // Set font for all controls
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(component->hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(component->hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(component->hwndButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    return component;
}

// Destroy a file browser component
void DestroyFileBrowser(FileBrowserComponent* component) {
    if (!component) {
        return;
    }
    
    // Destroy child windows
    if (component->hwndLabel) {
        DestroyWindow(component->hwndLabel);
    }
    if (component->hwndEdit) {
        DestroyWindow(component->hwndEdit);
    }
    if (component->hwndButton) {
        DestroyWindow(component->hwndButton);
    }
    
    // Free allocated memory
    SAFE_FREE(component->label);
    SAFE_FREE(component->filter);
    SAFE_FREE(component->currentPath);
    SAFE_FREE(component->base.childControls);
    SAFE_FREE(component);
}

// Validate file browser (check if file exists)
BOOL ValidateFileBrowser(FileBrowserComponent* component, wchar_t* errorMsg, size_t errorMsgSize) {
    if (!component) {
        if (errorMsg && errorMsgSize > 0) {
            wcsncpy(errorMsg, L"Invalid component", errorMsgSize - 1);
            errorMsg[errorMsgSize - 1] = L'\0';
        }
        return FALSE;
    }
    
    if (!component->currentPath || wcslen(component->currentPath) == 0) {
        if (errorMsg && errorMsgSize > 0) {
            swprintf(errorMsg, errorMsgSize, L"%s is required", component->label);
        }
        return FALSE;
    }
    
    // Check if file exists
    DWORD attrs = GetFileAttributesW(component->currentPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (errorMsg && errorMsgSize > 0) {
            swprintf(errorMsg, errorMsgSize, L"File does not exist: %s", component->currentPath);
        }
        return FALSE;
    }
    
    // Check if it's a file (not a directory)
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        if (errorMsg && errorMsgSize > 0) {
            swprintf(errorMsg, errorMsgSize, L"Path is a directory, not a file: %s", component->currentPath);
        }
        return FALSE;
    }
    
    return TRUE;
}

// Get the current file path
const wchar_t* GetFileBrowserPath(FileBrowserComponent* component) {
    if (!component) {
        return NULL;
    }
    return component->currentPath;
}

// Set the file path
void SetFileBrowserPath(FileBrowserComponent* component, const wchar_t* path) {
    if (!component) {
        return;
    }
    
    // Free old path
    SAFE_FREE(component->currentPath);
    
    // Allocate and copy new path
    if (path && wcslen(path) > 0) {
        size_t pathLen = wcslen(path) + 1;
        component->currentPath = (wchar_t*)SAFE_MALLOC(pathLen * sizeof(wchar_t));
        if (component->currentPath) {
            wcscpy(component->currentPath, path);
        }
    }
    
    // Update edit control
    if (component->hwndEdit) {
        SetWindowTextW(component->hwndEdit, path ? path : L"");
    }
}

// Folder browser component implementation

// Handle browse button click for folder
static void HandleFolderBrowseClick(FolderBrowserComponent* component) {
    if (!component || !component->hwndEdit) {
        return;
    }
    
    BROWSEINFOW bi;
    wchar_t displayName[MAX_PATH];
    
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = GetParent(component->hwndButton);
    bi.pszDisplayName = displayName;
    bi.lpszTitle = L"Select Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t folderPath[MAX_EXTENDED_PATH];
        if (SHGetPathFromIDListW(pidl, folderPath)) {
            SetFolderBrowserPath(component, folderPath);
        }
        
        // Free the PIDL
        CoTaskMemFree(pidl);
    }
}

// Handle WM_COMMAND messages for folder browser component
BOOL HandleFolderBrowserCommand(FolderBrowserComponent* component, WPARAM wParam, LPARAM lParam) {
    (void)lParam;  // Unused parameter
    
    if (!component) {
        return FALSE;
    }
    
    int controlId = LOWORD(wParam);
    int notificationCode = HIWORD(wParam);
    
    // Check if this is the browse button being clicked
    if (controlId == (component->controlId + 2) && notificationCode == BN_CLICKED) {
        HandleFolderBrowseClick(component);
        return TRUE;
    }
    
    return FALSE;
}

// Create a folder browser component
FolderBrowserComponent* CreateFolderBrowser(HWND parent, int x, int y, int width,
                                            const wchar_t* label, int controlId) {
    if (!parent || !label) {
        return NULL;
    }
    
    FolderBrowserComponent* component = (FolderBrowserComponent*)SAFE_MALLOC(sizeof(FolderBrowserComponent));
    if (!component) {
        return NULL;
    }
    
    ZeroMemory(component, sizeof(FolderBrowserComponent));
    component->controlId = controlId;
    
    // Allocate and copy label
    size_t labelLen = wcslen(label) + 1;
    component->label = (wchar_t*)SAFE_MALLOC(labelLen * sizeof(wchar_t));
    if (!component->label) {
        SAFE_FREE(component);
        return NULL;
    }
    wcscpy(component->label, label);
    
    // Create label control
    component->hwndLabel = CreateWindowW(
        L"STATIC", label,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, width, 20,
        parent, (HMENU)(INT_PTR)controlId,
        GetModuleHandle(NULL), NULL
    );
    
    // Create edit control (read-only)
    component->hwndEdit = CreateWindowW(
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        x, y + 22, width - 85, 24,
        parent, (HMENU)(INT_PTR)(controlId + 1),
        GetModuleHandle(NULL), NULL
    );
    
    // Create browse button
    component->hwndButton = CreateWindowW(
        L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + width - 80, y + 22, 75, 24,
        parent, (HMENU)(INT_PTR)(controlId + 2),
        GetModuleHandle(NULL), NULL
    );
    
    // Set up child controls array
    component->base.childCount = 3;
    component->base.childControls = (HWND*)SAFE_MALLOC(sizeof(HWND) * 3);
    if (component->base.childControls) {
        component->base.childControls[0] = component->hwndLabel;
        component->base.childControls[1] = component->hwndEdit;
        component->base.childControls[2] = component->hwndButton;
    }
    
    // Set font for all controls
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(component->hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(component->hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(component->hwndButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    return component;
}

// Destroy a folder browser component
void DestroyFolderBrowser(FolderBrowserComponent* component) {
    if (!component) {
        return;
    }
    
    // Destroy child windows
    if (component->hwndLabel) {
        DestroyWindow(component->hwndLabel);
    }
    if (component->hwndEdit) {
        DestroyWindow(component->hwndEdit);
    }
    if (component->hwndButton) {
        DestroyWindow(component->hwndButton);
    }
    
    // Free allocated memory
    SAFE_FREE(component->label);
    SAFE_FREE(component->currentPath);
    SAFE_FREE(component->base.childControls);
    SAFE_FREE(component);
}

// Validate folder browser (check if folder exists)
BOOL ValidateFolderBrowser(FolderBrowserComponent* component, wchar_t* errorMsg, size_t errorMsgSize) {
    if (!component) {
        if (errorMsg && errorMsgSize > 0) {
            wcsncpy(errorMsg, L"Invalid component", errorMsgSize - 1);
            errorMsg[errorMsgSize - 1] = L'\0';
        }
        return FALSE;
    }
    
    if (!component->currentPath || wcslen(component->currentPath) == 0) {
        if (errorMsg && errorMsgSize > 0) {
            swprintf(errorMsg, errorMsgSize, L"%s is required", component->label);
        }
        return FALSE;
    }
    
    // Check if folder exists
    DWORD attrs = GetFileAttributesW(component->currentPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (errorMsg && errorMsgSize > 0) {
            swprintf(errorMsg, errorMsgSize, L"Folder does not exist: %s", component->currentPath);
        }
        return FALSE;
    }
    
    // Check if it's a directory (not a file)
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        if (errorMsg && errorMsgSize > 0) {
            swprintf(errorMsg, errorMsgSize, L"Path is a file, not a folder: %s", component->currentPath);
        }
        return FALSE;
    }
    
    return TRUE;
}

// Get the current folder path
const wchar_t* GetFolderBrowserPath(FolderBrowserComponent* component) {
    if (!component) {
        return NULL;
    }
    return component->currentPath;
}

// Set the folder path
void SetFolderBrowserPath(FolderBrowserComponent* component, const wchar_t* path) {
    if (!component) {
        return;
    }
    
    // Free old path
    SAFE_FREE(component->currentPath);
    
    // Allocate and copy new path
    if (path && wcslen(path) > 0) {
        size_t pathLen = wcslen(path) + 1;
        component->currentPath = (wchar_t*)SAFE_MALLOC(pathLen * sizeof(wchar_t));
        if (component->currentPath) {
            wcscpy(component->currentPath, path);
        }
    }
    
    // Update edit control
    if (component->hwndEdit) {
        SetWindowTextW(component->hwndEdit, path ? path : L"");
    }
}

// Labeled text input component implementation

// Create a labeled text input component
LabeledTextInput* CreateLabeledTextInput(HWND parent, int x, int y, int width,
                                         const wchar_t* label, ValidationType validation, int controlId) {
    if (!parent || !label) {
        return NULL;
    }
    
    LabeledTextInput* component = (LabeledTextInput*)SAFE_MALLOC(sizeof(LabeledTextInput));
    if (!component) {
        return NULL;
    }
    
    ZeroMemory(component, sizeof(LabeledTextInput));
    component->controlId = controlId;
    component->validationType = validation;
    component->isRequired = (validation == VALIDATION_REQUIRED);
    
    // Allocate and copy label
    size_t labelLen = wcslen(label) + 1;
    component->label = (wchar_t*)SAFE_MALLOC(labelLen * sizeof(wchar_t));
    if (!component->label) {
        SAFE_FREE(component);
        return NULL;
    }
    wcscpy(component->label, label);
    
    // Create label control
    component->hwndLabel = CreateWindowW(
        L"STATIC", label,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, width, 20,
        parent, (HMENU)(INT_PTR)controlId,
        GetModuleHandle(NULL), NULL
    );
    
    // Create edit control
    component->hwndEdit = CreateWindowW(
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        x, y + 22, width, 24,
        parent, (HMENU)(INT_PTR)(controlId + 1),
        GetModuleHandle(NULL), NULL
    );
    
    // Create error label (initially hidden)
    component->hwndError = CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | SS_LEFT,
        x, y + 48, width, 20,
        parent, (HMENU)(INT_PTR)(controlId + 2),
        GetModuleHandle(NULL), NULL
    );
    
    // Set up child controls array
    component->base.childCount = 3;
    component->base.childControls = (HWND*)SAFE_MALLOC(sizeof(HWND) * 3);
    if (component->base.childControls) {
        component->base.childControls[0] = component->hwndLabel;
        component->base.childControls[1] = component->hwndEdit;
        component->base.childControls[2] = component->hwndError;
    }
    
    // Set font for all controls
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(component->hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(component->hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(component->hwndError, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    return component;
}

// Destroy a labeled text input component
void DestroyLabeledTextInput(LabeledTextInput* component) {
    if (!component) {
        return;
    }
    
    // Destroy child windows
    if (component->hwndLabel) {
        DestroyWindow(component->hwndLabel);
    }
    if (component->hwndEdit) {
        DestroyWindow(component->hwndEdit);
    }
    if (component->hwndError) {
        DestroyWindow(component->hwndError);
    }
    
    // Free allocated memory
    SAFE_FREE(component->label);
    SAFE_FREE(component->base.childControls);
    SAFE_FREE(component);
}

// Validate labeled text input
BOOL ValidateLabeledTextInput(LabeledTextInput* component, wchar_t* errorMsg, size_t errorMsgSize) {
    if (!component) {
        if (errorMsg && errorMsgSize > 0) {
            wcsncpy(errorMsg, L"Invalid component", errorMsgSize - 1);
            errorMsg[errorMsgSize - 1] = L'\0';
        }
        return FALSE;
    }
    
    // Get the current value
    wchar_t value[MAX_BUFFER_SIZE];
    GetWindowTextW(component->hwndEdit, value, MAX_BUFFER_SIZE);
    
    // Check if required and empty
    if (component->isRequired && wcslen(value) == 0) {
        if (errorMsg && errorMsgSize > 0) {
            swprintf(errorMsg, errorMsgSize, L"%s is required", component->label);
        }
        // Show error message
        SetWindowTextW(component->hwndError, errorMsg);
        ShowWindow(component->hwndError, SW_SHOW);
        return FALSE;
    }
    
    // Perform validation based on type
    switch (component->validationType) {
        case VALIDATION_NONE:
            break;
            
        case VALIDATION_REQUIRED:
            // Already checked above
            break;
            
        case VALIDATION_NUMERIC:
            // Check if value is numeric
            for (size_t i = 0; i < wcslen(value); i++) {
                if (!iswdigit(value[i]) && value[i] != L'.' && value[i] != L'-') {
                    if (errorMsg && errorMsgSize > 0) {
                        swprintf(errorMsg, errorMsgSize, L"%s must be numeric", component->label);
                    }
                    SetWindowTextW(component->hwndError, errorMsg);
                    ShowWindow(component->hwndError, SW_SHOW);
                    return FALSE;
                }
            }
            break;
            
        case VALIDATION_PATH:
            // Check if path exists
            if (wcslen(value) > 0) {
                DWORD attrs = GetFileAttributesW(value);
                if (attrs == INVALID_FILE_ATTRIBUTES) {
                    if (errorMsg && errorMsgSize > 0) {
                        swprintf(errorMsg, errorMsgSize, L"Path does not exist: %s", value);
                    }
                    SetWindowTextW(component->hwndError, errorMsg);
                    ShowWindow(component->hwndError, SW_SHOW);
                    return FALSE;
                }
            }
            break;
            
        case VALIDATION_URL:
            // Basic URL validation (starts with http:// or https://)
            if (wcslen(value) > 0) {
                if (wcsncmp(value, L"http://", 7) != 0 && wcsncmp(value, L"https://", 8) != 0) {
                    if (errorMsg && errorMsgSize > 0) {
                        swprintf(errorMsg, errorMsgSize, L"%s must be a valid URL", component->label);
                    }
                    SetWindowTextW(component->hwndError, errorMsg);
                    ShowWindow(component->hwndError, SW_SHOW);
                    return FALSE;
                }
            }
            break;
            
        case VALIDATION_CUSTOM:
            // Use custom validator if provided
            if (component->customValidator) {
                if (!component->customValidator(value, errorMsg, errorMsgSize)) {
                    SetWindowTextW(component->hwndError, errorMsg);
                    ShowWindow(component->hwndError, SW_SHOW);
                    return FALSE;
                }
            }
            break;
    }
    
    // Validation passed - hide error message
    SetWindowTextW(component->hwndError, L"");
    ShowWindow(component->hwndError, SW_HIDE);
    
    return TRUE;
}

// Get the current text input value
const wchar_t* GetTextInputValue(LabeledTextInput* component) {
    if (!component || !component->hwndEdit) {
        return NULL;
    }
    
    static wchar_t value[MAX_BUFFER_SIZE];
    GetWindowTextW(component->hwndEdit, value, MAX_BUFFER_SIZE);
    return value;
}

// Set the text input value
void SetTextInputValue(LabeledTextInput* component, const wchar_t* value) {
    if (!component || !component->hwndEdit) {
        return;
    }
    
    SetWindowTextW(component->hwndEdit, value ? value : L"");
    
    // Hide error message when value is set
    if (component->hwndError) {
        SetWindowTextW(component->hwndError, L"");
        ShowWindow(component->hwndError, SW_HIDE);
    }
}

// Set custom validator function
void SetCustomValidator(LabeledTextInput* component, CustomValidationFunc validator) {
    if (!component) {
        return;
    }
    
    component->customValidator = validator;
    component->validationType = VALIDATION_CUSTOM;
}
