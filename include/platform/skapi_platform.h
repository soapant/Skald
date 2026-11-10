#ifndef SKAPI_PLATFORM_H
#define SKAPI_PLATFORM_H

#include "core/types/skapi_types.h"

char *sk_search_file_in_dir_platform(const char *directory,
                                     const char *search_string,
                                     char *output_path);
bool sk_launch_with_args_platform(const char *path, const char *args);
bool sk_launch_with_args_as_admin_platform(const char *path, const char *args);
bool sk_place_editbox_platform(const SkEditSpec *spec);
void sk_run_after_time_platform(double seconds, void (*callback)(void));
void sk_sleep_platform(double seconds);
SkNetStatus sk_net_status_platform();
int sk_get_active_interface_platform(char out[], size_t max_len);
unsigned long long sk_net_rx_bytes_platform();
bool sk_dismiss_message_platform(const char *name);
int sk_get_pid_platform();

#endif
