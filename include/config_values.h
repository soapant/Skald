#ifndef CONFIG_VALUES_H
#define CONFIG_VALUES_H

#include <stddef.h>
#include <stdbool.h>
#include "core/types/color.h"

extern bool Tray;
extern bool RememberLastIndex;
extern bool MessageAfterEachHotkey;
extern int TimeOutForMessages;
extern int HotkeyRepeatDelayMs;
extern bool DebugMode;
extern bool RoundedCorners;
extern int CornerRadius;
extern int MessageScreenGap;
extern float WindowOpacity;
extern int FontSize;
extern const char *UsedFont;
extern int IconSize;
extern int WidgetGap;
extern int PromptWidth;
extern int PromptHeight;
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
extern const char *RegHotkey;
extern const char *RegAliasHotkey;
extern bool MessagesEnabled;
extern bool EnableAutoPairing;
extern bool EnableBorders;
extern bool RainbowText;
extern bool WindowTitleText;
extern const char *Theme;
extern const char *BackgroundImage;

extern ColorRef COLOR_BG;
extern ColorRef COLOR_CURSOR;
extern ColorRef COLOR_TEXT;
extern ColorRef COLOR_SELECTED_TEXT;
extern ColorRef COLOR_BORDER;

#endif
