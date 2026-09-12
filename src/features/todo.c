#include "core/flapi.h"
#if !defined(_WIN32)
#include "platform/x11/drawing.h"
#include "platform/x11/window_factory.h"
#endif

typedef struct {
    const char* alias;
    const char* alias_label;
} TodoAliasEntry;

typedef struct {
    const char* flag;
    const char* internal_name;
    const char* tag_label;
    ColorRef color;
    bool is_alias;
} TodoConfigEntry;


typedef struct {
    const char* flag;
    void (*handler)(const char*);
} GenericFlagEntry;

typedef struct {
    const char* open_bracket;
    const char* close_bracket; const char* tag_separator;
    const char* tag_prefix;
    const char* alias_separator;
} TagFormatConfig;

#define TODO_TAG_OPEN "["
#define TODO_TAG_CLOSE "]"
#define TODO_TAG_SEPARATOR "|"
#define TODO_ENTRY_DELIMITER "||"

const TagFormatConfig TODO_TAG_FORMAT = {
    "[",
    "]",
    "|",
    "",
    " "
};
#define TODO_TAG_FORMAT TODO_TAG_FORMAT

#define TODO_ALIAS_MASTER \
    X("-p", "Project:") \
    X("-n", "Note:")

#define TODO_CONFIG_MASTER \
    X("-pc", "Cprogram",         "Cprogram",         RGB(137, 180, 250), true) \
    X("-ph", "HeaderC",          "HeaderC",          RGB(148, 226, 183), true) \
    X("-pm", "MinecraftModpack", "MinecraftModpack", RGB(250, 179, 135), true) \
    X("-pb", "CocBot",           "CocBot",           RGB(249, 226, 175), true) \
    X("-pp", "MusicPlayer",      "MusicPlayer",      RGB(209, 106, 175), true) \
    X("-pv", "VideoViewer",      "VideoViewer",      RGB(219, 226, 105), true) \
    X("-pd", "Ded",              "Ded",              RGB(19,  226, 105), true) \
    X("-pi", "IndieGame",     "IndieGame",     RGB(19,  26, 105), true) \
    X("-m",  NULL,               "Misc",             RGB(225, 225, 225), false) \
    X("-l",  NULL,               "Learning",         RGB(166, 227, 161), false) \
    X("-b",  NULL,               "Bored",            RGB(203, 166, 247), false) \
    X("-d",  NULL,               "DontForget",       RGB(255, 109, 108), false) \
    X("-my", NULL,               "Maybe",            RGB(150, 5,   142), false) \
    X("-bg", NULL,               "Bug",              RGB(250, 9,   2),   false) \
    X("-v",  NULL,               "Nvim",             RGB(57,  235, 20),  false) \
    X("-f",  NULL,               "Fun",              RGB(210, 9,   102), false) \
    X("-r",  NULL,               "Research",         RGB(235, 60,  235), false) \
    X("-u",  NULL,               "Urgent",           RGB(255, 0,   35),  false) \
    X("-ne", NULL,               "NotImplemetedYet", RGB(155, 255, 35),  false) \
    X("-ft", NULL,               "Feature",          RGB(250, 205, 102), false) \
    X("-c",  NULL,               "Change",           RGB(150, 105, 12),  false)

#define X(flag, label) {flag, label},
const TodoAliasEntry TODO_ALIAS_DICT[] = {
    TODO_ALIAS_MASTER
};
const size_t TODO_ALIAS_DICT_COUNT = ARRAY_LEN(TODO_ALIAS_DICT);
#undef X
#define X(flag, label) {flag, NULL},
const GenericFlagEntry TODO_ALIAS_FLAGS[] = {
    TODO_ALIAS_MASTER
};
const size_t TODO_ALIAS_FLAGS_COUNT = ARRAY_LEN(TODO_ALIAS_FLAGS);
#undef X
#define X(flag, internal, label, color, is_alias) {flag, NULL},
const GenericFlagEntry TODO_CONFIG_FLAGS[] = {
    TODO_CONFIG_MASTER
};
const size_t TODO_CONFIG_FLAGS_COUNT = ARRAY_LEN(TODO_CONFIG_FLAGS);
#undef X
#define X(flag, internal, label, color, is_alias) {flag, internal, label, color, is_alias},
const TodoConfigEntry TODO_DICT[] = {
    TODO_CONFIG_MASTER
};
const size_t TODO_DICT_COUNT = ARRAY_LEN(TODO_DICT);
#undef X

ColorRef GetTagColor(const char* tag_text) {
    if (!tag_text || strlen(tag_text) == 0) return COLOR_TEXT;
    
    for (size_t i = 0; i < TODO_CONFIG_FLAGS_COUNT; i++) {
        if (TODO_DICT[i].tag_label) {
            if (strstr(tag_text, TODO_DICT[i].tag_label) != NULL) {
                return TODO_DICT[i].color;
            }
        }
    }
    for (size_t i = 0; i < TODO_ALIAS_DICT_COUNT; i++) {
        if (strstr(tag_text, TODO_ALIAS_DICT[i].alias) != NULL) {
            return COLOR_TEXT;
        }
    }
    return COLOR_TEXT;
}

void ParseTodoInput(const char* input, TodoAliasEntry* alias_out, TodoConfigEntry* config_out, char* clean_text, char tags_out[][64], int* tag_count) {
    if (!input) return;
    
    memset(alias_out, 0, sizeof(TodoAliasEntry));
    if (config_out) memset(config_out, 0, sizeof(TodoConfigEntry));
    clean_text[0] = '\0';
    *tag_count = 0;
    
    char input_copy[512];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    FoundFlag alias_matches[10];
    int alias_match_count = SearchFlagsInText(input_copy, TODO_ALIAS_FLAGS, TODO_ALIAS_FLAGS_COUNT, sizeof(GenericFlagEntry), alias_matches, 10);
    
    for (int i = 0; i < alias_match_count && *tag_count < 10; i++) {
        char* param = GetFlagParameter(input_copy, alias_matches[i].index);
        if (param && strlen(param) > 0) {
            int alias_idx = -1;
            for (size_t j = 0; j < TODO_ALIAS_DICT_COUNT; j++) {
                if (strcmp(alias_matches[i].flag, TODO_ALIAS_DICT[j].alias) == 0) {
                    alias_idx = j;
                    break;
                }
            }
            if (alias_idx >= 0) {
                snprintf(tags_out[*tag_count], 64, "%s%s %s%s", 
                    TODO_TAG_OPEN, TODO_ALIAS_DICT[alias_idx].alias_label, param, TODO_TAG_CLOSE);
                (*tag_count)++;
            }
        }
    }
    
    FoundFlag config_matches[10];
    int config_match_count = SearchFlagsInText(input_copy, TODO_CONFIG_FLAGS, TODO_CONFIG_FLAGS_COUNT, sizeof(GenericFlagEntry), config_matches, 10);
    
    for (int i = 0; i < config_match_count && *tag_count < 10; i++) {
        for (size_t j = 0; j < TODO_CONFIG_FLAGS_COUNT; j++) {
            if (strcmp(config_matches[i].flag, TODO_CONFIG_FLAGS[j].flag) == 0) {
                if (TODO_DICT[j].tag_label) {
                    snprintf(tags_out[*tag_count], 64, "%s%s%s", 
                        TODO_TAG_OPEN, TODO_DICT[j].tag_label, TODO_TAG_CLOSE);
                    (*tag_count)++;
                }
                if (config_out && TODO_DICT[j].is_alias) {
                    char* param = GetFlagParameter(input_copy, config_matches[i].index);
                    if (param && strlen(param) > 0) {
                        memcpy((void*)config_out->internal_name, TODO_DICT[j].internal_name, sizeof(config_out->internal_name) - 1);
                        snprintf(tags_out[*tag_count], 64, "%s%s %s%s", 
                            TODO_TAG_OPEN, TODO_DICT[j].internal_name, param, TODO_TAG_CLOSE);
                        (*tag_count)++;
                    }
                }
                break;
            }
        }
    }
    
    char clean[512] = {0};
    char* token = strtok(input_copy, " ");
    bool first = true;
    
    while (token != NULL) {
        bool is_flag = false;
        
        for (size_t i = 0; i < TODO_ALIAS_FLAGS_COUNT; i++) {
            if (strcmp(token, TODO_ALIAS_FLAGS[i].flag) == 0) {
                is_flag = true;
                token = strtok(NULL, " ");
                break;
            }
        }
        
        if (!is_flag) {
            for (size_t i = 0; i < TODO_CONFIG_FLAGS_COUNT; i++) {
                if (strcmp(token, TODO_CONFIG_FLAGS[i].flag) == 0) {
                    is_flag = true;
                    if (TODO_DICT[i].is_alias) {
                        token = strtok(NULL, " ");
                    }
                    break;
                }
            }
        }
        
        if (!is_flag && token[0] != '-') {
            if (first) {
                strcpy(clean, token);
                first = false;
            } else {
                strcat(clean, " ");
                strcat(clean, token);
            }
        }
        
        token = strtok(NULL, " ");
    }
    
    strncpy(clean_text, clean, 511);
    clean_text[511] = '\0';
}

void ParseTodoLine(const char* line, char* clean_text, char tags_out[][64], int* tag_count) {
    clean_text[0] = '\0';
    *tag_count = 0;
    
    char line_copy[512];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    
    char* delimiter = strstr(line_copy, TODO_ENTRY_DELIMITER);
    if (delimiter) {
        *delimiter = '\0';
        strncpy(clean_text, line_copy, 511);
        clean_text[511] = '\0';
        
        char* tag_part = delimiter + strlen(TODO_ENTRY_DELIMITER);
        char* tag_token = strtok(tag_part, TODO_TAG_SEPARATOR);
        while (tag_token && *tag_count < 10) {
            strncpy(tags_out[*tag_count], tag_token, 63);
            tags_out[*tag_count][63] = '\0';
            (*tag_count)++;
            tag_token = strtok(NULL, TODO_TAG_SEPARATOR);
        }
    } else {
        strncpy(clean_text, line_copy, 511);
        clean_text[511] = '\0';
    }
}

void AddTodoToFile(const char* text) {
    if (!text || strlen(text) == 0) return;
    
    const char* lines[100];
    int count = fl_load_file_list(TODO_FILE_PATH, lines, 100);
    
    char input_copy[512];
    strncpy(input_copy, text, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    TodoAliasEntry alias;
    TodoConfigEntry config;
    char clean_text[512];
    char tags[10][64];
    int tag_count = 0;
    
    ParseTodoInput(input_copy, &alias, &config, clean_text, tags, &tag_count);
    
    for (int i = 0; i < count; i++) {
        char existing_clean[512];
        char existing_tags[10][64];
        int existing_tag_count = 0;
        ParseTodoLine(lines[i], existing_clean, existing_tags, &existing_tag_count);
        if (strcmp(existing_clean, clean_text) == 0) {
            fl_free_file_list(lines, count);
            return;
        }
    }
    
    char new_entry[512];
    new_entry[0] = '\0';
    strncpy(new_entry, clean_text, sizeof(new_entry) - 1);
    
    if (tag_count > 0) {
        strcat(new_entry, TODO_ENTRY_DELIMITER);
        for (int i = 0; i < tag_count; i++) {
            strcat(new_entry, tags[i]);
            if (i < tag_count - 1) {
                strcat(new_entry, TODO_TAG_SEPARATOR);
            }
        }
    }
    
    const char* to_write[101];
    for (int i = 0; i < count; i++) to_write[i] = lines[i];
    to_write[count] = new_entry;
    fl_write_to_file(TODO_FILE_PATH, to_write, count + 1);
    fl_free_file_list(lines, count);
}

void RemoveTodoItemFromFile(const char* item_to_remove) {
    if (!item_to_remove) return;
    const char* lines[100];
    int count = fl_load_file_list(TODO_FILE_PATH, lines, 100);
    if (count == 0) return;
    const char* kept[100];
    int kept_count = 0;
    char clean_item[512];
    strncpy(clean_item, item_to_remove, sizeof(clean_item) - 1);
    clean_item[sizeof(clean_item) - 1] = '\0';
    char* item_delim = strstr(clean_item, TODO_ENTRY_DELIMITER);
    if (item_delim) *item_delim = '\0';
    clean_item[strcspn(clean_item, "\r\n")] = '\0';
    for (int i = 0; i < count; i++) {
        char clean_line[512];
        strncpy(clean_line, lines[i], sizeof(clean_line) - 1);
        clean_line[sizeof(clean_line) - 1] = '\0';
        clean_line[strcspn(clean_line, "\r\n")] = '\0';
        char compare_text[512];
        strncpy(compare_text, clean_line, sizeof(compare_text) - 1);
        compare_text[sizeof(compare_text)-1] = '\0';
        char* delim = strstr(compare_text, TODO_ENTRY_DELIMITER);
        if (delim) *delim = '\0';
        if (strcmp(compare_text, clean_item) == 0) {
            printf("[TODO-MGR] Dropped file line item: \"%s\"\n", item_to_remove);
            continue;
        }
        kept[kept_count++] = lines[i];
    }
    fl_write_to_file(TODO_FILE_PATH, kept, kept_count);
    fl_free_file_list(lines, count);
}

void todo_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[100];
    int todo_count = fl_load_file_list(TODO_FILE_PATH, items, 100);

    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, items, todo_count, NULL);
    fl_free_file_list(items, todo_count);
}

bool todo_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
        RemoveTodoItemFromFile(selection);
    } 
    else {
        AddTodoToFile(selection);
    }
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
    return true;
}

bool todo_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
        RemoveTodoItemFromFile(selection);
    } else {
        AddTodoToFile(selection);
        UI_SetText(FlintContext_Get()->window.hEditControl, "");
    }
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
    return false;
}

void todo_copy_callback(const char* selection,int true_index,int type) {
    char buffer[512];
    strncpy(buffer, selection, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char* pipe = strchr(buffer, '|');
    if (pipe) {
        *pipe = '\0';
    }
    
    fl_copy_to_clipboard(buffer);
}

#if !defined(_WIN32)
void todo_render_callback(Display *dpy, Window *win, PromptState *state) {
    ListBoxInfo *lb = &state->listbox;
    int screen = DefaultScreen(dpy);
    
    XftDraw* xd = Linux_CreateXftDraw(dpy, *win);
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    
    int y0 = 70;
    int itemH = FontSize + 36; 

    {
        if (!EnableShaders || ShaderPreset == 0) {
            Visual *visClear; Colormap cmapClear;
            if (!Linux_GetDrawableVisual(dpy, *win, &visClear, &cmapClear, NULL)) { visClear = DefaultVisual(dpy, screen); cmapClear = DefaultColormap(dpy, screen); }
            unsigned long bg_hex = rgb_to_hex(COLOR_BG);
            XRenderColor rc0 = { 
                (bg_hex >> 16 & 0xFF) * 0x101, 
                (bg_hex >> 8 & 0xFF) * 0x101, 
                (bg_hex & 0xFF) * 0x101, 
                OpacityAlpha16() 
            };
            XftColor c0; 
            XftColorAllocValue(dpy, visClear, cmapClear, &rc0, &c0);
            XftDrawRect(xd, &c0, 0, y0, state->width, state->height - y0 - 40);
            XftColorFree(dpy, visClear, cmapClear, &c0);
        }
    }

    if (!lb->populated_count) {
        XftDrawDestroy(xd);
        Platform_DestroyFont(font);
        return;
    }

    int visible = (state->height - y0 - 40) / itemH;
    if (visible < 1) visible = 1;
    if (visible > lb->populated_count) visible = lb->populated_count;

    if (lb->selected_index >= 0) {
        if (lb->selected_index < lb->view_offset + PaddingForScrolling)
            lb->view_offset = lb->selected_index - PaddingForScrolling;
        else if (lb->selected_index >= lb->view_offset + visible - PaddingForScrolling)
            lb->view_offset = lb->selected_index - visible + PaddingForScrolling + 1;
    }
    if (lb->view_offset < 0) lb->view_offset = 0;
    if (lb->view_offset + visible > lb->populated_count)
        lb->view_offset = lb->populated_count - visible;
    if (lb->view_offset < 0) lb->view_offset = 0;

    Visual *vis; Colormap cmap;
    if (!Linux_GetDrawableVisual(dpy, *win, &vis, &cmap, NULL)) { vis = DefaultVisual(dpy, screen); cmap = DefaultColormap(dpy, screen); }

    for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + (i * itemH);
        bool sel = (idx == lb->selected_index);

        if (sel) {
            Linux_DrawSelectionBackground(dpy, xd, vis, cmap, 0, y, state->width, itemH, rgb_to_hex(COLOR_SELECTED_TEXT));
        } else {
            if (!EnableShaders || ShaderPreset == 0) {
                unsigned long bg = rgb_to_hex(COLOR_BG);
                XRenderColor rc = { (bg >> 16 & 0xFF) * 0x101, (bg >> 8 & 0xFF) * 0x101, (bg & 0xFF) * 0x101, OpacityAlpha16() };
                XftColor bgc; 
                XftColorAllocValue(dpy, vis, cmap, &rc, &bgc);
                XftDrawRect(xd, &bgc, 0, y, state->width, itemH);
                XftColorFree(dpy, vis, cmap, &bgc);
            }
        }

        char text_buf[256];
        snprintf(text_buf, sizeof(text_buf), "%s", lb->populated_cache[idx]);

        char main_text[512] = {0};
        char tags[10][64];
        int tag_count = 0;
        ParseTodoLine(text_buf, main_text, tags, &tag_count);

        unsigned long fg_raw = rgb_to_hex(GetRainbowColor((float)(idx % GradientItems) / (float)GradientItems, 0.85f, GradientBrightness));
        int XOffset = 10;
        fl_place_text_raw(.x=XOffset, .y=y + 4, .text=main_text, .color=fg_raw, .font=font, .align="LEFT", .pad=XOffset);

        if (tag_count > 0) {
            if (tag_count == 1) {
                int y_offset = 0;
                for (int t = 0; t < tag_count; t++) {
                    unsigned long tag_rgb = rgb_to_hex(GetTagColor(tags[t]));
                    fl_place_text_raw(.x=14, .y=y + 28 + y_offset, .text=tags[t], .color=tag_rgb, .font=font, .align="LEFT", .pad=14);
                    y_offset += 18;
                }
            } else {
                int x_offset = 0;
                for (int t = 0; t < tag_count; t++) {
                    unsigned long tag_rgb = rgb_to_hex(GetTagColor(tags[t]));
                    fl_place_text_raw(.x=14 + x_offset, .y=y + 28, .text=tags[t], .color=tag_rgb, .font=font, .align="LEFT", .pad=14 + x_offset);
                    x_offset += strlen(tags[t]) * 12;
                }
            }
        }
    }

    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
}
#endif
#if defined(_WIN32)
void todo_render_callback_impl(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex) {
    char main_text[512] = {0};
    char tags[10][64];
    int tag_count = 0;
    
    ParseTodoLine(text, main_text, tags, &tag_count);
    
    RECT textRect = *rect;
    textRect.left += 10;
    textRect.top += 4;
    textRect.bottom -= 30;
    if (!is_selected) {
        SetTextColor(hdc, GetRainbowColor((float)(itemIndex % GradientItems) / (float)GradientItems, 0.85f, GradientBrightness));
    }
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
    DrawTextA(hdc, main_text, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
    
    if (tag_count == 1) {
        RECT tagRect = *rect;
        tagRect.left += 14;
        tagRect.top += 28;
        tagRect.bottom -= 4;
        
        HFONT hOldTagFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
        
        int y_offset = 0;
        for (int i = 0; i < tag_count; i++) {
            SetTextColor(hdc, GetTagColor(tags[i]));
            RECT singleTagRect = tagRect;
            singleTagRect.top += y_offset;
            DrawTextA(hdc, tags[i], -1, &singleTagRect, DT_LEFT | DT_TOP);
            y_offset += 18;
        }
        
        SelectObject(hdc, hOldTagFont);
    } else {
        RECT tagRect = *rect;
        tagRect.left += 14;
        tagRect.top += 28;
        
        HFONT hOldTagFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
        
        int x_offset = 0;
        for (int i = 0; i < tag_count; i++) {
            SetTextColor(hdc, GetTagColor(tags[i]));
            RECT singleTagRect = tagRect;
            singleTagRect.left += x_offset;
            DrawTextA(hdc, tags[i], -1, &singleTagRect, DT_LEFT | DT_TOP);
            x_offset += strlen(tags[i]) * 12;
        }
        
        SelectObject(hdc, hOldTagFont);
    }
    
    SelectObject(hdc, hOldFont);
}

void todo_render_callback(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex) {
    todo_render_callback_impl(hdc, text, rect, is_selected, itemIndex);
}
#endif

void todo_edit_callback(const char* selection, ListBoxInfo* lb) {
    fl_edit_text_popup(selection, "Edit Todo", 960, 540, TODO_FILE_PATH, lb, todo_refresh_callback);
}

void TriggerTodo(void) {
    const char* ui_list[100];
    int count = fl_load_file_list(TODO_FILE_PATH, ui_list, 100);

    PROMPT(
        .title = "📝 Todo", .items = ui_list, .count = count,
        .height = 920, .width = 650, .item_height = 56,
        .refresh = todo_refresh_callback,
        .enter = todo_enter_callback,
        .ctrl_enter = todo_ctrlenter_callback,
        .copy = todo_copy_callback,
        .render = todo_render_callback,
        .edit = todo_edit_callback);

    fl_free_file_list(ui_list, count);
}

FLINT_FEATURE_INIT {
  fl_register_hotkey("Alt+T",TriggerTodo);

    fl_register_feature("Todo", TriggerTodo);
}
