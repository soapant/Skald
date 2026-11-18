#ifndef CONFIG_DEFS_H
#define CONFIG_DEFS_H

#define SKALD_CONFIG_VALUES(X)                                                 \
  X(bool, Tray, true)                                                          \
  X(bool, RememberLastIndex, true)                                             \
  X(bool, MessageAfterEachHotkey, false)                                       \
  X(int, TimeOutForMessages, 1)                                                \
  X(int, HotkeyRepeatDelayMs, 300)                                             \
  X(bool, DebugMode, false)                                                    \
  X(bool, RoundedCorners, true)                                                \
  X(int, CornerRadius, 14)                                                     \
  X(int, MessageScreenGap, 20)                                                 \
  X(float, WindowOpacity, 235.0)                                               \
  X(const char *, UsedFont, "0xProto Nerd Font")                               \
  X(int, FontSize, 20)                                                         \
  X(int, IconSize, 32)                                                         \
  X(int, WidgetGap, 10)                                                        \
  X(int, PromptWidth, 900)                                                     \
  X(int, PromptHeight, 640)                                                    \
  X(int, PromptX, -1)                                                          \
  X(int, PromptY, -1)                                                          \
  X(const char *, UsedTerminal, "")                                            \
  X(const char *, UsedShell, "")                                               \
  X(const char *, UsedBrowser, "")                                             \
  X(const char *, UsedSearchEngine, "google")                                  \
  X(const char *, FeaturePath, "features")                                     \
  X(const char *, IconPath, "resources/Skald.ico")                             \
  X(const char *, ControlHotkey, "Alt+Space")                                  \
  X(const char *, SettingsHotkey, "Alt+D")                                \
  X(const char *, ReloadFeaturesHotkey, "Alt+R")                               \
  X(const char *, ActionsHotkey, "Ctrl+K")                                     \
  X(const char *, RegHotkey, "Ctrl+Shift+E")                                   \
  X(const char *, RegAliasHotkey, "Ctrl+Shift+A")                              \
  X(const char *, Theme, "")                                                   \
  X(const char *, BackgroundImage, "")                                         \
  X(bool, MessagesEnabled, true)                                               \
  X(bool, EnableAutoPairing, false)                                            \
  X(bool, EnableBorders, true)                                                 \
  X(bool, RainbowText, false)                                                  \
  X(bool, WindowTitleText, false)                                              \
  X(bool, CloseOnLossOfFocus, false)                                           \
  X(ColorRef, COLOR_BG, RGB(15, 15, 15))                                       \
  X(ColorRef, COLOR_TEXT, RGB(255, 255, 255))                                  \
  X(ColorRef, COLOR_SELECTED_TEXT, RGB(0, 0, 0))                               \
  X(ColorRef, COLOR_CURSOR, RGB(255, 255, 255))                                \
  X(ColorRef, COLOR_BORDER, RGB(128, 128, 128))

#endif
