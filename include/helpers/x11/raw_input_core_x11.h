#ifndef INPUT_CORE_X11_H
#define INPUT_CORE_X11_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <X11/X.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <stdbool.h>

#include "helpers/x11/input_x11.h"
#include "deps/log.h"
#include "core/hotkey_manager.h"
#include "helpers/helpers_hotkey.h"
#include "core/remap_manager.h"
#include "core/snippet_manager.h"
#include "platform/platform.h"
#include "helpers/x11/env_helpers_x11.h"
#include "helpers/x11/raw_misc_x11.h"
#include "wm/wm.h"
#include "helpers/helpers_input_state.h"
#include "core/types/keys.h"
#include <linux/uinput.h>
#include <errno.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <signal.h>

#include "helpers/x11/display_x11.h"

#endif
