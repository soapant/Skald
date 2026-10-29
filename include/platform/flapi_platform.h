#ifndef FLAPI_PLATFORM_H
#define FLAPI_PLATFORM_H

#include "core/types/flapi_types.h"

char* fl_search_file_in_dir_platform(const char* directory, const char* search_string,char* output_path);
bool fl_launch_with_args_platform(const char* path, const char* args);
bool fl_launch_with_args_as_admin_platform(const char* path, const char* args);
bool fl_listbox_begin_platform(const PlListSpec *spec);
bool fl_listbox_next_platform(FlListItemCtx *out);
void fl_listbox_end_platform(void);
int fl_listbox_get_count(void);
bool fl_place_editbox_platform(const FlEditSpec *spec);
bool fl_type_text_platform(const char *s);
void fl_run_after_time_platform(double seconds, void (*callback)(void));
void fl_sleep_platform(double seconds);
FlNetStatus fl_net_status_platform();
int fl_get_active_interface_platform(char out[], size_t max_len);
unsigned long long fl_net_rx_bytes_platform();
bool fl_dismiss_message_platform(const char* name);
int fl_get_pid_platform();


#endif
