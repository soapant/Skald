#include "test.h"
#include <string.h>
#include "core/context.h"
#include "core/modal.h"
#include "core/vim.h"
#include "bindings/vim_motions.h"
#include "platform/x11/input_x11.h"

static FlintContext* vctx(void) { return FlintContext_Get(); }

static void vreset(const char* text, int cursor) {
    SetMode(MODE_NORMAL);
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

int main(void) {
    RUN(word_forward);
    RUN(word_backward);
    RUN(line_start_end);
    RUN(delete_line);
    RUN(delete_word_normal);
    RUN(edit_buffer_backspace);
    RUN(edit_buffer_delete_char);
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
