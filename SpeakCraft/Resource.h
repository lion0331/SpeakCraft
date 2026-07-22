#pragma once

// ─── Menu IDs ────────────────────────────────────
#define IDR_MAIN_MENU           101

#define IDM_FILE_EXIT           1001
#define IDM_PRACTICE_START      2001
#define IDM_PRACTICE_STOP       2002
#define IDM_PRACTICE_NEXT       2003
#define IDM_PRACTICE_PREV       2004
#define IDM_SETTINGS_APIKEY     3001
#define IDM_SETTINGS_VOICE      3002
#define IDM_HELP_ABOUT          4001

// ─── Dialog IDs ──────────────────────────────────
#define IDD_SETTINGS            201
#define IDD_ABOUT               202
#define IDD_VOICE_SETTINGS      203
#define IDD_CHAT                 204

// ─── Control IDs ─────────────────────────────────
// Main window controls
#define IDC_LESSON_TREE         1101
#define IDC_LESSON_CONTENT      1102
#define IDC_CHAT_HISTORY        1103
#define IDC_CHAT_INPUT          1104
#define IDC_SEND_BTN            1105
#define IDC_RECORD_BTN          1106
#define IDC_PLAY_BTN            1107
#define IDC_STATUS_BAR          1108
#define IDC_SPLITTER            1109

// Mode buttons (6)
#define IDC_MODE_BTN_0          1110    // Text Shadowing
#define IDC_MODE_BTN_1          1111    // Role Play
#define IDC_MODE_BTN_2          1112    // Sentence Pattern
#define IDC_MODE_BTN_3          1113    // Free Conversation
#define IDC_MODE_BTN_4          1114    // Grammar Correction
#define IDC_MODE_BTN_5          1115    // Learning Report

// Settings dialog
#define IDC_API_ENDPOINT        1201
#define IDC_API_KEY             1202
#define IDC_MODEL_NAME          1203
#define IDC_VOICE_COMBO         1204
#define IDC_TEST_CONNECTION     1205
#define IDC_SAVE_SETTINGS       1206

// Voice settings dialog
#define IDC_VOICE_LIST          1301
#define IDC_VOICE_RATE          1302
#define IDC_VOICE_RATE_LABEL    1303
#define IDC_VOICE_TEST          1304

// Chat controls
#define IDC_CHAT_EDIT           1301
#define IDC_CHAT_SEND           1302

// ─── String Table IDs ────────────────────────────
#define IDS_APP_TITLE           5001
#define IDS_READY               5002
#define IDS_RECORDING           5003
#define IDS_PROCESSING          5004
#define IDS_ERROR_API           5005
#define IDS_NO_LESSON           5006

// ─── Icon ID ─────────────────────────────────────
#define IDI_APP_ICON            601

// ─── Custom Messages ─────────────────────────────
#define WM_AI_RESPONSE          (WM_USER + 100)
#define WM_SPEECH_RESULT        (WM_USER + 101)
#define WM_SPEECH_COMPLETE      (WM_USER + 102)
#define WM_UPDATE_STATUS        (WM_USER + 103)
