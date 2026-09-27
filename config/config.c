#include "config.h"

// <flint-config-generated>
static const PathVar user_path_vars[] = {
    {"DEV", "C:\\Programming\\Projects", "/home/diggy/Programming/Projects"},
    {"MISC", "D:\\retard games\\miscellaneous", "/home/diggy/miscellaneous"},
    {"MUSIC", "C:\\Music", "/home/diggy/Music"},
    {"VIDEOS", "C:\\videos", "/home/diggy/Videos"},
    {"PROGRAMDATA", "C:\\ProgramData", ""},
    {"SystemRoot", "C:\\Windows", ""},
};

static const AliasEntry user_aliases[] = {
    {"brw", "Brawlhalla", "AppLauncher"},
};

void ConfigApply(void) {
    Tray = true;
    EnableVimMode = true;
    RememberLastIndex = true;
    KeepSelectionAfterClose = false;
    MessageAfterEachHotkey = false;
    AllowOtherFeatureTogglesInFeature = false;
    ModePersistsThroughFeatures = false;
    TimeOutForMessages = 1;
    DoTimeDebugs = true;
    DebugMode = false;
    RoundedCorners = true;
    CornerRadius = 14;
    EnableShaders = true;
    ShaderPreset = 1;
    ShaderIntensity = 0.5f;
    TimeBetweenHotReload = 2;
    WindowOpacity = 235.0f;
    EditPopUpPos = "middle";
    EditPopUpWidthMin = 400;
    PageUpDownLen = 6;
    UsedFont = "0xProto Nerd Font";
    FontSize = 20;
    IconSize = 32;
    MaxFeatures = 1000;
    VimFontSize = 18;
    PaddingForScrolling = 3;
    UsedTerminal = "";
    UsedShell = "nu";
    UsedBrowser = "";
    UsedImageViewer = "";
    UsedVideoPlayer = "";
    UsedSearchEngine = "google";
    FeaturePath = "src/features";
    IconPath = "{MISC}/switch_window.ico";
    ControlHotkey = "Alt+Space";
    FullTextNeedsKeyPress = true;
    MessagesEnabled = true;
    RainbowText = false;
    AlternatingText = false;
    VimModeText = false;
    WindowTitleText = false;
    ColoredVimModeText = true;
    GradientItems = 30;
    GradientBrightness = 1.0f;
    CloseOnLossOfFocus = false;
    Theme = "gruvbox";
    COLOR_BG = RGB(25, 23, 36);
    COLOR_TEXT = RGB(255, 255, 255);
    COLOR_ALTERNATE = RGB(57, 255, 20);
    COLOR_SELECTED_TEXT = RGB(19, 21, 23);
    COLOR_ALIAS_TEXT = RGB(255, 255, 255);
    COLOR_ERROR = RGB(180, 0, 0);
    COLOR_NORMAL = RGB(200, 200, 0);
    COLOR_TIP = RGB(0, 160, 0);
    COLOR_INSERT_MODE = RGB(50, 255, 50);
    COLOR_NORMAL_MODE = RGB(100, 255, 255);
    COLOR_VISUAL_MODE = RGB(200, 3, 222);
    COLOR_CURSOR = RGB(82, 79, 103);
    G_PATH_VARS = user_path_vars;
    PATH_VAR_COUNT = 6;
    Aliases = user_aliases;
    ALIAS_ENTRY_COUNT = 1;
}
// </flint-config-generated>
