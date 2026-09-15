
#ifndef VIM_MOTIONS_H
#define VIM_MOTIONS_H

#include <stdbool.h>
#include "core/modal.h"
#include "core/vim.h"

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

typedef struct {
    int vk_code;
    int modifiers;
    MotionAction action_id;
    void (*handler)(FlintContext* ctx);
    EditorMode mode;
    VimScope scope;
} MotionBinding;

typedef struct {
    const char* sequence;
    MotionAction action_id;
    void (*handler)(FlintContext* ctx);
    bool is_prefix;
    EditorMode mode;
    VimScope scope;
} SequenceBinding;

void HandleEnterNormal(FlintContext* ctx);
void HandleEnterInsertOut(FlintContext* ctx);
void HandleEnterInsertIn(FlintContext* ctx);
void HandleEnterVisual(FlintContext* ctx);
void HandleMoveLeft(FlintContext* ctx);
void HandleEnter(FlintContext* ctx);
void HandleMoveUp(FlintContext* ctx);
void HandleMoveRight(FlintContext* ctx);
void HandleMoveDown(FlintContext* ctx);
void HandlePersistentDelete(FlintContext* ctx);
void HandleDeleteCharInsert(FlintContext* ctx);
void HandleDeleteCharNormal(FlintContext* ctx);
void HandleDeleteLine(FlintContext* ctx);
void HandleDeleteLineInsert(FlintContext* ctx);
void HandleMoveListboxStart(FlintContext* ctx);
void HandleMoveListboxEnd(FlintContext* ctx);
void HandleMoveLineEnd(FlintContext* ctx);
void HandleMoveLineStart(FlintContext* ctx);
void HandleEnterInsertInStart(FlintContext* ctx);
void HandleEnterInsertOutEnd(FlintContext* ctx);
void HandleExitWindow(FlintContext* ctx);
void HandleYankLine(FlintContext* ctx);
void HandleInsertControlEnter(FlintContext* ctx);
void HandleMoveWordForward(FlintContext* ctx);
void HandleMoveWordBackward(FlintContext* ctx);
void HandleDeleteSingleQuotesInsert(FlintContext* ctx);
void HandleDeleteSingleQuotesNormal(FlintContext* ctx);
void HandleDeleteQuotesInsert(FlintContext* ctx);
void HandleDeleteQuotesNormal(FlintContext* ctx);
void HandleDeleteBracketsInsert(FlintContext* ctx);
void HandleDeleteBracketsNormal(FlintContext* ctx);
void HandleDeleteCurlyBracketsInsert(FlintContext* ctx);
void HandleDeleteCurlyBracketsNormal(FlintContext* ctx);
void HandleDeleteSquareBracketsInsert(FlintContext* ctx);
void HandleDeleteSquareBracketsNormal(FlintContext* ctx);
void HandleDeleteWordInsert(FlintContext* ctx);
void HandleDeleteWordNormal(FlintContext* ctx);
void HandleSendEdit(FlintContext* ctx);
void HandleSendCopyShift(FlintContext* ctx);
void HandleSendCopyControl(FlintContext* ctx);
void HandleCopyToSearch(FlintContext* ctx);
void HandleToggleHistory(FlintContext* ctx);
void HandleShowFullText(FlintContext* ctx);
void SearchTextForward(int key);
void SearchTextBackward(int key);
void HandleSearchTextForward(FlintContext* ctx);
void HandleSearchTextBackward(FlintContext* ctx);
void HandleDeleteTillEnd(FlintContext* ctx);
void HandlePageUp(FlintContext* ctx);
void HandlePageDown(FlintContext* ctx);
void HandleNormalEdit(FlintContext* ctx);

#endif
