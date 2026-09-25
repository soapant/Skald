#ifndef CLIPBOARD_HISTORY_H
#define CLIPBOARD_HISTORY_H

#include "core/flapi.h"

extern const unsigned char FL_PNG_MAGIC[8];

void fl_clipboard_png_dims(const unsigned char* png, size_t len, uint32_t* out_w, uint32_t* out_h);

bool fl_clipboard_history_push_text_platform(const char* text);
bool fl_clipboard_history_push_image_platform(const char* path, unsigned int w, unsigned int h);
bool fl_clipboard_set_png_path_platform(const char* path);
size_t fl_clipboard_history_count_platform(void);
const FlClipboardEntry* fl_clipboard_history_get_platform(size_t index);
void fl_clipboard_clear_history_platform(void);

#endif
