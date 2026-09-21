#include "core/flapi.h"

#define TODO_FILE_PATH "todo.txt"

typedef struct { char dummy[1]; } TodoAliasEntry;
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

#define TODO_CONFIG_MASTER \
    X("-pf", "Flint",         "Flint",         RGB(137, 180, 250), true) \
    X("-pm", "MinecraftModpack", "MinecraftModpack", RGB(250, 179, 135), true) \
    X("-pb", "CocBot",           "CocBot",           RGB(249, 226, 175), true) \
    X("-pi", "IndieGame",        "IndieGame",     RGB(19,  26, 105), true) \
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
    X("-ne", NULL,               "NotImplementedYet", RGB(155, 255, 35),  false) \
    X("-ft", NULL,               "Feature",          RGB(250, 205, 102), false) \
    X("-c",  NULL,               "Change",           RGB(150, 105, 12),  false) \
    X("-fl",  NULL,              "FlApi",            RGB(10, 105, 12),  false)

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
    return COLOR_TEXT;
}

void ParseTodoInput(const char* input, TodoAliasEntry* alias_out, TodoConfigEntry* config_out, char* clean_text, char tags_out[][64], int* tag_count) {
    if (!input) return;
    
    memset(alias_out, 0, sizeof(TodoAliasEntry));
    if (config_out) memset(config_out, 0, sizeof(TodoConfigEntry));
    clean_text[0] = '\0';
    *tag_count = 0;
    
    Fl_FlagList flist = fl_parse_flags(input);
    for (size_t i = 0; i < flist.count && *tag_count < 10; i++) {
        Fl_Flag *f = &flist.flags[i];
        for (size_t j = 0; j < TODO_CONFIG_FLAGS_COUNT; j++) {
            const char *flag = TODO_CONFIG_FLAGS[j].flag;
            const char *bare = flag[0]=='-' ? flag+1 : flag;
            if (bare[0]=='-' && bare[1]) bare++;
            if (strcmp(f->name, bare) == 0) {
                if (TODO_DICT[j].tag_label) {
                    snprintf(tags_out[*tag_count], 64, "%s%s%s",
                        TODO_TAG_OPEN, TODO_DICT[j].tag_label, TODO_TAG_CLOSE);
                    (*tag_count)++;
                }
                break;
            }
        }
    }
    char input_copy[512];
    strncpy(input_copy, input, sizeof(input_copy)-1);
    input_copy[sizeof(input_copy)-1]='\0';
    char clean[512] = {0};
    char* token = strtok(input_copy, " ");
    bool first = true;

    while (token != NULL) {
        bool is_flag = false;
        bool flag_has_val = false;
        for (size_t i = 0; i < flist.count; i++) {
            Fl_Flag *f = &flist.flags[i];
            char withdash[68]; snprintf(withdash, sizeof(withdash), "-%s", f->name);
            char withdash2[68]; snprintf(withdash2, sizeof(withdash2), "--%s", f->name);
            if (strcmp(token, withdash) == 0 || strcmp(token, withdash2) == 0) { is_flag = true; flag_has_val = f->has_value; break; }
        }
        if (is_flag) {
            if (flag_has_val) token = strtok(NULL, " ");
        } else if (token[0] != '-') {
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
    fl_free_flags(&flist);
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

    FlStringList lines = fl_load_file_list(TODO_FILE_PATH);
    int count = lines.count;
    
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
        ParseTodoLine(lines.items[i], existing_clean, existing_tags, &existing_tag_count);
        if (strcmp(existing_clean, clean_text) == 0) {
            fl_free_file_list(&lines);
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

    char** grown = (char**)realloc(lines.items, (size_t)(count + 1) * sizeof(char*));
    if (!grown) { fl_free_file_list(&lines); return; }
    lines.items = grown;
    lines.items[count] = new_entry;
    fl_write_to_file(TODO_FILE_PATH, (const char**)lines.items, count + 1);
    lines.items[count] = NULL;
    fl_free_file_list(&lines);
}

void RemoveTodoItemFromFile(const char* item_to_remove) {
    if (!item_to_remove) return;
    FlStringList lines = fl_load_file_list(TODO_FILE_PATH);
    int count = lines.count;
    if (count == 0) { fl_free_file_list(&lines); return; }
    int kept_count = 0;
    char clean_item[512];
    strncpy(clean_item, item_to_remove, sizeof(clean_item) - 1);
    clean_item[sizeof(clean_item) - 1] = '\0';
    char* item_delim = strstr(clean_item, TODO_ENTRY_DELIMITER);
    if (item_delim) *item_delim = '\0';
    clean_item[strcspn(clean_item, "\r\n")] = '\0';
    for (int i = 0; i < count; i++) {
        char clean_line[512];
        strncpy(clean_line, lines.items[i], sizeof(clean_line) - 1);
        clean_line[sizeof(clean_line) - 1] = '\0';
        clean_line[strcspn(clean_line, "\r\n")] = '\0';
        char compare_text[512];
        strncpy(compare_text, clean_line, sizeof(compare_text) - 1);
        compare_text[sizeof(compare_text)-1] = '\0';
        char* delim = strstr(compare_text, TODO_ENTRY_DELIMITER);
        if (delim) *delim = '\0';
        if (strcmp(compare_text, clean_item) == 0) {
            free(lines.items[i]);
            continue;
        }
        lines.items[kept_count++] = lines.items[i];
    }
    fl_write_to_file(TODO_FILE_PATH, (const char**)lines.items, kept_count);
    for (int i = kept_count; i < count; i++) lines.items[i] = NULL;
    fl_free_file_list(&lines);
}

void todo_refresh_callback(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    FlStringList files = fl_load_file_list(TODO_FILE_PATH);

    PopulateListBox(lb, FlintContext_Get()->window.hListBox, filter, (const char**)files.items, files.count, NULL);
    fl_free_file_list(&files);
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

void todo_render_callback(PromptState *state) {
    ListBoxInfo *lb = &state->listbox;
    FlListSpec spec = {
        .lb = lb,
        .item_height = FontSize + 36
    };
    fl_place_editbox();
    if (!fl_listbox_begin(&spec)) return;
    FlListItemCtx ctx;
    while (fl_listbox_next(&ctx)) {
        char text_buf[256];
        snprintf(text_buf, sizeof(text_buf), "%s", ctx.text);
        char main_text[512] = {0};
        char tags[10][64];
        int tag_count = 0;
        ParseTodoLine(text_buf, main_text, tags, &tag_count);
        fl_draw_text(.x=ctx.x + 10, .y=ctx.y + 4,.color=rgb_to_hex(COLOR_TEXT),.text=main_text, .align="LEFT", .pad=10);
        if (tag_count > 0) {
            if (tag_count == 1) {
                int y_offset = 0;
                for (int t = 0; t < tag_count; t++) {
                    unsigned long tag_rgb = rgb_to_hex(GetTagColor(tags[t]));
                    fl_draw_text(.x=14, .y=ctx.y + 28 + y_offset, .text=tags[t], .color=tag_rgb, .align="LEFT", .pad=14);
                    y_offset += 18;
                }
            } else {
                int x_offset = 0;
                for (int t = 0; t < tag_count; t++) {
                    unsigned long tag_rgb = rgb_to_hex(GetTagColor(tags[t]));
                    fl_draw_text(.x=14 + x_offset, .y=ctx.y + 28, .text=tags[t], .color=tag_rgb, .align="LEFT", .pad=14 + x_offset);
                    x_offset += strlen(tags[t]) * 12;
                }
            }
        }
    }
    fl_listbox_end();
}

void todo_edit_callback(const char* selection, ListBoxInfo* lb) {
    int idx = -1;
    char* edited = fl_edit_text_popup(.initial_text = selection,.lb = lb, .out_index = &idx);
    if (!edited) return;
    if (idx >= 0) fl_replace_line_in_file(TODO_FILE_PATH, idx, edited);
    free(edited);
    todo_refresh_callback("", FlintContext_Get()->window.hListBox, lb);
    UI_Redraw(lb, FlintContext_Get()->window.hListBox);
}

void TriggerTodo(void) {
    PROMPT(
        .title = "📝 Todo",
        .refresh = todo_refresh_callback,
        .enter = todo_enter_callback,
        .ctrl_enter = todo_ctrlenter_callback,
        .copy = todo_copy_callback,
        .render = todo_render_callback,
        .edit = todo_edit_callback);

}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+T",TriggerTodo,"Fortnite");

    fl_register_feature("Todo", TriggerTodo);
}
