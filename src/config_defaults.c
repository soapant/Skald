
#include <stdbool.h>

#include "config_values.h"
#include "core/types/color.h"

bool Tray = true;
bool RememberLastIndex = true;
bool MessageAfterEachHotkey = false;
int TimeOutForMessages = 1;
int HotkeyRepeatDelayMs = 300;
bool DebugMode = false;
bool RoundedCorners = true;
int CornerRadius = 14;
int MessageScreenGap = 20;
float WindowOpacity = 235.0;

const char *UsedFont = "0xProto Nerd Font";
int FontSize = 20;
int IconSize = 32;
int WidgetGap = 10;
const char *UsedTerminal = "";
const char *UsedShell = "";
const char *UsedBrowser = "";
const char *UsedSearchEngine = "google";
const char *FeaturePath = "features";
const char *IconPath = "skald.ico";
const char *ControlHotkey = "Alt+Space";
const char *ReloadFeaturesHotkey = "Alt+R";
const char *ActionsHotkey = "Ctrl+K";
const char *RegHotkey = "Ctrl+Shift+E";
const char *RegAliasHotkey = "Ctrl+Shift+A";
const char *Theme = "rose-pine";
const char *BackgroundImage = "";

bool MessagesEnabled = true;
bool EnableAutoPairing = false;
bool EnableBorders = true;
bool RainbowText = false;
bool WindowTitleText = false;
bool CloseOnLossOfFocus = false;

ColorRef COLOR_BG = RGB(40, 40, 40);
ColorRef COLOR_TEXT = RGB(255, 255, 255);
ColorRef COLOR_SELECTED_TEXT = RGB(0, 0, 0);
ColorRef COLOR_CURSOR = RGB(255, 255, 255);
ColorRef COLOR_BORDER = RGB(128, 128, 128);
