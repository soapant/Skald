#ifndef CONFIG_H
#define CONFIG_H
#include <string.h>
#include <windows.h>
#include "types.h"
#include "macro_bindings.h"

#define SWITCHER            VK_SPACE
#define TODO                'T'
#define CALCULATOR          'C'
#define EXE                 'E'
#define KILL_PROCESS        'U'
#define GOOGLE_SEARCH       'G'  
#define TERMINAL            'W'
#define YAZI                'Q'
#define TIMER               'O'
#define EXPLORER            'R'
#define TERMINAL_COMMAND    VK_OEM_4
#define VOLUME              'M'
#define SCREENSHOT          'A'
#define POS                 'Z'
#define COLOR               'X'
#define PIXEL_EQUALS        'B'
#define WAIT_FOR_PIXEL      'N'
#define REGION              'Y'
#define NVIM                'I'
#define MONITOR_FOCUS_MAIN  'L'
#define MONITOR_FOCUS_SIDE  'H'
#define REMINDER            'S'
#define MUTE                'P'
#define MUSIC_PLAYER        'P'
#define VIDEO_VIEWER        'V'


static bool TRAY = true;

static bool COLORFUL_MODE = false;

#define MAX_VISIBLE_ITEMS 60

const char* UsedTerminal = "Alacritty";

const char* BASE_SCREENSHOT_FOLDER_PATH = "C:\\Users\\HP\\Pictures\\Screenshots\\";

const char* TEMP_FILE_PATH = "temp_list_of_positions.md";

const char* UsedImageEditor =  "picview.exe";

const char* UsedFont =  "0xProto Nerd Font";

const int FontSize = 24;

const int NumberFontSize = 18;

const char* SowonPath = "D:\\Projects\\Tscoding Timer\\sowon\\sowon.exe";

const char* MusicPlayerPath = "D:\\Projects\\Music Player\\music_player.exe";

const char* VideoViewerPath = "D:\\Projects\\Video Viewer\\video_viewer.exe";

const char* TextEditorPath = "C:\\Program Files\\Neovide\\neovide.exe";

static const char* MusicPath = "D:\\Music\\Mp3";

static const char* VideosPath = "D:\\videos";

static const char* IconPath = "D:\\retard games\\miscellaneous\\switch_window.ico";

const bool TimerAvailable = true;

const bool MusicPlayerAvailable = true;

const bool VideoViewerAvailable = true;

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
    {'M', HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT, "*", "https:messenger.com" },
};

#define QUICKLINK_HOTKEY_COUNT (sizeof(G_QUICKLINK_HOTKEYS) / sizeof(G_QUICKLINK_HOTKEYS[0]))

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
    MUSIC_PLAYER,
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
    VIDEO_VIEWER,
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

static const TagFormatConfig TODO_TAG_FORMAT = {
    "[",
    "]",
    "|",
    "",
    " "
};

#define TODO_TAG_OPEN "["
#define TODO_TAG_CLOSE "]"
#define TODO_TAG_SEPARATOR "|"
#define TODO_ENTRY_DELIMITER "||"

// ============================================================
// SINGLE SOURCE OF TRUTH: Master flag definitions
// Add/modify flags HERE ONLY - all other arrays are generated
// ============================================================

// Alias flags: flag, display_label
#define TODO_ALIAS_MASTER \
    X("-p", "Project:") \
    X("-n", "Note:")

// Config flags: flag, internal_name, tag_label, color, is_alias
#define TODO_CONFIG_MASTER \
    X("-pc", "Cprogram",         "Cprogram",         RGB(137, 180, 250), true) \
    X("-ph", "HeaderC",          "HeaderC",          RGB(148, 226, 183), true) \
    X("-pm", "MinecraftModpack", "MinecraftModpack", RGB(250, 179, 135), true) \
    X("-pb", "CocBot",           "CocBot",           RGB(249, 226, 175), true) \
    X("-pp", "MusicPlayer",      "MusicPlayer",      RGB(209, 106, 175), true) \
    X("-pv", "VideoViewer",      "VideoViewer",      RGB(219, 226, 105), true) \
    X("-m",  NULL,               "Misc",             RGB(225, 225, 225), false) \
    X("-l",  NULL,               "Learning",         RGB(166, 227, 161), false) \
    X("-b",  NULL,               "Bored",            RGB(203, 166, 247), false) \
    X("-d",  NULL,               "DontForget",       RGB(243, 139, 168), false) \
    X("-v",  NULL,               "NVIM",             RGB(57, 235, 20), false)

// Generate TodoAliasEntry array
#define X(flag, label) {flag, label},
static const TodoAliasEntry TODO_ALIAS_DICT[] = {
    TODO_ALIAS_MASTER
};
#undef X
enum { TODO_ALIAS_DICT_COUNT = sizeof(TODO_ALIAS_DICT) / sizeof(TODO_ALIAS_DICT[0]) };

// Generate GenericFlagEntry for alias flags (for SearchFlagsInText)
#define X(flag, label) {flag, NULL},
static const GenericFlagEntry TODO_ALIAS_FLAGS[] = {
    TODO_ALIAS_MASTER
};
#undef X
enum { TODO_ALIAS_FLAGS_COUNT = sizeof(TODO_ALIAS_FLAGS) / sizeof(TODO_ALIAS_FLAGS[0]) };

// Generate GenericFlagEntry for config flags (for SearchFlagsInText)
#define X(flag, internal, label, color, is_alias) {flag, NULL},
static const GenericFlagEntry TODO_CONFIG_FLAGS[] = {
    TODO_CONFIG_MASTER
};
#undef X
enum { TODO_CONFIG_FLAGS_COUNT = sizeof(TODO_CONFIG_FLAGS) / sizeof(TODO_CONFIG_FLAGS[0]) };

// Generate TodoConfigEntry array (full config)
#define X(flag, internal, label, color, is_alias) {flag, internal, label, color, is_alias},
static const TodoConfigEntry TODO_DICT[] = {
    TODO_CONFIG_MASTER
};
#undef X

#define TODO_CONFIG_FLAGS_ENTRIES TODO_DICT

enum { TODO_DICT_COUNT = sizeof(TODO_DICT) / sizeof(TODO_DICT[0]) };

#endif
