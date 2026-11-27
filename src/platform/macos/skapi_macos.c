#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include "core/types/skapi_types.h"
#include "helpers/helpers_owner.h"
#include "platform/platform.h"

bool sk_run_after_time_arm_platform(SkRunAfterPayload *payload) {
  (void)payload;
  return false;
}

char *sk_search_file_in_dir_platform(const char *directory,
                                     const char *search_string) {
  (void)directory;
  (void)search_string;
  return NULL;
}

bool sk_launch_with_args_platform(const char *path, const char *args) {
  (void)path;
  (void)args;
  return false;
}

bool sk_launch_with_args_as_admin_platform(const char *path, const char *args) {
  (void)path;
  (void)args;
  return false;
}

SkNetStatus sk_net_status_platform(void) { return SK_NET_OFFLINE; }

int sk_get_active_interface_platform(char out[], size_t max_len) {
  (void)out;
  (void)max_len;
  return 0;
}

unsigned long long sk_net_rx_bytes_platform(void) { return 0; }

bool sk_dismiss_message_platform(const char *name) {
  (void)name;
  return false;
}

int sk_get_pid_platform(void) { return (int)getpid(); }
