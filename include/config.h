#ifndef CONFIG_H
#define CONFIG_H
#include <string.h>
#include <wchar.h>
#include "core/types.h"


#define SWITCHER            VK_SPACE
#define TODO                'T'
#define FORGETFULL          'F'
#define CALCULATOR          'C'
#define EXE                 'E'
#define KILL_PROCESS        'U'
#define GOOGLE_SEARCH       'G'  
#define TERMINAL            'W'
#define YAZI                'Q'
#define TIMER               'O'
#define EXPLORER            'R'
#define TERMINAL_COMMAND    VK_OEM_4 // {
#define VOLUME              'M'
#define SCREENSHOT          'A'
#define POS                 'Z'
#define COLOR               'X'
#define PIXEL_EQUALS        'B'
#define WAIT_FOR_PIXEL      'N'
#define REGION              'Y'
#define NVIM                'I'
#define MONITOR_FOCUS_MAIN  'L'
#define MONITOR_FOCUS_SIDE  'H'
#define REMINDER            'S'
#define MUTE                'P'
#define SETTINGS            'D'
#define MONITOR_CONTROL     '1'
#define MUSIC_PLAYER_KEY    'P'
#define VIDEO_VIEWER_KEY    'V'
#define REPLICA             'R'
#define KILL_MYPROGRAM      'M'
#define WIFI_TOGGLE         'W'
#define BLUETOOTH_TOGGLE    'B'
#define EDIT_TEXT_KEY       'E'
#define HELP_SCREEN_KEY     'H'
#define COPY_SELECTION_KEY  'Y'
#define TOGGLE_PIXEL_HOTKEYS_KEY 'Z'
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
extern bool SearchBoxDiffColor;
extern float WindowOpacity;
extern const wchar_t* HttpUrlForNotis;
extern const char* BASE_SCREENSHOT_FOLDER_PATH;
extern const char* TEMP_FILE_PATH;
extern const char* UsedFont;
extern const int FontSize;
extern const int NumberFontSize;
extern const int VimFontSize;
extern const int PaddingForScrolling;
extern const char* UsedTerminal;
extern const char* ImageEditorPath;
extern const char* TimerPath;
extern const char* MusicPlayerPath;
extern const char* VideoViewerPath;
extern const char* TextEditorPath;
extern const char* MusicPath;
extern const char* VideosPath;
extern const char* IconPath;
extern const bool TimerAvailable;
extern const bool MusicPlayerAvailable;
extern const bool VideoViewerAvailable;
extern const bool ReplicaAvailable;
extern const bool FullTextNeedsKeyPress;
extern const bool MessagesEnabled;
extern const bool RainbowText;
extern const bool ColoredVimModeText;
extern const bool AlternatingText;
extern const bool EnableLineNumbers;
extern const int GradientItems;
extern const float GradientBrightness;
extern bool PixelHotkeys;
extern bool CloseOnLossOfFocus;
extern float WindowOpacity;

typedef struct {
    const char* path;
    const char* name;
} PathEntry;

typedef struct {
    const char* name;
    const char* win32_value;
    const char* linux_value;
} PathVar;

extern const char* ListOfProgramPaths[20];
extern const PathEntry SystemShortcuts[];
#define SYSTEM_SHORTCUT_COUNT 19


extern const PathVar G_PATH_VARS[];
extern const unsigned int PATH_VAR_COUNT;

extern const AppHotkey G_APP_HOTKEYS[];
extern const unsigned int APP_HOTKEY_COUNT;

extern const MiscHotkey G_MISC_HOTKEYS[];
extern const unsigned int MISC_HOTKEY_COUNT;

extern const TerminalCommandHotkey G_TERMINAL_COMMAND_HOTKEYS[];
extern const unsigned int TERMINAL_COMMAND_HOTKEY_COUNT;

extern const QuickLinkHotkey G_QUICKLINK_HOTKEYS[];
extern const unsigned int QUICKLINK_HOTKEY_COUNT;

extern const TextSnippet G_TEXT_SNIPPETS[];
extern const unsigned int TEXT_SNIPPET_COUNT;

extern const MusicPlayerBinding G_MUSIC_PLAYERS[];
extern const unsigned int MUSIC_PLAYER_COUNT;

extern const VideoViewerBinding G_VIDEO_PLAYERS[];
extern const unsigned int VIDEO_PLAYER_COUNT;

extern const char* bkp_windows[];
extern const BulkKillProcessBinding G_BULK_KILL_PROCESS[];
extern const unsigned int BULKKILLPROCESS_COUNT;

extern const char* focus_blacklist_games[];
extern const char* games_windows[];
extern const char* focus_blacklist_browsers[];
extern const char* browsers_windows[];
extern const char* focus_blacklist_coding[];
extern const char* coding_windows[];
extern const char* focus_blacklist_terminal[];
extern const char* terminal_windows[];
extern const FocusSwitcherBinding G_FOCUS_SWITCHERS[];
extern const unsigned int FOCUS_SWITCHER_COUNT;

extern const AliasEntry Aliases[];
extern const size_t ALIAS_ENTRY_COUNT;

extern const TagFormatConfig TODO_TAG_FORMAT;

#define TODO_TAG_OPEN "["
#define TODO_TAG_CLOSE "]"
#define TODO_TAG_SEPARATOR "|"
#define TODO_ENTRY_DELIMITER "||"

extern const TodoAliasEntry TODO_ALIAS_DICT[];
extern const size_t TODO_ALIAS_DICT_COUNT;

extern const GenericFlagEntry TODO_ALIAS_FLAGS[];
extern const size_t TODO_ALIAS_FLAGS_COUNT;

extern const GenericFlagEntry TODO_CONFIG_FLAGS[];
extern const size_t TODO_CONFIG_FLAGS_COUNT;

extern const TodoConfigEntry TODO_DICT[];
#define TODO_CONFIG_FLAGS_ENTRIES TODO_DICT
extern const size_t TODO_DICT_COUNT;

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

#endif
