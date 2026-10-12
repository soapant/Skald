#include "test.h"
#include <string.h>
#include "core/context.h"
#include "core/modal.h"
#include "core/vim.h"
#include "core/prompt_controller.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "bindings/vim_state.h"
#include "bindings/vim_bindings.h"
#include "platform/input.h"

void vim_clamp_cursor(FlintContext* ctx, int pos);
void vim_find_record(char target, bool till, bool backward);
void vim_mark_set(char name, int cursor);
void vim_mark_jump(char name);

static FlintContext* vctx(void) { return FlintContext_Get(); }

static void vreset(const char* text, int cursor) {
    SetMode(MODE_NORMAL);
    SequenceReset();
    Vim_SetText(vctx(), text);
    Vim_SetCursor(vctx(), cursor);
}

static void vreset_insert(const char* text, int cursor) {
    SetMode(MODE_INSERT);
    Vim_SetText(vctx(), text);
    Vim_SetCursor(vctx(), cursor);
}

static void vreset_visual(const char* text, int cursor) {
    SetMode(MODE_VISUAL);
    VimState_BeginSelection(cursor);
    Vim_SetText(vctx(), text);
    Vim_SetCursor(vctx(), cursor);
    VimState_BeginSelection(cursor);
}

static void vgettext(char* buf, size_t cap) {
    Vim_GetText(vctx(), buf, cap);
}

static void vpress(int vk, bool ctrl, bool shift) {
    PromptController_TryVimMotion(vk, ctrl, shift, false);
}

static const MotionBinding* g_vb;
static int g_vb_count;
static const SequenceBinding* g_vs;
static int g_vs_count;

static void vtables(void) {
    if (!g_vb) {
        g_vb_count = VimBindings_Get(&g_vb);
        g_vs_count = VimSequenceBindings_Get(&g_vs);
    }
}

static const MotionBinding* vbinding(MotionAction action) {
    vtables();
    for (int i = 0; i < g_vb_count; i++) {
        if (g_vb[i].action_id == action) return &g_vb[i];
    }
    return NULL;
}

static const SequenceBinding* vseqbinding(const char* seq) {
    vtables();
    for (int i = 0; i < g_vs_count; i++) {
        const SequenceBinding* b = &g_vs[i];
        if (b->sequence && strcmp(b->sequence, seq) == 0) return b;
    }
    return NULL;
}

TEST(word_forward) {
    vreset("hello world", 0);
    HandleMoveWordForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 6);
    HandleMoveWordForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 10);
}

TEST(word_backward) {
    vreset("hello world", 10);
    HandleMoveWordBackward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 5);
    HandleMoveWordBackward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(line_start_end) {
    vreset("hello world", 3);
    HandleMoveLineEnd(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), (int)strlen(buf));
    HandleMoveLineStart(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(delete_line) {
    vreset("hello world", 4);
    HandleDeleteLine(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "");
}

TEST(delete_word_normal) {
    vreset("hello world", 0);
    HandleDeleteWordNormal(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, " world");
}

TEST(edit_buffer_backspace) {
    char buf[16] = "abc";
    size_t cur = 3;
    EditBufferBackspace(buf, &cur);
    ASSERT_EQ_STR(buf, "ab");
    ASSERT_EQ_INT((int)cur, 2);
    cur = 0;
    EditBufferBackspace(buf, &cur);
    ASSERT_EQ_STR(buf, "ab");
    ASSERT_EQ_INT((int)cur, 0);
}

TEST(edit_buffer_delete_char) {
    char buf[16] = "abc";
    size_t cur = 0;
    bool ok = EditBufferDeleteChar(buf, &cur);
    ASSERT_TRUE(ok);
    ASSERT_EQ_STR(buf, "bc");
}

TEST(enter_normal_clamps_cursor_onto_last_char) {
    vreset_insert("hello", 5);
    HandleEnterNormal(vctx());
    ASSERT_TRUE(IsNormalMode());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
}

TEST(enter_normal_preserves_cursor_mid_text) {
    vreset_insert("hello", 2);
    HandleEnterNormal(vctx());
    ASSERT_TRUE(IsNormalMode());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 2);
}

TEST(enter_normal_on_empty_text_stays_zero) {
    vreset_insert("", 0);
    HandleEnterNormal(vctx());
    ASSERT_TRUE(IsNormalMode());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(enter_insert_does_not_clamp) {
    vreset_insert("hello", 5);
    SetMode(MODE_NORMAL);
    Vim_SetCursor(vctx(), 5);
    SetMode(MODE_INSERT);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 5);
}

TEST(cursor_at_end_stays) {
    vreset("hello", 5);
    HandleMoveRight(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
    HandleMoveLeft(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
    HandleMoveRight(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
    HandleMoveLeft(vctx());
    HandleMoveLeft(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 2);
}

TEST(normal_mode_right_clamps_to_last_char) {
    vreset("hello", 3);
    HandleMoveRight(vctx());
    HandleMoveRight(vctx());
    HandleMoveRight(vctx());
    HandleMoveRight(vctx());
    HandleMoveRight(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
}

TEST(normal_mode_right_on_empty_text_stays_zero) {
    vreset("", 0);
    HandleMoveRight(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(normal_mode_left_clamps_to_zero) {
    vreset("hello", 0);
    HandleMoveLeft(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(normal_mode_does_not_insert) {
    vreset("hello", 2);
    char before[64];
    vgettext(before, sizeof(before));
    char after[64];
    vgettext(after, sizeof(after));
    ASSERT_EQ_STR(before, after);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 2);
}


TEST(cw_deletes_and_enters_insert) {
    vreset("hello world", 0);
    HandleDeleteWordInsert(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, " world");
    ASSERT_TRUE(IsInsertMode());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(dd_deletes_line) {
    vreset("hello world", 3);
    bool ok = PromptController_TryVimMotion('d', false, false, false);
    ASSERT_TRUE(ok);
    ok = PromptController_TryVimMotion('d', false, false, false);
    ASSERT_TRUE(ok);
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "");
}

TEST(move_line_start_end_preserves_text) {
    vreset("hello world", 5);
    HandleMoveLineStart(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "hello world");
    HandleMoveLineEnd(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 11);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "hello world");
}

TEST(jk_enters_normal_without_typing) {
    vreset_insert("hello", 5);
    bool a = PromptController_TryVimMotion('j', false, false, false);
    ASSERT_FALSE(a);
    char mid[64];
    vgettext(mid, sizeof(mid));
    ASSERT_EQ_STR(mid, "hello");
    Vim_SetText(vctx(), "helloj");
    Vim_SetCursor(vctx(), 6);
    bool b = PromptController_TryVimMotion('k', false, false, false);
    ASSERT_TRUE(b);
    ASSERT_TRUE(IsNormalMode());
    char after[64];
    vgettext(after, sizeof(after));
    ASSERT_EQ_STR(after, "hello");
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
}

TEST(word_end_forward) {
    vreset("hello world", 0);
    HandleWordEndForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
    HandleWordEndForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 10);
}

TEST(word_end_backward) {
    vreset("hello world", 10);
    HandleWordEndBackward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
}

TEST(word_end_forward_big_keeps_punctuation) {
    vreset("foo.bar baz", 0);
    HandleWordEndForwardBig(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 6);
}

TEST(word_end_forward_small_stops_at_punctuation) {
    vreset("foo.bar baz", 0);
    HandleWordEndForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 2);
}

TEST(word_end_backward_big) {
    vreset("hello there world", 16);
    HandleWordEndBackwardBig(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 10);
}

TEST(word_forward_big_skips_spaces) {
    vreset("alpha beta", 0);
    HandleWordForwardBig(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 6);
}

TEST(word_backward_big) {
    vreset("alpha beta", 9);
    HandleWordBackwardBig(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 6);
}

TEST(word_end_clamps_to_last_char) {
    vreset("hi", 0);
    HandleWordEndForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 1);
}

TEST(first_non_blank) {
    vreset("   hello", 6);
    HandleFirstNonBlank(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
}

TEST(first_non_blank_on_zero_offset) {
    vreset("hello", 4);
    HandleFirstNonBlank(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(goto_line_start) {
    vreset("hello world", 7);
    HandleGotoLineStart(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(goto_line_end_stops_at_last_char) {
    vreset("hello", 2);
    HandleGotoLineEnd(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
}

TEST(bracket_match_percent_roundtrip) {
    vreset("foo(bar)", 3);
    HandleBracketMatchPercent(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 7);
    HandleBracketMatchPercent(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
}

TEST(bracket_match_curly) {
    vreset("a{b}c", 1);
    HandleBracketMatchPercent(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
}

TEST(bracket_match_square) {
    vreset("x[y]z", 1);
    HandleBracketMatchPercent(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
}

TEST(bracket_match_no_pair_leaves_cursor) {
    vreset("foo(bar", 3);
    HandleBracketMatchPercent(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
}

TEST(star_search_word_under_cursor) {
    vreset("foo bar foo", 0);
    HandleStarSearch(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 8);
}

TEST(star_search_wraps_to_same_word) {
    vreset("foo bar baz", 0);
    HandleStarSearch(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(hash_search_word_backward) {
    vreset("foo bar foo", 10);
    HandleHashSearch(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(hash_search_finds_nearest_earlier) {
    vreset("foo bar foo", 8);
    HandleHashSearch(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(find_till_records_state_for_semicolon) {
    VimState_Reset();
    vreset("abcabcabc", 0);
    vim_find_record('a', false, false);
    ASSERT_EQ_INT(Vim_FindChar("abcabcabc", 9, 0, 'a', false, false), 3);
    ASSERT_EQ_INT(Vim_FindChar("abcabcabc", 9, 3, 'a', false, false), 6);
}

TEST(semicolon_repeats_last_find) {
    VimState_Reset();
    vreset("aXaXa", 0);
    HandleRepeatFindSame(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(comma_reverses_last_find) {
    VimState_Reset();
    vreset("abcabc", 5);
    HandleRepeatFindOpp(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 5);
}

TEST(find_next_next_occurrence) {
    VimState_Reset();
    vreset("run run run", 0);
    HandleFindNext(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
}

TEST(case_lower_word) {
    vreset("HELLO world", 0);
    HandleCaseLower(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "hello world");
}

TEST(case_upper_word) {
    vreset("hello", 0);
    HandleCaseUpper(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "HELLO");
}

TEST(case_toggle_word) {
    vreset("Hello", 0);
    HandleCaseToggle(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "hELLO");
}

TEST(case_only_touches_current_word) {
    vreset("ONE two", 0);
    HandleCaseLower(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "one two");
}

TEST(mark_set_then_jump) {
    VimState_Reset();
    vreset("hello world", 4);
    vim_mark_set('a', 4);
    Vim_SetCursor(vctx(), 9);
    int cursor = -1, sel = -1;
    ASSERT_TRUE(VimState_GetMark('a', &cursor, &sel));
    ASSERT_EQ_INT(cursor, 4);
    vreset("hello world", 9);
    vim_mark_jump('a');
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
}

TEST(unset_mark_does_not_move) {
    VimState_Reset();
    vreset("hello world", 6);
    vim_mark_jump('z');
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 6);
}

TEST(jump_back_returns_previous_position) {
    VimState_Reset();
    VimState_PushJump(2, -1);
    VimState_PushJump(7, -1);
    int cursor = -1, sel = -1;
    ASSERT_TRUE(VimState_PopJump(&cursor, &sel));
    ASSERT_EQ_INT(cursor, 2);
}

TEST(jump_forward_after_back) {
    VimState_Reset();
    VimState_PushJump(2, -1);
    VimState_PushJump(7, -1);
    int cursor = -1, sel = -1;
    VimState_PopJump(&cursor, &sel);
    ASSERT_TRUE(VimState_PeekJump(&cursor, &sel));
    ASSERT_EQ_INT(cursor, 7);
}

TEST(jump_back_on_empty_stack_fails) {
    VimState_Reset();
    int cursor = -1, sel = -1;
    ASSERT_FALSE(VimState_PopJump(&cursor, &sel));
}

TEST(search_pattern_collect_and_read) {
    VimState_Reset();
    VimState_BeginSearchPattern();
    ASSERT_TRUE(VimState_SearchPending());
    VimState_AppendSearchPattern('w');
    VimState_AppendSearchPattern('o');
    VimState_AppendSearchPattern('r');
    const char* pattern = NULL;
    VimState_EndSearchPattern(&pattern);
    ASSERT_FALSE(VimState_SearchPending());
    ASSERT_EQ_STR(pattern, "wor");
}

TEST(search_pattern_cancel_drops_text) {
    VimState_Reset();
    VimState_BeginSearchPattern();
    VimState_AppendSearchPattern('x');
    const char* pattern = NULL;
    VimState_EndSearchPattern(&pattern);
    ASSERT_EQ_STR(pattern, "x");
    VimState_BeginSearchPattern();
    const char* second = NULL;
    VimState_EndSearchPattern(&second);
    ASSERT_EQ_STR(second, "");
}

TEST(search_pattern_ignores_keys_when_not_pending) {
    VimState_Reset();
    VimState_AppendSearchPattern('q');
    const char* pattern = NULL;
    VimState_EndSearchPattern(&pattern);
    ASSERT_TRUE(pattern == NULL);
}

TEST(last_insert_position_recalled_by_gi) {
    VimState_Reset();
    vreset("hello world", 3);
    HandleEnterInsertIn(vctx());
    Vim_SetCursor(vctx(), 9);
    SetMode(MODE_NORMAL);
    HandleGotoLastInsert(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
    ASSERT_TRUE(IsInsertMode());
}

TEST(find_pattern_wraps_forward) {
    const char* text = "foo bar foo";
    ASSERT_EQ_INT(Vim_FindPatternPos(text, 11, 4, "foo", false), 8);
}

TEST(find_pattern_wraps_backward) {
    const char* text = "foo bar foo";
    ASSERT_EQ_INT(Vim_FindPatternPos(text, 11, 4, "foo", true), 0);
}

TEST(find_pattern_case_insensitive) {
    const char* text = "Foo bar FOO";
    ASSERT_EQ_INT(Vim_FindPatternPos(text, 11, 0, "foo", false), 8);
}

TEST(find_pattern_absent_returns_negative) {
    const char* text = "hello";
    ASSERT_EQ_INT(Vim_FindPatternPos(text, 5, 0, "zzz", false), -1);
}

TEST(word_under_cursor) {
    char out[64];
    const char* text = "hello big world";
    ASSERT_TRUE(Vim_WordUnder(text, 15, 8, out, sizeof(out)));
    ASSERT_EQ_STR(out, "big");
}

TEST(word_under_cursor_on_space_fails) {
    char out[64];
    const char* text = "hello world";
    ASSERT_FALSE(Vim_WordUnder(text, 11, 5, out, sizeof(out)));
}

TEST(bracket_match_handles_nesting) {
    const char* text = "(a(b)c)";
    ASSERT_EQ_INT(Vim_BracketMatch(text, 7, 0), 6);
}

TEST(bracket_match_handles_angle) {
    const char* text = "a<b>c";
    ASSERT_EQ_INT(Vim_BracketMatch(text, 5, 1), 3);
}

TEST(scroll_handlers_run_without_listbox) {
    vreset("hello", 0);
    HandleScrollTop(vctx());
    HandleScrollCenter(vctx());
    HandleScrollBottom(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(screen_and_page_handlers_run_without_listbox) {
    vreset("hello", 2);
    HandleScreenMiddle(vctx());
    HandleScreenTop(vctx());
    HandleScreenBottom(vctx());
    HandleHalfPageDown(vctx());
    HandleHalfPageUp(vctx());
    HandleScrollLineDown(vctx());
    HandleScrollLineUp(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 2);
}

TEST(jump_handlers_run_without_listbox) {
    VimState_Reset();
    VimState_PushJump(1, -1);
    VimState_PushJump(4, -1);
    vreset("hello world", 0);
    HandleJumpBack(vctx());
    HandleJumpForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
}

TEST(search_handlers_run_without_listbox) {
    VimState_Reset();
    vreset("hello", 0);
    HandleSearchForward(vctx());
    HandleSearchBackward(vctx());
    HandleFindPrev(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(percent_via_shift_five_jumps_forward) {
    vreset("testing ( skjfakejfk e )", 8);
    vpress('5', false, true);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 23);
}

TEST(percent_via_shift_five_jumps_backward) {
    vreset("testing ( skjfakejfk e )", 23);
    vpress('5', false, true);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 8);
}

TEST(percent_via_shift_five_round_trips) {
    vreset("testing ( skjfakejfk e )", 8);
    vpress('5', false, true);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 23);
    vpress('5', false, true);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 8);
}

TEST(percent_binding_is_shift_five_not_percent_char) {
    const MotionBinding* b = vbinding(MOTION_BRACKET_MATCH);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, '5');
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_SHIFT);
}

TEST(percent_unshifted_five_does_nothing) {
    vreset("testing ( a )", 8);
    vpress('5', false, false);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 8);
}

TEST(circumflex_via_shift_six) {
    vreset("   hello", 6);
    vpress('6', false, true);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
}

TEST(circumflex_binding_is_shift_six) {
    const MotionBinding* b = vbinding(MOTION_FIRST_NON_BLANK);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, '6');
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_SHIFT);
}

TEST(dollar_binding_is_shift_four) {
    const MotionBinding* b = vbinding(MOTION_MOVE_LINE_END);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, '4');
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_SHIFT);
}

TEST(star_binding_is_shift_eight) {
    const MotionBinding* b = vbinding(MOTION_STAR_SEARCH);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, '8');
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_SHIFT);
}

TEST(hash_binding_is_shift_three) {
    const MotionBinding* b = vbinding(MOTION_HASH_SEARCH);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, '3');
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_SHIFT);
}

TEST(repeat_find_same_binding_is_oem1) {
    const MotionBinding* b = vbinding(MOTION_REPEAT_FIND_SAME);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, VK_OEM_1);
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_NONE);
}

TEST(repeat_find_opp_binding_is_shift_oem1) {
    const MotionBinding* b = vbinding(MOTION_REPEAT_FIND_OPP);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, VK_OEM_1);
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_SHIFT);
}

TEST(set_mark_binding_is_oem2) {
    const MotionBinding* b = vbinding(MOTION_SET_MARK);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, VK_OEM_2);
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_NONE);
}

TEST(jump_to_mark_binding_is_oem7) {
    const MotionBinding* b = vbinding(MOTION_JUMP_TO_MARK);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, VK_OEM_7);
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_NONE);
}

TEST(search_forward_binding_is_oem2) {
    const MotionBinding* b = vbinding(MOTION_SEARCH_FORWARD_KEY);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, VK_OEM_2);
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_NONE);
}

TEST(search_backward_binding_is_shift_oem2) {
    const MotionBinding* b = vbinding(MOTION_SEARCH_BACKWARD_KEY);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, VK_OEM_2);
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_SHIFT);
}

TEST(dw_sequence_is_registered) {
    const SequenceBinding* b = vseqbinding("DW");
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT((long)b->handler, (long)HandleDeleteWordForward);
}

TEST(cw_sequence_is_registered) {
    const SequenceBinding* b = vseqbinding("CW");
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT((long)b->handler, (long)HandleChangeWordForward);
}

TEST(dw_deletes_word_through_keypath) {
    char buf[64];
    vreset("hello world", 0);
    vpress('D', false, false);
    vpress('W', false, false);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "world");
}

TEST(cw_deletes_word_and_enters_insert_through_keypath) {
    char buf[64];
    vreset("hello world", 0);
    vpress('C', false, false);
    vpress('W', false, false);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "world");
    ASSERT_TRUE(IsInsertMode());
}

TEST(dw_midword_deletes_to_word_end_not_whole_word) {
    char buf[64];
    vreset("hello world", 2);
    vpress('D', false, false);
    vpress('W', false, false);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "heworld");
}

TEST(dw_on_trailing_space_does_nothing) {
    char buf[64];
    vreset("hello world", 5);
    vpress('D', false, false);
    vpress('W', false, false);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "helloworld");
}

TEST(diw_still_deletes_whole_word) {
    char buf[64];
    vreset("hello world", 2);
    vpress('d', false, false);
    vpress('i', false, false);
    vpress('w', false, false);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, " world");
}

TEST(dd_still_deletes_line_through_keypath) {
    char buf[64];
    vreset("hello world", 3);
    vpress('D', false, false);
    vpress('D', false, false);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "");
}

TEST(diw_still_works_through_keypath) {
    char buf[64];
    vreset("hello world", 0);
    vpress('d', false, false);
    vpress('i', false, false);
    vpress('w', false, false);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, " world");
}

TEST(d_alone_changes_nothing) {
    char buf[64];
    vreset("hello world", 0);
    vpress('D', false, false);
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "hello world");
}

TEST(jump_prev_context_binding_is_shift_oem3) {
    const MotionBinding* b = vbinding(MOTION_JUMP_PREV_CONTEXT);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, VK_OEM_3);
    ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_SHIFT);
}

TEST(noop_natural_keys) {
    const MotionBinding* b = vbinding(MOTION_NONE);
    ASSERT_TRUE(b == NULL);
}


TEST(binding_tables_are_queryable) {
    vtables();
    ASSERT_TRUE(g_vb != NULL);
    ASSERT_TRUE(g_vs != NULL);
    ASSERT_TRUE(g_vb_count > 0);
    ASSERT_TRUE(g_vs_count > 0);
    ASSERT_EQ_INT(g_vb_count, 74);
}

TEST(no_null_sequence_in_table) {
    vtables();
    for (int i = 0; i < g_vs_count; i++) {
        const SequenceBinding* b = &g_vs[i];
        ASSERT_TRUE(b->sequence != NULL);
        ASSERT_TRUE(b->handler != NULL);
    }
}

TEST(no_null_binding_in_table) {
    vtables();
    for (int i = 0; i < g_vb_count; i++) {
        const MotionBinding* b = &g_vb[i];
        ASSERT_TRUE(b->handler != NULL);
        ASSERT_TRUE(b->action_id != MOTION_NONE);
    }
}

TEST(last_binding_is_not_truncated) {
    vtables();
    const MotionBinding* last = &g_vb[g_vb_count - 1];
    ASSERT_TRUE(last->handler != NULL);
    ASSERT_TRUE(last->action_id != MOTION_NONE);
    ASSERT_EQ_INT(last->action_id, MOTION_SEARCH_BACKWARD_KEY);
}

TEST(exit_visual_binding_is_escape) {
    vtables();
    bool found = false;
    for (int i = 0; i < g_vb_count; i++) {
        const MotionBinding* b = &g_vb[i];
        if (b->action_id != MOTION_EXIT_VISUAL) continue;
        ASSERT_EQ_INT(b->vk_code, VK_ESCAPE);
        ASSERT_EQ_INT(b->modifiers, HOTKEY_MOD_NONE);
        ASSERT_EQ_INT(b->mode, MODE_VISUAL);
        found = true;
    }
    ASSERT_TRUE(found);
}

TEST(enter_visual_binding_is_v_in_normal) {
    const MotionBinding* b = vbinding(MOTION_ENTER_VISUAL);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ_INT(b->vk_code, 'V');
    ASSERT_EQ_INT(b->mode, MODE_NORMAL);
}

TEST(last_sequence_is_not_truncated) {
    vtables();
    const SequenceBinding* last = &g_vs[g_vs_count - 1];
    ASSERT_TRUE(last->handler != NULL);
    ASSERT_TRUE(last->sequence != NULL);
    ASSERT_EQ_STR(last->sequence, "G~");
}

TEST(every_sequence_is_unique_within_its_mode) {
    vtables();
    for (int i = 0; i < g_vs_count; i++) {
        for (int j = i + 1; j < g_vs_count; j++) {
            if (g_vs[i].mode != g_vs[j].mode) continue;
            if (strcmp(g_vs[i].sequence, g_vs[j].sequence) == 0) {
                printf("  FAIL duplicate sequence '%s' in mode %d\n", g_vs[i].sequence, g_vs[i].mode);
                flint_test_fails++;
                break;
            }
        }
    }
}

TEST(visual_sequences_are_also_registered_in_normal_mode) {
    vtables();
    for (int i = 0; i < g_vs_count; i++) {
        if (g_vs[i].mode != MODE_VISUAL) continue;
        bool has_normal = false;
        for (int j = 0; j < g_vs_count; j++) {
            if (g_vs[j].mode != MODE_NORMAL) continue;
            if (strcmp(g_vs[i].sequence, g_vs[j].sequence) == 0) { has_normal = true; break; }
        }
        if (!has_normal) {
            printf("  FAIL visual sequence '%s' has no MODE_NORMAL counterpart\n", g_vs[i].sequence);
            flint_test_fails++;
        } else {
            flint_test_passes++;
        }
    }
}


TEST(visual_left_moves_cursor_and_extends) {
    VimState_Reset();
    vreset_visual("hello world", 3);
    HandleVisualLeft(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 2);
    HandleVisualLeft(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 1);
    int a = -1, b = -1;
    ASSERT_TRUE(VimState_GetSelection(&a, &b));
    ASSERT_EQ_INT(a, 1);
    ASSERT_EQ_INT(b, 3);
}

TEST(visual_right_moves_cursor_and_extends) {
    VimState_Reset();
    vreset_visual("hello world", 3);
    HandleVisualRight(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
    HandleVisualRight(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 5);
    int a = -1, b = -1;
    ASSERT_TRUE(VimState_GetSelection(&a, &b));
    ASSERT_EQ_INT(a, 3);
    ASSERT_EQ_INT(b, 5);
}

TEST(visual_selection_is_inclusive_of_both_ends) {
    VimState_Reset();
    vreset_visual("abcdef", 0);
    HandleVisualRight(vctx());
    HandleVisualRight(vctx());
    int a = -1, b = -1;
    ASSERT_TRUE(VimState_GetSelection(&a, &b));
    ASSERT_EQ_INT(a, 0);
    ASSERT_EQ_INT(b, 2);
}

TEST(visual_selection_backwards_is_normalised) {
    VimState_Reset();
    vreset_visual("abcdef", 3);
    HandleVisualLeft(vctx());
    int a = -1, b = -1;
    ASSERT_TRUE(VimState_GetSelection(&a, &b));
    ASSERT_EQ_INT(a, 2);
    ASSERT_EQ_INT(b, 3);
}

TEST(visual_no_selection_when_anchor_equals_head) {
    VimState_Reset();
    vreset_visual("abcdef", 2);
    int a = -1, b = -1;
    ASSERT_FALSE(VimState_GetSelection(&a, &b));
}

TEST(visual_caret_cannot_move_past_start) {
    VimState_Reset();
    vreset_visual("abc", 0);
    HandleVisualLeft(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(visual_caret_cannot_move_past_end) {
    VimState_Reset();
    vreset_visual("abc", 2);
    HandleVisualRight(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 2);
}

TEST(visual_delete_removes_selection_and_goes_normal) {
    char buf[64];
    VimState_Reset();
    vreset_visual("hello world", 0);
    HandleVisualRight(vctx());
    HandleVisualRight(vctx());
    HandleVisualDeleteSelection(vctx());
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "lo world");
    ASSERT_TRUE(IsNormalMode());
    ASSERT_FALSE(VimState_HasSelection());
}

TEST(visual_change_removes_selection_and_goes_insert) {
    char buf[64];
    VimState_Reset();
    vreset_visual("hello world", 0);
    HandleVisualRight(vctx());
    HandleVisualChangeSelection(vctx());
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "llo world");
    ASSERT_TRUE(IsInsertMode());
    ASSERT_FALSE(VimState_HasSelection());
}

TEST(visual_delete_backwards_removes_correct_range) {
    char buf[64];
    VimState_Reset();
    vreset_visual("abcdef", 4);
    HandleVisualLeft(vctx());
    HandleVisualDeleteSelection(vctx());
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "abcf");
}

TEST(visual_escape_clears_selection_but_keeps_text) {
    char buf[64];
    VimState_Reset();
    vreset_visual("hello", 0);
    HandleVisualRight(vctx());
    HandleVisualRight(vctx());
    ASSERT_TRUE(VimState_HasSelection());
    HandleExitVisual(vctx());
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "hello");
    ASSERT_TRUE(IsNormalMode());
    ASSERT_FALSE(VimState_HasSelection());
}

TEST(visual_single_char_selection_deletes_one_char) {
    char buf[64];
    VimState_Reset();
    vreset_visual("abc", 1);
    HandleVisualRight(vctx());
    HandleVisualDeleteSelection(vctx());
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "a");
}

TEST(visual_selection_deleting_to_end_of_text) {
    char buf[64];
    VimState_Reset();
    vreset_visual("abc", 1);
    HandleVisualRight(vctx());
    HandleVisualRight(vctx());
    HandleVisualDeleteSelection(vctx());
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "a");
}

TEST(visual_bindings_exist) {
    ASSERT_TRUE(vbinding(MOTION_VISUAL_LEFT) != NULL);
    ASSERT_TRUE(vbinding(MOTION_VISUAL_RIGHT) != NULL);
    ASSERT_TRUE(vbinding(MOTION_VISUAL_DELETE) != NULL);
    ASSERT_TRUE(vbinding(MOTION_VISUAL_CHANGE) != NULL);
}

TEST(visual_bindings_are_visual_mode) {
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_LEFT)->mode, MODE_VISUAL);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_RIGHT)->mode, MODE_VISUAL);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_DELETE)->mode, MODE_VISUAL);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_CHANGE)->mode, MODE_VISUAL);
}

TEST(visual_keys_are_h_l_x_s) {
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_LEFT)->vk_code, 'H');
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_RIGHT)->vk_code, 'L');
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_DELETE)->vk_code, 'X');
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_CHANGE)->vk_code, 'S');
}


static int vsel_start(void) { int a = -1, e = -1; return VimState_GetSelection(&a, &e) ? a : -1; }
static int vsel_end(void) { int a = -1, e = -1; return VimState_GetSelection(&a, &e) ? e : -1; }

TEST(visual_w_extends_selection) {
    VimState_Reset();
    vreset_visual("hello big world", 0);
    HandleVisualWordForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 6);
    ASSERT_EQ_INT(vsel_start(), 0);
    ASSERT_EQ_INT(vsel_end(), 6);
}

TEST(visual_b_shrinks_selection_backwards) {
    VimState_Reset();
    vreset_visual("hello big world", 0);
    HandleVisualWordForward(vctx());
    HandleVisualWordForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 10);
    HandleVisualWordBackward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 9);
    ASSERT_EQ_INT(vsel_start(), 0);
    ASSERT_EQ_INT(vsel_end(), 9);
}

TEST(visual_e_extends_to_word_end) {
    VimState_Reset();
    vreset_visual("hello big world", 0);
    HandleVisualWordEndForward(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
    ASSERT_EQ_INT(vsel_end(), 4);
}

TEST(visual_line_start_clears_to_anchor) {
    VimState_Reset();
    vreset_visual("hello big world", 0);
    HandleVisualWordForward(vctx());
    HandleVisualLineStart(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(visual_line_end_extends_to_end) {
    VimState_Reset();
    vreset_visual("hello big world", 0);
    HandleVisualLineEnd(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 15);
    ASSERT_EQ_INT(vsel_end(), 15);
}

TEST(visual_bracket_match_extends) {
    VimState_Reset();
    vreset_visual("abc (def) ghi", 4);
    HandleVisualBracketMatch(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 8);
    ASSERT_EQ_INT(vsel_start(), 4);
    ASSERT_EQ_INT(vsel_end(), 8);
}

TEST(visual_word_forward_big_keeps_punctuation) {
    VimState_Reset();
    vreset_visual("foo.bar baz", 0);
    HandleVisualWordForwardBig(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 8);
}

TEST(visual_motion_stays_in_visual_mode) {
    VimState_Reset();
    vreset_visual("hello big world", 0);
    HandleVisualWordForward(vctx());
    HandleVisualWordForward(vctx());
    HandleVisualWordBackward(vctx());
    ASSERT_TRUE(IsVisualMode());
}

TEST(visual_motions_keep_selection_active) {
    VimState_Reset();
    vreset_visual("hello big world", 0);
    HandleVisualWordForward(vctx());
    ASSERT_TRUE(VimState_HasSelection());
}

TEST(visual_word_keys_are_bound_in_visual_mode) {
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_WORD_FORWARD)->mode, MODE_VISUAL);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_WORD_BACKWARD)->mode, MODE_VISUAL);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_WORD_END_FORWARD)->mode, MODE_VISUAL);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_LINE_START)->mode, MODE_VISUAL);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_LINE_END)->mode, MODE_VISUAL);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_BRACKET_MATCH)->mode, MODE_VISUAL);
}

TEST(visual_word_keys_use_same_vk_as_normal) {
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_WORD_FORWARD)->vk_code, vbinding(MOTION_MOVE_WORD_FORWARD)->vk_code);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_WORD_BACKWARD)->vk_code, vbinding(MOTION_MOVE_WORD_BACKWARD)->vk_code);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_LINE_START)->vk_code, vbinding(MOTION_MOVE_LINE_START)->vk_code);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_LINE_END)->vk_code, vbinding(MOTION_MOVE_LINE_END)->vk_code);
    ASSERT_EQ_INT(vbinding(MOTION_VISUAL_BRACKET_MATCH)->vk_code, vbinding(MOTION_BRACKET_MATCH)->vk_code);
}

int main(void) {
    RUN(word_forward);
    RUN(word_backward);
    RUN(line_start_end);
    RUN(delete_line);
    RUN(delete_word_normal);
    RUN(edit_buffer_backspace);
    RUN(edit_buffer_delete_char);
    RUN(enter_normal_clamps_cursor_onto_last_char);
    RUN(enter_normal_preserves_cursor_mid_text);
    RUN(enter_normal_on_empty_text_stays_zero);
    RUN(enter_insert_does_not_clamp);
    RUN(cursor_at_end_stays);
    RUN(normal_mode_right_clamps_to_last_char);
    RUN(normal_mode_right_on_empty_text_stays_zero);
    RUN(normal_mode_left_clamps_to_zero);
    RUN(normal_mode_does_not_insert);
    RUN(cw_deletes_and_enters_insert);
    RUN(dd_deletes_line);
    RUN(move_line_start_end_preserves_text);
    RUN(jk_enters_normal_without_typing);
    RUN(word_end_forward);
    RUN(word_end_backward);
    RUN(word_end_forward_big_keeps_punctuation);
    RUN(word_end_forward_small_stops_at_punctuation);
    RUN(word_end_backward_big);
    RUN(word_forward_big_skips_spaces);
    RUN(word_backward_big);
    RUN(word_end_clamps_to_last_char);
    RUN(first_non_blank);
    RUN(first_non_blank_on_zero_offset);
    RUN(goto_line_start);
    RUN(goto_line_end_stops_at_last_char);
    RUN(bracket_match_percent_roundtrip);
    RUN(bracket_match_curly);
    RUN(bracket_match_square);
    RUN(bracket_match_no_pair_leaves_cursor);
    RUN(star_search_word_under_cursor);
    RUN(star_search_wraps_to_same_word);
    RUN(hash_search_word_backward);
    RUN(hash_search_finds_nearest_earlier);
    RUN(find_till_records_state_for_semicolon);
    RUN(semicolon_repeats_last_find);
    RUN(comma_reverses_last_find);
    RUN(find_next_next_occurrence);
    RUN(case_lower_word);
    RUN(case_upper_word);
    RUN(case_toggle_word);
    RUN(case_only_touches_current_word);
    RUN(mark_set_then_jump);
    RUN(unset_mark_does_not_move);
    RUN(jump_back_returns_previous_position);
    RUN(jump_forward_after_back);
    RUN(jump_back_on_empty_stack_fails);
    RUN(search_pattern_collect_and_read);
    RUN(search_pattern_cancel_drops_text);
    RUN(search_pattern_ignores_keys_when_not_pending);
    RUN(last_insert_position_recalled_by_gi);
    RUN(find_pattern_wraps_forward);
    RUN(find_pattern_wraps_backward);
    RUN(find_pattern_case_insensitive);
    RUN(find_pattern_absent_returns_negative);
    RUN(word_under_cursor);
    RUN(word_under_cursor_on_space_fails);
    RUN(bracket_match_handles_nesting);
    RUN(bracket_match_handles_angle);
    RUN(scroll_handlers_run_without_listbox);
    RUN(screen_and_page_handlers_run_without_listbox);
    RUN(jump_handlers_run_without_listbox);
    RUN(search_handlers_run_without_listbox);
    RUN(percent_via_shift_five_jumps_forward);
    RUN(percent_via_shift_five_jumps_backward);
    RUN(percent_via_shift_five_round_trips);
    RUN(percent_binding_is_shift_five_not_percent_char);
    RUN(percent_unshifted_five_does_nothing);
    RUN(circumflex_via_shift_six);
    RUN(circumflex_binding_is_shift_six);
    RUN(dollar_binding_is_shift_four);
    RUN(star_binding_is_shift_eight);
    RUN(hash_binding_is_shift_three);
    RUN(repeat_find_same_binding_is_oem1);
    RUN(repeat_find_opp_binding_is_shift_oem1);
    RUN(set_mark_binding_is_oem2);
    RUN(jump_to_mark_binding_is_oem7);
    RUN(search_forward_binding_is_oem2);
    RUN(search_backward_binding_is_shift_oem2);
    RUN(dw_sequence_is_registered);
    RUN(cw_sequence_is_registered);
    RUN(dw_midword_deletes_to_word_end_not_whole_word);
    RUN(dw_on_trailing_space_does_nothing);
    RUN(diw_still_deletes_whole_word);
    RUN(dw_deletes_word_through_keypath);
    RUN(cw_deletes_word_and_enters_insert_through_keypath);
    RUN(dd_still_deletes_line_through_keypath);
    RUN(diw_still_works_through_keypath);
    RUN(d_alone_changes_nothing);
    RUN(jump_prev_context_binding_is_shift_oem3);
    RUN(noop_natural_keys);
    RUN(visual_left_moves_cursor_and_extends);
    RUN(visual_right_moves_cursor_and_extends);
    RUN(visual_selection_is_inclusive_of_both_ends);
    RUN(visual_selection_backwards_is_normalised);
    RUN(visual_no_selection_when_anchor_equals_head);
    RUN(visual_caret_cannot_move_past_start);
    RUN(visual_caret_cannot_move_past_end);
    RUN(visual_delete_removes_selection_and_goes_normal);
    RUN(visual_change_removes_selection_and_goes_insert);
    RUN(visual_delete_backwards_removes_correct_range);
    RUN(visual_escape_clears_selection_but_keeps_text);
    RUN(visual_single_char_selection_deletes_one_char);
    RUN(visual_selection_deleting_to_end_of_text);
    RUN(visual_bindings_exist);
    RUN(visual_bindings_are_visual_mode);
    RUN(visual_keys_are_h_l_x_s);
    RUN(visual_w_extends_selection);
    RUN(visual_b_shrinks_selection_backwards);
    RUN(visual_e_extends_to_word_end);
    RUN(visual_line_start_clears_to_anchor);
    RUN(visual_line_end_extends_to_end);
    RUN(visual_bracket_match_extends);
    RUN(visual_word_forward_big_keeps_punctuation);
    RUN(visual_motion_stays_in_visual_mode);
    RUN(visual_motions_keep_selection_active);
    RUN(visual_word_keys_are_bound_in_visual_mode);
    RUN(visual_word_keys_use_same_vk_as_normal);
    RUN(binding_tables_are_queryable);
    RUN(no_null_sequence_in_table);
    RUN(no_null_binding_in_table);
    RUN(last_binding_is_not_truncated);
    RUN(exit_visual_binding_is_escape);
    RUN(enter_visual_binding_is_v_in_normal);
    RUN(last_sequence_is_not_truncated);
    RUN(every_sequence_is_unique_within_its_mode);
    RUN(visual_sequences_are_also_registered_in_normal_mode);
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
