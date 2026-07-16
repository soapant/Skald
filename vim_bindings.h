// vim_bindings.h
#ifndef VIM_BINDINGS_H
#define VIM_BINDINGS_H

#include "vim_motions.h"
#include "config.h"

static const MotionBinding G_VIM_BINDINGS[] = {
  
    { 'I',      HOTKEY_MOD_NONE,    MOTION_ENTER_INSERT,        "Insert",   HandleEnterInsert, MODE_NORMAL },
    { 'V',      HOTKEY_MOD_NONE,    MOTION_ENTER_VISUAL,        "Visual",   HandleEnterVisual, MODE_NORMAL },
    
    { 'H',      HOTKEY_MOD_NONE,    MOTION_MOVE_LEFT,           "Left",     HandleMoveLeft, MODE_NORMAL },
    { 'J',      HOTKEY_MOD_NONE,    MOTION_MOVE_DOWN,           "Down",     HandleMoveDown, MODE_NORMAL },
    { 'K',      HOTKEY_MOD_NONE,    MOTION_MOVE_UP,             "Up",       HandleMoveUp, MODE_NORMAL },
    { 'L',      HOTKEY_MOD_NONE,    MOTION_MOVE_RIGHT,          "Right",    HandleMoveRight, MODE_NORMAL },
    
    { 'X',      HOTKEY_MOD_NONE,    MOTION_DELETE_CHAR,         "Del",  HandleDelete, MODE_NORMAL },
    
    { 'D',      HOTKEY_MOD_NONE,   MOTION_DELETE_CHAR_PERSISTENT,"DelPersistent",  HandlePersistentDelete, MODE_NORMAL },
    
};

#define VIM_BINDING_COUNT (sizeof(G_VIM_BINDINGS) / sizeof(G_VIM_BINDINGS[0]))

static const SequenceBinding G_VIM_SEQUENCE_BINDINGS[] = {
  
    { "JK", MOTION_ENTER_NORMAL,    "Enter Mapping", HandleEnterNormal,        false, MODE_INSERT },
    // { "gg",    MOTION_MOVE_FILE_START, "FileStart",   HandleMoveFileStart,      false, MODE_NORMAL },
    // { "G",     MOTION_MOVE_FILE_END,   "FileEnd",     HandleMoveFileEnd,        false, MODE_NORMAL },
    // { "0",     MOTION_MOVE_LINE_START, "LineStart",   HandleMoveLineStart,      false, MODE_NORMAL },
    // { "$",     MOTION_MOVE_LINE_END,   "LineEnd",     HandleMoveLineEnd,        false, MODE_NORMAL },
    // 
    // // Insert variations (NORMAL mode only)
    // { "gi",    MOTION_ENTER_INSERT,    "InsertEnd",   HandleEnterInsertAtEnd,   false, MODE_NORMAL },
    // { "I",     MOTION_ENTER_INSERT,    "InsertStart", HandleEnterInsertAtStart, false, MODE_NORMAL },
    // { "a",     MOTION_ENTER_INSERT,    "InsertAfter", HandleEnterInsertAfter,   false, MODE_NORMAL },
    // { "A",     MOTION_ENTER_INSERT,    "InsertEnd",   HandleEnterInsertAtEnd,   false, MODE_NORMAL },
    // { "o",     MOTION_ENTER_INSERT,    "InsertBelow", HandleEnterInsertBelow,   false, MODE_NORMAL },
    // { "O",     MOTION_ENTER_INSERT,    "InsertAbove", HandleEnterInsertAbove,   false, MODE_NORMAL },
    // 
    // // Delete (NORMAL mode only)
    // { "dd",    MOTION_DELETE_LINE,     "DeleteLine",  HandleDeleteLine,         false, MODE_NORMAL },
    // { "dw",    MOTION_DELETE_WORD,     "DeleteWord",  HandleDeleteWord,         false, MODE_NORMAL },
    // { "d$",    MOTION_DELETE_TO_END,   "DeleteToEnd", HandleDeleteToEnd,        false, MODE_NORMAL },
    // { "D",     MOTION_DELETE_TO_END,   "DeleteToEnd", HandleDeleteToEnd,        false, MODE_NORMAL },
    // 
    // // Change (NORMAL mode only)
    // { "cw",    MOTION_CHANGE_WORD,     "ChangeWord",  HandleChangeWord,         false, MODE_NORMAL },
    // { "c$",    MOTION_CHANGE_TO_END,   "ChangeToEnd", HandleChangeToEnd,        false, MODE_NORMAL },
    // { "C",     MOTION_CHANGE_TO_END,   "ChangeToEnd", HandleChangeToEnd,        false, MODE_NORMAL },
    // { "cc",    MOTION_CHANGE_LINE,     "ChangeLine",  HandleChangeLine,         false, MODE_NORMAL },
    // 
    // // Yank (NORMAL mode only)
    // { "yy",    MOTION_YANK_LINE,       "YankLine",    HandleYankLine,           false, MODE_NORMAL },
    // { "yw",    MOTION_YANK_WORD,       "YankWord",    HandleYankWord,           false, MODE_NORMAL },
    // { "y$",    MOTION_YANK_TO_END,     "YankToEnd",   HandleYankToEnd,          false, MODE_NORMAL },
    // { "Y",     MOTION_YANK_LINE,       "YankLine",    HandleYankLine,           false, MODE_NORMAL },
 };

#define VIM_SEQUENCE_BINDING_COUNT (sizeof(G_VIM_SEQUENCE_BINDINGS) / sizeof(G_VIM_SEQUENCE_BINDINGS[0]))

#endif
