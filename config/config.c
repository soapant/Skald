#include "config.h"

// <flint-config-generated>
static const PathVar user_path_vars[] = {
    {"DEV", "C:\\Programming\\Projects", "/home/dog/Programming/Projects"},
    {"MISC", "D:\\retard games\\miscellaneous", "/home/dog/miscellaneous"},
    {"MUSIC", "C:\\Music", "/home/dog/Music"},
    {"VIDEOS", "C:\\videos", "/home/dog/Videos"},
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
    DoTimeDebugs = false;
    DebugMode = true;
    RoundedCorners = false;
    CornerRadius = 14;
    EnableShaders = false;
    ShaderPreset = 1;
    ShaderIntensity = 0.5f;
    ShaderBrightness = 0.55f;
    TimeBetweenHotReload = 2;
    WindowOpacity = 205.0f;
    EditPopUpPos = "middle";
    EditPopUpWidthMin = 400;
    PageUpDownLen = 6;
    UsedFont = "0xProto Nerd Font";
    FontSize = 22;
    IconSize = 32;
    WidgetGap = 0;
    MaxFeatures = 1000;
    VimFontSize = 18;
    PaddingForScrolling = 3;
    UsedTerminal = "";
    UsedShell = "nu";
    UsedBrowser = "";
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
    COLOR_BG = RGB(40, 40, 40);
    COLOR_TEXT = RGB(235, 219, 178);
    COLOR_ALTERNATE = RGB(184, 187, 38);
    COLOR_SELECTED_TEXT = RGB(30, 30, 30);
    COLOR_ALIAS_TEXT = RGB(131, 165, 152);
    COLOR_ERROR = RGB(251, 73, 52);
    COLOR_NORMAL = RGB(250, 189, 47);
    COLOR_TIP = RGB(184, 187, 38);
    COLOR_INSERT_MODE = RGB(184, 187, 38);
    COLOR_NORMAL_MODE = RGB(131, 165, 152);
    COLOR_VISUAL_MODE = RGB(211, 134, 155);
    COLOR_CURSOR = RGB(235, 219, 178);
    G_PATH_VARS = user_path_vars;
    PATH_VAR_COUNT = 6;
    Aliases = user_aliases;
    ALIAS_ENTRY_COUNT = 1;
}
// </flint-config-generated>
