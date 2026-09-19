#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "platform/x11/drawing_x11.h"
#include "core/platform.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_flag.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "core/vim.h"
#include "platform/x11/input_x11.h"
#include "bindings/bindings.h"
#include "bindings/vim_motions.h"
#include "bindings/vim_helpers.h"
#include "core/history.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <sys/time.h>
#include <string.h>
#include "core/timer.h"
#include "platform/draw.h"
#include "platform/x11/input_x11.h"

void EditBufferBackspace(char* buf, size_t* cursor) {
    PROFILE_START(timer);
    size_t n = strlen(buf);
    if (*cursor > 0) {
        memmove(buf + *cursor - 1,
                buf + *cursor,
                n - *cursor + 1);
        (*cursor)--;
    }
    PROFILE_END_VAR(timer, "subclasses_x11:EditBufferBackspace");
}

bool EditBufferDeleteChar(char* buf, size_t* cursor) {
    PROFILE_START(timer);
    size_t n = strlen(buf);
    if (*cursor >= n) { PROFILE_END_VAR(timer, "subclasses_x11:EditBufferDeleteChar"); return false; }
    memmove(buf + *cursor,
            buf + *cursor + 1,
            n - *cursor);
    PROFILE_END_VAR(timer, "subclasses_x11:EditBufferDeleteChar");
    return true;
}

