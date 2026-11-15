#ifndef TYPES_SKAPI_TYPES_H
#define TYPES_SKAPI_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/types.h"
#include "helpers/helpers_owner.h"

typedef PlTimer SkTimer;

typedef enum {
  SK_NET_OFFLINE,
  SK_NET_ETHERNET,
  SK_NET_WIFI,
} SkNetStatus;

typedef struct {
  const char *buffer;
  int x;
  int y;
  int w;
  int h;
  const char *placeholder;
  ColorRef color_bg;
  ColorRef color_text;
  FontHandle font;
  int fontsize;
  int caret_height;
  int caret_width;
  int caret_x;
  int caret_y;
  int caret_pos;
} SkEditSpec;

typedef struct {
  int idx;
  bool selected;
  int x;
  int y;
  int w;
  int h;
  const char *text;
} SkListItemCtx;

typedef struct SkRunAfterPayload {
  double seconds;
  void (*callback)(void);
  char owner[OWNER_PATH_MAX];
} SkRunAfterPayload;

bool sk_run_after_time_arm_platform(SkRunAfterPayload *payload);

char *sk_search_file_in_dir_platform(const char *directory,
                                     const char *search_string);
bool sk_launch_with_args_platform(const char *path, const char *args);
bool sk_launch_with_args_as_admin_platform(const char *path, const char *args);
bool sk_place_editbox_platform(const SkEditSpec *spec);
void sk_sleep_platform(double seconds);
SkNetStatus sk_net_status_platform(void);
int sk_get_active_interface_platform(char out[], size_t max_len);
unsigned long long sk_net_rx_bytes_platform(void);
bool sk_dismiss_message_platform(const char *name);
int sk_get_pid_platform(void);

#endif
