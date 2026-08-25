#include "common.h"
#include "config.h"
#include <ctype.h>
#if defined(_WIN32)
#include <shellapi.h>
#include <shlwapi.h>
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#endif
#include "helpers.h"
#include "core/timer.h"
#include "core/common_ui.h"

int g_refresh_count = 0;
ColorRef DisplayMessageText;
ColorRef g_rainbow_text_cache[MAX_VISIBLE_ITEMS + 1] = {0};
bool draw_background = true;

char* SelectionData[2048] = {0};
int SelectionDataCount = 0;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color) {
    if (MessagesEnabled) {
        DisplayMessageText = color;
        if (WH_TO_HWND(hPopupWnd) != NULL) {
          MSG_TOP(buffer,msg_height,timeout);
        }else {
          MSG_MIDDLE_BOTTOM(buffer,msg_height / 2,timeout);
        }
        DisplayMessageText = COLOR_TEXT;
    }
}
EditorMode g_last_mode = MODE_INSERT;
bool AlwaysEnabled(void) { return true; }
bool AlwaysDisabled(void) { return false; }
bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                                  int pressed_vk, int modifiers,
                                  const char* current_title) {
    for (int i = 0; i < bindings_count; i++) {
        const HotkeyBinding* b = &bindings[i];
        
        if (b->vk_code != pressed_vk) continue;
        if (b->modifiers != modifiers) continue;
        
        if (b->is_enabled && !b->is_enabled()) {
            continue;
        }
        
        if (b->blacklist && b->blacklist_count > 0) {
            bool blacklisted = false;
            for (int j = 0; j < b->blacklist_count; j++) {
                if (b->blacklist[j][0] == '\0') continue;
#if defined(_WIN32)
                bool found = StrStrIA(current_title, b->blacklist[j]) != NULL;
#else
                bool found = strstr(current_title, b->blacklist[j]) != NULL;
#endif
                if (found) {
                    blacklisted = true;
                    break;
                }
            }
            if (blacklisted) return false;
        }
        
        if (AllowOtherFeatureTogglesInFeature) {
#if defined(_WIN32)
            if (IsPopupActive(hPopupWnd)) {
                KillTimer(WH_TO_HWND(hPopupWnd), 999);
                DestroyWindow(WH_TO_HWND(hPopupWnd));
                hPopupWnd = WH_NULL;
            }
#else
            if (IsPopupActive(hPopupWnd)) {
                extern bool selection_complete;
                selection_complete = true;
            }
            // this is the actual closing of the current window since i am too lazy to do the refactoring necessary for this just leave it for now
            // g_currentPromptState->selection;
#endif
        } else {
          if (IsPopupActive(hPopupWnd)) {
            return false;
          }
        }
        
        if (b->handler) {
            g_timer = TimerStart();
            if (MessageAfterEachHotkey) {
              char buffer[64];
              snprintf(buffer,sizeof(buffer),"[%s] Triggered \n",b->name);
              MSG_BOTTOM(buffer,140,TimeOutForMessages);
            }
            b->handler();
            return true;
        }
    }
    return false;
}

int ParseTimeToSeconds(const char* input) {
    if (!input || strlen(input) == 0) return 0;
    
    int total = 0;
    char buffer[64];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char* token = strtok(buffer, " ");
    while (token != NULL) {
        int value = 0;
        char unit = 's';
        int len = strlen(token);
        
        if (len > 1) {
            char last = tolower(token[len - 1]);
            if (last == 's' || last == 'm' || last == 'h' || last == 'd') {
                token[len - 1] = '\0';
                value = atoi(token);
                unit = last;
            } else {
                value = atoi(token);
            }
        } else {
            value = atoi(token);
        }
        
        switch (unit) {
            case 's': total += value; break;
            case 'm': total += value * 60; break;
            case 'h': total += value * 3600; break;
            case 'd': total += value * 86400; break;
            default: total += value; break;
        }
        
        token = strtok(NULL, " ");
    }
    
    return total;
}

int SearchFlagsInText(const char* text, const void* flag_array, int flag_count, size_t flag_size, FoundFlag* matches, int max_matches) {
    if (!text || !flag_array || !matches) return 0;
    
    int match_count = 0;
    char* text_copy = _strdup(text);
    if (!text_copy) return 0;
    
    for (int i = 0; text_copy[i]; i++) {
        text_copy[i] = tolower(text_copy[i]);
    }
    
    for (int i = 0; i < flag_count && match_count < max_matches; i++) {
        const char* flag_str = NULL;
        
        if (flag_size == sizeof(char*)) {
            flag_str = ((char**)flag_array)[i];
        } else if (flag_size == sizeof(ActionDef)) {
            flag_str = ((ActionDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(ConditionDef)) {
            flag_str = ((ConditionDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(TimerDef)) {
            flag_str = ((TimerDef*)flag_array)[i].str;
        } else if (flag_size == sizeof(GenericFlagEntry)) {
            flag_str = ((const GenericFlagEntry*)flag_array)[i].flag;
        }
        
        if (!flag_str) continue;
        
        char* found = strstr(text_copy, flag_str);
        if (found) {
            int before_ok = (found == text_copy || *(found - 1) == ' ');
            
            int after_ok = 1;
            char* after = found + strlen(flag_str);
            if (*after != '\0' && *after != ' ' && *after != '\t') {
                after_ok = 0;
            }
            
            if (before_ok && after_ok) {
                matches[match_count].flag = (char*)flag_str;
                matches[match_count].index = found - text_copy;
                match_count++;
            }
        }
    }
    
    free(text_copy);
    return match_count;
}

char* GetFlagParameter(const char* text, int flag_index) {
    if (!text || flag_index < 0) return NULL;
    
    static char param[256];
    param[0] = '\0';
    
    int pos = flag_index;
    while (text[pos] && text[pos] != ' ') pos++;
    while (text[pos] && text[pos] == ' ') pos++;
    
    int start = pos;
    while (text[pos]) {
        if (text[pos] == '-' && (pos == 0 || text[pos-1] == ' ')) {
            break;
        }
        pos++;
    }
    
    int len = pos - start;
    if (len > 0) {
        strncpy(param, text + start, len);
        param[len] = '\0';
        
        char* end = param + strlen(param) - 1;
        while (end > param && *end == ' ') end--;
        *(end + 1) = '\0';
        
        return param;
    }
    
    return NULL;
}

char* GetCleanText(const char* text) {
    if (!text) return NULL;
    
    static char clean[256];
    clean[0] = '\0';
    
    const char* flags[] = {"-f", "-cg", "-trw", "-tr", "-t"};
    int flag_count = sizeof(flags) / sizeof(flags[0]);
    
    char* text_copy = _strdup(text);
    if (!text_copy) return NULL;
    
    char* result = text_copy;
    char* write_ptr = text_copy;
    
    while (*result) {
        int skip = 0;
        for (int i = 0; i < flag_count; i++) {
            if (strncmp(result, flags[i], strlen(flags[i])) == 0) {
                skip = 1;
                result += strlen(flags[i]);
                while (*result && *result == ' ') result++;
                while (*result && *result != ' ') result++;
                while (*result && *result == ' ') result++;
                break;
            }
        }
        
        if (!skip) {
            *write_ptr = *result;
            write_ptr++;
            result++;
        }
    }
    
    *write_ptr = '\0';
    
    char* start = text_copy;
    while (*start == ' ') start++;
    char* end = text_copy + strlen(text_copy) - 1;
    while (end > start && *end == ' ') end--;
    *(end + 1) = '\0';
    
    strncpy(clean, start, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    
    free(text_copy);
    return clean;
}

bool IsWindowBlacklisted(const char* window_title, const char* blacklist[], int count) {
    for (int i = 0; i < count; i++) {
#if defined(_WIN32)
        if (StrStrIA(window_title, blacklist[i]) != NULL) {
#else
        if (strstr(window_title, blacklist[i]) != NULL) {
#endif
            return true;
        }
    }
    return false;
}

int UI_LookupTrueIndex(const char* name) {
    if (!g_item_index_map || !name) return -1;
    size_t value = (size_t)HashMap_Get(g_item_index_map, name);
    return value > 0 ? (int)(value - 1) : -1;
}

void SaveFeatureHistory(char* string) {
    char** HistoryList = NULL;
    int count = 0;
    
    HistoryList = (char**)HashMap_Get(feature_history, g_current_feature);
    while (HistoryList && HistoryList[count]) count++;
    
    HistoryList = StringList_Append(HistoryList, &count, string);
    
    HashMap_Put(feature_history, g_current_feature, HistoryList);
}

int ReplaceLineInFile(const char* filepath, int line_index, const char* new_text) {
    if (!filepath || !new_text || line_index < 0) {
        return 0;
    }
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        return 0;
    }
    
    char lines[100][512];
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), f) && count < 100) {
        lines[count][strcspn(lines[count], "\r\n")] = '\0';
        count++;
    }
    fclose(f);
    
    if (line_index >= count) {
        return 0;
    }
    
    strncpy(lines[line_index], new_text, sizeof(lines[line_index]) - 1);
    lines[line_index][sizeof(lines[line_index]) - 1] = '\0';
    
    f = fopen(filepath, "w");
    if (!f) {
        return 0;
    }
    
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    
    return 1;
}

void WriteToCacheIndex(ListBoxInfo* lb, int index, const char* text) {
    if (!lb) return;
    if (index < 0 || index >= MAX_ITEMS || !text) return;
    
    strncpy(lb->populated_cache[index], text, sizeof(lb->populated_cache[index]) - 1);
    lb->populated_cache[index][sizeof(lb->populated_cache[index]) - 1] = '\0';
}

int ReadMdFile(char lines[100][512], const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    
    int count = 0;
    char buffer[512];
    
    while (fgets(buffer, sizeof(buffer), f) && count < 100) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        
        if (strlen(buffer) > 0) {
            strncpy(lines[count], buffer, 511);
            lines[count][511] = '\0';
            
            count++;
        } 
    }
    
    fclose(f);
    return count;
}

void StripCheckboxes(char* str) {
    char* read_ptr = str;
    char* write_ptr = str;
    bool is_first = true;

    while (*read_ptr != '\0') {
        if (strncmp(read_ptr, "- [ ] ", 6) == 0) {
            if (!is_first) {
                *write_ptr = ','; 
                write_ptr++;
            }
            is_first = false; 
            read_ptr += 6;
        } else {
            *write_ptr = *read_ptr;
            write_ptr++;
            read_ptr++;
        }
    }
    *write_ptr = '\0';
}

void WriteMdFile(char lines[100][512], int count, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
}

HashMap* history_indices = NULL;

bool GetFromHistory(ListBoxInfo* lb) {
    char** HistoryList = (char**)HashMap_Get(feature_history, g_current_feature);
    if (HistoryList) {
        int* idx_ptr = (int*)HashMap_Get(history_indices, g_current_feature);
        int idx = idx_ptr ? *idx_ptr : 0;
        
        int count = 0;
        while (HistoryList[count]) count++;
        
        if (count > 0) {
            idx = (idx - 1 + count) % count;
            
            UI_SetText(hEditControl, HistoryList[idx]);
            UI_SetCursorPosition(hEditControl,strlen(HistoryList[idx]));

            if (!IsNormalMode()) {
              UI_Redraw(lb, hListBox);
            }
            if (!idx_ptr) {
                idx_ptr = malloc(sizeof(int));
                if (!idx_ptr) return true;
                if (!history_indices) history_indices = HashMap_Create(32);
                if (!history_indices) { free(idx_ptr); return true; }
                HashMap_Put(history_indices, g_current_feature, idx_ptr);
                if (!HashMap_Get(history_indices, g_current_feature)) { free(idx_ptr); return true; }
            }
            *idx_ptr = idx;
            return true;
        } 
    }
    return false;
}

int convert_string_to_int(const char *str) {
    if (*str == '\0') return 0;
    int num = 0;
    for (int i = 0;i < strlen(str);i++) {
        num *= 10;
        num += str[i] - '0';
    }
    return num;
}

void HandleCommandExecution(ListBoxInfo* lb, char* string) {
    if (!lb) return;
    int num = convert_string_to_int(string);
    UI_SetSelection(lb, hListBox, num - 1);
    UI_LB_SetCaretIndex(hListBox, num, TRUE);
}

const char* get_alias_for_item(const char* item) {
    if (!g_add_alias_to_string) return NULL;
    if (!g_current_feature) return NULL;
    if (!item) return NULL;

    for (size_t i = 0; i < ALIAS_ENTRY_COUNT; i++) {
        if (strcmp(Aliases[i].feature, g_current_feature) == 0) continue;
        const char* expanded = Aliases[i].text;
        if (!expanded || expanded[0]=='\0') expanded = Aliases[i].text;
        if (!expanded) continue;
        if (strcasecmp(item, expanded) == 0) {
            return Aliases[i].alias;
        }
    }
    return NULL;
}

unsigned long rgb_to_hex(unsigned long win_color) {
    unsigned long r = win_color & 0xFF;          
    unsigned long g = (win_color >> 8) & 0xFF;  
    unsigned long b = (win_color >> 16) & 0xFF;

    return (r << 16) | (g << 8) | b;
}

void eval_window_title_color(PromptState* state,unsigned long *window_title_color) {
  unsigned long final_color;
  if (state->item_count > 1000) {
    final_color = rgb_to_hex(RGB(255,0,0));
  } else if (state->item_count > 500) {
    final_color = rgb_to_hex(RGB(255,165,0));
  } else if (state->item_count > 200) {
    final_color = rgb_to_hex(RGB(255,255,0));
  } else if (state->item_count > 100) {
    final_color = rgb_to_hex(RGB(255,255,50));
  } else {
    final_color = rgb_to_hex(RGB(30,255,30));
  }
  *window_title_color = final_color;
}

void eval_xoffset_for_alias(char* alias_text,int main_text_lenght,int* xOffsetAlias) {
  int main_text_w = (main_text_lenght * FontSize) / 2;
  // if the main text is small we want the offset to be smaller vice versa
  *xOffsetAlias = main_text_w;
}

unsigned short OpacityAlpha16(void) {
    float t = WindowOpacity / 255.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (unsigned short)(t * 65535.0f + 0.5f);
}

#if !defined(_WIN32)
static int xft_width(Display* dpy, XftFont* xf, const char* text, int len) {
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, len, &ext);
    return ext.xOff;
}

void draw_wrapped_text(Display* dpy, XftDraw* xd, XftColor* color, FontHandle font, const char* text, int startY, char* centered_to, int xOffset) {
    if (!text) return;
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) return;
    int lineHeight = xf->ascent + xf->descent;
    int spaceWidth = xft_width(dpy, xf, " ", 1);
    int y = startY + xf->ascent;
    int text_len = strlen(text);
    int maxWidth = 0;   
    int alignment = 0;

    if (centered_to != NULL) {
        if (strstr(centered_to, "CENTER") != NULL) {
            alignment = 1;
            maxWidth = 870;
        } else if (strstr(centered_to, "RIGHT") != NULL) {
            alignment = 2;
            maxWidth = 900;
        } else {
            alignment = 0;
            maxWidth = FontSize * text_len;
        }
    } else {
        alignment = 0;
        maxWidth = FontSize * text_len;
    }

    const char* p = text;
    const char* lineStart = text;
    int lineWidth = 0;

    while (*p) {
        const char* word_start = p;
        while (*p && *p != ' ') p++;
        int wordWidth = xft_width(dpy, xf, word_start, p - word_start);
        int add = wordWidth + (lineWidth > 0 ? spaceWidth : 0);
        
        if (lineWidth > 0 && lineWidth + add > maxWidth) {
            int lw = xft_width(dpy, xf, lineStart, word_start - lineStart);
            
            int drawX = 0;
            if (alignment == 1) drawX = (maxWidth - lw) / 2 + xOffset;
            else if (alignment == 2) drawX = maxWidth - lw - xOffset;      
            else                     drawX = xOffset;                      
            
            XftDrawStringUtf8(xd, color, xf, drawX, y, (FcChar8*)lineStart, word_start - lineStart);
            y += lineHeight;
            lineStart = word_start;
            lineWidth = wordWidth;
        } else {
            lineWidth += add;
        }
        while (*p == ' ') p++;
    }
    
    if (p > lineStart) {
        int lw = xft_width(dpy, xf, lineStart, p - lineStart);

        int drawX = 0;
        if (alignment == 1)      drawX = xOffset + (maxWidth - lw) / 2;
        else if (alignment == 2) drawX = maxWidth - lw - xOffset;
        else                     drawX = xOffset;

        XftDrawStringUtf8(xd, color, xf, drawX, y, (FcChar8*)lineStart, p - lineStart); 
    }
}

void Linux_XftAllocColor(Display* dpy, Visual* vis, Colormap cmap, unsigned long hex, unsigned short alpha, XftColor* out) {
    XRenderColor rc = {
        ((hex >> 16) & 0xFF) * 0x101,
        ((hex >> 8) & 0xFF) * 0x101,
        (hex & 0xFF) * 0x101,
        alpha
    };
    XftColorAllocValue(dpy, vis, cmap, &rc, out);
}

unsigned long VimModeTextColor(void) {
    if (g_current_mode == MODE_NORMAL) return rgb_to_hex(COLOR_NORMAL_MODE_TEXT);
    if (g_current_mode == MODE_INSERT) return rgb_to_hex(COLOR_INSERT_MODE_TEXT);
    return rgb_to_hex(COLOR_VISUAL_MODE_TEXT);
}

int Linux_UpdateEditScrollOffset(Display* dpy, XftFont* xf, const char* text, int text_len, int cursor, int width, int prev_scroll, int* out_cursor_x) {
    int visible_w = width - 40;
    int cursor_x = 0;
    int text_w = 0;
    if (xf) {
        XGlyphInfo cext;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, cursor, &cext);
        cursor_x = cext.xOff;
        XGlyphInfo textExt;
        XftTextExtentsUtf8(dpy, xf, (FcChar8*)text, text_len, &textExt);
        text_w = textExt.xOff;
    }
    int scroll = prev_scroll;
    int margin = 20;
    if (cursor_x - scroll > visible_w - margin) scroll = cursor_x - visible_w + margin;
    if (cursor_x - scroll < margin) scroll = cursor_x - margin;
    if (scroll < 0) scroll = 0;
    if (text_w - scroll < visible_w && scroll > 0) {
        if (text_w < visible_w) scroll = 0;
        else scroll = text_w - visible_w;
    }
    if (cursor_x - scroll < 0) scroll = cursor_x;
    if (cursor_x - scroll > visible_w) scroll = cursor_x - visible_w;
    *out_cursor_x = cursor_x;
    return scroll;
}

void Linux_DrawEditCursor(XftDraw* xd, XftColor* color, int cursor_x, int scroll, int width) {
    int cursor_draw_x = 22 + cursor_x - scroll;
    if (cursor_draw_x < 20) cursor_draw_x = 20;
    if (cursor_draw_x > width - 10) cursor_draw_x = width - 10;
    if (IsInsertMode()) {
        XftDrawRect(xd, color, cursor_draw_x, 47, 2, FontSize);
    } else {
        XftDrawRect(xd, color, cursor_draw_x, 47, 12, FontSize);
    }
}

const char* ListBoxItemDisplayText(ListBoxInfo* lb, int idx, char* trunc, size_t trunc_size) {
    int raw = (idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : -1;
    const char* baseText = NULL;
    if (g_alias_display_active && raw >= 0 && raw < g_alias_display_count && g_alias_display[raw][0] != '\0')
        baseText = g_alias_display[raw];
    else
        baseText = lb->populated_cache[idx];
    snprintf(trunc, trunc_size, "%.80s", baseText ? baseText : "");
    return trunc;
}

void DrawListBoxItemText(Display* dpy, XftDraw* xd, Visual* vis, Colormap cmap, FontHandle font,
                         XftColor* num_color, XftColor* body_color, XftColor* plain_color,
                         ListBoxInfo* lb, int idx, int y, int text_x) {
    XftFont* xf = FH_TO_XFT(font);
    if (!xf) return;


    char trunc[512];
    ListBoxItemDisplayText(lb, idx, trunc, sizeof(trunc));
    int XOffset = 10;
    if (EnableLineNumbers) {
        char num[8];
        snprintf(num, sizeof(num), "%d.", idx + 1);
        draw_wrapped_text(dpy, xd, num_color, font, num, y, "LEFT", 10);
        XOffset = get_offset_based_on_number(idx);
    }

    char* alias_text = strstr(trunc, " [");

    if (alias_text) {
        XftColor aliasCol;
        int XOffsetAlias = 0;
        Linux_XftAllocColor(dpy, vis, cmap, rgb_to_hex(COLOR_ALIAS_TEXT), 0xFFFF, &aliasCol);
        eval_xoffset_for_alias(alias_text,strlen(trunc),&XOffsetAlias);
        draw_wrapped_text(dpy, xd, &aliasCol, font, alias_text, y, "LEFT",XOffsetAlias);
        XftColorFree(dpy, vis, cmap, &aliasCol);
    }

    char* pos =strchr(trunc,'['); 
    if(pos) {
      *pos = 0;
    }
    draw_wrapped_text(dpy, xd, plain_color, font, trunc, y, "LEFT", XOffset);

}

bool CursorBack(size_t* cursor, size_t text_len) {
    if (*cursor == 0) return false; 
    (*cursor)--;
    return true;
}

bool CursorForward(size_t* cursor, size_t text_len) {
    if (*cursor >= text_len) return false;
    (*cursor)++;
    return true;
}

static int x11_error_occurred = 0;

static int dummy_x_error_handler(Display *dpy, XErrorEvent *error) {
    if (error->error_code == BadWindow) {
        x11_error_occurred = 1;
        return 0;
    }
    return 0;
}

pid_t GetWindowPid(Display* dpy, Window w) {
    if (w == 0) return -1;

    Atom prop = XInternAtom(dpy, "_NET_WM_PID", True);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop_data = NULL;

    // Set custom error trap
    x11_error_occurred = 0;
    int (*old_handler)(Display*, XErrorEvent*) = XSetErrorHandler(dummy_x_error_handler);

    pid_t pid = -1;
    if (prop != None) {
        int status = XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_CARDINAL,
                                        &actual_type, &actual_format, &nitems,
                                        &bytes_after, &prop_data);
        if (status == Success && !x11_error_occurred && prop_data && nitems > 0) {
            pid = *((pid_t*)prop_data);
        }
    }

    if (prop_data) {
        XFree(prop_data);
    }

    // Restore old error handler
    XSetErrorHandler(old_handler);

    if (x11_error_occurred) {
        return -1; // Window was destroyed mid-call
    }

    return pid;
}

void GetExeNameFromPid(pid_t pid, char* outExe, size_t maxLen) {
    if (pid <= 0) {
        strncpy(outExe, "unknown", maxLen);
        return;
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/exe", pid);
    
    ssize_t len = readlink(path, outExe, maxLen - 1);
    if (len != -1) {
        outExe[len] = '\0';
        char* basename = strrchr(outExe, '/');
        if (basename) {
            memmove(outExe, basename + 1, strlen(basename));
        }
    } else {
        strncpy(outExe, "unknown", maxLen);
    }
}

int get_offset_based_on_number(int index) {
    if (index < 9) {        
        return 35;
    } else if (index < 99) { 
        return 50;
    } else if (index < 999) {
        return 60;
    } else if (index < 9999) {
        return 70;
    }
}

Window Linux_CreateWindowEx(Display* dpy, int x, int y, int w, int h, unsigned long bg_color, const char* res_name, const char* res_class, long event_mask) {
    int screen = DefaultScreen(dpy);
    extern float WindowOpacity;
    XVisualInfo vinfo;
    Visual* visual = NULL;
    int depth = 0;
    Colormap cmap = 0;
    bool use_argb = false;
    static Colormap cached_cmap = None;
    static Visual* cached_visual = NULL;
    static int cached_screen = -1;
    if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
        visual = vinfo.visual;
        depth = 32;
        if (cached_cmap == None || cached_screen != screen || cached_visual != visual) {
            if (cached_cmap != None) XFreeColormap(dpy, cached_cmap);
            cached_cmap = XCreateColormap(dpy, RootWindow(dpy, screen), visual, AllocNone);
            cached_visual = visual;
            cached_screen = screen;
        }
        cmap = cached_cmap;
        use_argb = true;
    }
    Window win;
    if (use_argb) {
        XSetWindowAttributes attrs = {0};
        unsigned int r = (bg_color >> 16) & 0xFF;
        unsigned int g = (bg_color >> 8) & 0xFF;
        unsigned int b = bg_color & 0xFF;
        unsigned int a = (unsigned int)WindowOpacity;
        attrs.background_pixel = (a << 24) | (r << 16) | (g << 8) | b;
        attrs.colormap = cmap;
        attrs.border_pixel = 0;
        win = XCreateWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0, depth, InputOutput, visual, CWBackPixel|CWColormap|CWBorderPixel, &attrs);
    } else {
        win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), x, y, w, h, 0, bg_color, bg_color);
    }
    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wm_type_dialog, 1);
    if (res_name || res_class) {
        XClassHint* ch = XAllocClassHint();
        ch->res_name = (char*)(res_name ? res_name : "colored");
        ch->res_class = (char*)(res_class ? res_class : "colored");
        XSetClassHint(dpy, win, ch);
        XFree(ch);
    }
    XWMHints* hints = XAllocWMHints();
    hints->flags = InputHint;
    hints->input = True;
    if (res_name && strcmp(res_name, "flashtext")==0) hints->input = False;
    XSetWMHints(dpy, win, hints);
    XFree(hints);
    XSelectInput(dpy, win, event_mask);
    return win;
}

Window Linux_CreatePromptWindow(Display* dpy, int x, int y, int w, int h) {
    unsigned long bg = rgb_to_hex(COLOR_BG);
    return Linux_CreateWindowEx(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask);
}
Window Linux_CreateSimplePromptWindow(Display* dpy, int x, int y, int w, int h) {
    unsigned long bg = rgb_to_hex(COLOR_BG);
    return Linux_CreateWindowEx(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask);
}
Window Linux_CreateEditPopupWindow(Display* dpy, int x, int y, int w, int h) {
    unsigned long bg = rgb_to_hex(COLOR_BG);
    return Linux_CreateWindowEx(dpy, x, y, w, h, bg, "colored", "colored", KeyPressMask | ExposureMask | FocusChangeMask);
}
Window Linux_CreateDisplayMessageWindow(Display* dpy, int x, int y, int w, int h) {
    unsigned long bg = rgb_to_hex(COLOR_BG);
    return Linux_CreateWindowEx(dpy, x, y, w, h, bg, "flashtext", "flashtext", ExposureMask | KeyPressMask);
}

XftDraw* Linux_CreateXftDraw(Display* dpy, Window win) {
    XWindowAttributes attrs;
    if (XGetWindowAttributes(dpy, win, &attrs)) {
        return XftDrawCreate(dpy, win, attrs.visual, attrs.colormap);
    }
    int screen = DefaultScreen(dpy);
    return XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
}

#endif
