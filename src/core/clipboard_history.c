#include "core/clipboard_history.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLIPBOARD_HISTORY_MAX 64

const unsigned char PNG_MAGIC[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

static ClipboardHistory g_clipboard_history = {0};

static void clipboard_entry_free(ClipboardEntry* e) {
    if (!e) return;
    free(e->text);
    e->text = NULL;
    free(e->image_path);
    e->image_path = NULL;
}

static bool clipboard_history_push(ClipboardEntry e) {
    ClipboardHistory* h = &g_clipboard_history;
    if (h->count >= CLIPBOARD_HISTORY_MAX) {
        clipboard_entry_free(&h->entries[0]);
        memmove(h->entries, h->entries + 1, (h->count - 1) * sizeof(h->entries[0]));
        h->count--;
    }
    if (h->count >= h->capacity) {
        size_t ncap = h->capacity ? h->capacity * 2 : 16;
        if (ncap > CLIPBOARD_HISTORY_MAX) ncap = CLIPBOARD_HISTORY_MAX;
        ClipboardEntry* n = realloc(h->entries, ncap * sizeof(*n));
        if (!n) {
            clipboard_entry_free(&e);
            return false;
        }
        h->entries = n;
        h->capacity = ncap;
    }
    h->entries[h->count++] = e;
    return true;
}

void clipboard_png_dims(const unsigned char* png, size_t len, uint32_t* out_w, uint32_t* out_h) {
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!png || len < 24) return;
    if (memcmp(png, PNG_MAGIC, 8) != 0) return;
    if (memcmp(png + 12, "IHDR", 4) != 0) return;
    uint32_t w = ((uint32_t)png[16] << 24) | ((uint32_t)png[17] << 16) | ((uint32_t)png[18] << 8) | png[19];
    uint32_t h = ((uint32_t)png[20] << 24) | ((uint32_t)png[21] << 16) | ((uint32_t)png[22] << 8) | png[23];
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

ClipboardImageFormat clipboard_image_format(const unsigned char* buf, size_t len) {
    if (!buf) return CLIPBOARD_FMT_UNKNOWN;
    if (len >= 8 && memcmp(buf, PNG_MAGIC, 8) == 0) return CLIPBOARD_FMT_PNG;
    if (len >= 3 && buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) return CLIPBOARD_FMT_JPEG;
    if (len >= 6 && memcmp(buf, "GIF8", 4) == 0) return CLIPBOARD_FMT_GIF;
    if (len >= 2 && buf[0] == 0x42 && buf[1] == 0x4D) return CLIPBOARD_FMT_BMP;
    return CLIPBOARD_FMT_UNKNOWN;
}

const char* clipboard_image_mime(ClipboardImageFormat fmt) {
    switch (fmt) {
        case CLIPBOARD_FMT_PNG:  return "image/png";
        case CLIPBOARD_FMT_JPEG: return "image/jpeg";
        case CLIPBOARD_FMT_GIF:  return "image/gif";
        case CLIPBOARD_FMT_BMP:  return "image/bmp";
        default: return NULL;
    }
}

static void clipboard_jpeg_dims(const unsigned char* buf, size_t len, uint32_t* out_w, uint32_t* out_h) {
    size_t i = 2;
    while (i + 3 < len) {
        if (buf[i] != 0xFF) { i++; continue; }
        unsigned char marker = buf[i + 1];
        if (marker == 0xFF) { i++; continue; }
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) { i += 2; continue; }
        if (marker == 0xD9 || marker == 0xDA) return;
        if (i + 3 >= len) return;
        size_t seg = ((size_t)buf[i + 2] << 8) | buf[i + 3];
        if (seg < 2) return;
        int is_sof = (marker >= 0xC0 && marker <= 0xC3) ||
                     (marker >= 0xC5 && marker <= 0xC7) ||
                     (marker >= 0xC9 && marker <= 0xCB) ||
                     (marker >= 0xCD && marker <= 0xCF);
        if (is_sof) {
            if (i + 9 > len) return;
            uint32_t h = ((uint32_t)buf[i + 5] << 8) | buf[i + 6];
            uint32_t w = ((uint32_t)buf[i + 7] << 8) | buf[i + 8];
            if (out_w) *out_w = w;
            if (out_h) *out_h = h;
            return;
        }
        i += 2 + seg;
    }
}

static void clipboard_gif_dims(const unsigned char* buf, size_t len, uint32_t* out_w, uint32_t* out_h) {
    if (len < 10) return;
    uint32_t w = (uint32_t)buf[6] | ((uint32_t)buf[7] << 8);
    uint32_t h = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

static void clipboard_bmp_dims(const unsigned char* buf, size_t len, uint32_t* out_w, uint32_t* out_h) {
    if (len < 26) return;
    int32_t w = (int32_t)((uint32_t)buf[18] | ((uint32_t)buf[19] << 8) |
                          ((uint32_t)buf[20] << 16) | ((uint32_t)buf[21] << 24));
    int32_t h = (int32_t)((uint32_t)buf[22] | ((uint32_t)buf[23] << 8) |
                          ((uint32_t)buf[24] << 16) | ((uint32_t)buf[25] << 24));
    if (out_w) *out_w = (w > 0) ? (uint32_t)w : 0u;
    if (out_h) *out_h = (h < 0) ? (uint32_t)(-h) : (uint32_t)h;
}

void clipboard_image_dims(const unsigned char* buf, size_t len, uint32_t* out_w, uint32_t* out_h) {
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!buf) return;
    switch (clipboard_image_format(buf, len)) {
        case CLIPBOARD_FMT_PNG:  clipboard_png_dims(buf, len, out_w, out_h); break;
        case CLIPBOARD_FMT_JPEG: clipboard_jpeg_dims(buf, len, out_w, out_h); break;
        case CLIPBOARD_FMT_GIF:  clipboard_gif_dims(buf, len, out_w, out_h); break;
        case CLIPBOARD_FMT_BMP:  clipboard_bmp_dims(buf, len, out_w, out_h); break;
        default: break;
    }
}

bool clipboard_history_push_text_platform(const char* text) {
    ClipboardEntry e = {0};
    e.type = CLIPBOARD_TEXT;
    e.timestamp = (uint64_t)time(NULL);
    e.text = text ? strdup(text) : NULL;
    if (text && !e.text) return false;
    if (!clipboard_history_push(e)) return false;
    return true;
}

bool clipboard_history_push_image_platform(const char* path, unsigned int w, unsigned int h) {
    ClipboardEntry e = {0};
    e.type = CLIPBOARD_IMAGE;
    e.timestamp = (uint64_t)time(NULL);
    e.image_path = path ? strdup(path) : NULL;
    if (path && !e.image_path) return false;
    e.image_width = w;
    e.image_height = h;
    if (!clipboard_history_push(e)) return false;
    return true;
}

size_t clipboard_history_count_platform(void) {
    return g_clipboard_history.count;
}

const ClipboardEntry* clipboard_history_get_platform(size_t index) {
    if (index >= g_clipboard_history.count) return NULL;
    return &g_clipboard_history.entries[index];
}

void clipboard_clear_history_platform(void) {
    for (size_t i = 0; i < g_clipboard_history.count; i++)
        clipboard_entry_free(&g_clipboard_history.entries[i]);
    free(g_clipboard_history.entries);
    g_clipboard_history.entries = NULL;
    g_clipboard_history.count = 0;
    g_clipboard_history.capacity = 0;
}

bool clipboard_set_png_path_platform(const char* path) {
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
    if (got == (size_t)n && clipboard_image_format(buf, (size_t)n) != CLIPBOARD_FMT_UNKNOWN) {
        uint32_t w = 0, h = 0;
        clipboard_image_dims(buf, (size_t)n, &w, &h);
        ok = Platform_CopyImageToClipboard(buf, (size_t)n);
        if (ok) clipboard_history_push_image_platform(path, w, h);
    }
    free(buf);
    return ok;
}
