#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include <ctype.h>
#if defined(_WIN32)
#include <shellapi.h>
#include <shlwapi.h>
#endif
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "core/timer.h"
#include "core/common_ui.h"

int g_refresh_count = 0;
#if !defined(_WIN32)
bool draw_background = true;
#endif

void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color) {
    if (MessagesEnabled) {
        if (IsPopupActive(WH_NULL)) {
          MSG_TOP(buffer,msg_height,timeout,color);
        }else {
          MSG_MIDDLE_BOTTOM(buffer,msg_height / 2,timeout,color);
        }
    }
}
EditorMode g_last_mode = MODE_INSERT;