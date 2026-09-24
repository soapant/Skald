#ifndef CONFIG_H
#define CONFIG_H
#include <string.h>
#include <stddef.h>
#include <wchar.h>
#include "core/types.h"

#define EDIT_TEXT_KEY       'E'
#define HELP_SCREEN_KEY     'H'
#define COPY_SELECTION_KEY  'Y'
#define SETTINGS_KEY        'D'
#define COMMAND_MODE_KEY        VK_OEM_1
#define SEARCH_PROGRAM_KEY      VK_OEM_2

extern bool Tray;
extern bool RememberLastIndex;
extern bool EnableVimMode;
extern bool KeepSelectionAfterClose;
extern bool MessageAfterEachHotkey;
extern bool AllowOtherFeatureTogglesInFeature;
extern bool ModePersistsThroughFeatures;
extern int  TimeOutForMessages;
extern bool DoTimeDebugs;
extern bool DebugMode;
extern bool RoundedCorners;
extern int CornerRadius;
extern bool EnableShaders;
extern int ShaderPreset;
extern float ShaderIntensity;
extern char* EditPopUpPos;
extern int EditPopUpWidthMin;
extern int PageUpDownLen;
extern int TimeBetweenHotReload;
extern float WindowOpacity;
extern int FontSize;
extern const char* UsedFont;
extern int IconSize;
extern int MaxFeatures;
extern int VimFontSize;
extern int PaddingForScrolling;
extern bool CloseOnLossOfFocus;
extern const char* UsedTerminal;
extern const char* UsedShell;
extern const char* UsedBrowser;
extern const char* UsedImageViewer;
extern const char* UsedVideoPlayer;
extern const char* UsedSearchEngine;
extern const char* FeaturePath;
extern const char* IconPath;
extern const char* ControlHotkey;
extern bool FullTextNeedsKeyPress;
extern bool MessagesEnabled;
extern bool RainbowText;
extern bool ColoredVimModeText;
extern bool WindowTitleText;
extern bool AlternatingText;
extern bool VimModeText;
extern int GradientItems;
extern float GradientBrightness;

extern ColorRef COLOR_BG;
extern ColorRef COLOR_TEXT;
extern ColorRef COLOR_ALTERNATE;
extern ColorRef COLOR_SELECTED_TEXT;
extern ColorRef COLOR_ALIAS_TEXT;
extern ColorRef COLOR_ERROR;
extern ColorRef COLOR_NORMAL;
extern ColorRef COLOR_TIP;
extern ColorRef COLOR_INSERT_MODE;
extern ColorRef COLOR_NORMAL_MODE;
extern ColorRef COLOR_VISUAL_MODE;
#define COLOR_INSERT_MODE_TEXT (ColoredVimModeText ? COLOR_INSERT_MODE : COLOR_TEXT)
#define COLOR_NORMAL_MODE_TEXT (ColoredVimModeText ? COLOR_NORMAL_MODE : COLOR_TEXT)
#define COLOR_VISUAL_MODE_TEXT (ColoredVimModeText ? COLOR_VISUAL_MODE : COLOR_TEXT)

typedef struct {
    const char* path;
    const char* name;
} PathEntry;

typedef struct {
    const char* name;
    const char* win32_value;
    const char* linux_value;
} PathVar;

extern const PathVar* G_PATH_VARS;
extern unsigned int PATH_VAR_COUNT;

extern const AliasEntry* Aliases;
extern size_t ALIAS_ENTRY_COUNT;

#define MAX_FILE_LINES 10000
#define MAX_FILE_LINE_LENGTH 1024

void Config_ResolveDefaultApps(void);
bool Config_Load(void);
bool Config_Reload(void);
void Config_Unload(void);
void Config_Save(void);
void Config_PollWatcher(void);

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif
