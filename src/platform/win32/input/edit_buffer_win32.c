#include <stdbool.h>
#include <string.h>
#include "core/timer.h"
#include "config.h"
#include "platform/input.h"

void EditBufferBackspace(char* buf, size_t* cursor) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    size_t n = strlen(buf);
    if (*cursor > 0) {
        memmove(buf + *cursor - 1,
                buf + *cursor,
                n - *cursor + 1);
        (*cursor)--;
    }
    PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferBackspace");
}

bool EditBufferDeleteChar(char* buf, size_t* cursor) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    size_t n = strlen(buf);
    if (*cursor >= n) { PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferDeleteChar"); return false; }
    memmove(buf + *cursor,
            buf + *cursor + 1,
            n - *cursor);
    PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferDeleteChar");
    { PROFILE_END_VAR(_pt, "EditBufferDeleteChar"); return true; }
}

static bool fl_edit_is_quote(char c) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_edit_is_quote"); return c == '"' || c == '\''; }
}

static bool fl_edit_is_closer(char c) {
        PROFILE_START(_pt);
{ PROFILE_END_VAR(_pt, "fl_edit_is_closer"); return c == '}' || c == ')' || c == ']' || fl_edit_is_quote(c); }
}

static bool fl_edit_quote_at_word_start(const char* buf, size_t cursor) {
        PROFILE_START(_pt);
if (cursor == 0) { PROFILE_END_VAR(_pt, "fl_edit_quote_at_word_start"); return true; }
    unsigned char prev = (unsigned char)buf[cursor - 1];
    if (prev >= 0x80) { PROFILE_END_VAR(_pt, "fl_edit_quote_at_word_start"); return false; }
    if (prev >= 'a' && prev <= 'z') { PROFILE_END_VAR(_pt, "fl_edit_quote_at_word_start"); return false; }
    if (prev >= 'A' && prev <= 'Z') { PROFILE_END_VAR(_pt, "fl_edit_quote_at_word_start"); return false; }
    if (prev >= '0' && prev <= '9') { PROFILE_END_VAR(_pt, "fl_edit_quote_at_word_start"); return false; }
    { PROFILE_END_VAR(_pt, "fl_edit_quote_at_word_start"); return true; }
}

char EditBufferPairFor(char c) {
        PROFILE_START(_pt);
switch (c) {
        case '{': return '}';
        case '(': return ')';
        case '[': return ']';
        case '"': return '"';
        case '\'': return '\'';
        default: return 0;
    }
}

size_t EditBufferInsertText(char* buf, size_t cap, size_t* cursor, const char* text, size_t len) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!buf || !cursor || !text || len == 0 || cap == 0) { PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferInsertText"); return 0; }
    size_t n = strlen(buf);
    if (*cursor > n) *cursor = n;

    char closer = 0;
    if (EnableAutoPairing && len == 1) {
        if (fl_edit_is_closer(text[0]) && *cursor < n && buf[*cursor] == text[0]) {
            (*cursor)++;
            PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferInsertText");
            return 0;
        }
        if (!fl_edit_is_quote(text[0]) || fl_edit_quote_at_word_start(buf, *cursor)) {
            closer = EditBufferPairFor(text[0]);
        }
    }

    size_t total = len + (closer ? (size_t)1 : (size_t)0);
    if (n + total >= cap) { PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferInsertText"); return 0; }
    memmove(buf + *cursor + total, buf + *cursor, n - *cursor + 1);
    memcpy(buf + *cursor, text, len);
    if (closer) buf[*cursor + len] = closer;
    *cursor += len;
    PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferInsertText");
    { PROFILE_END_VAR(_pt, "EditBufferInsertText"); return total; }
}

bool EditBufferBackspacePair(char* buf, size_t* cursor) {
        PROFILE_START(_pt);
PROFILE_START(timer);
    if (!buf || !cursor || !EnableAutoPairing) { PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferBackspacePair"); return false; }
    size_t c = *cursor;
    size_t n = strlen(buf);
    if (c == 0 || c > n) { PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferBackspacePair"); return false; }
    char closer = buf[c];
    if (closer == '\0' || !fl_edit_is_closer(closer)) { PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferBackspacePair"); return false; }
    if (EditBufferPairFor(buf[c - 1]) != closer) { PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferBackspacePair"); return false; }
    memmove(buf + c - 1, buf + c + 1, n - c);
    (*cursor)--;
    PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferBackspacePair");
    { PROFILE_END_VAR(_pt, "EditBufferBackspacePair"); return true; }
}
