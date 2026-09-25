#include <string.h>
#include "core/timer.h"
#include "platform/input.h"

void EditBufferBackspace(char* buf, size_t* cursor) {
    PROFILE_START(timer);
    size_t n = strlen(buf);
    if (*cursor > 0) {
        memmove(buf + *cursor - 1,
                buf + *cursor,
                n - *cursor + 1);
        (*cursor)--;
    }
    PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferBackspace");
}

bool EditBufferDeleteChar(char* buf, size_t* cursor) {
    PROFILE_START(timer);
    size_t n = strlen(buf);
    if (*cursor >= n) { PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferDeleteChar"); return false; }
    memmove(buf + *cursor,
            buf + *cursor + 1,
            n - *cursor);
    PROFILE_END_VAR(timer, "edit_buffer_win32:EditBufferDeleteChar");
    return true;
}
