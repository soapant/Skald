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

char* SelectionData[2048] = {0};
int SelectionDataCount = 0;

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
bool AlwaysEnabled(void) { return true; }
bool AlwaysDisabled(void) { return false; }
bool ProcessHotkey(const HotkeyBinding* bindings, int bindings_count,
                                  int pressed_vk, int modifiers,
                                  const char* current_title) {
    for (int i = 0; i < bindings_count; i++) {
        const HotkeyBinding* b = &bindings[i];

        if (b->vk_code != pressed_vk) continue;
        if (b->modifiers != modifiers) continue;

        if (b->hotkey_condition && !b->hotkey_condition()) {
            continue;
        }

        if (b->blacklist && b->blacklist_count > 0) {
            bool blacklisted = false;
            for (int j = 0; j < b->blacklist_count; j++) {
                if (b->blacklist[j][0] == '\0') continue;
#if defined(_WIN32)
                bool found = StrStrIA(current_title, b->blacklist[j]) != NULL;
#else
                bool found = strstr(current_title, b->blacklist[j]) != NULL;
#endif
                if (found) {
                    blacklisted = true;
                    break;
                }
            }
            if (blacklisted) return false;
        }

        if (AllowOtherFeatureTogglesInFeature) {
#if defined(_WIN32)
            if (IsPopupActive(hPopupWnd)) {
                KillTimer(WH_TO_HWND(hPopupWnd), 999);
                DestroyWindow(WH_TO_HWND(hPopupWnd));
                hPopupWnd = WH_NULL;
            }
#else
            if (IsPopupActive(hPopupWnd)) {
                extern bool selection_complete;
                selection_complete = true;
            }
#endif
        } else {
          if (IsPopupActive(hPopupWnd)) {
            return false;
          }
        }

        g_timer = TimerStart();
        if (MessageAfterEachHotkey) {
          char buffer[64];
          //figure out later
          snprintf(buffer,sizeof(buffer),"[%i] Triggered \n",b->vk_code);
          MSG_BOTTOM(buffer,140,TimeOutForMessages,COLOR_TEXT);
        }
        b->hotkey_callback();
        return true;
    }
    return false;
}
