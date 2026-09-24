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
bool DebugMode = true;
bool RoundedCorners = true;
int CornerRadius = 14;
bool EnableShaders = true;
int ShaderPreset = 1;
float ShaderIntensity = 0.55f;
int TimeBetweenHotReload = 2;
float WindowOpacity = 235.0;

char* EditPopUpPos = "middle";
int EditPopUpWidthMin = 400;
int PageUpDownLen = 6;

const char* UsedFont =  "0xProto Nerd Font";
int FontSize = 20;
const int IconSize = 32;
const int MaxFeatures = 1000;
const int VimFontSize = 18;
const int PaddingForScrolling = 3;
const char* UsedTerminal = "";
const char* UsedShell = "nu";
const char* UsedBrowser = "zen-browser";
const char* UsedImageViewer = "feh";
const char* UsedVideoPlayer = "";
const char* UsedSearchEngine = "google";
const char* FeaturePath = "src/features";
const char* IconPath = "{MISC}/switch_window.ico";
const char* ControlHotkey = "Alt+Space";

const PathVar G_PATH_VARS[] = {
    {"DEV",          "D:\\Projects",                    "/home/diggy/Programming/Projects"},
    {"MISC",         "D:\\retard games\\miscellaneous", "/home/diggy/miscellaneous"},
    {"MUSIC",        "D:\\Music",                       "/home/diggy/Music"},
    {"VIDEOS",       "D:\\videos",                      "/home/diggy/Videos"},
    {"PROGRAMDATA",  "C:\\ProgramData",                 ""},
    {"SystemRoot",   "C:\\Windows",                     ""},
};
const unsigned int PATH_VAR_COUNT = ARRAY_LEN(G_PATH_VARS);

const bool FullTextNeedsKeyPress = true;
const bool MessagesEnabled = true;
const bool RainbowText = false;
const bool AlternatingText = false;
const bool VimModeText = false;
const bool WindowTitleText = false;
const bool ColoredVimModeText = true;
const int GradientItems = 30;
const float GradientBrightness = 1.0f;
bool CloseOnLossOfFocus = false;

const AliasEntry Aliases[] = {
  {"brw","Brawlhalla","AppLauncher"},
};
const size_t ALIAS_ENTRY_COUNT = ARRAY_LEN(Aliases);
