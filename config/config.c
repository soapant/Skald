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
    DoTimeDebugs = true;
    DebugMode = false;
    RoundedCorners = true;
    CornerRadius = 14;
    EnableShaders = false;
    ShaderPreset = 1;
    ShaderIntensity = 0.3f;
    ShaderBrightness = 0.55f;
    TimeBetweenHotReload = 2;
    WindowOpacity = 235.0f;
    EditPopUpPos = "middle";
    EditPopUpWidthMin = 400;
    PageUpDownLen = 6;
    UsedFont = "0xProto Nerd Font";
    FontSize = 21;
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
    Theme = "github-dark";
    COLOR_BG = RGB(40, 44, 52);
    COLOR_TEXT = RGB(171, 178, 191);
    COLOR_ALTERNATE = RGB(152, 195, 121);
    COLOR_SELECTED_TEXT = RGB(33, 37, 43);
    COLOR_ALIAS_TEXT = RGB(97, 175, 239);
    COLOR_ERROR = RGB(224, 108, 117);
    COLOR_NORMAL = RGB(229, 192, 123);
    COLOR_TIP = RGB(152, 195, 121);
    COLOR_INSERT_MODE = RGB(152, 195, 121);
    COLOR_NORMAL_MODE = RGB(86, 182, 194);
    COLOR_VISUAL_MODE = RGB(198, 120, 221);
    COLOR_VISUAL_CURSOR = RGB(255, 165, 0);
    COLOR_CURSOR = RGB(255, 255, 255);
    G_PATH_VARS = user_path_vars;
    PATH_VAR_COUNT = 6;
    Aliases = user_aliases;
    ALIAS_ENTRY_COUNT = 1;
}
// </flint-config-generated>
