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
    bool DebugMode = false;
bool RoundedCorners = true;
int CornerRadius = 14;
bool EnableShaders = true;
int ShaderPreset = 1;
float ShaderIntensity = 0.55f;
float ShaderBrightness = 0.55f;
int TimeBetweenHotReload = 2;
float WindowOpacity = 235.0;

char* EditPopUpPos = "middle";
int EditPopUpWidthMin = 400;
int PageUpDownLen = 6;

const char* UsedFont =  "0xProto Nerd Font";
int FontSize = 20;
int IconSize = 32;
int WidgetGap = 10;
int MaxFeatures = 1000;
int VimFontSize = 18;
int PaddingForScrolling = 3;
const char* UsedTerminal = "";
const char* UsedShell = "nu";
const char* UsedBrowser = "";
const char* UsedSearchEngine = "google";
const char* FeaturePath = "src/features";
const char* IconPath = "{MISC}/switch_window.ico";
const char* ControlHotkey = "Alt+Space";
const char* Theme = "rose-pine";

static const PathVar default_path_vars[] = {
    {"DEV",          "D:\\Projects",                    "/home/dog/Programming/Projects"},
    {"MISC",         "D:\\retard games\\miscellaneous", "/home/dog/miscellaneous"},
    {"MUSIC",        "D:\\Music",                       "/home/dog/Music"},
    {"VIDEOS",       "D:\\videos",                      "/home/dog/Videos"},
    {"PROGRAMDATA",  "C:\\ProgramData",                 ""},
    {"SystemRoot",   "C:\\Windows",                     ""},
};
const PathVar* G_PATH_VARS = default_path_vars;
unsigned int PATH_VAR_COUNT = ARRAY_LEN(default_path_vars);

bool FullTextNeedsKeyPress = true;
bool MessagesEnabled = true;
bool RainbowText = false;
bool AlternatingText = false;
bool VimModeText = false;
bool WindowTitleText = false;
bool ColoredVimModeText = true;
int GradientItems = 30;
float GradientBrightness = 1.0f;
bool CloseOnLossOfFocus = false;

ColorRef COLOR_BG = RGB(40, 40, 40);
ColorRef COLOR_TEXT = RGB(255, 255, 255);
ColorRef COLOR_ALTERNATE = RGB(57, 255, 20);
ColorRef COLOR_SELECTED_TEXT = RGB(0, 0, 0);
ColorRef COLOR_CURSOR = RGB(255, 255, 255);
ColorRef COLOR_ALIAS_TEXT = RGB(255, 255, 255);
ColorRef COLOR_ERROR = RGB(180, 0, 0);
ColorRef COLOR_NORMAL = RGB(200, 200, 0);
ColorRef COLOR_TIP = RGB(0, 160, 0);
ColorRef COLOR_INSERT_MODE = RGB(50, 255, 50);
ColorRef COLOR_NORMAL_MODE = RGB(100, 255, 255);
ColorRef COLOR_VISUAL_MODE = RGB(200, 3, 222);

static const AliasEntry default_aliases[] = {
  {"brw","Brawlhalla","AppLauncher"},
};
const AliasEntry* Aliases = default_aliases;
size_t ALIAS_ENTRY_COUNT = ARRAY_LEN(default_aliases);
