
#include <stdbool.h>

#include "config_values.h"
#include "core/types/color.h"

bool Tray = true;
bool EnableEditMode = true;
bool RememberLastIndex = true;
bool MessageAfterEachHotkey = false;
int TimeOutForMessages = 1;
int HotkeyRepeatDelayMs = 300;
bool DebugMode = false;
bool RoundedCorners = true;
int CornerRadius = 14;
float WindowOpacity = 235.0;

char *EditPopUpPos = "middle";
int EditPopUpWidthMin = 400;
int PageUpDownLen = 6;

const char *UsedFont = "0xProto Nerd Font";
int FontSize = 20;
int IconSize = 32;
int WidgetGap = 10;
int MaxFeatures = 1000;
int EditFontSize = 18;
int PaddingForScrolling = 3;
const char *UsedTerminal = "";
const char *UsedShell = "nu";
const char *UsedBrowser = "";
const char *UsedSearchEngine = "google";
const char *FeaturePath = "src/features";
const char *IconPath = "{MISC}/switch_window.ico";
const char *ControlHotkey = "Alt+Space";
const char *ReloadFeaturesHotkey = "Alt+R";
const char *ActionsHotkey = "Ctrl+K";
const char *Theme = "rose-pine";
const char *BackgroundImage = "";

bool FullTextNeedsKeyPress = true;
bool MessagesEnabled = true;
bool EnableAutoPairing = false;
bool EnableBorders = true;
bool RainbowText = false;
bool AlternatingText = false;
bool WindowTitleText = false;
int GradientItems = 30;
float GradientBrightness = 1.0f;
bool CloseOnLossOfFocus = false;

ColorRef COLOR_BG = RGB(40, 40, 40);
ColorRef COLOR_TEXT = RGB(255, 255, 255);
ColorRef COLOR_ALTERNATE = RGB(57, 255, 20);
ColorRef COLOR_SELECTED_TEXT = RGB(0, 0, 0);
ColorRef COLOR_CURSOR = RGB(255, 255, 255);
ColorRef COLOR_ERROR = RGB(180, 0, 0);
ColorRef COLOR_NORMAL = RGB(200, 200, 0);
ColorRef COLOR_TIP = RGB(0, 160, 0);
ColorRef COLOR_INSERT_MODE = RGB(50, 255, 50);
ColorRef COLOR_NORMAL_MODE = RGB(100, 255, 255);
ColorRef COLOR_VISUAL_MODE = RGB(200, 3, 222);
ColorRef COLOR_VISUAL_CURSOR = RGB(255, 165, 0);
ColorRef COLOR_BORDER = RGB(128, 128, 128);
