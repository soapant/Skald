#ifndef TYPES_H
#define TYPES_H

#if defined(_WIN32)
#include <windows.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#if !defined(_WIN32)
#include <time.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif
#define MAX_VISIBLE_ITEMS 4000
#define MAX_ITEMS 4000
#define MAX_MATCHES 4000

#define MAX_SEQUENCE_LEN 8
#define SEQUENCE_TIMEOUT_MS 500

#if !defined(MAX_PATH)
#define MAX_PATH 260
#endif

#if !defined(_strdup) && !defined(_WIN32)
#define _strdup strdup
#endif

#if !defined(VK_BACK)
#define VK_BACK 0x08
#endif
#if !defined(VK_TAB)
#define VK_TAB 0x09
#endif
#if !defined(VK_RETURN)
#define VK_RETURN 0x0D
#endif
#if !defined(VK_SHIFT)
#define VK_SHIFT 0x10
#endif
#if !defined(VK_CONTROL)
#define VK_CONTROL 0x11
#endif
#if !defined(VK_MENU)
#define VK_MENU 0x12
#endif
#if !defined(VK_ESCAPE)
#define VK_ESCAPE 0x1B
#endif
#if !defined(VK_SPACE)
#define VK_SPACE 0x20
#endif
#if !defined(VK_LEFT)
#define VK_LEFT 0x25
#endif
#if !defined(VK_UP)
#define VK_UP 0x26
#endif
#if !defined(VK_RIGHT)
#define VK_RIGHT 0x27
#endif
#if !defined(VK_DOWN)
#define VK_DOWN 0x28
#endif
#if !defined(VK_LWIN)
#define VK_LWIN 0x5B
#endif
#if !defined(VK_RWIN)
#define VK_RWIN 0x5C
#endif
#if !defined(VK_OEM_COMMA)
#define VK_OEM_COMMA 0xBC
#endif
#if !defined(VK_OEM_PERIOD)
#define VK_OEM_PERIOD 0xBE
#endif
#if !defined(VK_OEM_1)
#define VK_OEM_1 0xBA
#endif
#if !defined(VK_OEM_2)
#define VK_OEM_2 0xBF
#endif
#if !defined(VK_OEM_4)
#define VK_OEM_4 0xDB
#endif
#if !defined(VK_OEM_7)
#define VK_OEM_7 0xDE
#endif
#if !defined(VK_OEM_6)
#define VK_OEM_6 0xDD
#endif
#if !defined(VK_OEM_3)
#define VK_OEM_3 0xC0
#endif
#if !defined(VK_OEM_MINUS)
#define VK_OEM_MINUS 0xBD
#endif
#if !defined(VK_OEM_PLUS)
#define VK_OEM_PLUS 0xBB
#endif
#if !defined(VK_LSHIFT)
#define VK_LSHIFT 0xA0
#endif
#if !defined(VK_RSHIFT)
#define VK_RSHIFT 0xA1
#endif
#if !defined(VK_DELETE)
#define VK_DELETE 0x2E
#endif
#if !defined(VK_HOME)
#define VK_HOME 0x24
#endif
#if !defined(VK_END)
#define VK_END 0x23
#endif
#if !defined(VK_PRIOR)
#define VK_PRIOR 0x21
#endif
#if !defined(VK_NEXT)
#define VK_NEXT 0x22
#endif
#if !defined(VK_F1)
#define VK_F1 0x70
#endif
#if !defined(VK_F12)
#define VK_F12 0x7B
#endif
#if !defined(VK_LCONTROL)
#define VK_LCONTROL 0xA2
#endif
#if !defined(VK_RCONTROL)
#define VK_RCONTROL 0xA3
#endif
#if !defined(VK_LMENU)
#define VK_LMENU 0xA4
#endif
#if !defined(VK_RMENU)
#define VK_RMENU 0xA5
#endif

typedef uint32_t ColorRef;

#if !defined(FALSE)
#define FALSE 0
#define TRUE 1
#endif

#if !defined(RGB)
#define RGB(r, g, b) ((uint32_t)(((r) & 0xFF) | (((g) & 0xFF) << 8) | (((b) & 0xFF) << 16)))
#endif

typedef struct ListBoxInfo ListBoxInfo;

struct ListBoxInfo {
    char populated_cache[MAX_ITEMS][512];
    int  populated_count;
    int  matching_indices[MAX_MATCHES];
    int  total_matches;
    int  selected_index;
    int  view_offset;
    const char **last_items;
    int  last_item_count;
    char last_filter[256];
    bool last_filter_valid;
    int  prev_matches[MAX_MATCHES];
    int  last_filter_full_count;
};

typedef struct WindowHandle {
#if defined(_WIN32)
    HWND hwnd;
#else
    unsigned long xid;
#endif
} WindowHandle;

typedef struct {
     char**      items;
     size_t      item_count;
     char        filter[512];
     size_t      cursor;
     int         scroll_offset;
     int         width, height;
     char*       window_title;
     ListBoxInfo listbox;
     bool        has_listbox;
} PromptState;

typedef struct {
     char*       display_message;
     size_t      cursor;
     int         scroll_offset;
     char        user_input[512];
     int         width, height;
     char*       window_title;
} SimplePromptState;

#define WH_NULL ((WindowHandle){0})

#if defined(_WIN32)
#define WH_FROM_HWND(h) ((WindowHandle){ .hwnd = (HWND)(h) })
#define WH_TO_HWND(h) ((h).hwnd)
#else
#define WH_FROM_HWND(h) ((void)(h), WH_NULL)
#define WH_TO_HWND(h) ((h).xid)
#endif

#if !defined(_WIN32)
typedef struct _XDisplay Display;
typedef struct _XftFont XftFont;
#endif

typedef struct FontHandle {
#if defined(_WIN32)
    HFONT hfont;
#else
    XftFont* xfont;
    Display* dpy;
#endif
} FontHandle;

#define FH_NULL ((FontHandle){0})

#if defined(_WIN32)
#define FH_FROM_HFONT(h) ((FontHandle){ .hfont = (HFONT)(h) })
#define FH_TO_HFONT(h) ((h).hfont)
#else
#define FH_TO_XFT(h) ((h).xfont)
#define FH_TO_DPY(h) ((h).dpy)
#endif


typedef void (*RefreshCallback)(const char* filter, WindowHandle hListBox, ListBoxInfo* lb);
typedef void (*EnterCallback)(const char* selection, ListBoxInfo* lb);
typedef void (*CtrlEnterCallback)(const char* selection, ListBoxInfo* lb);
typedef void (*TabCallback)(ListBoxInfo* lb);
#if defined(_WIN32)
typedef void (*RenderCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex);
typedef void (*AliasCallback)(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex);
#else
typedef void (*RenderCallback)(Display *dpy, Window *win, PromptState *state);
typedef void (*AliasCallback)(void* hdc, const char* text, void* rect, bool is_selected, int itemIndex);
#endif
typedef void (*EditCallback)(const char* selection, ListBoxInfo* lb);
typedef void (*FilterCallback)(const char* item,const char* filter);
typedef void (*CopyCallback)(const char* selection,int true_index,int type);
typedef void (*HelpCallback)();
typedef void (*KeyCallback)(int vkCode);


static inline void ListBoxInfo_Init(ListBoxInfo *lb) {
    if (!lb) return;
    memset(lb, 0, sizeof(*lb));
    lb->selected_index = -1;
    lb->populated_count = 0;
    lb->total_matches = 0;
    lb->view_offset = 0;
}

static inline const char* ListBoxInfo_GetSelectedText(ListBoxInfo *lb) {
    if (!lb) return NULL;
    if (lb->selected_index < 0 || lb->selected_index >= lb->populated_count) return NULL;
    return lb->populated_cache[lb->selected_index];
}

static inline int ListBoxInfo_GetCount(ListBoxInfo *lb) {
    return lb ? lb->populated_count : 0;
}

static inline bool ListBoxInfo_IsSelected(ListBoxInfo *lb) {
    return lb && lb->selected_index != -1;
}

extern KeyCallback g_key_callback;
extern char* Platform_PromptUser(const char* window_list[], const char* window_title, int count, int height, int width, int item_height,
    RefreshCallback refresh_callback, 
    EnterCallback enter_callback,
    CtrlEnterCallback ctrl_enter_callback,
    TabCallback tab_callback,
    CopyCallback copy_callback,
    RenderCallback render_callback,
    EditCallback edit_callback,
    FilterCallback filter_callback,
    HelpCallback help_callback);
extern char* Platform_SimplePromptUser(const char* prompt, const char* window_title,int height, int width, int item_height);

#define ImportantPromptUser(window_list,window_title,count,height,width,item_height,refresh_callback,enter_callback,edit_callback,help_callback) Platform_PromptUser(window_list,window_title,count,height,width,item_height,refresh_callback,enter_callback,NULL,NULL,NULL,NULL,edit_callback,NULL,help_callback)

#define OnlyIOPromptUser(window_list,window_title,count,height,width,item_height,refresh_callback,enter_callback,ctrl_enter_callback,tab_callback) Platform_PromptUser(window_list,window_title,count,height,width,item_height,refresh_callback,enter_callback,ctrl_enter_callback,tab_callback,NULL,NULL,NULL,NULL,NULL)

void UpdateTextCaret(WindowHandle hwndEditControl);

#if defined(_WIN32)
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

LRESULT CALLBACK SettingsEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

LRESULT CALLBACK SimpleEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

LRESULT CALLBACK SimpleListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
#endif

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color);

typedef struct {
    char keys[MAX_SEQUENCE_LEN];
    int len;
    int typed_count;
    unsigned int timer_id;
    bool waiting;
    bool timed_out;
} SequenceState;

extern SequenceState g_seq_state;

typedef struct {
    const char* name;                         
    void (*func)(const char** items, int count);
} SortOption;

typedef struct {
#if defined(_WIN32)
    LARGE_INTEGER start;
    LARGE_INTEGER frequency;
#else
    struct timespec start;
#endif
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
   char* win_text;
   char* linux_text;
   char* feature;
}AliasEntry;

typedef struct {
    char* initial_text;
    char* result;
#if defined(_WIN32)
    HFONT hFont;
#else
    void* hFont;
#endif
    int alpha;
    WindowHandle hEdit;
} EditPopupData;

typedef struct {
    int index;
    WindowHandle hwnd;
    char title[256];
    char exe[256];
} SavedWindow;

typedef struct {
    WindowHandle hwnd;
    char title[256];
    char exe[256];
} WindowData;

typedef struct {
    int x, y, w, h;
    int item_height;
} WindowLayout;

typedef struct {
#if defined(_WIN32)
    HANDLE hFile;
    HANDLE hMapping;
    size_t fileSize;   
    size_t capacity;   
#else 
     int fd;           
     size_t fileSize;
#endif
    char *pBuffer;
} MappedFile;


#if defined(_WIN32)
typedef struct {
     ListBoxInfo listbox;
     bool        has_listbox;
     WindowHandle hListBox;
     WindowHandle hEditControl;
     WindowHandle hPopupWnd;
} WinPromptState;
#endif


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
    WindowHandle hwnd;
    char title[256];
} MonitorWindowData;

typedef struct {
#if defined(_WIN32)
    HMONITOR targetMonitor;
#else
    unsigned long targetMonitor;
#endif
    int count;
    int capacity;
    MonitorWindowData items[64];
} MonitorWindowList;

typedef struct {
    int targetIndex;
    int currentIndex;
#if defined(_WIN32)
    HMONITOR foundMonitor;
#else
    unsigned long foundMonitor;
#endif
} MonitorSearch;

typedef struct {
    const char* alias;
    const char* alias_label;
} TodoAliasEntry;

typedef struct {
    const char* flag;
    const char* internal_name;
    const char* tag_label;
    ColorRef color;
    bool is_alias;
} TodoConfigEntry;

typedef struct {
    char tag_label[64];
    char internal_name[64];
    ColorRef color;
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

#ifndef RGB
#define RGB(r, g, b)  ((unsigned long)(((unsigned char)(r)|((unsigned short)((unsigned char)(g))<<8))|(((unsigned long)(unsigned char)(b))<<16)))
#endif
#define COLOR_BG      RGB(5, 5, 10)   
#define COLOR_TEXT    RGB(255, 255, 255)
#define COLOR_ALTERNATE RGB(57, 255, 20)
#define COLOR_SELECTED_TEXT     RGB(57, 105, 20)
#define COLOR_VISUAL_SELECTED_TEXT    RGB(48,25,52)
#define COLOR_ALIAS_TEXT    RGB(255,255,255)
#define COLOR_INSERT_MODE_TEXT (RainbowText ? RGB(50, 255, 50) : COLOR_TEXT)
#define COLOR_NORMAL_MODE_TEXT (RainbowText ? RGB(100, 255, 255) : COLOR_TEXT)
#define COLOR_VISUAL_MODE_TEXT (RainbowText ? RGB(200, 3, 222) : COLOR_TEXT)
#if defined(_WIN32)
#define DATA_DIR "data\\"
#else
#define DATA_DIR "data/"
#endif
#define TODO_FILE_PATH DATA_DIR "todo.txt"
#define FORGETFULL_FILE_PATH DATA_DIR "forgetfull.txt"
#define ID_TRAY_ICON 1001
#if defined(_WIN32)
#define WM_TRAYICON (WM_USER + 1)
#define WM_TRIGGER_IMG_CAPTURE (WM_USER + 2)
#define WM_TRIGGER_UNIVERSAL_MACRO (WM_USER + 3)
#endif
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

#if defined(_WIN32)
#define MAIN_CENTER_X (GetSystemMetrics(SM_CXSCREEN) / 2)
#define MAIN_CENTER_Y (GetSystemMetrics(SM_CYSCREEN) / 2)
#endif

#define MSG_LEFT(msg, msg_width, timeout) \
    Platform_DisplayMessage((msg), \
                   (960 - 450 - GAP - ((msg_width) / 2)), \
                   540, \
                   (msg_width), 650, (timeout))

#define MSG_RIGHT(msg, msg_width, timeout) \
    Platform_DisplayMessage((msg), \
                   (960 + 450 + GAP + ((msg_width) / 2)), \
                   540, \
                   (msg_width), 650, (timeout))

#define MSG_TOP(msg, msg_height, timeout) \
    Platform_DisplayMessage((msg), \
                   960, \
                   (960 - 540  - GAP * 10 - ((msg_height) / 2)), \
                   900, (msg_height), (timeout))

#define MSG_BOTTOM(msg, msg_height, timeout) \
    Platform_DisplayMessage((msg), \
                   960, \
                   (540 + (960 / 2) + GAP + ((msg_height) / 2)), \
                   900, (msg_height), (timeout))
#define MSG_MIDDLE_BOTTOM(msg,msg_height,timeout) \
    Platform_DisplayMessage((msg), \
                   960, \
                   1080 - msg_height, \
                   900,(msg_height), (timeout))

#define MIDDLE_POS 960, 540, 1000, 300
#define TOP_POS 960, 540, 1000, 300
#define BOTTOM_POS 960, 540, 1000, 300
#define LEFT_POS 960, 540, 1000, 300
#define RIGHT_POS 960, 540, 1000, 300

#endif
