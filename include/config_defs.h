#ifndef CONFIG_DEFS_H
#define CONFIG_DEFS_H

#define CONFIG_VALUES(X)                                                       \
  X(bool, Tray, true)                                                          \
  X(bool, RememberLastIndex, true)                                             \
  X(bool, MessageAfterEachHotkey, false)                                       \
  X(int, TimeOutForMessages, 1)                                                \
  X(int, HotkeyRepeatDelayMs, 300)                                             \
  X(bool, DebugMode, false)                                                    \
  X(bool, CompactMode, false)                                                  \
  X(bool, RoundedCorners, true)                                                \
  X(int, CornerRadius, 14)                                                     \
  X(int, MessageScreenGap, 20)                                                 \
  X(float, WindowOpacity, 235.0)                                               \
  X(const char *, UsedFont, "0xProto Nerd Font")                               \
  X(int, FontSize, 20)                                                         \
  X(int, IconSize, 32)                                                         \
  X(bool, EnableIcons, true)                                                   \
  X(int, WidgetGap, 10)                                                        \
  X(int, PromptW, 900)                                                         \
  X(int, PromptH, 640)                                                         \
  X(int, PromptX, -1)                                                          \
  X(int, PromptY, -1)                                                          \
  X(const char *, UsedTerminal, "")                                            \
  X(const char *, UsedShell, "")                                               \
  X(const char *, UsedBrowser, "")                                             \
  X(const char *, UsedSearchEngine, "google")                                  \
  X(const char *, FeaturePath, "features")                                     \
  X(const char *, IconPath, "resources/Skald.ico")                             \
  X(const char *, ControlHotkey, "Alt+Space")                                  \
  X(const char *, SettingsHotkey, "Alt+D")                                     \
  X(const char *, ReloadFeaturesHotkey, "Alt+R")                               \
  X(const char *, ActionsHotkey, "Ctrl+K")                                     \
  X(const char *, RegHotkey, "Ctrl+Shift+E")                                   \
  X(const char *, RegAliasHotkey, "Ctrl+Shift+A")                              \
  X(const char *, Theme, "")                                                   \
  X(const char *, BackgroundImage, "")                                         \
  X(bool, MessagesEnabled, true)                                               \
  X(bool, EnableAutoPairing, false)                                            \
  X(bool, EnableBorders, true)                                                 \
  X(bool, WindowTitleText, false)                                              \
  X(bool, CloseOnLossOfFocus, false)                                           \
  X(bool, AllowWindowOverride, false)                                          \
  X(ColorRef, COLOR_BG, RGB(13, 13, 17))                                       \
  X(ColorRef, COLOR_TEXT, RGB(226, 230, 236))                                  \
  X(ColorRef, COLOR_SELECTION_HIGHLIGHT, RGB(13, 13, 13))                      \
  X(ColorRef, COLOR_CURSOR, RGB(130, 170, 255))                                \
  X(ColorRef, COLOR_BORDER, RGB(50, 55, 65))

#endif
