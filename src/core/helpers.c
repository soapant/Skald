#include "common.h"
#include "config.h"
#include <ctype.h>
#if defined(_WIN32)
#include <shellapi.h>
#include <shlwapi.h>
#endif
#include "helpers.h"
#include "core/timer.h"
#include "core/common_ui.h"

int g_refresh_count = 0;
bool draw_background = true;

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color) {
    if (MessagesEnabled) {
        if (WH_TO_HWND(hPopupWnd) != NULL) {
          MSG_TOP(buffer,msg_height,timeout,color);
        }else {
          MSG_MIDDLE_BOTTOM(buffer,msg_height / 2,timeout,color);
        }
    }
}
EditorMode g_last_mode = MODE_INSERT;
