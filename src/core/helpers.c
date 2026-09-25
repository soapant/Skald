#include "core/context.h"
#include "config.h"
#include "config.h"
#include "helpers/helpers_hotkey.h"
#include "platform/ui.h"
#include "core/platform.h"

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
