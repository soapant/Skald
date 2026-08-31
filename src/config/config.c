#include "config.h"

bool Tray = true;
bool EnableVimMode = true;
bool RememberLastIndex = true;
bool KeepSelectionAfterClose = false;
bool MessageAfterEachHotkey = false;
bool AllowOtherFeatureTogglesInFeature = false;
bool ModePersistsThroughFeatures = false;
int  TimeOutForMessages = 1;
bool DoTimeDebugs = false;
//right now this is doing nothing
char* EditPopUpPos = "middle";
int EditPopUpWidthMin = 400;
int PageUpDownLen = 6;
bool ColorfullMode = false;
bool SearchBoxDiffColor = false; 
float WindowOpacity = 235.0;
const wchar_t* HttpUrlForNotis = L"doingthistomakeitrarebitch";

const char* BASE_SCREENSHOT_FOLDER_PATH = "{PICTURES}/Screenshots/";
const char* TEMP_FILE_PATH = DATA_DIR "temp_list_of_positions.md";
const char* UsedFont =  "0xProto Nerd Font";
const int FontSize = 20;
const int NumberFontSize = 18;
const int VimFontSize = 18;
const int PaddingForScrolling = 3;
const char* UsedTerminal = "foot";
const char* ImageEditorPath =  "{APPDATA}/Microsoft/Windows/Start Menu/Programs/Scoop Apps/PicView.lnk"; // Not Implemented on Linux
const char* TimerPath = "{DEV}/Tscoding Timer/sowon/sowon.exe"; 
const char* MusicPlayerPath = "{DEV}/Music Player/music_player.exe";
const char* VideoViewerPath = "{DEV}/Video Viewer/video_viewer.exe";
const char* TextEditorPath = "{PROGRAM_FILES}/bin/nvim";
const char* MusicPath = "{MUSIC}";
const char* VideosPath = "{VIDEOS}";
const char* IconPath = "{MISC}/switch_window.ico";

const PathVar G_PATH_VARS[] = {
    {"DEV",          "D:\\Projects",                    "/home/diggy/Programming/Projects"},
    {"MISC",         "D:\\retard games\\miscellaneous", "/home/diggy/miscellaneous"},
    {"MUSIC",        "D:\\Music",                       "/home/diggy/Music"},
    {"VIDEOS",       "D:\\videos",                      "/home/diggy/Videos"},
    {"PROGRAMDATA",  "C:\\ProgramData",                 ""}, // Not A Thing On Linux
    {"SystemRoot",   "C:\\Windows",                     ""}, // Not A Thing On Linux
};
const unsigned int PATH_VAR_COUNT = sizeof(G_PATH_VARS) / sizeof(G_PATH_VARS[0]);

const bool TimerAvailable = true;
const bool MusicPlayerAvailable = true;
const bool VideoViewerAvailable = true;
const bool ReplicaAvailable = false;
const bool FullTextNeedsKeyPress = true;
const bool MessagesEnabled = true;
const bool RainbowText = false;
const bool AlternatingText = true;
const bool EnableLineNumbers = true;
const bool ColoredVimModeText = true;
const int GradientItems = 30;
const float GradientBrightness = 1.0f;
bool PixelHotkeys = false;
bool CloseOnLossOfFocus = false;

// Per-feature hotkey bindings moved to their feature files:
// G_TERMINAL_COMMAND_HOTKEYS -> src/features/terminal_command.c
// G_APP_HOTKEYS               -> src/features/app_launcher.c
// G_MISC_HOTKEYS              -> src/features/misc_hotkeys.c
// G_QUICKLINK_HOTKEYS         -> src/features/quicklink.c
// G_TEXT_SNIPPETS             -> src/features/text_snippets.c
// G_MUSIC_PLAYERS             -> src/features/music_player.c
// G_VIDEO_PLAYERS             -> src/features/video_viewer.c
// G_BULK_KILL_PROCESS         -> src/features/process_killer.c
// G_FOCUS_SWITCHERS           -> src/features/focus_switcher.c

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
