#include <windows.h>

#define IDC_TEXT_FIELD    1001
#define IDC_LABEL1        1002
#define IDC_LABEL2        1003
#define IDC_LABEL3        1004
#define IDC_LIST          1005
#define IDC_BUTTON1       1006
#define IDC_BUTTON2       1007
#define IDC_BUTTON3       1008

#define IDD_MAIN_DIALOG   101

void ResizeControls(HWND hDlg) {
    RECT rect;
    GetClientRect(hDlg, &rect);
    
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    // Resize URL text field
    SetWindowPos(GetDlgItem(hDlg, IDC_TEXT_FIELD), NULL, 45, 8, width - 135, 14, SWP_NOZORDER);
    
    // Resize listbox (leave 90px for buttons + margin)
    SetWindowPos(GetDlgItem(hDlg, IDC_LIST), NULL, 10, 70, width - 100, height - 80, SWP_NOZORDER);
    
    // Position buttons on the right
    int buttonX = width - 80;
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON1), NULL, buttonX, 70, 70, 25, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON2), NULL, buttonX, 100, 70, 25, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hDlg, IDC_BUTTON3), NULL, buttonX, 130, 70, 25, SWP_NOZORDER);
}

// Dialog procedure function
INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            // Initialize dialog controls
            SetDlgItemText(hDlg, IDC_LABEL2, "Status: Ready");
            SetDlgItemText(hDlg, IDC_LABEL3, "Items: 0");
            
            // Add some sample items to the listbox
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)"Sample Video 1.mp4");
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)"Sample Video 2.mp4");
            SendDlgItemMessage(hDlg, IDC_LIST, LB_ADDSTRING, 0, (LPARAM)"Sample Video 3.mp4");
            
            // Set minimum window size
            SetWindowPos(hDlg, NULL, 0, 0, 400, 300, SWP_NOMOVE | SWP_NOZORDER);
            
            return TRUE;
            
        case WM_SIZE:
            ResizeControls(hDlg);
            return TRUE;
            
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 350;
            mmi->ptMinTrackSize.y = 250;
            return 0;
        }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BUTTON1:
                    MessageBox(hDlg, "Download functionality not implemented yet", "Download", MB_OK);
                    break;
                    
                case IDC_BUTTON2:
                    MessageBox(hDlg, "Play functionality not implemented yet", "Play", MB_OK);
                    break;
                    
                case IDC_BUTTON3:
                    MessageBox(hDlg, "Delete functionality not implemented yet", "Delete", MB_OK);
                    break;
                    
                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hDlg, 0);
            return TRUE;
    }
    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Create and show the dialog
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, DialogProc);
    
    return 0;
}