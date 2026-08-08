#include "common.h"
#include "bindings/vim_helpers.h"
#include "core/common_ui.h"
#include <ctype.h>

void DeleteCharUnderCursor(void) {
    if (!g_ActiveEditControl || !IsWindow(g_ActiveEditControl)) return;
    
    char text[512];
    UI_GetCurrentText(g_ActiveEditControl, text);
    DWORD start, end;
    UI_Edit_GetSel(g_ActiveEditControl, &start, &end);
    
    int pos = (int)start;
    int len = (int)strlen(text);
    
    if (pos < len) {
        memmove(&text[pos], &text[pos + 1], len - pos);
        
        UI_SetText(g_ActiveEditControl,text);
        
        UI_SetCursorPosition(g_ActiveEditControl,pos);
    }
}

void GoToFirstListBoxItem(void) {
    UI_SetSelection(hListBox,0,g_SelectedIndex);
    UI_LB_SetTopIndex(hListBox,0);
}

void GoToLastListBoxItem(void) {
    int lastIndex = g_populated_count - 1;
    UI_SetSelection(hListBox,lastIndex,g_SelectedIndex);
    UI_LB_SetCaretIndex(hListBox, lastIndex, FALSE);
}

void SetCursorToStartOfText(void) {
    if (g_ActiveEditControl && IsWindow(g_ActiveEditControl)) {
        SetFocus(g_ActiveEditControl);
        UI_SetCursorPosition(g_ActiveEditControl,0);
    }
}

void SetCursorToEndOfText(void) {
    if (g_ActiveEditControl && IsWindow(g_ActiveEditControl)) {
        SetFocus(g_ActiveEditControl);
        int len = GetWindowTextLengthA(g_ActiveEditControl);
        UI_SetCursorPosition(g_ActiveEditControl,len);
    }
}

void DeleteInsideDelimiter(char open_char, char close_char, bool keep_mode) {
    if (!g_ActiveEditControl || !IsWindow(g_ActiveEditControl)) {
        return;
    }
    
    char text[512];
    UI_GetCurrentText(g_ActiveEditControl, text);
    int len = (int)strlen(text);
    
    DWORD start_pos, end_pos;
    UI_Edit_GetSel(g_ActiveEditControl, &start_pos, &end_pos);
    int cursor = (int)start_pos;
    
    int start = -1, end = -1;
    
    int open_pos = -1;
    for (int i = cursor - 1; i >= 0; i--) {
        if (text[i] == open_char) {
            open_pos = i;
            break;
        }
    }
    
    int close_pos = -1;
    for (int i = cursor; i < len; i++) {
        if (text[i] == close_char) {
            close_pos = i;
            break;
        }
    }
    
    if (open_pos != -1 && close_pos != -1 && open_pos < close_pos) {
        start = open_pos;
        end = close_pos;
    } else {
        // === BACKWARDS: Cursor is after the closing bracket ===
        // Search left for close_char
        int close_left = -1;
        for (int i = cursor - 1; i >= 0; i--) {
            if (text[i] == close_char) {
                close_left = i;
                break;
            }
        }
        
        // Search left from close_char for matching open_char
        int open_left = -1;
        if (close_left != -1) {
            int depth = 0;
            for (int i = close_left - 1; i >= 0; i--) {
                if (text[i] == close_char) depth++;
                if (text[i] == open_char) {
                    if (depth == 0) {
                        open_left = i;
                        break;
                    } else {
                        depth--;
                    }
                }
            }
        }
        
        if (open_left != -1 && close_left != -1 && open_left < close_left) {
            start = open_left;
            end = close_left;
        } else if (open_char == close_char) {
            // For quotes (same char), find the pair
            int found = 0;
            for (int i = 0; i < len; i++) {
                if (text[i] == open_char) {
                    if (found == 0) { start = i; found = 1; }
                    else { end = i; break; }
                }
            }
        } else {
            // Fallback: scan entire text
            int depth = 0;
            int first_open = -1;
            for (int i = 0; i < len; i++) {
                if (text[i] == open_char) {
                    if (first_open == -1) first_open = i;
                    depth++;
                }
                if (text[i] == close_char && depth > 0) {
                    depth--;
                    if (depth == 0) {
                        start = first_open;
                        end = i;
                        break;
                    }
                }
            }
        }
    }
    
    if (start == -1 || end == -1 || start >= end - 1) {
        printf("[DEBUG] No valid delimiters found!\n");
        return;
    }
    
    // Delete inside
    int delete_start = start + 1;
    int delete_end = end;
    
    printf("[DEBUG] Deleting from %d to %d (inside %c%c)\n", delete_start, delete_end, open_char, close_char);
    
    memmove(&text[delete_start], &text[delete_end], len - delete_end + 1);
    UI_SetText(g_ActiveEditControl, text);
    UI_Edit_SetSel(g_ActiveEditControl, delete_start, delete_start);
    
    if (keep_mode) {
        SetMode(MODE_INSERT);
    } else {
        SetMode(MODE_NORMAL);
    }
}

void DeleteInnerWord(bool keep_mode) {
    if (!g_ActiveEditControl || !IsWindow(g_ActiveEditControl)) return;
    
    char text[512];
    UI_GetCurrentText(g_ActiveEditControl, text);
    int len = (int)strlen(text);
    
    DWORD start_pos, end_pos;
    UI_Edit_GetSel(g_ActiveEditControl, &start_pos, &end_pos);
    int cursor = (int)start_pos;
    
    
    int word_start = cursor;
    while (word_start > 0) {
        char c = text[word_start - 1];
        if (!isalnum(c) && c != '_') break;
        word_start--;
    }
    
    int word_end = cursor;
    while (word_end < len) {
        char c = text[word_end];
        if (!isalnum(c) && c != '_') break;
        word_end++;
    }
    
    if (word_start == cursor && word_end == cursor) {
        word_end = cursor;
        while (word_end < len) {
            char c = text[word_end];
            if (isalnum(c) || c == '_') break;
            word_end++;
        }
        if (word_end >= len) return; // No word found
        word_start = word_end;
        while (word_start < len) {
            char c = text[word_start];
            if (!isalnum(c) && c != '_') break;
            word_start++;
        }
        int temp = word_start;
        word_start = cursor;
        word_end = temp;
    }
    
    if (word_start == cursor && word_end == cursor) {
        word_start = cursor;
        while (word_start > 0) {
            char c = text[word_start - 1];
            if (!isalnum(c) && c != '_') break;
            word_start--;
        }
        word_end = cursor;
        while (word_end < len) {
            char c = text[word_end];
            if (!isalnum(c) && c != '_') break;
            word_end++;
        }
    }
    
    if (word_start == cursor && word_end == cursor) {
        return;
    }
    
    
    if (word_start > word_end) {
        int temp = word_start;
        word_start = word_end;
        word_end = temp;
    }
    
    if (word_start >= 0 && word_end <= len && word_start < word_end) {
        int delete_len = word_end - word_start;
        memmove(&text[word_start], &text[word_end], len - word_end + 1);
        
        UI_SetText(g_ActiveEditControl, text);
        
        UI_Edit_SetSel(g_ActiveEditControl, word_start, word_start);
        
        if (keep_mode) {
            SetMode(MODE_INSERT);
        } else {
            SetMode(MODE_NORMAL);
        }
    }
}
