#ifndef FLAPI_PLATFORM_H
#define FLAPI_PLATFORM_H

#include "core/flapi.h"

char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path);
bool fl_launch_with_args_platform(const char* path, const char* args);
bool fl_launch_with_args_as_admin_platform(const char* path, const char* args);
bool fl_listbox_begin_platform(const FlListSpec *spec);
bool fl_listbox_next_platform(FlListItemCtx *out);
void fl_listbox_end_platform(void);
int fl_listbox_get_count(void);
bool fl_place_editbox_platform(const FlEditSpec *spec);
void fl_editbox_reset_placed(void);
bool fl_editbox_was_placed(void);
void fl_toggle_play_pause_platform(void);
int fl_get_current_volume_platform(void);
bool fl_set_volume_platform(int volume);
bool fl_decrement_volume_by_platform(int decrement);
bool fl_increment_volume_by_platform(int increment);
bool fl_increment_volume_by_percent_platform(int increment_percent);
bool fl_decrement_volume_by_percent_platform(int decrement_percent);
bool fl_type_text_platform(const char *s);
void fl_run_after_time_platform(double seconds, void (*callback)(void));
FlTimer fl_start_timer_platform(void);
double fl_end_timer_platform(FlTimer t);
void fl_sleep_platform(double seconds);
void* fl_thread_create_platform(void* (*fn)(void*), void* arg);
void fl_thread_join_platform(void* thread);
void* fl_mutex_create_platform(void);
void fl_mutex_destroy_platform(void* mutex);
void fl_mutex_lock_platform(void* mutex);
void fl_mutex_unlock_platform(void* mutex);
bool fl_clipboard_history_push_text_platform(const char* text);
bool fl_clipboard_history_push_image_platform(const char* path, unsigned int w, unsigned int h);
bool fl_clipboard_set_png_path_platform(const char* path);
size_t fl_clipboard_history_count_platform(void);
const FlClipboardEntry* fl_clipboard_history_get_platform(size_t index);
void fl_clipboard_clear_history_platform(void);
void fl_screenshot_copy_area_platform(void);
void fl_screenshot_copy_platform(void);
void fl_screenshot_save_area_platform(char* save_path);
void fl_screenshot_save_platform(char* save_path);
void fl_ocr_area_to_clipboard_platform();

#endif
