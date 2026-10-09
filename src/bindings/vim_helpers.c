#include "core/context.h"
#include "deps/log.h"
#include "core/modal.h"
#include "bindings/vim_helpers.h"
#include "platform/ui.h"
#include "core/vim.h"
#include <ctype.h>

static bool vim_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool vim_is_word(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static bool vim_in_class(char c, bool bigword) {
    return bigword ? !vim_is_space(c) : vim_is_word(c);
}

int Vim_FirstNonBlank(const char* text, int len) {
    for (int i = 0; i < len; i++) {
        if (!vim_is_space(text[i])) return i;
    }
    return 0;
}

int Vim_WordStep(const char* text, int len, int pos, bool bigword, bool to_end, bool forward) {
    if (len <= 0) return 0;
    if (pos < 0) pos = 0;
    if (pos > len) pos = len;

    if (forward) {
        int i = pos;
        if (!to_end) {
            bool in_class = false;
            if (i < len) in_class = bigword ? !vim_is_space(text[i]) : vim_is_word(text[i]);
            while (i < len) {
                bool c = bigword ? !vim_is_space(text[i]) : vim_is_word(text[i]);
                if (c != in_class) break;
                i++;
            }
            while (i < len && vim_is_space(text[i])) i++;
        } else {
            if (i >= len) return len - 1 > 0 ? len - 1 : 0;
            if (vim_in_class(text[i], bigword)) {
                bool at_word_end = (i + 1 >= len) || !vim_in_class(text[i + 1], bigword);
                if (!at_word_end) {
                    while (i + 1 < len && vim_in_class(text[i + 1], bigword)) i++;
                    return i;
                }
                i++;
            }
            while (i < len && !vim_in_class(text[i], bigword)) i++;
            if (i >= len) return len - 1 > 0 ? len - 1 : 0;
            while (i + 1 < len && vim_in_class(text[i + 1], bigword)) i++;
            return i;
        }
        if (i >= len) return len - 1 > 0 ? len - 1 : 0;
        return i;
    }

    int i = pos;
    if (!to_end) {
        while (i > 0 && vim_is_space(text[i - 1])) i--;
        bool in_class = false;
        if (i > 0) in_class = bigword ? !vim_is_space(text[i - 1]) : vim_is_word(text[i - 1]);
        while (i > 0) {
            char c = text[i - 1];
            bool cls = bigword ? !vim_is_space(c) : vim_is_word(c);
            if (cls != in_class) break;
            i--;
        }
    } else {
        while (i > 0 && i < len && vim_in_class(text[i - 1], bigword)) i--;
        i--;
        while (i >= 0 && !vim_in_class(text[i], bigword)) i--;
        if (i < 0) return 0;
        while (i + 1 < len && vim_in_class(text[i + 1], bigword)) i++;
    }
    if (i < 0) i = 0;
    return i;
}

int Vim_FindChar(const char* text, int len, int pos, char target, bool till, bool backward) {
    if (len <= 0) return -1;
    if (!backward) {
        for (int i = pos + 1; i < len; i++) {
            if (text[i] != target) continue;
            return till ? (i - 1 > pos ? i - 1 : pos) : i;
        }
        return -1;
    }
    for (int i = pos - 1; i >= 0; i--) {
        if (text[i] != target) continue;
        return till ? (i + 1 < pos ? i + 1 : pos) : i;
    }
    return -1;
}

int Vim_FindPatternPos(const char* text, int len, int pos, const char* pattern, bool backward) {
    int plen = (int)strlen(pattern);
    if (plen <= 0 || len <= 0) return -1;
    int limit = backward ? 0 : len - plen;
    if (backward) {
        for (int i = pos - 1; i >= limit; i--)
            if (strncasecmp(text + i, pattern, plen) == 0) return i;
        for (int i = len - plen; i >= limit; i--)
            if (i < pos && strncasecmp(text + i, pattern, plen) == 0) return i;
        return -1;
    }
    for (int i = pos + 1; i + plen <= len; i++)
        if (strncasecmp(text + i, pattern, plen) == 0) return i;
    for (int i = 0; i + plen <= len; i++)
        if (i <= pos && strncasecmp(text + i, pattern, plen) == 0) return i;
    return -1;
}

int Vim_BracketMatch(const char* text, int len, int pos) {
    static const char* pairs = "(){}[]<>";
    if (len <= 0) return -1;
    if (pos < 0) pos = 0;
    if (pos >= len) pos = len - 1;

    char open = 0, close = 0;
    for (int i = 0; i < 6; i++) {
        char o = pairs[i * 2];
        char c = pairs[i * 2 + 1];
        if (text[pos] == o) { open = o; close = c; break; }
        if (text[pos] == c) { open = o; close = c; break; }
    }

    if (open && text[pos] == open) {
        int depth = 0;
        for (int i = pos; i < len; i++) {
            if (text[i] == open) depth++;
            else if (text[i] == close) { depth--; if (depth == 0) return i; }
        }
        return -1;
    }
    if (open && text[pos] == close) {
        int depth = 0;
        for (int i = pos; i >= 0; i--) {
            if (text[i] == close) depth++;
            else if (text[i] == open) { depth--; if (depth == 0) return i; }
        }
        return -1;
    }

    int start = -1, end = -1;
    for (int i = pos + 1; i < len; i++) {
        char o = 0, c = 0;
        for (int p = 0; p < 6; p++) {
            if (text[i] == pairs[p * 2] || text[i] == pairs[p * 2 + 1]) { o = pairs[p * 2]; c = pairs[p * 2 + 1]; break; }
        }
        if (o) { start = i; end = -1; break; }
    }
    if (start < 0) return -1;
    for (int i = start; i < len; i++) {
        for (int p = 0; p < 6; p++) {
            if (pairs[p * 2] == text[start] && text[i] == pairs[p * 2 + 1]) { end = i; break; }
        }
        if (end >= 0) break;
    }
    if (end < 0) return -1;
    return end;
}

bool Vim_WordUnder(const char* text, int len, int pos, char* out, int cap) {
    if (!out || cap <= 0) return false;
    out[0] = '\0';
    if (len <= 0) return false;
    if (pos < 0 || pos >= len) return false;
    if (vim_is_space(text[pos])) return false;
    int start = pos;
    while (start > 0 && !vim_is_space(text[start - 1])) start--;
    int end = pos;
    while (end < len && !vim_is_space(text[end])) end++;
    int n = end - start;
    if (n >= cap) n = cap - 1;
    memcpy(out, text + start, n);
    out[n] = '\0';
    return n > 0;
}

void Vim_TransformCase(FlintContext* ctx, int mode) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    int pos = Vim_GetCursor(ctx);
    if (len <= 0 || pos < 0 || pos >= len) return;
    int start = pos, end = pos;
    while (start > 0 && !vim_is_space(text[start - 1])) start--;
    while (end < len && !vim_is_space(text[end])) end++;
    for (int i = start; i < end; i++) {
        unsigned char c = (unsigned char)text[i];
        if (mode == 0) text[i] = (char)tolower(c);
        else if (mode == 1) text[i] = (char)toupper(c);
        else text[i] = (char)(isupper(c) ? tolower(c) : toupper(c));
    }
    Vim_SetText(ctx, text);
    Vim_SetCursor(ctx, pos);
}

void DeleteCharUnderCursor(FlintContext* ctx) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    Vim_GetText(ctx, text, sizeof(text));
    int pos = Vim_GetCursor(ctx);
    int len = (int)strlen(text);
    if (pos < len) {
        memmove(&text[pos], &text[pos + 1], len - pos);
        Vim_SetText(ctx, text);
        Vim_SetCursor(ctx, pos);
    }
}

void GoToFirstListBoxItem(FlintContext* ctx) {
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb) return;
    UI_SetSelection(lb, ctx->window.hListBox, 0);
    UI_LB_SetTopIndex(ctx->window.hListBox,0);
}

void GoToLastListBoxItem(FlintContext* ctx) {
    ListBoxInfo* lb = Vim_GetListBox(ctx);
    if (!lb) return;
    int lastIndex = lb->populated_count - 1;
    UI_SetSelection(lb, ctx->window.hListBox, lastIndex);
    UI_LB_SetCaretIndex(ctx->window.hListBox, lastIndex, false);
}

void SetCursorToStartOfText(FlintContext* ctx) {
    if (!ctx) return;
    Vim_SetCursor(ctx, 0);
}

void SetCursorToEndOfText(FlintContext* ctx) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    Vim_GetText(ctx, text, sizeof(text));
    Vim_SetCursor(ctx, (int)strlen(text));
}

void DeleteInsideDelimiter(FlintContext* ctx, char open_char, char close_char, bool keep_mode) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    int cursor = Vim_GetCursor(ctx);
    int start = -1, end = -1;
    int open_pos = -1;
    for (int i = cursor - 1; i >= 0; i--) {
        if (text[i] == open_char) { open_pos = i; break; }
    }
    int close_pos = -1;
    for (int i = cursor; i < len; i++) {
        if (text[i] == close_char) { close_pos = i; break; }
    }
    if (open_pos != -1 && close_pos != -1 && open_pos < close_pos) {
        start = open_pos; end = close_pos;
    } else {
        int close_left = -1;
        for (int i = cursor - 1; i >= 0; i--) if (text[i] == close_char) { close_left = i; break; }
        int open_left = -1;
        if (close_left != -1) {
            int depth = 0;
            for (int i = close_left - 1; i >= 0; i--) {
                if (text[i] == close_char) depth++;
                if (text[i] == open_char) {
                    if (depth == 0) { open_left = i; break; } else depth--;
                }
            }
        }
        if (open_left != -1 && close_left != -1 && open_left < close_left) {
            start = open_left; end = close_left;
        } else if (open_char == close_char) {
            int found = 0;
            for (int i = 0; i < len; i++) if (text[i] == open_char) {
                if (found == 0) { start = i; found = 1; } else { end = i; break; }
            }
        } else {
            int depth = 0; int first_open = -1;
            for (int i = 0; i < len; i++) {
                if (text[i] == open_char) { if (first_open == -1) first_open = i; depth++; }
                if (text[i] == close_char && depth > 0) { depth--; if (depth == 0) { start = first_open; end = i; break; } }
            }
        }
    }
    if (start == -1 || end == -1 || start >= end - 1) {
        log_info("No valid delimiters found!");
        return;
    }
    int delete_start = start + 1;
    int delete_end = end;
    memmove(&text[delete_start], &text[delete_end], len - delete_end + 1);
    Vim_SetText(ctx, text);
    Vim_SetCursor(ctx, delete_start);
    if (keep_mode) SetMode(MODE_INSERT); else SetMode(MODE_NORMAL);
}

void DeleteWordForward(FlintContext* ctx, bool keep_mode) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    int cursor = Vim_GetCursor(ctx);
    if (len == 0 || cursor >= len) {
        if (keep_mode) SetMode(MODE_INSERT); else SetMode(MODE_NORMAL);
        return;
    }

    int start = cursor;
    int end = cursor;
    while (end < len && !vim_is_space(text[end])) end++;
    while (end < len && vim_is_space(text[end])) end++;

    if (end == start) {
        while (start > 0 && vim_is_space(text[start - 1])) start--;
    }

    memmove(&text[start], &text[end], (size_t)(len - end + 1));
    Vim_SetText(ctx, text);
    Vim_SetCursor(ctx, start);
    if (keep_mode) SetMode(MODE_INSERT); else SetMode(MODE_NORMAL);
}

void DeleteInnerWord(FlintContext* ctx, bool keep_mode) {
    if (!ctx) return;
    char text[VIM_TEXT_BUF];
    Vim_GetText(ctx, text, sizeof(text));
    int len = (int)strlen(text);
    int cursor = Vim_GetCursor(ctx);
    int word_start = cursor;
    while (word_start > 0) { char c = text[word_start - 1]; if (!isalnum((unsigned char)c) && c != '_') break; word_start--; }
    int word_end = cursor;
    while (word_end < len) { char c = text[word_end]; if (!isalnum((unsigned char)c) && c != '_') break; word_end++; }
    if (word_start == cursor && word_end == cursor) {
        word_end = cursor;
        while (word_end < len) { char c = text[word_end]; if (isalnum((unsigned char)c) || c == '_') break; word_end++; }
        if (word_end >= len) return;
        word_start = word_end;
        while (word_start < len) { char c = text[word_start]; if (!isalnum((unsigned char)c) && c != '_') break; word_start++; }
        int temp = word_start; word_start = cursor; word_end = temp;
    }
    if (word_start == cursor && word_end == cursor) {
        word_start = cursor;
        while (word_start > 0) { char c = text[word_start - 1]; if (!isalnum((unsigned char)c) && c != '_') break; word_start--; }
        word_end = cursor;
        while (word_end < len) { char c = text[word_end]; if (!isalnum((unsigned char)c) && c != '_') break; word_end++; }
    }
    if (word_start == cursor && word_end == cursor) return;
    if (word_start > word_end) { int temp = word_start; word_start = word_end; word_end = temp; }
    if (word_start > 0 && word_end < len && text[word_start - 1] == ' ' && text[word_end] == ' ') {
        word_end++;
        if (word_end > len) word_end = len;
    }
    if (word_start >= 0 && word_end <= len && word_start < word_end) {
        memmove(&text[word_start], &text[word_end], len - word_end + 1);
        Vim_SetText(ctx, text);
        Vim_SetCursor(ctx, word_start);
        if (keep_mode) SetMode(MODE_INSERT); else SetMode(MODE_NORMAL);
    }
}
