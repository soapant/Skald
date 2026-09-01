#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "core/modal.h"
#include "core/types.h"
#include "core/hashmap.h"
#include "config.h"
#include "core/platform.h"
#if !defined(_WIN32)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#endif

// Modular split - includes for structure, helpers.h remains compatibility shim
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "platform/x11/drawing.h"
#include "platform/x11/window_factory.h"

#endif
