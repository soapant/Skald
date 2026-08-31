#include "core/flapi.h"

static ColorRef GetTagColor(const char* tag_text) {
    if (!tag_text || strlen(tag_text) == 0) return COLOR_TEXT;
    
    for (int i = 0; i < TODO_CONFIG_FLAGS_COUNT; i++) {
        if (TODO_CONFIG_FLAGS_ENTRIES[i].tag_label) {
            if (strstr(tag_text, TODO_CONFIG_FLAGS_ENTRIES[i].tag_label) != NULL) {
                return TODO_CONFIG_FLAGS_ENTRIES[i].color;
            }
        }
    }
    for (int i = 0; i < TODO_ALIAS_DICT_COUNT; i++) {
        if (strstr(tag_text, TODO_ALIAS_DICT[i].alias) != NULL) {
            return COLOR_TEXT;
        }
    }
    return COLOR_TEXT;
}

static void ParseTodoInput(const char* input, TodoAliasEntry* alias_out, TodoConfigEntry* config_out, char* clean_text, char tags_out[][64], int* tag_count) {
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
            for (int j = 0; j < TODO_ALIAS_DICT_COUNT; j++) {
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
        for (int j = 0; j < TODO_CONFIG_FLAGS_COUNT; j++) {
            if (strcmp(config_matches[i].flag, TODO_CONFIG_FLAGS[j].flag) == 0) {
                if (TODO_CONFIG_FLAGS_ENTRIES[j].tag_label) {
                    snprintf(tags_out[*tag_count], 64, "%s%s%s", 
                        TODO_TAG_OPEN, TODO_CONFIG_FLAGS_ENTRIES[j].tag_label, TODO_TAG_CLOSE);
                    (*tag_count)++;
                }
                if (config_out && TODO_CONFIG_FLAGS_ENTRIES[j].is_alias) {
                    char* param = GetFlagParameter(input_copy, config_matches[i].index);
                    if (param && strlen(param) > 0) {
                        memcpy((void*)config_out->internal_name, TODO_CONFIG_FLAGS_ENTRIES[j].internal_name, sizeof(config_out->internal_name) - 1);
                        snprintf(tags_out[*tag_count], 64, "%s%s %s%s", 
                            TODO_TAG_OPEN, TODO_CONFIG_FLAGS_ENTRIES[j].internal_name, param, TODO_TAG_CLOSE);
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
        
        for (int i = 0; i < TODO_ALIAS_FLAGS_COUNT; i++) {
            if (strcmp(token, TODO_ALIAS_FLAGS[i].flag) == 0) {
                is_flag = true;
                token = strtok(NULL, " ");
                break;
            }
        }
        
        if (!is_flag) {
            for (int i = 0; i < TODO_CONFIG_FLAGS_COUNT; i++) {
                if (strcmp(token, TODO_CONFIG_FLAGS[i].flag) == 0) {
                    is_flag = true;
                    if (TODO_CONFIG_FLAGS_ENTRIES[i].is_alias) {
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

static void ParseTodoLine(const char* line, char* clean_text, char tags_out[][64], int* tag_count) {
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

static void AddTodoToFile(const char* text) {
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
    
    FILE* f = fopen(TODO_FILE_PATH, "w");
    if (f) {
        for (int i = 0; i < count; i++) {
            fprintf(f, "%s\n", lines[i]);
        }
        fprintf(f, "%s\n", new_entry);
        fclose(f);
    }
    fl_free_file_list(lines, count);
}

static void RemoveTodoItemFromFile(const char* item_to_remove) {
    FILE* infile = fopen(TODO_FILE_PATH, "r");
    if (!infile) return;

    FILE* outfile = fopen("todo.tmp", "w");
    if (!outfile) {
        fclose(infile);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), infile)) {
        char clean_line[512];
        strncpy(clean_line, line, sizeof(clean_line) - 1);
        clean_line[sizeof(clean_line) - 1] = '\0';
        clean_line[strcspn(clean_line, "\r\n")] = '\0';

        char compare_text[512];
        strncpy(compare_text, clean_line, sizeof(compare_text) - 1);
        char* delimiter = strstr(compare_text, TODO_ENTRY_DELIMITER);
        if (delimiter) {
            *delimiter = '\0';
        }
        
        char clean_item[512];
        strncpy(clean_item, item_to_remove, sizeof(clean_item) - 1);
        char* item_delimiter = strstr(clean_item, TODO_ENTRY_DELIMITER);
        if (item_delimiter) {
            *item_delimiter = '\0';
        }
        
        if (strcmp(compare_text, clean_item) == 0) {
            printf("[TODO-MGR] Dropped file line item: \"%s\"\n", item_to_remove);
            continue; 
        }
        fprintf(outfile, "%s\n", clean_line);
    }

    fclose(infile);
    fclose(outfile);

    #if defined(_WIN32)
    DeleteFileA(TODO_FILE_PATH);
    MoveFileA("todo.tmp", TODO_FILE_PATH);
#else
    remove(TODO_FILE_PATH);
    rename("todo.tmp", TODO_FILE_PATH);
#endif
}

static void todo_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    const char* items[100];
    int todo_count = fl_load_file_list(TODO_FILE_PATH, items, 100);

    PopulateListBox(lb, hListBox, filter, items, todo_count, NULL);
    fl_free_file_list(items, todo_count);
}

static void todo_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
        RemoveTodoItemFromFile(selection);
    } 
    else {
        AddTodoToFile(selection);
    }
    UI_Redraw(lb, hListBox);
}

static void todo_ctrlenter_callback(const char* selection, ListBoxInfo* lb) {
    if (UI_IsListBoxSelected(lb)) {
        RemoveTodoItemFromFile(selection);
    } else {
        AddTodoToFile(selection);
        UI_SetText(hEditControl, "");
    }
    UI_Redraw(lb, hListBox);
    selection_complete = false;
}

static void todo_copy_callback(const char* selection,int true_index,int type) {
    char buffer[512];
    strncpy(buffer, selection, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char* pipe = strchr(buffer, '|');
    if (pipe) {
        *pipe = '\0';
    }
    
    fl_copy_into_clipboard(buffer);
}

#if !defined(_WIN32)
static void todo_render_callback(Display *dpy, Window *win, PromptState *state) {
    if (!state->has_listbox) return;
    ListBoxInfo *lb = &state->listbox;
    int screen = DefaultScreen(dpy);
    
    XftDraw* xd = Linux_CreateXftDraw(dpy, *win);
    FontHandle font = Platform_CreateFontOnDisplay(dpy, FontSize, UsedFont);
    
    int y0 = 100;
    int itemH = FontSize + 36; 

    {
        Visual *visClear = DefaultVisual(dpy, screen);
        Colormap cmapClear = DefaultColormap(dpy, screen);
        unsigned long bg_hex = rgb_to_hex(COLOR_BG);
        XRenderColor rc0 = { 
            (bg_hex >> 16 & 0xFF) * 0x101, 
            (bg_hex >> 8 & 0xFF) * 0x101, 
            (bg_hex & 0xFF) * 0x101, 
            OpacityAlpha16() 
        };
        XftColor c0; 
        XftColorAllocValue(dpy, visClear, cmapClear, &rc0, &c0);
        XftDrawRect(xd, &c0, 0, y0, state->width, state->height - y0);
        XftColorFree(dpy, visClear, cmapClear, &c0);
    }

    if (!lb->populated_count) {
        XftDrawDestroy(xd);
        Platform_DestroyFont(font);
        return;
    }

    int visible = (state->height - y0) / itemH;
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

    Visual *vis = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);

    for (int i = 0; i < visible; i++) {
        int idx = lb->view_offset + i;
        int y = y0 + (i * itemH);
        bool sel = (idx == lb->selected_index);

        unsigned long bg = sel ? rgb_to_hex(COLOR_SELECTED_TEXT) : rgb_to_hex(COLOR_BG);
        XRenderColor rc = { (bg >> 16 & 0xFF) * 0x101, (bg >> 8 & 0xFF) * 0x101, (bg & 0xFF) * 0x101, OpacityAlpha16() };
        XftColor bgc; 
        XftColorAllocValue(dpy, vis, cmap, &rc, &bgc);
        XftDrawRect(xd, &bgc, 0, y, state->width, itemH);
        XftColorFree(dpy, vis, cmap, &bgc);

        char text_buf[256];
        snprintf(text_buf, sizeof(text_buf), "%s", lb->populated_cache[idx]);

        char main_text[512] = {0};
        char tags[10][64];
        int tag_count = 0;
        ParseTodoLine(text_buf, main_text, tags, &tag_count);
        char num_buf[128];
        snprintf(num_buf, sizeof(num_buf), "%d.", idx + 1);

        unsigned long fg_raw = rgb_to_hex(GetRainbowColor((float)(idx % GradientItems) / (float)GradientItems, 0.85f, GradientBrightness));
        
        XRenderColor rcf = { (fg_raw >> 16 & 0xFF) * 0x101, (fg_raw >> 8 & 0xFF) * 0x101, (fg_raw & 0xFF) * 0x101, 0xFFFF };
        XftColor fg_color;
        XftColorAllocValue(dpy, vis, cmap, &rcf, &fg_color);

        draw_wrapped_text(dpy, xd, &fg_color, font, num_buf, y + 4, "LEFT", 10);

        int XOffset = get_offset_based_on_number(idx);
        draw_wrapped_text(dpy, xd, &fg_color, font, main_text, y + 4, "LEFT",XOffset);
        XftColorFree(dpy, vis, cmap, &fg_color);

        if (tag_count > 0) {
            if (tag_count == 1) {
                int y_offset = 0;
                for (int t = 0; t < tag_count; t++) {
                    unsigned long tag_rgb = rgb_to_hex(GetTagColor(tags[t]));
                    XRenderColor rct = { (tag_rgb >> 16 & 0xFF) * 0x101, (tag_rgb >> 8 & 0xFF) * 0x101, (tag_rgb & 0xFF) * 0x101, 0xFFFF };
                    XftColor tag_col;
                    XftColorAllocValue(dpy, vis, cmap, &rct, &tag_col);

                    draw_wrapped_text(dpy, xd, &tag_col, font, tags[t], y + 28 + y_offset, "LEFT", 14);
                    XftColorFree(dpy, vis, cmap, &tag_col);
                    y_offset += 18;
                }
            } else {
                int x_offset = 0;
                for (int t = 0; t < tag_count; t++) {
                    unsigned long tag_rgb = rgb_to_hex(GetTagColor(tags[t]));
                    XRenderColor rct = { (tag_rgb >> 16 & 0xFF) * 0x101, (tag_rgb >> 8 & 0xFF) * 0x101, (tag_rgb & 0xFF) * 0x101, 0xFFFF };
                    XftColor tag_col;
                    XftColorAllocValue(dpy, vis, cmap, &rct, &tag_col);

                    draw_wrapped_text(dpy, xd, &tag_col, font, tags[t], y + 28, "LEFT", 14 + x_offset);
                    
                    x_offset += strlen(tags[t]) * 12; 
                    XftColorFree(dpy, vis, cmap, &tag_col);
                }
            }
        }
    }

    XftDrawDestroy(xd);
    Platform_DestroyFont(font);
}
#endif
#if defined(_WIN32)
static void todo_render_callback_impl(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex) {
    if (!text || !rect) return;

    UI_RedrawBackground(hdc,rect,is_selected);
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
    
    HFONT hOldFont = (HFONT)SelectObject(hdc, g_hMainFont);
    DrawTextA(hdc, main_text, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
    
    if (tag_count == 1) {
        RECT tagRect = *rect;
        tagRect.left += 14;
        tagRect.top += 28;
        tagRect.bottom -= 4;
        
        HFONT hOldTagFont = (HFONT)SelectObject(hdc, g_hMainFont);
        
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
        
        HFONT hOldTagFont = (HFONT)SelectObject(hdc, g_hMainFont);
        
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

static void todo_render_callback(HDC hdc, const char* text, RECT* rect, BOOL is_selected, int itemIndex) {
    todo_render_callback_impl(hdc, text, rect, is_selected, itemIndex);
}
#endif

static void todo_edit_confirm_callback(const char* new_text, void* user_data) {
    EditCallbackData* data = (EditCallbackData*)user_data;
    if (new_text && data) {
        data->new_text = (char*)new_text;
        EditCallback_Handle(data, todo_refresh_callback);
    }
    free(data);
}

static void todo_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = lb ? lb->selected_index : -1;
    int true_idx = (lb && idx >= 0 && idx < lb->populated_count) ? lb->matching_indices[idx] : idx;
    
    WindowManager* wm = WindowManager_Get();
    
    EditCallbackData* data = calloc(1, sizeof(EditCallbackData));
    
    data->filepath = TODO_FILE_PATH;
    data->lb = lb;
    data->selected_index = idx;
    data->true_index = true_idx;
    data->new_text = NULL;
    data->restore_selection = true;
    data->hListBox = hListBox;
    data->hEditControl = hEditControl;
    
    Popup* popup = EditPopup_Create(wm, selection, "Edit Todo", 960, 540,
                                    todo_edit_confirm_callback, data);
    WindowManager_PushPopup(wm, popup);
}

void TriggerTodo(void) {
    const char* ui_list[100];
    int count = fl_load_file_list(TODO_FILE_PATH, ui_list, 100);

    char* selected_item = PROMPT(
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
