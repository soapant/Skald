#ifndef CLIPBOARD_HISTORY_H
#define CLIPBOARD_HISTORY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CLIPBOARD_TEXT,
    CLIPBOARD_IMAGE,
} ClipboardType;

typedef struct {
    ClipboardType type;
    uint64_t timestamp;
    char* text;
    char* image_path;
    uint32_t image_width;
    uint32_t image_height;
} ClipboardEntry;

typedef struct {
    ClipboardEntry* entries;
    size_t count;
    size_t capacity;
} ClipboardHistory;

extern const unsigned char PNG_MAGIC[8];

void clipboard_png_dims(const unsigned char* png, size_t len, uint32_t* out_w, uint32_t* out_h);

bool clipboard_history_push_text_platform(const char* text);
bool clipboard_history_push_image_platform(const char* path, unsigned int w, unsigned int h);
bool clipboard_set_png_path_platform(const char* path);
size_t clipboard_history_count_platform(void);
const ClipboardEntry* clipboard_history_get_platform(size_t index);
void clipboard_clear_history_platform(void);

#endif
