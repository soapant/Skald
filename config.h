#ifndef CONFIG_H
#define CONFIG_H
#include <string.h>
#include <windows.h>

#define HOTKEY_MOD_NONE         0x00
#define HOTKEY_MOD_ALT          0x01
#define HOTKEY_MOD_SHIFT        0x02
#define HOTKEY_MOD_CTRL         0x04
#define HOTKEY_MOD_ALT_SHIFT    (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT)
#define HOTKEY_MOD_ALT_CTRL     (HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL)
#define HOTKEY_MOD_SHIFT_CTRL   (HOTKEY_MOD_SHIFT | HOTKEY_MOD_CTRL)
#define HOTKEY_MOD_ALT_SHIFT_CTRL (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT | HOTKEY_MOD_CTRL)

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

static bool TRAY = true;

static const char* ListOfProgramPaths[20] = {
    "C:\\Users\\HP\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu",
    "C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs",
    "D:\\Projects",
    "D:\\retard games\\miscellaneous"
};

static const char* ListOfShitToIgnore[30] = {
  "node_modules",
  ".git",
  "bin",
  "dll",
  "gtnh 2.8",
  ".h",
  "yazi",
  "obj",
  "build"
};

static const char* MusicPath = "D:\\Music";
static const char* VideosPath = "D:\\videos";
static const char* IconPath = "D:\\retard games\\miscellaneous\\switch_window.ico";

typedef struct {
    const char* name;
    const char* path;
    MappedFile* mf;
} FileEntry;

static FileEntry G_FILE_ENTRIES[] = {
    {"TerminalHistory", "D:\\Projects\\C Code\\TerminalHistory.txt", NULL},
    {"YaziZoxide",      "D:\\Projects\\C Code\\YaziZoxide.txt", NULL},
    {"NvimZoxide",      "D:\\Projects\\C Code\\NvimZoxide.txt", NULL},
    {"TerminalZoxide",      "D:\\Projects\\C Code\\TerminalZoxide.txt", NULL},
    {"ExplorerZoxide",  "D:\\Projects\\C Code\\ExplorerZoxide.txt", NULL},
    {"GoogleZoxide",  "D:\\Projects\\C Code\\GoogleZoxide.txt", NULL},
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

static const AppHotkey G_APP_HOTKEYS[] = {
    { VK_RETURN, HOTKEY_MOD_ALT, "*", "Alacritty" },
    { 'B', HOTKEY_MOD_CTRL | HOTKEY_MOD_SHIFT, "*", "Zen" },
};

#define APP_HOTKEY_COUNT (sizeof(G_APP_HOTKEYS) / sizeof(G_APP_HOTKEYS[0]))

static const MiscHotkey G_MISC_HOTKEYS[] = {
    {'G', HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT, "*", "D:\\retard games\\Grand Theft Auto V Legacy\\PlayGTAV.bat" },
};

#define MISC_HOTKEY_COUNT (sizeof(G_MISC_HOTKEYS) / sizeof(G_MISC_HOTKEYS[0]))

static const QuickLinkHotkey G_QUICKLINK_HOTKEYS[] = {
    {'Y', HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT, "*", "https:youtube.com" },
    {'A', HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT, "*", "https://gemini.google.com/app" },
};

#define QUICKLINK_HOTKEY_COUNT (sizeof(G_QUICKLINK_HOTKEYS) / sizeof(G_QUICKLINK_HOTKEYS[0]))

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

static const char* coding_windows[] = {"Nvim","Neovide","Alacritty","vim","pwsh"};

static const FocusSwitcherBinding G_FOCUS_SWITCHERS[] = {
    {
        'O',           
        HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,               
        focus_blacklist_games,      
        sizeof(focus_blacklist_games) / sizeof(focus_blacklist_games[0]),
        games_windows,              
        sizeof(games_windows) / sizeof(games_windows[0])
    },
    {
        'I',           
        HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,               
        focus_blacklist_browsers,      
        sizeof(focus_blacklist_browsers) / sizeof(focus_blacklist_browsers[0]),
        browsers_windows,              
        sizeof(browsers_windows) / sizeof(browsers_windows[0])
    },
    {
        'U',           
        HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,               
        focus_blacklist_coding,      
        sizeof(focus_blacklist_coding) / sizeof(focus_blacklist_coding[0]),
        coding_windows,              
        sizeof(coding_windows) / sizeof(coding_windows[0])
    },
};

#define FOCUS_SWITCHER_COUNT (sizeof(G_FOCUS_SWITCHERS) / sizeof(G_FOCUS_SWITCHERS[0]))

#endif
