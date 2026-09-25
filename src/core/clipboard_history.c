#include "core/clipboard_history.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FL_CLIPBOARD_HISTORY_MAX 64

const unsigned char FL_PNG_MAGIC[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

static FlClipboardHistory g_fl_clipboard_history = {0};

static void fl_clipboard_entry_free(FlClipboardEntry* e) {
    if (!e) return;
    free(e->text);
    e->text = NULL;
    free(e->image_path);
    e->image_path = NULL;
}

static bool fl_clipboard_history_push(FlClipboardEntry e) {
    FlClipboardHistory* h = &g_fl_clipboard_history;
    if (h->count >= FL_CLIPBOARD_HISTORY_MAX) {
        fl_clipboard_entry_free(&h->entries[0]);
        memmove(h->entries, h->entries + 1, (h->count - 1) * sizeof(h->entries[0]));
        h->count--;
    }
    if (h->count >= h->capacity) {
        size_t ncap = h->capacity ? h->capacity * 2 : 16;
        if (ncap > FL_CLIPBOARD_HISTORY_MAX) ncap = FL_CLIPBOARD_HISTORY_MAX;
        FlClipboardEntry* n = realloc(h->entries, ncap * sizeof(*n));
        if (!n) {
            fl_clipboard_entry_free(&e);
            return false;
        }
        h->entries = n;
        h->capacity = ncap;
    }
    h->entries[h->count++] = e;
    return true;
}

void fl_clipboard_png_dims(const unsigned char* png, size_t len, uint32_t* out_w, uint32_t* out_h) {
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!png || len < 24) return;
    if (memcmp(png, FL_PNG_MAGIC, 8) != 0) return;
    if (memcmp(png + 12, "IHDR", 4) != 0) return;
    uint32_t w = ((uint32_t)png[16] << 24) | ((uint32_t)png[17] << 16) | ((uint32_t)png[18] << 8) | png[19];
    uint32_t h = ((uint32_t)png[20] << 24) | ((uint32_t)png[21] << 16) | ((uint32_t)png[22] << 8) | png[23];
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

bool fl_clipboard_history_push_text_platform(const char* text) {
    FlClipboardEntry e = {0};
    e.type = FL_CLIPBOARD_TEXT;
    e.timestamp = (uint64_t)time(NULL);
    e.text = text ? strdup(text) : NULL;
    if (text && !e.text) return false;
    if (!fl_clipboard_history_push(e)) return false;
    return true;
}

bool fl_clipboard_history_push_image_platform(const char* path, unsigned int w, unsigned int h) {
    FlClipboardEntry e = {0};
    e.type = FL_CLIPBOARD_IMAGE;
    e.timestamp = (uint64_t)time(NULL);
    e.image_path = path ? strdup(path) : NULL;
    if (path && !e.image_path) return false;
    e.image_width = w;
    e.image_height = h;
    if (!fl_clipboard_history_push(e)) return false;
    return true;
}

size_t fl_clipboard_history_count_platform(void) {
    return g_fl_clipboard_history.count;
}

const FlClipboardEntry* fl_clipboard_history_get_platform(size_t index) {
    if (index >= g_fl_clipboard_history.count) return NULL;
    return &g_fl_clipboard_history.entries[index];
}

void fl_clipboard_clear_history_platform(void) {
    for (size_t i = 0; i < g_fl_clipboard_history.count; i++)
        fl_clipboard_entry_free(&g_fl_clipboard_history.entries[i]);
    free(g_fl_clipboard_history.entries);
    g_fl_clipboard_history.entries = NULL;
    g_fl_clipboard_history.count = 0;
    g_fl_clipboard_history.capacity = 0;
}

bool fl_clipboard_set_png_path_platform(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (32 * 1024 * 1024)) {
        fclose(f);
        return 0;
    }
    unsigned char* buf = malloc((size_t)n);
    if (!buf) {
        fclose(f);
        return 0;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    bool ok = 0;
    if (got == (size_t)n && memcmp(buf, FL_PNG_MAGIC, 8) == 0) {
        uint32_t w = 0, h = 0;
        fl_clipboard_png_dims(buf, (size_t)n, &w, &h);
        ok = Platform_CopyImageToClipboard(buf, (size_t)n);
        if (ok) fl_clipboard_history_push_image_platform(path, w, h);
    }
    free(buf);
    remove(path);
    return ok;
}
