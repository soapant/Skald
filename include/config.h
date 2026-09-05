#ifndef CONFIG_H
#define CONFIG_H
#include <string.h>
#include <wchar.h>
#include "core/types.h"

#define EDIT_TEXT_KEY       'E'
#define HELP_SCREEN_KEY     'H'
#define COPY_SELECTION_KEY  'Y'
#define COMMAND_MODE_KEY        VK_OEM_1 // ;
#define SEARCH_PROGRAM_KEY      VK_OEM_2 // /
#define COLOR_ERROR             RGB(180, 0, 0) 
#define COLOR_NORMAL            RGB(200, 200, 0) 
#define COLOR_TIP               RGB(0, 160, 0)

extern bool Tray;
extern bool EnableVimMode;
extern bool RememberLastIndex;
extern bool KeepSelectionAfterClose;
extern bool MessageAfterEachHotkey;
extern bool AllowOtherFeatureTogglesInFeature;
extern bool ModePersistsThroughFeatures;
extern int  TimeOutForMessages;
extern bool DoTimeDebugs;
extern char* EditPopUpPos;
extern int EditPopUpWidthMin;
extern int PageUpDownLen;
extern bool ColorfullMode;
extern float WindowOpacity;
extern const char* UsedFont;
extern const int FontSize;
extern const int NumberFontSize;
extern const int VimFontSize;
extern const int PaddingForScrolling;
extern bool CloseOnLossOfFocus;
extern const char* UsedTerminal;
extern const char* UsedShell;
extern const char* UsedFileManager;
extern const char* UsedTextEditor;
extern const char* ImageEditorPath;
extern const char* IconPath;
extern const bool FullTextNeedsKeyPress;
extern const bool MessagesEnabled;
extern const bool RainbowText;
extern const bool ColoredVimModeText;
extern const bool AlternatingText;
extern const bool EnableLineNumbers;
extern const int GradientItems;
extern const float GradientBrightness;

typedef struct {
    const char* path;
    const char* name;
} PathEntry;

typedef struct {
    const char* name;
    const char* win32_value;
    const char* linux_value;
} PathVar;

extern const PathVar G_PATH_VARS[];
extern const unsigned int PATH_VAR_COUNT;

extern const AliasEntry Aliases[];
extern const size_t ALIAS_ENTRY_COUNT;

#define MAX_FILE_LINES 10000
#define MAX_FILE_LINE_LENGTH 1024

extern const int MaxFileIOLines;

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif
