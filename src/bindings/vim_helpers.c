#include "core/context.h"
#include "core/modal.h"
#include "bindings/vim_helpers.h"
#include "core/common_ui.h"
#include "core/vim.h"
#include <ctype.h>
#if defined(_WIN32)
#include <windows.h>
#endif

void DeleteCharUnderCursor(VimContext* ctx) {
    if (!ctx) return;
    char text[512];
    Vim_GetText(ctx, text, sizeof(text));
    int pos = Vim_GetCursor(ctx);
    int len = (int)strlen(text);
    if (pos < len) {
        memmove(&text[pos], &text[pos + 1], len - pos);
        Vim_SetText(ctx, text);
        Vim_SetCursor(ctx, pos - 1);
    }
}

void GoToFirstListBoxItem(VimContext* ctx) {
    if (!ctx || !ctx->lb) return;
    UI_SetSelection(ctx->lb, ctx->hListBox, 0);
    UI_LB_SetTopIndex(ctx->hListBox,0);
}

void GoToLastListBoxItem(VimContext* ctx) {
    if (!ctx || !ctx->lb) return;
    int lastIndex = ctx->lb->populated_count - 1;
    UI_SetSelection(ctx->lb, ctx->hListBox, lastIndex);
    UI_LB_SetCaretIndex(ctx->hListBox, lastIndex, false);
}

void SetCursorToStartOfText(VimContext* ctx) {
    if (!ctx) return;
#if defined(_WIN32)
    SetFocus(WH_TO_HWND(ctx->hEdit));
#endif
    Vim_SetCursor(ctx, 0);
}

void SetCursorToEndOfText(VimContext* ctx) {
    if (!ctx) return;
#if defined(_WIN32)
    SetFocus(WH_TO_HWND(ctx->hEdit));
    int len = GetWindowTextLengthA(WH_TO_HWND(ctx->hEdit));
    Vim_SetCursor(ctx, len);
    return;
#endif
    char text[512];
    Vim_GetText(ctx, text, sizeof(text));
    Vim_SetCursor(ctx, (int)strlen(text));
}

void DeleteInsideDelimiter(VimContext* ctx, char open_char, char close_char, bool keep_mode) {
    if (!ctx) return;
    char text[512];
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
        printf("[DEBUG] No valid delimiters found!\n");
        return;
    }
    int delete_start = start + 1;
    int delete_end = end;
    memmove(&text[delete_start], &text[delete_end], len - delete_end + 1);
    Vim_SetText(ctx, text);
    Vim_SetCursor(ctx, delete_start);
    if (keep_mode) SetMode(MODE_INSERT); else SetMode(MODE_NORMAL);
}

void DeleteInnerWord(VimContext* ctx, bool keep_mode) {
    if (!ctx) return;
    char text[512];
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
    if (word_start >= 0 && word_end <= len && word_start < word_end) {
        memmove(&text[word_start], &text[word_end], len - word_end + 1);
        Vim_SetText(ctx, text);
        Vim_SetCursor(ctx, word_start);
        if (keep_mode) SetMode(MODE_INSERT); else SetMode(MODE_NORMAL);
    }
}
