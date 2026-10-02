#include "core/context.h"
#include "config.h"
#include "config.h"
#include "helpers/helpers_hotkey.h"
#include "helpers/helpers_ui.h"
#include "platform/ui.h"
#include "platform/platform.h"
#include "platform/wm_platform.h"
#include <ctype.h>
#include <string.h>

static bool MsgTokenEqualsAt(const char* p, const char* token, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (tolower((unsigned char)p[i]) != tolower((unsigned char)token[i])) return false;
    }
    return true;
}

static bool MsgHasToken(const char* position, const char* token) {
    size_t n = strlen(token);
    for (const char* p = position; *p; p++) {
        if (*p == ' ' || *p == '-' || *p == '_') continue;
        if (!MsgTokenEqualsAt(p, token, n)) continue;
        char after = p[n];
        if (after == '\0' || after == ' ' || after == '-' || after == '_') return true;
    }
    return false;
}

void ParseMessageAnchor(const char* position, MsgAnchorH* h, MsgAnchorV* v) {
    *h = MSG_ANCHOR_H_CENTER;
    *v = MSG_ANCHOR_V_CENTER;
    if (!position || !position[0]) return;
    if (MsgHasToken(position, "left")) *h = MSG_ANCHOR_H_LEFT;
    else if (MsgHasToken(position, "right")) *h = MSG_ANCHOR_H_RIGHT;
    if (MsgHasToken(position, "top")) *v = MSG_ANCHOR_V_TOP;
    else if (MsgHasToken(position, "bottom")) *v = MSG_ANCHOR_V_BOTTOM;
}

bool MessagePositionIsFreehand(const char* position) {
    if (!position || !position[0]) return false;
    return MsgHasToken(position, "free") || MsgHasToken(position, "custom");
}

void ResolveMessageCenter(const char* position, int max_w, int max_h, int* out_x, int* out_y) {
    int anchor_w = max_w > 0 ? max_w : MSG_ANCHOR_MAX_W;
    int anchor_h = max_h > 0 ? max_h : MSG_ANCHOR_MAX_H;
    MsgAnchorH h;
    MsgAnchorV v;
    ParseMessageAnchor(position, &h, &v);
    int mox = 0, moy = 0, mow = 0, moh = 0;
    Platform_WM_GetMonitorGeometry(&mox, &moy, &mow, &moh);
    if (mow <= 0) mow = 1920;
    if (moh <= 0) moh = 1080;
    int x;
    if (h == MSG_ANCHOR_H_LEFT) x = mox + GAP + anchor_w / 2;
    else if (h == MSG_ANCHOR_H_RIGHT) x = mox + mow - GAP - anchor_w / 2;
    else x = mox + mow / 2;
    int y;
    if (v == MSG_ANCHOR_V_TOP) y = moy + GAP + anchor_h / 2;
    else if (v == MSG_ANCHOR_V_BOTTOM) y = moy + moh - GAP - anchor_h / 2;
    else y = moy + moh / 2;
    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
}

void DisplayMessageAnchored(const char* message, const char* position, int max_w, int max_h, int timeout, ColorRef color) {
    if (!message || !message[0]) return;
    int x = 0;
    int y = 0;
    ResolveMessageCenter(position, max_w, max_h, &x, &y);
    Platform_DisplayMessage(message, x, y, max_w, max_h, timeout, color);
}

// void SubjectiveMessageDisplay(const char* buffer,int msg_height,int timeout,ColorRef color) {
//     if (MessagesEnabled) {
//         if (IsPopupActive(WH_NULL)) {
//           MSG_TOP(buffer,msg_height,timeout,color);
//         }else {
//           MSG_MIDDLE_BOTTOM(buffer,msg_height / 2,timeout,color);
//         }
//     }
// }
EditorMode g_last_mode = MODE_INSERT;
