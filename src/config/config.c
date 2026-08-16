#include "config.h"

bool Tray = true;
bool RememberLastIndex = true;
bool KeepSelectionAfterClose = false;
bool MessageAfterEachHotkey = false;
bool AllowOtherFeatureTogglesInFeature = false;
bool ModePersistsThroughFeatures = false;
int  TimeOutForMessages = 1;
bool DoTimeDebugs = false;
char* EditPopUpPos = "middle";
int EditPopUpWidthMin = 400;
int PageUpDownLen = 6;
bool ColorfullMode = false;
bool SearchBoxDiffColor = false;

const char* BASE_SCREENSHOT_FOLDER_PATH = "C:\\Users\\HP\\Pictures\\Screenshots\\";
const char* TEMP_FILE_PATH = DATA_DIR "temp_list_of_positions.md";
const char* UsedFont =  "0xProto Nerd Font";
const int FontSize = 24;
const int NumberFontSize = 18;
const int PaddingForScrolling = 3;
const char* UsedTerminal = "Alacritty";
const char* ImageEditorPath =  "C:\\Users\\HP\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Scoop Apps\\PicView.lnk";
const char* TimerPath = "D:\\Projects\\Tscoding Timer\\sowon\\sowon.exe";
const char* MusicPlayerPath = "D:\\Projects\\Music Player\\music_player.exe";
const char* VideoViewerPath = "D:\\Projects\\Video Viewer\\video_viewer.exe";
const char* TextEditorPath = "C:\\Program Files\\Neovide\\neovide.exe";
const char* MusicPath = "D:\\Music\\Mp3";
const char* VideosPath = "D:\\videos";
const char* IconPath = "D:\\retard games\\miscellaneous\\switch_window.ico";

const bool TimerAvailable = true;
const bool MusicPlayerAvailable = true;
const bool VideoViewerAvailable = true;
const bool ReplicaAvailable = true;
const bool FullTextNeedsKeyPress = true;
const bool MessagesEnabled = true;
const bool RainbowText = true;
const int GradientItems = 30;
const float GradientBrightness = 1.0f;
bool PixelHotkeys = false;

const char* ListOfProgramPaths[20] = {
    "C:\\Users\\HP\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu",
    "C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs",
    "D:\\Projects",
    "D:\\retard games\\miscellaneous",
    "C:\\Windows\\System32\\",             
    "shell:AppsFolder",                   
};

const PathEntry SystemShortcuts[] = {
    {"control.exe", "Control Panel"},
    {"devmgmt.msc", "Device Manager"},
    {"compmgmt.msc", "Computer Management"},
    {"diskmgmt.msc", "Disk Management"},
    {"services.msc", "Services"},
    {"taskmgr.exe", "Task Manager"},
    {"notepad.exe", "Notepad"},
    {"calc.exe", "Calculator"},
    {"mspaint.exe", "Paint"},
    {"cmd.exe", "Command Prompt"},
    {"powershell.exe", "PowerShell"},
    {"msconfig.exe", "System Configuration"},
    {"dxdiag.exe", "DirectX Diagnostic Tool"},
    {"sysdm.cpl", "System Properties"},
    {"inetcpl.cpl", "Internet Options"},
    {"appwiz.cpl", "Programs and Features"},
    {"timedate.cpl", "Date and Time"},
    {"desk.cpl", "Display Settings"},
    {"main.cpl", "Mouse Properties"}
};

const char* ListOfShitToIgnore[30] = {
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

const AppHotkey G_APP_HOTKEYS[] = {
    { VK_RETURN, HOTKEY_MOD_ALT, "*", "Alacritty" },
    { 'B', HOTKEY_MOD_CTRL | HOTKEY_MOD_SHIFT, "*", "Zen" },
};
const unsigned int APP_HOTKEY_COUNT = sizeof(G_APP_HOTKEYS) / sizeof(G_APP_HOTKEYS[0]);

const MiscHotkey G_MISC_HOTKEYS[] = {
    {'G', HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT, "*", "D:\\retard games\\Grand Theft Auto V Legacy\\PlayGTAV.bat" },
    {'O', HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT, "*", "C:\\Users\\HP\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\OnTopReplica\\OnTopReplica.lnk"}
};
const unsigned int MISC_HOTKEY_COUNT = sizeof(G_MISC_HOTKEYS) / sizeof(G_MISC_HOTKEYS[0]);

const QuickLinkHotkey G_QUICKLINK_HOTKEYS[] = {
    {'Y', HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT, "*", "https://youtube.com" },
    {'M', HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT, "*", "https://messenger.com" },
};
const unsigned int QUICKLINK_HOTKEY_COUNT = sizeof(G_QUICKLINK_HOTKEYS) / sizeof(G_QUICKLINK_HOTKEYS[0]);

const TextSnippet G_TEXT_SNIPPETS[] = {
    { 'O', HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, "EpicGamesLauncher", "seifisbad@gmail.com" },
    { 'P', HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, "EpicGamesLauncher", "i_am_retarded1" },
};
const unsigned int TEXT_SNIPPET_COUNT = sizeof(G_TEXT_SNIPPETS) / sizeof(G_TEXT_SNIPPETS[0]);

const MusicPlayerBinding G_MUSIC_PLAYERS[] = {
  { VK_OEM_COMMA, HOTKEY_MOD_ALT, 1 },
  { VK_OEM_2, HOTKEY_MOD_ALT, 5 },
  { VK_OEM_1, HOTKEY_MOD_ALT, 6 },
  { VK_OEM_7, HOTKEY_MOD_ALT, 2 },
  { MUSIC_PLAYER_KEY, HOTKEY_MOD_ALT, 69 },
};

const unsigned int MUSIC_PLAYER_COUNT = sizeof(G_MUSIC_PLAYERS) / sizeof(G_MUSIC_PLAYERS[0]);

const VideoPlayerBinding G_VIDEO_PLAYERS[] = {
  { VK_OEM_COMMA, HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, 1 },
  { VK_OEM_PERIOD, HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, 5 },
  { VK_OEM_1, HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, 6 },
  { VIDEO_VIEWER_KEY, HOTKEY_MOD_ALT, 69 },
};

const unsigned int VIDEO_PLAYER_COUNT = sizeof(G_VIDEO_PLAYERS) / sizeof(G_VIDEO_PLAYERS[0]);

const char* bkp_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Steam"};

const BulkKillProcessBinding G_BULK_KILL_PROCESS[] = {
    {
        'P',           
        HOTKEY_MOD_ALT | HOTKEY_MOD_CTRL,               
        bkp_windows,              
        sizeof(bkp_windows) / sizeof(bkp_windows[0])
    },
};
const unsigned int BULKKILLPROCESS_COUNT = sizeof(G_BULK_KILL_PROCESS) / sizeof(G_BULK_KILL_PROCESS[0]);

const char* focus_blacklist_games[] = {"CrosshairX"};
const char* games_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Minecraft"};
const char* focus_blacklist_browsers[] = {"CrosshairX"};
const char* browsers_windows[] = {"Zen","Chrome"};
const char* focus_blacklist_coding[] = {"CrosshairX"};
const char* coding_windows[] = {"Neovide","vim"};
const char* focus_blacklist_terminal[] = {"CrosshairX"};
const char* terminal_windows[] = {"Alacritty","pwsh"};

const FocusSwitcherBinding G_FOCUS_SWITCHERS[] = {
    { 'O', HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,
      focus_blacklist_games, sizeof(focus_blacklist_games) / sizeof(focus_blacklist_games[0]),
      games_windows, sizeof(games_windows) / sizeof(games_windows[0]) },
    { 'I', HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,
      focus_blacklist_browsers, sizeof(focus_blacklist_browsers) / sizeof(focus_blacklist_browsers[0]),
      browsers_windows, sizeof(browsers_windows) / sizeof(browsers_windows[0]) },
    { 'U', HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,
      focus_blacklist_coding, sizeof(focus_blacklist_coding) / sizeof(focus_blacklist_coding[0]),
      coding_windows, sizeof(coding_windows) / sizeof(coding_windows[0]) },
    { 'L', HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT,
      focus_blacklist_terminal, sizeof(focus_blacklist_terminal) / sizeof(focus_blacklist_terminal[0]),
      terminal_windows, sizeof(terminal_windows) / sizeof(terminal_windows[0]) },
};

const unsigned int FOCUS_SWITCHER_COUNT = sizeof(G_FOCUS_SWITCHERS) / sizeof(G_FOCUS_SWITCHERS[0]);

const AliasEntry Aliases[] = {
  {"brw","Brawlhalla","AppLauncher"},
};
const size_t ALIAS_ENTRY_COUNT = ARRAY_LEN(Aliases);

const TagFormatConfig TODO_TAG_FORMAT = {
    "[",
    "]",
    "|",
    "",
    " "
};

#define TODO_ALIAS_MASTER \
    X("-p", "Project:") \
    X("-n", "Note:")

#define TODO_CONFIG_MASTER \
    X("-pc", "Cprogram",         "Cprogram",         RGB(137, 180, 250), true) \
    X("-ph", "HeaderC",          "HeaderC",          RGB(148, 226, 183), true) \
    X("-pm", "MinecraftModpack", "MinecraftModpack", RGB(250, 179, 135), true) \
    X("-pb", "CocBot",           "CocBot",           RGB(249, 226, 175), true) \
    X("-pp", "MusicPlayer",      "MusicPlayer",      RGB(209, 106, 175), true) \
    X("-pv", "VideoViewer",      "VideoViewer",      RGB(219, 226, 105), true) \
    X("-pd", "Ded",              "Ded",              RGB(19,  226, 105), true) \
    X("-pi", "IndieGame",     "IndieGame",     RGB(19,  26, 105), true) \
    X("-m",  NULL,               "Misc",             RGB(225, 225, 225), false) \
    X("-l",  NULL,               "Learning",         RGB(166, 227, 161), false) \
    X("-b",  NULL,               "Bored",            RGB(203, 166, 247), false) \
    X("-d",  NULL,               "DontForget",       RGB(255, 109, 108), false) \
    X("-my", NULL,               "Maybe",            RGB(150, 5,   142), false) \
    X("-bg", NULL,               "Bug",              RGB(250, 9,   2),   false) \
    X("-v",  NULL,               "Nvim",             RGB(57,  235, 20),  false) \
    X("-f",  NULL,               "Fun",              RGB(210, 9,   102), false) \
    X("-r",  NULL,               "Research",         RGB(235, 60,  235), false) \
    X("-u",  NULL,               "Urgent",           RGB(255, 0,   35),  false) \
    X("-ne", NULL,               "NotImplemetedYet", RGB(155, 255, 35),  false) \
    X("-ft", NULL,               "Feature",          RGB(250, 205, 102), false) \
    X("-c",  NULL,               "Change",           RGB(150, 105, 12),  false) \


#define X(flag, label) {flag, label},
const TodoAliasEntry TODO_ALIAS_DICT[] = {
    TODO_ALIAS_MASTER
};
const size_t TODO_ALIAS_DICT_COUNT = ARRAY_LEN(TODO_ALIAS_DICT);
#undef X

#define X(flag, label) {flag, NULL},
const GenericFlagEntry TODO_ALIAS_FLAGS[] = {
    TODO_ALIAS_MASTER
};
const size_t TODO_ALIAS_FLAGS_COUNT = ARRAY_LEN(TODO_ALIAS_FLAGS);
#undef X

#define X(flag, internal, label, color, is_alias) {flag, NULL},
const GenericFlagEntry TODO_CONFIG_FLAGS[] = {
    TODO_CONFIG_MASTER
};
const size_t TODO_CONFIG_FLAGS_COUNT = ARRAY_LEN(TODO_CONFIG_FLAGS);
#undef X

#define X(flag, internal, label, color, is_alias) {flag, internal, label, color, is_alias},
const TodoConfigEntry TODO_DICT[] = {
    TODO_CONFIG_MASTER
};
const size_t TODO_DICT_COUNT = ARRAY_LEN(TODO_DICT);
#undef X
