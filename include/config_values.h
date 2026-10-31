#ifndef CONFIG_VALUES_H
#define CONFIG_VALUES_H

#include <stddef.h>
#include <stdbool.h>
#include "core/types/color.h"

extern bool Tray;
extern bool RememberLastIndex;
extern bool EnableVimMode;
extern bool MessageAfterEachHotkey;
extern int TimeOutForMessages;
extern int HotkeyRepeatDelayMs;
extern bool DoTimeDebugs;
extern bool DebugMode;
extern bool RoundedCorners;
extern int CornerRadius;
extern char *EditPopUpPos;
extern int EditPopUpWidthMin;
extern int PageUpDownLen;
extern float WindowOpacity;
extern int FontSize;
extern const char *UsedFont;
extern int IconSize;
extern int WidgetGap;
extern int MaxFeatures;
extern int VimFontSize;
extern int PaddingForScrolling;
extern bool CloseOnLossOfFocus;
extern const char *UsedTerminal;
extern const char *UsedShell;
extern const char *UsedBrowser;
extern const char *UsedSearchEngine;
extern const char *FeaturePath;
extern const char *IconPath;
extern const char *ControlHotkey;
extern const char *ReloadFeaturesHotkey;
extern const char *ActionsHotkey;
extern bool FullTextNeedsKeyPress;
extern bool MessagesEnabled;
extern bool EnableAutoPairing;
extern bool EnableBorders;
extern bool RainbowText;
extern bool ColoredVimModeText;
extern bool WindowTitleText;
extern bool AlternatingText;
extern bool VimModeText;
extern int GradientItems;
extern float GradientBrightness;
extern const char *Theme;
extern const char *BackgroundImage;

extern ColorRef COLOR_BG;
extern ColorRef COLOR_CURSOR;
extern ColorRef COLOR_TEXT;
extern ColorRef COLOR_ALTERNATE;
extern ColorRef COLOR_SELECTED_TEXT;
extern ColorRef COLOR_ERROR;
extern ColorRef COLOR_NORMAL;
extern ColorRef COLOR_TIP;
extern ColorRef COLOR_INSERT_MODE;
extern ColorRef COLOR_NORMAL_MODE;
extern ColorRef COLOR_VISUAL_CURSOR;
extern ColorRef COLOR_VISUAL_MODE;
extern ColorRef COLOR_BORDER;

#endif
