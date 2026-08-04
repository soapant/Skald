#ifndef TYPES_H
#define TYPES_H

#include <windows.h>
#include <stdbool.h>
#include <string.h>

#define MAX_VISIBLE_ITEMS 100

typedef void (*RefreshCallback)(const char* filter, HWND hListBox);
typedef void (*EnterCallback)(const char* selection);
typedef void (*CtrlEnterCallback)(const char* selection);
typedef void (*TabCallback)(void);
typedef void (*RenderCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex);
typedef void (*EditCallback)(const char*);
typedef void (*FilterCallback)(const char* item,const char* filter);
typedef void (*CopyCallback)(const char* selection,int true_index,int type);
typedef void (*HelpCallback)();
typedef void (*KeyCallback)(int vkCode);

KeyCallback g_key_callback = NULL;


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

char* DisplayMessage(const char* message, int centerX, int centerY, int width, int height,int timeout);

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
    const char** last_selections;
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

typedef struct HashMapEntry {
    char* key;
    void* value;
    struct HashMapEntry* next;
} HashMapEntry;

typedef struct {
    HashMapEntry** buckets;
    int size;
    int count;
} HashMap;

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

static FileEntry G_FILE_ENTRIES[] = {
    {"TerminalHistory", "D:\\Projects\\C Code\\TerminalHistory.txt", NULL},
    {"YaziZoxide",      "D:\\Projects\\C Code\\YaziZoxide.txt", NULL},
    {"NvimZoxide",      "D:\\Projects\\C Code\\NvimZoxide.txt", NULL},
    {"TerminalZoxide",      "D:\\Projects\\C Code\\TerminalZoxide.txt", NULL},
    {"ExplorerZoxide",  "D:\\Projects\\C Code\\ExplorerZoxide.txt", NULL},
    {"GoogleZoxide",  "D:\\Projects\\C Code\\GoogleZoxide.txt", NULL},
    {"ReminderHistory",  "D:\\Projects\\C Code\\ReminderHistory.txt", NULL},
};

#define FILE_ENTRY_COUNT (sizeof(G_FILE_ENTRIES) / sizeof(G_FILE_ENTRIES[0]))

static inline const char* GetFilePath(const char* name) {
    for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
        if (strcmp(G_FILE_ENTRIES[i].name, name) == 0) {
            return G_FILE_ENTRIES[i].path;
        }
    }
    return NULL;
}

// ============================================================
// GLOBAL VARIABLE DECLARATIONS
// ============================================================

extern HWND hPopupWnd;
extern HWND hListBox;
extern HWND hDisplayMessage;
extern HWND hEditControl;
extern HWND g_ActiveEditControl;
extern HWND g_hEditPopup;
extern bool stop_key;
extern int stopped_key;
extern EditPopupData* g_EditData;
extern bool selection_complete;
extern RefreshCallback g_refresh_callback;
extern EnterCallback g_enter_callback;
extern CtrlEnterCallback g_ctrl_enter_callback;
extern TabCallback g_tab_callback;
extern CopyCallback g_copy_callback;
extern RenderCallback g_render_callback;
extern EditCallback g_edit_callback;
extern FilterCallback g_filter_callback;
extern const char** g_initial_items;
extern int g_initial_count;
extern int g_SelectedIndex;
extern const char* g_title;
extern const char* g_vim_mode;
extern COLORREF g_rainbow_text_cache[MAX_VISIBLE_ITEMS + 1];
#define MAX_ITEMS 1000
extern char g_populated_cache[MAX_ITEMS][512];
extern int g_populated_count;
extern int g_view_offset;
extern int g_TotalMatches; 
extern int g_last_match_count; 
extern const char** g_last_items;
extern int g_last_item_count;
extern char g_last_filter[256];
extern int g_last_matches[4000];
extern char g_filter_history[20][256];
extern int g_match_history[20][4000]; 
extern int g_match_count_history[20]; 
extern int g_history_index;       
extern int g_history_count;     
extern int g_matching_indices[4000];
extern int g_current_window_height;
extern int g_current_window_width;
extern int g_timeout;
extern int img_region_step;
extern long img_x1, img_y1, img_x2, img_y2;
extern long saved_x1;
extern int img_region_step;
extern int region_step;
extern long saved_y1;
extern bool g_skip_refresh;
extern char output_buffer[256];
extern HWND hGlobalTrayWnd;
extern char* g_current_feature;
extern HashMap* feature_history;
extern HashMap* feature_data;
extern HashMap* g_item_index_map;
extern COLORREF DisplayMessageText;
#define COLOR_BG      RGB(30, 35, 45)   
#define COLOR_TEXT    RGB(255, 255, 255)
#define COLOR_SELECTED_TEXT    RGB(70, 55, 90)
#define COLOR_VISUAL_SELECTED_TEXT    RGB(48,25,52)
#define TODO_FILE_PATH "todo.txt"
#define FORGETFULL_FILE_PATH "forgetfull.txt"
#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define WM_TRIGGER_UNIVERSAL_MACRO (WM_USER + 3)
#define WM_TRIGGER_IMG_CAPTURE (WM_USER + 2)
#define ID_EXIT 2001
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
extern HFONT g_hNumFont;
extern HFONT g_hMainFont;
extern WinTimer g_timer;

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
                   (540 - (g_current_window_height / 2) - GAP - ((msg_height) / 2)), \
                   900, (msg_height), (timeout))

#define MSG_BOTTOM(msg, msg_height, timeout) \
    DisplayMessage((msg), \
                   960, \
                   (540 + (g_current_window_height / 2) + GAP + ((msg_height) / 2)), \
                   900, (msg_height), (timeout))
#define MSG_MIDDLE_BOTTOM(msg,msg_height,timeout) \
    DisplayMessage((msg), \
                   960, \
                   1080 - msg_height, \
                   900,(msg_height), (timeout))

//todo make these

#define MIDDLE_POS 960, 540, 1000, 300

#define TOP_POS 960, 540, 1000, 300

#define BOTTOM_POS 960, 540, 1000, 300

#define LEFT_POS 960, 540, 1000, 300

#define RIGHT_POS 960, 540, 1000, 300

#endif
