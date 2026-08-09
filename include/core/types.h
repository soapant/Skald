#ifndef TYPES_H
#define TYPES_H

#include "minwindef.h"
#include <windows.h>
#include <stdbool.h>
#include <string.h>
#include "core/hashmap.h"

#define MAX_VISIBLE_ITEMS 100

#define MAX_SEQUENCE_LEN 8
#define SEQUENCE_TIMEOUT_MS 500

typedef void (*RefreshCallback)(const char* filter, HWND hListBox);
typedef void (*EnterCallback)(const char* selection);
typedef void (*CtrlEnterCallback)(const char* selection);
typedef void (*TabCallback)(void);
typedef void (*RenderCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex);
typedef void (*AliasCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex);
typedef void (*EditCallback)(const char*);
typedef void (*FilterCallback)(const char* item,const char* filter);
typedef void (*CopyCallback)(const char* selection,int true_index,int type);
typedef void (*HelpCallback)();
typedef void (*KeyCallback)(int vkCode);

extern KeyCallback g_key_callback;

char* PromptUser(const char* window_list[], const char* window_title, int count, 
                 int height, int width, int item_height, 
                 RefreshCallback refresh_callback, 
                 EnterCallback enter_callback,
                 CtrlEnterCallback ctrl_enter_callback,
                 TabCallback tab_callback,
                 CopyCallback copy_callback,
                 RenderCallback render_callback,
                 EditCallback edit_callback,
                 FilterCallback filter_callback,
                 HelpCallback help_callback);

#define ImportantPromptUser(window_list,window_title,count,height,width,item_height,refresh_callback,enter_callback,edit_callback,help_callback) PromptUser(window_list,window_title,count,height,width,item_height,refresh_callback,enter_callback,NULL,NULL,NULL,NULL,edit_callback,NULL,help_callback)

#define OnlyIOPromptUser(window_list,window_title,count,height,width,item_height,refresh_callback,enter_callback,ctrl_enter_callback,tab_callback) PromptUser(window_list,window_title,count,height,width,item_height,refresh_callback,enter_callback,ctrl_enter_callback,tab_callback,NULL,NULL,NULL,NULL,NULL)

char* SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height);

void DisplayMessage(const char* message, int centerX, int centerY, int width, int height,int timeout);

void SettingsPopUp();

char* EditTextPopup(const char* initial_text, int centerX, int centerY, int maxWidth, int maxHeight);

void SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath);

void UpdateTextCaret(HWND hwndEditControl);

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

LRESULT CALLBACK SimpleEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

LRESULT CALLBACK SimpleListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,COLORREF color);

void SetCursorToEndOfText(void);

void SetCursorToStartOfText(void);

void WaitForKeypressAsync(KeyCallback callback);

typedef struct {
    char keys[MAX_SEQUENCE_LEN];
    int len;
    int typed_count;
    UINT timer_id;
    bool waiting;
    bool timed_out;
} SequenceState;

extern SequenceState g_seq_state;

typedef struct {
    const char* name;                         
    void (*func)(const char** items, int count);
} SortOption;

typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER frequency;
} WinTimer;

struct feature_data {
    char* name;
    const char** items;
    const char* last_selections[2048];
    int selection_count;
    int item_count;    
    RefreshCallback refresh_callback;
    EnterCallback enter_callback;
    CtrlEnterCallback ctrl_enter_callback;
    TabCallback tab_callback;
    CopyCallback copy_callback;
    RenderCallback render_callback;
    EditCallback edit_callback;
    FilterCallback filter_callback;
    HelpCallback help_callback;
    bool (*filter_match)(const char*, const char*);
};

typedef struct {
   char* alias;
   char* expanded_text;
   char* feature;
}AliasEntry;

typedef struct {
    char* initial_text;
    char* result;
    HFONT hFont;
    int alpha;
    HWND hEdit;
} EditPopupData;

typedef struct {
    int index;
    HWND hwnd;
    char title[256];
    char exe[256];
} SavedWindow;

typedef struct {
    HWND hwnd;
    char title[256];
    char exe[256];
} WindowData;

typedef struct {
    int x, y, w, h;
    int item_height;
} WindowLayout;

typedef struct {
    HANDLE hFile;
    HANDLE hMapping;
    char *pBuffer;
    size_t fileSize;   
    size_t capacity;   
} MappedFile;

typedef struct {
    char *ptr;
    size_t length;
} Line;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* app_name;  
} AppHotkey;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* misc_path; 
} MiscHotkey;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* QuickLinkQuery; 
} QuickLinkHotkey;

typedef struct {
    int vk_code;
    int modifiers;
    const char** blacklist;      
    int blacklist_count;
    const char** window_titles;  
    int window_count;
} FocusSwitcherBinding;

typedef struct {
    int vk_code;
    int modifiers;
    int code;
} MusicPlayerBinding;

typedef struct {
    int vk_code;
    int modifiers;
    int code;
} VideoPlayerBinding;

typedef struct {
    int vk_code;
    int modifiers;
    const char** window_titles;  
    int window_count;
} BulkKillProcessBinding;

typedef struct {
    const char* name;
    const char* path;
    MappedFile* mf;
} FileEntry;

extern FileEntry G_FILE_ENTRIES[7];
#define FILE_ENTRY_COUNT (sizeof(G_FILE_ENTRIES) / sizeof(G_FILE_ENTRIES[0]))

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* text;  
} TextSnippet;

typedef struct {
    HWND hwnd;
    char title[256];
} MonitorWindowData;

typedef struct {
    HMONITOR targetMonitor;
    int count;
    int capacity;
    MonitorWindowData items[64];
} MonitorWindowList;

typedef struct {
    int targetIndex;
    int currentIndex;
    HMONITOR foundMonitor;
} MonitorSearch;

typedef struct {
    const char* alias;
    const char* alias_label;
} TodoAliasEntry;

typedef struct {
    const char* flag;
    const char* internal_name;
    const char* tag_label;
    COLORREF color;
    bool is_alias;
} TodoConfigEntry;

typedef struct {
    char tag_label[64];
    char internal_name[64];
    COLORREF color;
} TodoTag;

typedef struct {
    TodoTag tags[10];
    int tag_count;
    char text[256];
} TodoEntry;

typedef struct {
    const char* flag;
    void (*handler)(const char*);
} GenericFlagEntry;

typedef struct {
    const char* open_bracket;
    const char* close_bracket;
    const char* tag_separator;
    const char* tag_prefix;
    const char* alias_separator;
} TagFormatConfig;

#define MAX_ITEMS 1000
#define COLOR_BG      RGB(30, 35, 45)   
#define COLOR_TEXT    RGB(255, 255, 255)
#define COLOR_SELECTED_TEXT    RGB(70, 55, 90)
#define COLOR_VISUAL_SELECTED_TEXT    RGB(48,25,52)
#define COLOR_ALIAS_TEXT    RGB(40,40,250)
#define COLOR_INSERT_MODE_TEXT    RGB(50, 255, 50)
#define COLOR_NORMAL_MODE_TEXT    RGB(100, 255, 255)
#define COLOR_VISUAL_MODE_TEXT    RGB(200, 3, 222)
#define DATA_DIR "data\\"
#define TODO_FILE_PATH DATA_DIR "todo.txt"
#define FORGETFULL_FILE_PATH DATA_DIR "forgetfull.txt"
#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define WM_TRIGGER_IMG_CAPTURE (WM_USER + 2)
#define WM_TRIGGER_UNIVERSAL_MACRO (WM_USER + 3)
#define ID_EXIT 2001
#define ID_SETTINGS 2002
#define IDC_INPUT_BOX 101
#define IDC_LIST_BOX 102
#define HOTKEY_MOD_NONE         0x00
#define HOTKEY_MOD_ALT          0x01
#define HOTKEY_MOD_SHIFT        0x02
#define HOTKEY_MOD_CTRL         0x04
#define HOTKEY_MOD_ALT_SHIFT    (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT)
#define HOTKEY_MOD_ALT_CTRL     (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL)
#define HOTKEY_MOD_SHIFT_CTRL   (HOTKEY_MOD_SHIFT | HOTKEY_MOD_CTRL)
#define HOTKEY_MOD_ALT_SHIFT_CTRL (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT | HOTKEY_MOD_CTRL)

#define GAP 20

#define MAIN_CENTER_X (GetSystemMetrics(SM_CXSCREEN) / 2)
#define MAIN_CENTER_Y (GetSystemMetrics(SM_CYSCREEN) / 2)

#define MSG_LEFT(msg, msg_width, timeout) \
    DisplayMessage((msg), \
                   (960 - 450 - GAP - ((msg_width) / 2)), \
                   540, \
                   (msg_width), 650, (timeout))

#define MSG_RIGHT(msg, msg_width, timeout) \
    DisplayMessage((msg), \
                   (960 + 450 + GAP + ((msg_width) / 2)), \
                   540, \
                   (msg_width), 650, (timeout))

#define MSG_TOP(msg, msg_height, timeout) \
    DisplayMessage((msg), \
                   960, \
                   (960 - 540  - GAP * 10 - ((msg_height) / 2)), \
                   900, (msg_height), (timeout))

#define MSG_BOTTOM(msg, msg_height, timeout) \
    DisplayMessage((msg), \
                   960, \
                   (540 + (960 / 2) + GAP + ((msg_height) / 2)), \
                   900, (msg_height), (timeout))
#define MSG_MIDDLE_BOTTOM(msg,msg_height,timeout) \
    DisplayMessage((msg), \
                   960, \
                   1080 - msg_height, \
                   900,(msg_height), (timeout))

#define MIDDLE_POS 960, 540, 1000, 300
#define TOP_POS 960, 540, 1000, 300
#define BOTTOM_POS 960, 540, 1000, 300
#define LEFT_POS 960, 540, 1000, 300
#define RIGHT_POS 960, 540, 1000, 300

#endif
