// vim_bindings.h
#ifndef VIM_BINDINGS_H
#define VIM_BINDINGS_H

#include "hotkey_definitions.h"
#include "vim_motions.h"
#include "config.h"

static const MotionBinding G_VIM_BINDINGS[] = {
  
    { 'I', HOTKEY_MOD_NONE,  MOTION_ENTER_INSERT_IN,       HandleEnterInsertIn,      MODE_NORMAL },
    { 'A', HOTKEY_MOD_NONE,  MOTION_ENTER_INSERT_OUT,      HandleEnterInsertOut,     MODE_NORMAL },
    { 'I', HOTKEY_MOD_SHIFT, MOTION_ENTER_INSERT_IN_START, HandleEnterInsertInStart, MODE_NORMAL },
    { 'A', HOTKEY_MOD_SHIFT, MOTION_ENTER_INSERT_OUT_END,  HandleEnterInsertOutEnd,  MODE_NORMAL },
    { 'V', HOTKEY_MOD_NONE,  MOTION_ENTER_VISUAL,          HandleEnterVisual,        MODE_NORMAL },
    { 'J', HOTKEY_MOD_NONE,  MOTION_MOVE_DOWN,             HandleMoveDown,           MODE_NORMAL },
    { 'K', HOTKEY_MOD_NONE,  MOTION_MOVE_UP,               HandleMoveUp,             MODE_NORMAL },
    { 'X', HOTKEY_MOD_NONE,  MOTION_DELETE_CHAR,           HandleDeleteCharNormal,   MODE_NORMAL },
    { 'S', HOTKEY_MOD_NONE,  MOTION_DELETE_CHAR_INSERT,    HandleDeleteCharInsert,   MODE_NORMAL },
    { 'H', HOTKEY_MOD_NONE,  MOTION_MOVE_LEFT,             HandleMoveLeft,           MODE_NORMAL },
    { 'L', HOTKEY_MOD_NONE,  MOTION_MOVE_RIGHT,            HandleMoveRight,          MODE_NORMAL },
    { 'L', HOTKEY_MOD_SHIFT, MOTION_DELETE_CHAR_PERSISTENT,HandlePersistentDelete,   MODE_NORMAL },
    { 'S', HOTKEY_MOD_SHIFT, MOTION_DELETE_LINE_INSERT,    HandleDeleteLineInsert,   MODE_NORMAL },
    { 'G', HOTKEY_MOD_SHIFT, MOTION_MOVE_LISTBOX_END,      HandleMoveListboxEnd,     MODE_NORMAL },
    { '0', HOTKEY_MOD_NONE,  MOTION_MOVE_LINE_START,       HandleMoveLineStart,      MODE_NORMAL },
    { '4', HOTKEY_MOD_SHIFT, MOTION_MOVE_LINE_END,         HandleMoveLineEnd,        MODE_NORMAL },
    { 'Q', HOTKEY_MOD_NONE,  MOTION_EXIT_WINDOW,           HandleExitWindow,         MODE_NORMAL },
    { 'W', HOTKEY_MOD_NONE,  MOTION_MOVE_WORD_FORWARD,     HandleMoveWordForward,    MODE_NORMAL },
    { 'B', HOTKEY_MOD_NONE,  MOTION_MOVE_WORD_BACKWARD,    HandleMoveWordBackward,   MODE_NORMAL },
    { 'E', HOTKEY_MOD_NONE,  MOTION_SEND_EDIT,    HandleSendEdit,   MODE_NORMAL },
    { 'Y', HOTKEY_MOD_SHIFT, MOTION_YANK_LIST_BOX,         HandleYankListBox,        MODE_NORMAL },
    
};

#define VIM_BINDING_COUNT (sizeof(G_VIM_BINDINGS) / sizeof(G_VIM_BINDINGS[0]))

static const SequenceBinding G_VIM_SEQUENCE_BINDINGS[] = {
  
    { "JK",  MOTION_ENTER_NORMAL,                  HandleEnterNormal,                false, MODE_INSERT },
    { "JC",  MOTION_INSERTCTRL_ENTER,              HandleInsertControlEnter,         false, MODE_INSERT },
    { "JQ",  MOTION_EXIT_WINDOW,                   HandleExitWindow,                 false, MODE_INSERT },
    { "DD",  MOTION_DELETE_LINE,                   HandleDeleteLine,                 false, MODE_NORMAL },
    { "GG",  MOTION_MOVE_LISTBOX_START,            HandleMoveListboxStart,           false, MODE_NORMAL },
    { "YY",  MOTION_YANK_LINE,                     HandleYankLine,                   false, MODE_NORMAL },
    { "DIW", MOTION_DELETE_WORD_NORMAL,            HandleDeleteWordNormal,           false, MODE_NORMAL },
    { "CIW", MOTION_DELETE_WORD_INSERT,            HandleDeleteWordInsert,           false, MODE_NORMAL },
    { "CIQ", MOTION_DELETE_QUOTES_NORMAL,          HandleDeleteQuotesNormal,         false, MODE_NORMAL },
    { "DIQ", MOTION_DELETE_QUOTES_INSERT,          HandleDeleteQuotesInsert,         false, MODE_NORMAL },
    { "CIU", MOTION_DELETE_SINGLE_QUOTES_INSERT,   HandleDeleteSingleQuotesInsert,   false, MODE_NORMAL },
    { "DIU", MOTION_DELETE_SINGLE_QUOTES_NORMAL,   HandleDeleteSingleQuotesNormal,   false, MODE_NORMAL },
    { "DIB", MOTION_DELETE_BRACKETS_NORMAL,        HandleDeleteBracketsNormal,       false, MODE_NORMAL },
    { "CIB", MOTION_DELETE_BRACKETS_INSERT,        HandleDeleteBracketsInsert,       false, MODE_NORMAL },
    { "CIC", MOTION_DELETE_CURLY_BRACKETS_INSERT,  HandleDeleteCurlyBracketsInsert,  false, MODE_NORMAL },
    { "DIC", MOTION_DELETE_CURLY_BRACKETS_NORMAL,  HandleDeleteCurlyBracketsNormal,  false, MODE_NORMAL },
    { "CIS", MOTION_DELETE_SQUARE_BRACKETS_INSERT, HandleDeleteSquareBracketsInsert, false, MODE_NORMAL },
    { "DIS", MOTION_DELETE_SQUARE_BRACKETS_NORMAL, HandleDeleteSquareBracketsNormal, false, MODE_NORMAL },
};

#define VIM_SEQUENCE_BINDING_COUNT (sizeof(G_VIM_SEQUENCE_BINDINGS) / sizeof(G_VIM_SEQUENCE_BINDINGS[0]))

#endif
