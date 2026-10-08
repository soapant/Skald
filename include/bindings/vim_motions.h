
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
    MOTION_WORD_END_FORWARD,
    MOTION_WORD_END_BACKWARD,
    MOTION_WORD_END_FORWARD_BIG,
    MOTION_WORD_END_BACKWARD_BIG,
    MOTION_WORD_FORWARD_BIG,
    MOTION_WORD_BACKWARD_BIG,
    MOTION_FIRST_NON_BLANK,
    MOTION_GOTO_LINE_START,
    MOTION_GOTO_LINE_END,
    MOTION_BRACKET_MATCH,
    MOTION_STAR_SEARCH,
    MOTION_HASH_SEARCH,
    MOTION_GOTO_LAST_INSERT,
    MOTION_FIND_TILL_FORWARD,
    MOTION_FIND_TILL_BACKWARD,
    MOTION_REPEAT_FIND_SAME,
    MOTION_REPEAT_FIND_OPP,
    MOTION_FIND_NEXT,
    MOTION_FIND_PREV,
    MOTION_CASE_LOWER,
    MOTION_CASE_UPPER,
    MOTION_CASE_TOGGLE,
    MOTION_SCROLL_TOP,
    MOTION_SCROLL_CENTER,
    MOTION_SCROLL_BOTTOM,
    MOTION_SCREEN_TOP,
    MOTION_SCREEN_MIDDLE,
    MOTION_SCREEN_BOTTOM,
    MOTION_HALF_PAGE_DOWN,
    MOTION_HALF_PAGE_UP,
    MOTION_SCROLL_LINE_DOWN,
    MOTION_SCROLL_LINE_UP,
    MOTION_SET_MARK,
    MOTION_JUMP_TO_MARK,
    MOTION_JUMP_PREV_CONTEXT,
    MOTION_JUMP_BACK,
    MOTION_JUMP_FORWARD,
    MOTION_SEARCH_FORWARD_KEY,
    MOTION_SEARCH_BACKWARD_KEY,
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

void HandleWordEndForward(FlintContext* ctx);
void HandleWordEndBackward(FlintContext* ctx);
void HandleWordEndForwardBig(FlintContext* ctx);
void HandleWordEndBackwardBig(FlintContext* ctx);
void HandleWordForwardBig(FlintContext* ctx);
void HandleWordBackwardBig(FlintContext* ctx);
void HandleFirstNonBlank(FlintContext* ctx);
void HandleGotoLineStart(FlintContext* ctx);
void HandleGotoLineEnd(FlintContext* ctx);
void HandleBracketMatchPercent(FlintContext* ctx);
void HandleStarSearch(FlintContext* ctx);
void HandleHashSearch(FlintContext* ctx);
void HandleGotoLastInsert(FlintContext* ctx);
void HandleFindTillForward(FlintContext* ctx);
void HandleFindTillBackward(FlintContext* ctx);
void HandleRepeatFindSame(FlintContext* ctx);
void HandleRepeatFindOpp(FlintContext* ctx);
void HandleFindNext(FlintContext* ctx);
void HandleFindPrev(FlintContext* ctx);
void HandleCaseLower(FlintContext* ctx);
void HandleCaseUpper(FlintContext* ctx);
void HandleCaseToggle(FlintContext* ctx);
void HandleScrollTop(FlintContext* ctx);
void HandleScrollCenter(FlintContext* ctx);
void HandleScrollBottom(FlintContext* ctx);
void HandleScreenTop(FlintContext* ctx);
void HandleScreenMiddle(FlintContext* ctx);
void HandleScreenBottom(FlintContext* ctx);
void HandleHalfPageDown(FlintContext* ctx);
void HandleHalfPageUp(FlintContext* ctx);
void HandleScrollLineDown(FlintContext* ctx);
void HandleScrollLineUp(FlintContext* ctx);
void HandleSetMark(FlintContext* ctx);
void HandleJumpToMark(FlintContext* ctx);
void HandleJumpPrevContext(FlintContext* ctx);
void HandleJumpBack(FlintContext* ctx);
void HandleJumpForward(FlintContext* ctx);
void HandleSearchForward(FlintContext* ctx);
void HandleSearchBackward(FlintContext* ctx);
void Vim_SearchCollect(int key);
void Vim_SearchCommitForward(int key);
void Vim_SearchCommitBackward(int key);
void Vim_SearchCancel(int key);

#endif
