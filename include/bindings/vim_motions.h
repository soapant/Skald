// vim_motions.h
#ifndef VIM_MOTIONS_H
#define VIM_MOTIONS_H

#include <stdbool.h>
#include "core/modal.h"

typedef enum {
    MOTION_NONE = 0,
    MOTION_ENTER_NORMAL,
    MOTION_NORMAL_EDIT,
    MOTION_DELETE_TILL_END,
    MOTION_PAGE_UP,
    MOTION_PAGE_DOWN,
    MOTION_HISTORY_TOGGLE,
    MOTION_SEARCH_TEXT_FORWARD,
    MOTION_SEARCH_TEXT_BACKWARD,
    MOTION_SHOW_FULL_TEXT,
    MOTION_ENTER,
    MOTION_COPY_INTO_SEARCH,
    MOTION_SEND_COPY_SHIFT,
    MOTION_SEND_COPY_CONTROL,
    MOTION_SEND_EDIT,
    MOTION_NORMALENTER,
    MOTION_NORMALCTRL_ENTER,
    MOTION_INSERT_ENTER,
    MOTION_INSERTCTRL_ENTER,
    MOTION_DELETE_LINE_INSERT,
    MOTION_DELETE_LINE_NORMAL,
    MOTION_DELETE_CHAR_INSERT,
    MOTION_ENTER_INSERT_OUT,
    MOTION_ENTER_INSERT_IN,
    MOTION_ENTER_INSERT_OUT_END,
    MOTION_ENTER_INSERT_IN_START,
    MOTION_ENTER_VISUAL,
    MOTION_ENTER_VISUAL_LINE,
    MOTION_MOVE_LEFT,
    MOTION_MOVE_RIGHT,
    MOTION_MOVE_UP,
    MOTION_MOVE_DOWN,
    MOTION_MOVE_WORD_FORWARD,
    MOTION_MOVE_WORD_BACKWARD,
    MOTION_MOVE_LINE_START,
    MOTION_MOVE_LINE_END,
    MOTION_MOVE_LISTBOX_START,
    MOTION_MOVE_LISTBOX_END,
    MOTION_DELETE_CHAR,
    MOTION_DELETE_CHAR_PERSISTENT,
    MOTION_DELETE_LINE,
    MOTION_DELETE_WORD,
    MOTION_CHANGE_TO_END,
    MOTION_CHANGE_LINE,
    MOTION_YANK_LINE,
    MOTION_UNDO,
    MOTION_SEARCH_FORWARD,
    MOTION_SEARCH_BACKWARD,
    MOTION_EXIT_WINDOW,
    MOTION_DELETE_QUOTES_INSERT,
    MOTION_DELETE_QUOTES_NORMAL,
    MOTION_DELETE_SINGLE_QUOTES_INSERT,
    MOTION_DELETE_SINGLE_QUOTES_NORMAL,
    MOTION_DELETE_BRACKETS_INSERT,
    MOTION_DELETE_BRACKETS_NORMAL,
    MOTION_DELETE_CURLY_BRACKETS_INSERT,
    MOTION_DELETE_CURLY_BRACKETS_NORMAL,
    MOTION_DELETE_SQUARE_BRACKETS_INSERT,
    MOTION_DELETE_SQUARE_BRACKETS_NORMAL,
    MOTION_DELETE_WORD_NORMAL,
    MOTION_DELETE_WORD_INSERT,
} MotionAction;

typedef enum {
    VIM_SCOPE_ALL = 0,
    VIM_SCOPE_MODAL,
    VIM_SCOPE_POPUP,
} VimScope;

static inline bool ScopeMatches(VimScope scope, bool in_popup) {
    switch (scope) {
        case VIM_SCOPE_ALL:   return true;
        case VIM_SCOPE_MODAL: return !in_popup;
        case VIM_SCOPE_POPUP: return in_popup;
        default:              return false;
    }
}

typedef struct {
    int vk_code;
    int modifiers;
    MotionAction action_id;
    void (*handler)(void);
    EditorMode mode;
    VimScope scope;
} MotionBinding;

typedef struct {
    const char* sequence;
    MotionAction action_id;
    void (*handler)(void);
    bool is_prefix;
    EditorMode mode;
    VimScope scope;
} SequenceBinding;

void HandleEnterNormal(void);
void HandleEnterInsertOut(void);
void HandleEnterInsertIn(void);
void HandleEnterVisual(void);
void HandleMoveLeft(void);
void HandleEnter(void);
void HandleMoveUp(void);
void HandleMoveRight(void);
void HandleMoveDown(void);
void HandlePersistentDelete(void);
void HandleDeleteCharInsert(void);
void HandleDeleteCharNormal(void);
void HandleDeleteLine(void);
void HandleDeleteLineInsert(void);
void HandleMoveListboxStart(void);
void HandleMoveListboxEnd(void);
void HandleMoveLineEnd(void);
void HandleMoveLineStart(void);
void HandleEnterInsertInStart(void);
void HandleEnterInsertOutEnd(void);
void HandleExitWindow(void);
void HandleYankLine(void);
void HandleInsertControlEnter(void);
void HandleMoveWordForward(void);
void HandleMoveWordBackward(void);
void HandleDeleteSingleQuotesInsert(void);
void HandleDeleteSingleQuotesNormal(void);
void HandleDeleteQuotesInsert(void);
void HandleDeleteQuotesNormal(void);
void HandleDeleteBracketsInsert(void);
void HandleDeleteBracketsNormal(void);
void HandleDeleteCurlyBracketsInsert(void);
void HandleDeleteCurlyBracketsNormal(void);
void HandleDeleteSquareBracketsInsert(void);
void HandleDeleteSquareBracketsNormal(void);
void HandleDeleteWordInsert(void);
void HandleDeleteWordNormal(void);
void HandleSendEdit(void);
void HandleSendCopyShift(void);
void HandleSendCopyControl(void);
void HandleCopyToSearch(void);
void HandleToggleHistory(void);
void HandleShowFullText(void);
void SearchTextForward(int key);
void SearchTextBackward(int key);
void HandleSearchTextForward(void);
void HandleSearchTextBackward(void);
void HandleDeleteTillEnd(void);
void HandlePageUp(void);
void HandlePageDown(void);
void HandleNormalEdit(void);

#endif
