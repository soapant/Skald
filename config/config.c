#include "config.h"

// <flint-config-generated>
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
    FontSize = 24;
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
    COLOR_BG = RGB(13, 17, 23);
    COLOR_TEXT = RGB(201, 209, 217);
    COLOR_ALTERNATE = RGB(63, 185, 80);
    COLOR_SELECTED_TEXT = RGB(22, 27, 34);
    COLOR_ALIAS_TEXT = RGB(88, 166, 255);
    COLOR_ERROR = RGB(248, 81, 73);
    COLOR_NORMAL = RGB(210, 153, 34);
    COLOR_TIP = RGB(63, 185, 80);
    COLOR_INSERT_MODE = RGB(63, 185, 80);
    COLOR_NORMAL_MODE = RGB(88, 166, 255);
    COLOR_VISUAL_MODE = RGB(188, 140, 255);
    COLOR_VISUAL_CURSOR = RGB(255, 165, 0);
    COLOR_CURSOR = RGB(201, 209, 217);
    Aliases = user_aliases;
    ALIAS_ENTRY_COUNT = 1;
}
// </flint-config-generated>
