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
    ShaderIntensity = 0.5f;
    ShaderBrightness = 0.55f;
    TimeBetweenHotReload = 2;
    WindowOpacity = 205.0f;
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
    COLOR_BG = RGB(26, 27, 38);
    COLOR_TEXT = RGB(192, 202, 245);
    COLOR_ALTERNATE = RGB(158, 206, 106);
    COLOR_SELECTED_TEXT = RGB(16, 17, 28);
    COLOR_ALIAS_TEXT = RGB(122, 162, 247);
    COLOR_ERROR = RGB(247, 118, 142);
    COLOR_NORMAL = RGB(224, 175, 104);
    COLOR_TIP = RGB(158, 206, 106);
    COLOR_INSERT_MODE = RGB(158, 206, 106);
    COLOR_NORMAL_MODE = RGB(122, 162, 247);
    COLOR_VISUAL_MODE = RGB(187, 154, 247);
    COLOR_CURSOR = RGB(192, 202, 245);
    G_PATH_VARS = user_path_vars;
    PATH_VAR_COUNT = 6;
    Aliases = user_aliases;
    ALIAS_ENTRY_COUNT = 1;
}
// </flint-config-generated>
