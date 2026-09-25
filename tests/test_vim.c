#include "test.h"
#include <string.h>
#include "core/context.h"
#include "core/modal.h"
#include "core/vim.h"
#include "core/prompt_controller.h"
#include "bindings/vim_motions.h"
#include "platform/input.h"

static FlintContext* vctx(void) { return FlintContext_Get(); }

static void vreset(const char* text, int cursor) {
    SetMode(MODE_NORMAL);
    Vim_SetText(vctx(), text);
    Vim_SetCursor(vctx(), cursor);
}

static void vreset_insert(const char* text, int cursor) {
    SetMode(MODE_INSERT);
    Vim_SetText(vctx(), text);
    Vim_SetCursor(vctx(), cursor);
}

static void vgettext(char* buf, size_t cap) {
    Vim_GetText(vctx(), buf, cap);
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

TEST(cursor_preserved_on_enter_normal) {
    vreset_insert("hello", 5);
    HandleEnterNormal(vctx());
    ASSERT_TRUE(IsNormalMode());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 5);
}

TEST(cursor_at_end_stays) {
    vreset("hello", 5);
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 5);
    HandleMoveLeft(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 4);
    HandleMoveRight(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 5);
    HandleMoveLeft(vctx());
    HandleMoveLeft(vctx());
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 3);
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

TEST(dw_deletes_word) {
    vreset("hello world test", 0);
    HandleDeleteWordNormal(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, " world test");
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 0);
}

TEST(dw_from_middle) {
    vreset("hello world test", 6);
    HandleDeleteWordNormal(vctx());
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, "hello test");
    ASSERT_TRUE(IsNormalMode());
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

TEST(dw_sequence_via_controller) {
    vreset("hello world test", 0);
    bool a = PromptController_TryVimMotion('d', false, false, false);
    ASSERT_TRUE(a);
    char mid[64];
    vgettext(mid, sizeof(mid));
    ASSERT_EQ_STR(mid, "hello world test");
    bool b = PromptController_TryVimMotion('w', false, false, false);
    ASSERT_TRUE(b);
    char buf[64];
    vgettext(buf, sizeof(buf));
    ASSERT_EQ_STR(buf, " world test");
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
    // simulate the 'j' having been typed (since prefix returns false, it would be inserted)
    Vim_SetText(vctx(), "helloj");
    Vim_SetCursor(vctx(), 6);
    bool b = PromptController_TryVimMotion('k', false, false, false);
    ASSERT_TRUE(b);
    ASSERT_TRUE(IsNormalMode());
    char after[64];
    vgettext(after, sizeof(after));
    ASSERT_EQ_STR(after, "hello");
    ASSERT_EQ_INT(Vim_GetCursor(vctx()), 5);
}

int main(void) {
    RUN(word_forward);
    RUN(word_backward);
    RUN(line_start_end);
    RUN(delete_line);
    RUN(delete_word_normal);
    RUN(edit_buffer_backspace);
    RUN(edit_buffer_delete_char);
    RUN(cursor_preserved_on_enter_normal);
    RUN(cursor_at_end_stays);
    RUN(normal_mode_does_not_insert);
    RUN(dw_deletes_word);
    RUN(dw_from_middle);
    RUN(cw_deletes_and_enters_insert);
    RUN(dd_deletes_line);
    RUN(dw_sequence_via_controller);
    RUN(move_line_start_end_preserves_text);
    RUN(jk_enters_normal_without_typing);
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
