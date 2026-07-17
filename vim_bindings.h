// vim_bindings.h
#ifndef VIM_BINDINGS_H
#define VIM_BINDINGS_H

#include "vim_motions.h"
#include "config.h"

static const MotionBinding G_VIM_BINDINGS[] = {
  
    { 'I', HOTKEY_MOD_NONE,  MOTION_ENTER_INSERT_IN,        "InsertIn",        HandleEnterInsertIn,      MODE_NORMAL },
    { 'A', HOTKEY_MOD_NONE,  MOTION_ENTER_INSERT_OUT,       "InsertOut",        HandleEnterInsertOut,     MODE_NORMAL },
    { 'I', HOTKEY_MOD_SHIFT, MOTION_ENTER_INSERT_IN_START,  "InsertInStart",        HandleEnterInsertInStart, MODE_NORMAL },
    { 'A', HOTKEY_MOD_SHIFT, MOTION_ENTER_INSERT_OUT_END,   "InsertOutEnd",        HandleEnterInsertOutEnd,  MODE_NORMAL },
    { 'V', HOTKEY_MOD_NONE,  MOTION_ENTER_VISUAL,           "Visual",        HandleEnterVisual,        MODE_NORMAL },
    
    { 'H', HOTKEY_MOD_NONE,  MOTION_MOVE_LEFT,              "Left",          HandleMoveLeft,           MODE_NORMAL },
    { 'J', HOTKEY_MOD_NONE,  MOTION_MOVE_DOWN,              "Down",          HandleMoveDown,           MODE_NORMAL },
    { 'K', HOTKEY_MOD_NONE,  MOTION_MOVE_UP,                "Up",            HandleMoveUp,             MODE_NORMAL },
    { 'L', HOTKEY_MOD_NONE,  MOTION_MOVE_RIGHT,             "Right",         HandleMoveRight,          MODE_NORMAL },
    
    { 'X', HOTKEY_MOD_NONE,  MOTION_DELETE_CHAR,            "Del",           HandleDelete,             MODE_NORMAL },
    
    { 'S', HOTKEY_MOD_NONE,  MOTION_DELETE_CHAR_INSERT,            "DelInsert",           HandleDeleteInsert,             MODE_NORMAL },
    
    { 'D', HOTKEY_MOD_SHIFT,  MOTION_DELETE_CHAR_PERSISTENT, "DelPersistent", HandlePersistentDelete,   MODE_NORMAL },
    
    { 'S', HOTKEY_MOD_SHIFT, MOTION_DELETE_LINE_INSERT,     "DelLineInsert", HandleDeleteLineInsert,   MODE_NORMAL },
    
    { 'G', HOTKEY_MOD_SHIFT, MOTION_MOVE_LISTBOX_END,       "ListboxEnd",    HandleMoveListboxEnd,     MODE_NORMAL },
    
    { '0', HOTKEY_MOD_NONE,  MOTION_MOVE_LINE_START,        "LineStart",     HandleMoveLineStart,      MODE_NORMAL },
    
    { '4', HOTKEY_MOD_SHIFT, MOTION_MOVE_LINE_END,          "LineEnd",       HandleMoveLineEnd,      MODE_NORMAL },
    
    { 'Q', HOTKEY_MOD_NONE, MOTION_EXIT_WINDOW,          "ExitWindow",       HandleExitWindow,      MODE_NORMAL },
    
    { 'Y', HOTKEY_MOD_SHIFT, MOTION_YANK_LINE,          "YankLine",       HandleYankLine,      MODE_NORMAL },
    
};

#define VIM_BINDING_COUNT (sizeof(G_VIM_BINDINGS) / sizeof(G_VIM_BINDINGS[0]))

static const SequenceBinding G_VIM_SEQUENCE_BINDINGS[] = {
  
    { "JK", MOTION_ENTER_NORMAL,       "Enter Mapping", HandleEnterNormal,      false, MODE_INSERT },
    { "JQ", MOTION_EXIT_WINDOW,       "Exit Mapping",  HandleExitWindow,       false, MODE_INSERT },
    { "dd", MOTION_DELETE_LINE,        "DeleteLine",    HandleDeleteLine,       false, MODE_NORMAL },
    { "gg", MOTION_MOVE_LISTBOX_START, "ListboxStart",  HandleMoveListboxStart, false, MODE_NORMAL },
    // { "dw",    MOTION_DELETE_WORD,     "DeleteWord",  HandleDeleteWord,         false, MODE_NORMAL },
    // { "d$",    MOTION_DELETE_TO_END,   "DeleteToEnd", HandleDeleteToEnd,        false, MODE_NORMAL },
    { "yy",    MOTION_YANK_LIST_BOX,       "YankListBox",    HandleYankListBox,           false, MODE_NORMAL },
    // { "yw",    MOTION_YANK_WORD,       "YankWord",    HandleYankWord,           false, MODE_NORMAL },
    // { "y$",    MOTION_YANK_TO_END,     "YankToEnd",   HandleYankToEnd,          false, MODE_NORMAL },
 };

#define VIM_SEQUENCE_BINDING_COUNT (sizeof(G_VIM_SEQUENCE_BINDINGS) / sizeof(G_VIM_SEQUENCE_BINDINGS[0]))

#endif
