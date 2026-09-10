#include "config.h"

bool Tray = true;
bool EnableVimMode = true;
bool RememberLastIndex = true;
bool KeepSelectionAfterClose = false;
bool MessageAfterEachHotkey = false;
bool AllowOtherFeatureTogglesInFeature = false;
bool ModePersistsThroughFeatures = false;
int  TimeOutForMessages = 1;
bool DoTimeDebugs = true;
bool RoundedCorners = true;
int CornerRadius = 14;
bool EnableShaders = true;
int ShaderPreset = 1; // 0 solid, 1 vertical, 2 radial, 3 diagonal, 4 noise
bool EnableSelectionShaders = false;
int SelectionShaderPreset = 1; // 0 solid, 1 vertical, 2 radial, 3 diagonal, 4 noise
float ShaderIntensity = 0.25f; // 0.0-1.0 how strong the gradient is (0.35 subtle, 0.65 visible, 1.0 punchy)
float ShaderSmoothness = 5.0f; // 0.2 blocky (few rects) -> 5.0 ultra smooth, 1.0 = gradient spans full window
//right now this is doing nothing
char* EditPopUpPos = "middle";
int EditPopUpWidthMin = 400;
int PageUpDownLen = 6;
bool ColorfullMode = false;
float WindowOpacity = 235.0;

const char* UsedFont =  "0xProto Nerd Font";
const int FontSize = 20;
const int MaxFeatures = 130;
const int NumberFontSize = 18;
const int VimFontSize = 18;
const int PaddingForScrolling = 3;
const char* UsedTerminal = "foot";
const char* UsedShell = "nu";
const char* UsedFileManager = "thunar";
const char* UsedTextEditor = "nvim";
const char* UsedBrowser = "zen-browser";
const char* UsedSearchEngine = "google";
const char* ImageEditorPath =  "{APPDATA}/Microsoft/Windows/Start Menu/Programs/Scoop Apps/PicView.lnk";
const char* FeaturePath = "src/features";
const char* IconPath = "{MISC}/switch_window.ico";
const char* ControlHotkey = "Alt+Space";

const PathVar G_PATH_VARS[] = {
    {"DEV",          "D:\\Projects",                    "/home/diggy/Programming/Projects"},
    {"MISC",         "D:\\retard games\\miscellaneous", "/home/diggy/miscellaneous"},
    {"MUSIC",        "D:\\Music",                       "/home/diggy/Music"},
    {"VIDEOS",       "D:\\videos",                      "/home/diggy/Videos"},
    {"PROGRAMDATA",  "C:\\ProgramData",                 ""}, // Not A Thing On Linux
    {"SystemRoot",   "C:\\Windows",                     ""}, // Not A Thing On Linux
};
const unsigned int PATH_VAR_COUNT = ARRAY_LEN(G_PATH_VARS);

const bool FullTextNeedsKeyPress = true;
const bool MessagesEnabled = true;
const bool RainbowText = false;
const bool AlternatingText = true;
const bool EnableLineNumbers = true;
const bool ColoredVimModeText = true;
const int GradientItems = 30;
const float GradientBrightness = 1.0f;
bool CloseOnLossOfFocus = false;

const AliasEntry Aliases[] = {
  {"brw","Brawlhalla","AppLauncher"},
};
const size_t ALIAS_ENTRY_COUNT = ARRAY_LEN(Aliases);

