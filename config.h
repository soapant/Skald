#ifndef CONFIG_H
#define CONFIG_H


static bool TRAY = true;

// Hotkey IDs
#define SWITCHER_ID       1
#define SWITCHER_KEY      0x20  // Space
#define TODO_ID           2
#define TODO_KEY          0x54  // T
#define POS_ID            3
#define POS_KEY           0x5A  // Z
#define COLOR_ID          4
#define COLOR_KEY         0x58  // X
#define PIXEL_EQUALS_ID   5
#define PIXEL_EQUALS_KEY  0x42  // B
#define WAIT_FOR_PIXEL_ID 6
#define WAIT_FOR_PIXEL_KEY 0x4E // N
#define REGION_ID         7
#define REGION_KEY        0x41  // A
#define CALCULATOR_ID     8
#define CALCULATOR_KEY    0x43  // C
#define EXE_ID            9
#define EXE_KEY           0x45  // E
#define KILL_PROCESS_ID   10
#define KILL_PROCESS_KEY  0x55
#define GOOGLE_SEARCH_ID   11
#define GOOGLE_SEARCH_KEY  0x47 
#define NVIM_ID      12
#define NVIM_KEY     0x49
#define TERMINAL_ID      13
#define TERMINAL_KEY     0x57
#define YAZI_ID      14
#define YAZI_KEY     0x51
#define SCREENSHOT_ID      15
#define SCREENSHOT_KEY     0x59
#define TIMER_ID      16
#define TIMER_KEY     0x4F
#define HOTKEY_MOD_NONE         0x00
#define HOTKEY_MOD_ALT          0x01
#define HOTKEY_MOD_SHIFT        0x02
#define HOTKEY_MOD_CTRL         0x04
#define HOTKEY_MOD_ALT_SHIFT    (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT)
#define HOTKEY_MOD_ALT_CTRL     (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL)
#define HOTKEY_MOD_SHIFT_CTRL   (HOTKEY_MOD_SHIFT | HOTKEY_MOD_CTRL)
#define HOTKEY_MOD_ALT_SHIFT_CTRL (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT | HOTKEY_MOD_CTRL)


typedef enum {
    MACRO_NONE = 0,
    MACRO_FACTORIO_MAKE_TAG,
    MACRO_MINECRAFT_PASTE,
    MACRO_MINECRAFT_COPY,
    MACRO_MINECRAFT_ROTATE,
    MACRO_MINECRAFT_UP,
    MACRO_MINECRAFT_UNDO,
    MACRO_FACTORIO_SEARCH_FACTORY,
} MacroAction;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    MacroAction action_id;
    int delay;
} MacroBinding;

typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* app_name;  // Name of the app to launch from cache
} AppHotkey;

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

static const AppHotkey G_APP_HOTKEYS[] = {
    { VK_RETURN, HOTKEY_MOD_ALT, "*", "Alacritty" },
    { 'B', HOTKEY_MOD_CTRL | HOTKEY_MOD_SHIFT, "*", "Zen" },
};

#define APP_HOTKEY_COUNT (sizeof(G_APP_HOTKEYS) / sizeof(G_APP_HOTKEYS[0]))


#define APP_HOTKEY_COUNT (sizeof(G_APP_HOTKEYS) / sizeof(G_APP_HOTKEYS[0]))

static const MacroBinding G_MACRO_BINDINGS[] = {
    { 'V', HOTKEY_MOD_ALT, "Factorio", MACRO_FACTORIO_MAKE_TAG, 0 },
    { 'B', HOTKEY_MOD_ALT, "Factorio", MACRO_FACTORIO_SEARCH_FACTORY, 0 },
    { 'C', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_COPY, 0 },
    { 'V', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_PASTE, 0 },
    { 'Z', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_UNDO, 0 },
    { 'Q', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_UP, 0 },
    { 'R', HOTKEY_MOD_SHIFT, "Java", MACRO_MINECRAFT_ROTATE, 0 },
};

#define MACRO_BINDING_COUNT (sizeof(G_MACRO_BINDINGS) / sizeof(G_MACRO_BINDINGS[0]))


typedef struct {
    int vk_code;
    int modifiers;
    const char* exe_name;
    const char* text;  
} TextSnippet;

static const TextSnippet G_TEXT_SNIPPETS[] = {
    { 'O', HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, "EpicGamesLauncher", "seifisbad@gmail.com" },
    { 'P', HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, "EpicGamesLauncher", "i_am_retarded1" },
};

#define TEXT_SNIPPET_COUNT (sizeof(G_TEXT_SNIPPETS) / sizeof(G_TEXT_SNIPPETS[0]))


static const MusicPlayerBinding G_MUSIC_PLAYERS[] = {
  {
    VK_OEM_COMMA,
    HOTKEY_MOD_ALT,
    1 //PAUSE
  },
  { 
    VK_OEM_2,
    HOTKEY_MOD_ALT,
    5 // TOGGLE PLAYBACK
  },
  { 
    VK_OEM_1,
    HOTKEY_MOD_ALT,
    6 // TERMINATE
  },
  { 
    'P',
    HOTKEY_MOD_ALT,
    69 // PLAY SONG
  },
  
};
#define MUSIC_PLAYER_COUNT (sizeof(G_MUSIC_PLAYERS) / sizeof(G_MUSIC_PLAYERS[0]))
static const VideoPlayerBinding G_VIDEO_PLAYERS[] = {
  {
    VK_OEM_COMMA,
    HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,
    1 //PAUSE
  },
  { 
    VK_OEM_PERIOD,
    HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,
    5 // TOGGLE FullScreen
  },
  { 
    VK_OEM_1,
    HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,
    6 // TERMINATE
  },
  { 
    'V',
    HOTKEY_MOD_ALT,
    69 // PLAY VIDEO
  },
};
#define VIDEO_PLAYER_COUNT (sizeof(G_VIDEO_PLAYERS) / sizeof(G_VIDEO_PLAYERS[0]))

static const char* bkp_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Steam"};

static const BulkKillProcessBinding G_BULK_KILL_PROCESS[] = {
    {
        'P',           
        HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL,               
        bkp_windows,              
        sizeof(bkp_windows) / sizeof(bkp_windows[0])
    },
};

#define BULKKILLPROCESS_COUNT (sizeof(G_BULK_KILL_PROCESS) / sizeof(G_BULK_KILL_PROCESS[0]))


  
static const char* focus_blacklist_games[] = {"CrosshairX"};

static const char* games_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Minecraft"};

static const char* focus_blacklist_browsers[] = {"CrosshairX"};

static const char* browsers_windows[] = {"Zen","Chrome"};

static const char* focus_blacklist_coding[] = {"CrosshairX"};

static const char* coding_windows[] = {"Alacritty","Nvim","Neovide","vim","pwsh"};

static const FocusSwitcherBinding G_FOCUS_SWITCHERS[] = {
    {
        'O',           
        HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL,               
        focus_blacklist_games,      
        sizeof(focus_blacklist_games) / sizeof(focus_blacklist_games[0]),
        games_windows,              
        sizeof(games_windows) / sizeof(games_windows[0])
    },
    {
        'I',           
        HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL,               
        focus_blacklist_browsers,      
        sizeof(focus_blacklist_browsers) / sizeof(focus_blacklist_browsers[0]),
        browsers_windows,              
        sizeof(browsers_windows) / sizeof(browsers_windows[0])
    },
    {
        'U',           
        HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL,               
        focus_blacklist_coding,      
        sizeof(focus_blacklist_coding) / sizeof(focus_blacklist_coding[0]),
        coding_windows,              
        sizeof(coding_windows) / sizeof(coding_windows[0])
    },
};

#define FOCUS_SWITCHER_COUNT (sizeof(G_FOCUS_SWITCHERS) / sizeof(G_FOCUS_SWITCHERS[0]))

static const char* switcher_blacklist[] = {"Fortnite", "CrosshairX"};
static int switcher_blacklist_count = sizeof(switcher_blacklist) / sizeof(switcher_blacklist[0]);

static const char* todo_blacklist[] = {"Fortnite", "CrosshairX"};
static int todo_blacklist_count = sizeof(todo_blacklist) / sizeof(todo_blacklist[0]);

static const char* pos_blacklist[] = {"Fortnite", "CrosshairX"};
static int pos_blacklist_count = sizeof(pos_blacklist) / sizeof(pos_blacklist[0]);

static const char* color_blacklist[] = {"Fortnite", "CrosshairX"};
static int color_blacklist_count = sizeof(color_blacklist) / sizeof(color_blacklist[0]);

static const char* pixel_equals_blacklist[] = {"Fortnite", "CrosshairX"};
static int pixel_equals_blacklist_count = sizeof(pixel_equals_blacklist) / sizeof(pixel_equals_blacklist[0]);

static const char* wait_for_pixel_blacklist[] = {"Fortnite", "CrosshairX"};
static int wait_for_pixel_blacklist_count = sizeof(wait_for_pixel_blacklist) / sizeof(wait_for_pixel_blacklist[0]);

static const char* region_blacklist[] = {"Fortnite", "CrosshairX"};
static int region_blacklist_count = sizeof(region_blacklist) / sizeof(region_blacklist[0]);

static const char* calculator_blacklist[] = {"Fortnite", "CrosshairX"};
static int calculator_blacklist_count = sizeof(calculator_blacklist) / sizeof(calculator_blacklist[0]);

static const char* exe_blacklist[] = {"Fortnite", "CrosshairX"};
static int exe_blacklist_count = sizeof(exe_blacklist) / sizeof(exe_blacklist[0]);

static const char* kill_process_blacklist[] = {"Fortnite", "CrosshairX"};
static int kill_process_blacklist_count = sizeof(kill_process_blacklist) / sizeof(kill_process_blacklist[0]);

static const char* google_search_blacklist[] = {"Fortnite", "CrosshairX"};
static int google_search_blacklist_count = sizeof(google_search_blacklist) / sizeof(google_search_blacklist[0]);

static const char* nvim_blacklist[] = {"Fortnite", "CrosshairX"};
static int nvim_blacklist_count = sizeof(nvim_blacklist) / sizeof(nvim_blacklist[0]);

static const char* timer_blacklist[] = {"Fortnite", "CrosshairX"};
static int timer_blacklist_count = sizeof(timer_blacklist) / sizeof(timer_blacklist[0]);

static const char* terminal_blacklist[] = {"Fortnite", "CrosshairX"};
static int terminal_blacklist_count = sizeof(terminal_blacklist) / sizeof(terminal_blacklist[0]);

static const char* screenshot_blacklist[] = {"Fortnite", "CrosshairX"};
static int screenshot_blacklist_count = sizeof(screenshot_blacklist) / sizeof(screenshot_blacklist[0]);

static const char* yazi_blacklist[] = {"Fortnite", "CrosshairX"};
static int yazi_blacklist_count = sizeof(yazi_blacklist) / sizeof(yazi_blacklist[0]);

static const char* video_blacklist[] = {"Fortnite", "CrosshairX"};
static int video_blacklist_count = sizeof(video_blacklist) / sizeof(video_blacklist[0]);

#endif
