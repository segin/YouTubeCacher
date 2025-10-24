#ifndef RESOURCE_H
#define RESOURCE_H

// Control IDs
#define IDC_TEXT_FIELD    1001
#define IDC_LABEL1        1002
#define IDC_LABEL2        1003
#define IDC_LABEL3        1004
#define IDC_LIST          1005
#define IDC_BUTTON1       1006
#define IDC_BUTTON2       1007
#define IDC_BUTTON3       1008
#define IDC_DOWNLOAD_BTN  1009
#define IDC_GETINFO_BTN   1010
#define IDC_DOWNLOAD_GROUP 1011
#define IDC_OFFLINE_GROUP  1012
#define IDC_PROGRESS_BAR   1022
#define IDC_STATIC         -1

// Dialog and Menu Resource IDs
#define IDD_MAIN_DIALOG   101
#define IDR_MAIN_MENU     102
#define IDD_PROGRESS      104

// Menu Command IDs
#define ID_FILE_SETTINGS  2000
#define ID_FILE_EXIT      2001
#define ID_EDIT_SELECTALL 2002
#define ID_HELP_INSTALL_YTDLP 2003
#define ID_HELP_ABOUT     2004

// Settings Dialog
#define IDD_SETTINGS      103
#define IDC_YTDLP_LABEL   1013
#define IDC_YTDLP_PATH    1014
#define IDC_YTDLP_BROWSE  1015
#define IDC_FOLDER_LABEL  1016
#define IDC_FOLDER_PATH   1017
#define IDC_FOLDER_BROWSE 1018
#define IDC_PLAYER_LABEL  1019
#define IDC_PLAYER_PATH   1020
#define IDC_PLAYER_BROWSE 1021

// Color buttons
#define IDC_COLOR_GREEN   1023
#define IDC_COLOR_TEAL    1024
#define IDC_COLOR_BLUE    1025
#define IDC_COLOR_WHITE   1026

// Progress Dialog
#define IDC_PROGRESS_STATUS    1027
#define IDC_PROGRESS_PROGRESS  1028
#define IDC_PROGRESS_CANCEL    1029

// Custom Arguments
#define IDC_CUSTOM_ARGS_LABEL  1030
#define IDC_CUSTOM_ARGS_FIELD  1031
#define IDC_ENABLE_DEBUG       1055
#define IDC_ENABLE_LOGFILE     1056
#define IDC_ENABLE_AUTOPASTE   1057

// Video Information Controls
#define IDC_VIDEO_TITLE_LABEL  1041
#define IDC_VIDEO_TITLE        1042
#define IDC_VIDEO_DURATION_LABEL 1043
#define IDC_VIDEO_DURATION     1044
#define IDC_PROGRESS_TEXT      1045

// Unified Dialog - handles all message types (info, warning, error, success)
#define IDD_UNIFIED_DIALOG     105
#define IDC_UNIFIED_ICON       1032
#define IDC_UNIFIED_MESSAGE    1033
#define IDC_UNIFIED_DETAILS_BTN 1034
#define IDC_UNIFIED_TAB_CONTROL 1035
#define IDC_UNIFIED_TAB1_TEXT  1036
#define IDC_UNIFIED_TAB2_TEXT  1037
#define IDC_UNIFIED_TAB3_TEXT  1038
#define IDC_UNIFIED_COPY_BTN   1039
#define IDC_UNIFIED_OK_BTN     1040

// About Dialog
#define IDD_ABOUT_DIALOG       107
#define IDC_ABOUT_ICON         1058
#define IDC_ABOUT_TITLE        1059
#define IDC_ABOUT_VERSION      1060
#define IDC_ABOUT_DESCRIPTION  1061
#define IDC_ABOUT_GITHUB_LINK  1062
#define IDC_ABOUT_COPYRIGHT    1063
#define IDC_ABOUT_WARRANTY     1064
#define IDC_ABOUT_LICENSE_LINK 1065
#define IDC_ABOUT_CLOSE        1066


// Error dialog tab IDs
#define TAB_ERROR_DETAILS      0
#define TAB_ERROR_DIAGNOSTICS  1
#define TAB_ERROR_SOLUTIONS    2

#endif // RESOURCE_H