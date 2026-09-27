#include "core/flapi.h"
#include "core/context.h"
#include "helpers/helpers_ui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CH_FILE "clipboard_history.txt"
#define CH_MAX_STORED 1000
#define CH_MAX_TEXT 4096
#define CH_PREVIEW_LEN 128

static FlThread ch_thread = NULL;
static FlMutex ch_lock = NULL;
static bool ch_running = false;
static bool ch_inited = false;
static char** ch_items = NULL;
static int ch_count = 0;

static void ch_free_items(void) {
    if (!ch_items) { ch_count = 0; return; }
    for (int i = 0; i < ch_count; i++) free(ch_items[i]);
    free(ch_items);
    ch_items = NULL;
    ch_count = 0;
}

static size_t ch_escape(const char* src, char* dst, size_t cap) {
    size_t o = 0;
    for (const char* p = src ? src : ""; *p && o + 2 < cap; p++) {
        if (*p == '\\') { dst[o++] = '\\'; dst[o++] = '\\'; }
        else if (*p == '\n') { dst[o++] = '\\'; dst[o++] = 'n'; }
        else if (*p == '\r') { dst[o++] = '\\'; dst[o++] = 'r'; }
        else dst[o++] = *p;
    }
    dst[o] = '\0';
    return o;
}

static size_t ch_unescape(const char* src, char* dst, size_t cap) {
    size_t o = 0;
    for (const char* p = src ? src : ""; *p && o + 1 < cap; p++) {
        if (*p == '\\' && *(p + 1) == 'n') { dst[o++] = '\n'; p++; }
        else if (*p == '\\' && *(p + 1) == 'r') { dst[o++] = '\r'; p++; }
        else if (*p == '\\' && *(p + 1) == '\\') { dst[o++] = '\\'; p++; }
        else dst[o++] = *p;
    }
    dst[o] = '\0';
    return o;
}

static void ch_preview(const char* src, char* dst, size_t cap) {
    size_t o = 0;
    for (const char* p = src ? src : ""; *p && o + 1 < cap && o < CH_PREVIEW_LEN; p++) {
        if (*p == '\n' || *p == '\r' || *p == '\t') dst[o++] = ' ';
        else dst[o++] = *p;
    }
    dst[o] = '\0';
}

static void ch_remember_locked(const char* text) {
    if (!text || !text[0]) return;
    for (int i = 0; i < ch_count; i++) {
        if (strcmp(ch_items[i], text) == 0) {
            free(ch_items[i]);
            for (int k = i; k + 1 < ch_count; k++) ch_items[k] = ch_items[k + 1];
            ch_count--;
            break;
        }
    }
    if (ch_count >= CH_MAX_STORED) {
        free(ch_items[0]);
        for (int k = 0; k + 1 < ch_count; k++) ch_items[k] = ch_items[k + 1];
        ch_count--;
    }
    char* copy = malloc(CH_MAX_TEXT);
    if (!copy) return;
    strncpy(copy, text, CH_MAX_TEXT - 1);
    copy[CH_MAX_TEXT - 1] = '\0';
    char** tmp = realloc(ch_items, (size_t)(ch_count + 1) * sizeof(char*));
    if (!tmp) { free(copy); return; }
    ch_items = tmp;
    ch_items[ch_count++] = copy;
}

static void ch_save(void) {
    char** lines = NULL;
    int count = 0;
    fl_mutex_lock(ch_lock);
    if (ch_count > 0) {
        lines = malloc((size_t)ch_count * sizeof(char*));
        if (lines) {
            for (int i = 0; i < ch_count; i++) {
                lines[i] = malloc(CH_MAX_TEXT * 2);
                if (!lines[i]) break;
                ch_escape(ch_items[i], lines[i], CH_MAX_TEXT * 2);
                count++;
            }
        }
    }
    fl_mutex_unlock(ch_lock);
    if (lines && count > 0) fl_write_to_file(CH_FILE, (const char**)lines, count);
    if (lines) {
        for (int i = 0; i < count; i++) free(lines[i]);
        free(lines);
    }
}

static void ch_load(void) {
    FlStringList lines = fl_load_file_list(CH_FILE);
    fl_mutex_lock(ch_lock);
    for (int i = 0; i < lines.count; i++) {
        if (!lines.items[i] || !lines.items[i][0]) continue;
        char buf[CH_MAX_TEXT];
        ch_unescape(lines.items[i], buf, sizeof(buf));
        ch_remember_locked(buf);
    }
    fl_mutex_unlock(ch_lock);
    fl_free_file_list(&lines);
}

static void* ch_sampler(void* arg) {
    (void)arg;
    while (ch_running) {
        char* cur = fl_get_last_clipboard_entry();
        int changed = 0;
        fl_mutex_lock(ch_lock);
        if (cur && cur[0] && (ch_count == 0 || strcmp(ch_items[ch_count - 1], cur) != 0)) {
            ch_remember_locked(cur);
            changed = 1;
        }
        fl_mutex_unlock(ch_lock);
        if (cur) free(cur);
        if (changed) ch_save();
        fl_sleep(1);
    }
    return NULL;
}

void clipboard_history_refresh(const char* filter, WindowHandle hListBox, ListBoxInfo* lb) {
    fl_mutex_lock(ch_lock);
    int count = ch_count;
    const char** items = NULL;
    if (count > 0) {
        items = (const char**)malloc((size_t)count * sizeof(char*));
        for (int i = 0; i < count && items; i++) {
            char* buffer = malloc(CH_PREVIEW_LEN + 1);
            if (!buffer) break;
            ch_preview(ch_items[count - 1 - i], buffer, CH_PREVIEW_LEN + 1);
            items[i] = buffer;
        }
    }
    fl_mutex_unlock(ch_lock);
    PopulateListBox(lb, filter, items, count, NULL);
    fl_free_list(items, count);
}

bool clipboard_history_enter(const char* selection, ListBoxInfo* lb) {
    (void)selection;
    if (!UI_IsListBoxSelected(lb)) return true;
    int sel = lb->selected_index;
    if (sel < 0 || sel >= lb->total_matches) return true;
    int idx = lb->matching_indices[sel];
    char* copy = NULL;
    fl_mutex_lock(ch_lock);
    if (idx >= 0 && idx < ch_count && ch_items[ch_count - 1 - idx]) {
        copy = strdup(ch_items[ch_count - 1 - idx]);
        if (copy) ch_remember_locked(copy);
    }
    fl_mutex_unlock(ch_lock);
    if (copy) {
        fl_copy_to_clipboard(copy);
        free(copy);
        ch_save();
    }
    return true;
}

void TriggerClipboardHistory(void) {
    PROMPT(.title = "Clipboard History",
           .refresh = clipboard_history_refresh,
           .enter = clipboard_history_enter);
}

FLINT_FEATURE_EXPORT void FlintFeatureShutdown(void) {
    ch_running = false;
    if (ch_inited) fl_thread_join(ch_thread);
    ch_save();
    fl_mutex_lock(ch_lock);
    ch_free_items();
    fl_mutex_unlock(ch_lock);
    fl_mutex_destroy(ch_lock);
    ch_lock = NULL;
    ch_inited = false;
}

FLINT_FEATURE_INIT {
    if (ch_inited) return;
    ch_inited = true;
    ch_lock = fl_mutex_create();
    ch_load();
    ch_running = true;
    ch_thread = fl_thread_create(ch_sampler, NULL);
    fl_register_hotkey("Alt+Shift+R", TriggerClipboardHistory);
    fl_register_feature("Clipboard History", TriggerClipboardHistory);
}
