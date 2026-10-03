#include "core/context.h"
#include "bindings/vim_bindings.h"
#include "bindings/vim_motions.h"

const MotionBinding G_VIM_BINDINGS[] = {
  
    { 'I',                HOTKEY_MOD_NONE,  MOTION_ENTER_INSERT_IN,        HandleEnterInsertIn,      MODE_NORMAL, VIM_SCOPE_ALL },
    { 'A',                HOTKEY_MOD_NONE,  MOTION_ENTER_INSERT_OUT,       HandleEnterInsertOut,     MODE_NORMAL, VIM_SCOPE_ALL },
    { 'I',                HOTKEY_MOD_SHIFT, MOTION_ENTER_INSERT_IN_START,  HandleEnterInsertInStart, MODE_NORMAL, VIM_SCOPE_ALL },
    { 'A',                HOTKEY_MOD_SHIFT, MOTION_ENTER_INSERT_OUT_END,   HandleEnterInsertOutEnd,  MODE_NORMAL, VIM_SCOPE_ALL },
    { 'V',                HOTKEY_MOD_NONE,  MOTION_ENTER_VISUAL,           HandleEnterVisual,        MODE_NORMAL, VIM_SCOPE_ALL },
    { 'J',                HOTKEY_MOD_NONE,  MOTION_MOVE_DOWN,              HandleMoveDown,           MODE_NORMAL, VIM_SCOPE_MODAL },
    { 'K',                HOTKEY_MOD_NONE,  MOTION_MOVE_UP,                HandleMoveUp,             MODE_NORMAL, VIM_SCOPE_MODAL },
    { 'X',                HOTKEY_MOD_NONE,  MOTION_DELETE_CHAR,            HandleDeleteCharNormal,   MODE_NORMAL, VIM_SCOPE_ALL }, { 'S',                HOTKEY_MOD_NONE,  MOTION_DELETE_CHAR_INSERT,     HandleDeleteCharInsert,   MODE_NORMAL, VIM_SCOPE_ALL },
    { 'H',                HOTKEY_MOD_NONE,  MOTION_MOVE_LEFT,              HandleMoveLeft,           MODE_NORMAL, VIM_SCOPE_ALL },
    { 'L',                HOTKEY_MOD_NONE,  MOTION_ENTER,                  HandleEnter,              MODE_NORMAL, VIM_SCOPE_MODAL },
    { 'L',                HOTKEY_MOD_NONE,  MOTION_MOVE_RIGHT,             HandleMoveRight,          MODE_NORMAL, VIM_SCOPE_ALL },
    { 'L',                HOTKEY_MOD_SHIFT, MOTION_DELETE_CHAR_PERSISTENT, HandlePersistentDelete,   MODE_NORMAL, VIM_SCOPE_MODAL },
    { 'S',                HOTKEY_MOD_SHIFT, MOTION_DELETE_LINE_INSERT,     HandleDeleteLineInsert,   MODE_NORMAL, VIM_SCOPE_ALL },
    { 'G',                HOTKEY_MOD_SHIFT, MOTION_MOVE_LISTBOX_END,       HandleMoveListboxEnd,     MODE_NORMAL, VIM_SCOPE_MODAL },
    { '0',                HOTKEY_MOD_NONE,  MOTION_MOVE_LINE_START,        HandleMoveLineStart,      MODE_NORMAL, VIM_SCOPE_ALL },
    { '4',                HOTKEY_MOD_SHIFT, MOTION_MOVE_LINE_END,          HandleMoveLineEnd,        MODE_NORMAL, VIM_SCOPE_ALL },
    { 'Q',                HOTKEY_MOD_NONE,  MOTION_EXIT_WINDOW,            HandleExitWindow,         MODE_NORMAL, VIM_SCOPE_ALL },
    { 'W',                HOTKEY_MOD_NONE,  MOTION_MOVE_WORD_FORWARD,      HandleMoveWordForward,    MODE_NORMAL, VIM_SCOPE_ALL },
    { 'B',                HOTKEY_MOD_NONE,  MOTION_MOVE_WORD_BACKWARD,     HandleMoveWordBackward,   MODE_NORMAL, VIM_SCOPE_ALL },
    { 'H',                HOTKEY_MOD_SHIFT, MOTION_SHOW_FULL_TEXT,         HandleShowFullText,       MODE_NORMAL, VIM_SCOPE_ALL },
    { EDIT_TEXT_KEY,      HOTKEY_MOD_NONE,  MOTION_SEND_EDIT,              HandleSendEdit,           MODE_NORMAL, VIM_SCOPE_MODAL },
    { COPY_SELECTION_KEY, HOTKEY_MOD_SHIFT, MOTION_SEND_COPY_SHIFT,        HandleSendCopyShift,      MODE_NORMAL, VIM_SCOPE_MODAL },
    { COPY_SELECTION_KEY, HOTKEY_MOD_CTRL,  MOTION_SEND_COPY_CONTROL,      HandleSendCopyControl,    MODE_NORMAL, VIM_SCOPE_MODAL },
    { 'R',                HOTKEY_MOD_NONE,  MOTION_COPY_INTO_SEARCH,       HandleCopyToSearch,       MODE_NORMAL, VIM_SCOPE_MODAL },
    { 'O',                HOTKEY_MOD_NONE,  MOTION_HISTORY_TOGGLE,         HandleToggleHistory,      MODE_NORMAL, VIM_SCOPE_MODAL },
    { 'O',                HOTKEY_MOD_SHIFT, MOTION_NORMAL_EDIT,            HandleNormalEdit,         MODE_NORMAL, VIM_SCOPE_ALL },
    { 'F',                HOTKEY_MOD_NONE,  MOTION_SEARCH_TEXT_FORWARD,    HandleSearchTextForward,  MODE_NORMAL, VIM_SCOPE_ALL },
    { 'F',                HOTKEY_MOD_SHIFT, MOTION_SEARCH_TEXT_BACKWARD,   HandleSearchTextBackward, MODE_NORMAL, VIM_SCOPE_ALL },
    { 'D',                HOTKEY_MOD_SHIFT, MOTION_DELETE_TILL_END,        HandleDeleteTillEnd,      MODE_NORMAL, VIM_SCOPE_ALL },
    { 'U',                HOTKEY_MOD_SHIFT, MOTION_PAGE_DOWN,              HandlePageDown,           MODE_NORMAL, VIM_SCOPE_MODAL },
    { 'R',                HOTKEY_MOD_SHIFT, MOTION_PAGE_UP,                HandlePageUp,             MODE_NORMAL, VIM_SCOPE_MODAL },
    
};

const SequenceBinding G_VIM_SEQUENCE_BINDINGS[] = {
  
    { "JK",  MOTION_ENTER_NORMAL,                  HandleEnterNormal,                false, MODE_INSERT, VIM_SCOPE_ALL },
    { "JC",  MOTION_INSERTCTRL_ENTER,              HandleInsertControlEnter,         false, MODE_INSERT, VIM_SCOPE_ALL },
    { "JQ",  MOTION_EXIT_WINDOW,                   HandleExitWindow,                 false, MODE_INSERT, VIM_SCOPE_MODAL },
    { "DD",  MOTION_DELETE_LINE,                   HandleDeleteLine,                 false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "GG",  MOTION_MOVE_LISTBOX_START,            HandleMoveListboxStart,           false, MODE_NORMAL, VIM_SCOPE_MODAL },
    { "YY",  MOTION_YANK_LINE,                     HandleYankLine,                   false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "DIW", MOTION_DELETE_WORD_NORMAL,            HandleDeleteWordNormal,           false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "CIW", MOTION_DELETE_WORD_INSERT,            HandleDeleteWordInsert,           false, MODE_NORMAL, VIM_SCOPE_ALL },
    // i think this doesnt work temporary also add _ mapping
    // { "DW",  MOTION_DELETE_WORD_NORMAL,            HandleDeleteWordNormal,           false, MODE_NORMAL, VIM_SCOPE_ALL },
    // { "CW",  MOTION_DELETE_WORD_INSERT,            HandleDeleteWordInsert,           false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "CIQ", MOTION_DELETE_QUOTES_NORMAL,          HandleDeleteQuotesNormal,         false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "DIQ", MOTION_DELETE_QUOTES_INSERT,          HandleDeleteQuotesInsert,         false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "CIU", MOTION_DELETE_SINGLE_QUOTES_INSERT,   HandleDeleteSingleQuotesInsert,   false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "DIU", MOTION_DELETE_SINGLE_QUOTES_NORMAL,   HandleDeleteSingleQuotesNormal,   false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "DIB", MOTION_DELETE_BRACKETS_NORMAL,        HandleDeleteBracketsNormal,       false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "CIB", MOTION_DELETE_BRACKETS_INSERT,        HandleDeleteBracketsInsert,       false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "CIC", MOTION_DELETE_CURLY_BRACKETS_INSERT,  HandleDeleteCurlyBracketsInsert,  false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "DIC", MOTION_DELETE_CURLY_BRACKETS_NORMAL,  HandleDeleteCurlyBracketsNormal,  false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "CIS", MOTION_DELETE_SQUARE_BRACKETS_INSERT, HandleDeleteSquareBracketsInsert, false, MODE_NORMAL, VIM_SCOPE_ALL },
    { "DIS", MOTION_DELETE_SQUARE_BRACKETS_NORMAL, HandleDeleteSquareBracketsNormal, false, MODE_NORMAL, VIM_SCOPE_ALL },
};
