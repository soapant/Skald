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
const char* UsedShell = "nu";
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
const unsigned int PATH_VAR_COUNT = ARRAY_LEN(G_PATH_VARS);

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

const AliasEntry Aliases[] = {
  {"brw","Brawlhalla","AppLauncher"},
};
const size_t ALIAS_ENTRY_COUNT = ARRAY_LEN(Aliases);

// Per-feature TODO config moved to include/features/todo.h
