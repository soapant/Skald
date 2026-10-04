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
void fl_screenshot_copy_area_platform(void);
void fl_screenshot_copy_platform(void);
void fl_screenshot_save_area_platform(char* save_path);
void fl_screenshot_save_platform(char* save_path);
void fl_ocr_area_to_clipboard_platform();
FlNetStatus fl_net_status_platform();
int fl_get_active_interface_platform(char out[], size_t max_len);
unsigned long long fl_net_rx_bytes_platform(const char *iface);

#endif
