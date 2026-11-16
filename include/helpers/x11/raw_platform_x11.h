#ifndef RAW_PLATFORM_X11_H
#define RAW_PLATFORM_X11_H

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <stdbool.h>
#include <pthread.h>
#include "core/types.h"
#include "helpers/helpers_fs.h"
#include <X11/Xatom.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include "deps/log.h"
#include "helpers/helpers_input_state.h"
#include "config_values.h"
#include "core/types/window.h"
#include <X11/extensions/Xrandr.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <sys/file.h>
#include <fontconfig/fontconfig.h>
#include <curl/curl.h>
#include "helpers/x11/display_x11.h"
#include "platform/platform.h"
#include "helpers/x11/raw_misc_x11.h"
#include "core/context.h"
#include "helpers/helpers_owner.h"
#include "core/thread_registry.h"
#include "core/prompt_geometry.h"
#include "helpers/helpers_text.h"
#include "core/feature_path.h"
#include "helpers/x11/env_helpers_x11.h"
#include "wm/wm.h"
#include "helpers/x11/clipboard_x11.h"
#include "helpers/x11/clipboard_x11.h"

extern pthread_mutex_t g_font_cache_mtx;

bool x11_font_is_cached(Display *dpy, XftFont *xft);
bool x11_font_was_retired(Display *dpy, XftFont *xft);
void x11_cleanup_per_feature_edit_map(void);
int x11_button(int button);
unsigned char x11_mask_extract(unsigned long pixel, unsigned long mask);

#endif
