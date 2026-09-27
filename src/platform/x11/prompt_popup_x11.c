#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include "wm/wm.h"
#include "platform/platform.h"
#include "platform/ui.h"
#include "config.h"
#include "platform/wm_platform.h"
#include "platform/x11/subclasses_x11.h"
#include "platform/x11/drawing_x11.h"
#include "platform/x11/display_x11.h"
#include "core/timer.h"
#include "core/history.h"
#include "platform/x11/input_x11.h"
#include "platform/x11/window_factory_x11.h"
#include "platform/input.h"
#include "bindings/vim_helpers.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_ui.h"
#include "deps/log.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool Platform_PromptPopupShowWindow(Popup* self, int x, int y, int w, int h) {
    PROFILE_START(timer);
    (void)x;
    (void)y;
    WindowManager* wm = (WindowManager*)self->manager;
    Display* dpy = wm ? wm->display : NULL;
    if (!dpy) { PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow"); return false; }
    self->handle.xid = x11_create_prompt_window(dpy, x, y, w, h);
    XSelectInput(dpy, self->handle.xid, KeyPressMask | ExposureMask | FocusChangeMask);
    XMapWindow(dpy, self->handle.xid);
    XSync(dpy, False);
    {
        XWindowAttributes _a;
        if (XGetWindowAttributes(dpy, self->handle.xid, &_a)) {
            log_info("prompt mapped req=(%d,%d %dx%d) actual=(%d,%d %dx%d) state=%d", x, y, w, h, _a.x, _a.y, _a.width, _a.height, (int)_a.map_state);
        } else {
            log_info("prompt mapped req=(%d,%d %dx%d) actual=unknown", x, y, w, h);
        }
    }
    {
        int (*old)(Display*, XErrorEvent*) = XSetErrorHandler(x11_error_handler);
        XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
        XSync(dpy, False);
        XSetErrorHandler(old);
        Window _fw = None;
        int _rev = 0;
        XGetInputFocus(dpy, &_fw, &_rev);
        if (_fw != self->handle.xid) {
            old = XSetErrorHandler(x11_error_handler);
            XSetInputFocus(dpy, self->handle.xid, RevertToParent, CurrentTime);
            XSync(dpy, False);
            XSetErrorHandler(old);
            XGetInputFocus(dpy, &_fw, &_rev);
        }
        log_info("prompt focus win=%lu want=%lu ok=%d", (unsigned long)_fw, (unsigned long)self->handle.xid, _fw == self->handle.xid);
        char* _nm = NULL;
        if (XFetchName(dpy, self->handle.xid, &_nm) && _nm) {
            if (!x11_is_own_window_name(_nm)) {
                Wm_FocusByTitle(_nm);
            }
            XFree(_nm);
        }
    }
    XFlush(dpy);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupShowWindow");
    return true;
}

void Platform_PromptPopupHideWindow(Popup* self) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) {
        XUnmapWindow(dpy, self->handle.xid);
        XFlush(dpy);
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupHideWindow");
}

void Platform_PromptPopupRender(Popup* self) {
    PROFILE_START(timer);
    WindowManager* wm = (WindowManager*)self->manager;
    Display* dpy = wm ? wm->display : NULL;
    if (dpy) x11_render_prompt_user(dpy, &self->handle.xid, (PromptState*)self->user_data);
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupRender");
}

void Platform_PromptPopupDestroyWindow(Popup* self) {
    PROFILE_START(timer);
    Display* dpy = get_display();
    if (dpy) {
        x11_release_prompt_buffer(dpy, self->handle.xid);
        XDestroyWindow(dpy, self->handle.xid);
    }
    PROFILE_END_VAR(timer, "window_manager:Platform_PromptPopupDestroyWindow");
}

void HandleCommandExecution(ListBoxInfo* lb, char* string) {
    if (!lb) return;
    int num = atoi(string);
    UI_SetSelection(lb, FlintContext_Get()->window.hListBox, num - 1);
    UI_LB_SetCaretIndex(FlintContext_Get()->window.hListBox, num, TRUE);
}

static void command_confirm(const char* new_text, void* user_data) {
    PromptState* state = (PromptState*)user_data;
    HandleCommandExecution(&state->listbox, (char*)new_text);
}

bool Platform_PromptPopupOnKey(Popup* self, const PopupKeyEvent* key) {
    PROFILE_START(timer);
    PromptState* state = self ? (PromptState*)self->user_data : NULL;
    if (!state) { PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey"); return true; }

    int wParam = key->vk;
    bool ctrl_pressed = key->ctrl;
    bool shift_pressed = key->shift;
    bool alt_pressed = key->alt;
    log_info("prompt key wParam=%d shift=%d ctrl=%d alt=%d insert=%d", wParam, shift_pressed, ctrl_pressed, alt_pressed, IsInsertMode());
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey"); return true; }
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->filter[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->filter, &state->cursor);
        }
        History_ResetCursor(FlintContext_Get()->window.current_feature);
        if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, state->has_listbox ? &state->listbox : NULL);
        PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
        return true;
    }

    if (FlintContext_Get()->prompt_callbacks.refresh) {
        if (wParam == VK_ESCAPE) {
            if (IsInsertMode()) {
                SetMode(MODE_NORMAL);
            } else {
                FlintContext_Get()->window.selection_complete = true;
            }
            return true;
        }
    } else {
        if (wParam == VK_ESCAPE) {
            FlintContext_Get()->window.selection_complete = true;
            return true;
        }
    }
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->filter));
      PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->filter));
      PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
      return true;
    }
    if (wParam == VK_UP) {
      if (ctrl_pressed) {
        SetCursorToStartOfText(FlintContext_Get());
        PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
        return true;
      }
      if (state->has_listbox) {
        if (state->listbox.selected_index == -1) {
          char hist[512];
          const char* feat = FlintContext_Get()->window.current_feature;
          if (feat && History_CycleUp(feat, hist, sizeof(hist))) {
            strncpy(state->filter, hist, sizeof(state->filter)-1);
            state->filter[sizeof(state->filter)-1]='\0';
            state->cursor = (int)strlen(state->filter);
            if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, state->has_listbox ? &state->listbox : NULL);
            PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
            return true;
          }
        }
        UI_NavigateUp(&state->listbox, FlintContext_Get()->window.hListBox);
      }
      PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
      return true;
    }
    if (wParam == VK_DOWN) {
      if (ctrl_pressed) {
        SetCursorToEndOfText(FlintContext_Get());
        PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
        return true;
      }
      if (state->has_listbox) UI_NavigateDown(&state->listbox, FlintContext_Get()->window.hListBox);
      PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
      return true;
    }
    if (wParam == VK_DELETE) {
      if (state && EditBufferDeleteChar(state->filter, &state->cursor)) {
        History_ResetCursor(FlintContext_Get()->window.current_feature);
        if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, state->has_listbox ? &state->listbox : NULL);
      }
      PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
      return true;
    }
    if (wParam == VK_HOME) {
      SetCursorToStartOfText(FlintContext_Get());
      PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
      return true;
    }
    if (wParam == VK_END) {
      SetCursorToEndOfText(FlintContext_Get());
      PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
      return true;
    }
    if (wParam == VK_TAB) {
      if (FlintContext_Get()->prompt_callbacks.tab) FlintContext_Get()->prompt_callbacks.tab(&state->listbox);
      PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
      return true;
    }
    if (wParam == VK_RETURN) {
      bool is_ctrl = ctrl_pressed;
      const char* sel = NULL;
      char filter_copy[sizeof(state->filter)];
      if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) {
        sel = state->listbox.populated_cache[state->listbox.selected_index];
      } else {
        strncpy(filter_copy, state->filter, sizeof(filter_copy)-1);
        filter_copy[sizeof(filter_copy)-1]='\0';
        sel = filter_copy;
      }
      const char* feat = FlintContext_Get()->window.current_feature;
      if (feat && sel && sel[0]) History_Add(feat, sel);
      if (is_ctrl) {
        {
          FlintContext* _c = FlintContext_Get();
          bool should_close = false;
          if (_c->prompt_callbacks.ctrl_enter) should_close = _c->prompt_callbacks.ctrl_enter(sel, &state->listbox);
          else if (_c->prompt_callbacks.enter) should_close = _c->prompt_callbacks.enter(sel, &state->listbox);
          _c->window.selection_complete = should_close;
        }
        PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
        return true;
      } else {
        {
          FlintContext* _c2 = FlintContext_Get();
          bool should_close = true;
          if (_c2->prompt_callbacks.enter) should_close = _c2->prompt_callbacks.enter(sel, &state->listbox);
          _c2->window.selection_complete = should_close;
        }
        PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
        return true;
      }
    }
    if (wParam == COPY_SELECTION_KEY) {
      if (shift_pressed || ctrl_pressed) {
        int type = shift_pressed ? 0 : 1;
        int idx = -1;
        if (state->has_listbox && state->listbox.selected_index >= 0 && state->listbox.selected_index < state->listbox.populated_count) {
          idx = state->listbox.matching_indices[state->listbox.selected_index];
        } else {
          idx = UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer);
        }
        if (FlintContext_Get()->prompt_callbacks.copy) {
          FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer,idx,type);
        } else {
          Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
        }
        return true;
      }
    }
    if (wParam == HELP_SCREEN_KEY && ctrl_pressed) {
      if (FlintContext_Get()->prompt_callbacks.help) FlintContext_Get()->prompt_callbacks.help();
      return true;
    }

    if (wParam == COMMAND_MODE_KEY && shift_pressed) {
        WindowManager* wm = WindowManager_Get();
        Popup* popup = EditPopup_Create(wm,"", "Command Mode", 960, 540, 1000, 100, "LEFT", command_confirm,state);
        WindowManager_PushPopup(wm, popup);
        return true;
      }
      if (wParam == 'V' && ctrl_pressed) {
        char *clip = Platform_GetClipboardEntry();
        if (!clip) { log_warn("prompt Ctrl+V clipboard null"); return true; }
        log_info("prompt Ctrl+V clip len=%d", (int)strlen(clip));
        clip[strcspn(clip, "\r\n")] = '\0';
        if (clip[0] == '\0') { free(clip); return true; }

        size_t cur_len = strlen(state->filter);
        size_t clip_len = strlen(clip);
        size_t avail = sizeof(state->filter) - cur_len - 1;
        if (clip_len > avail) clip_len = avail;
        if (clip_len > 0) {
          memmove(state->filter + state->cursor + clip_len,
                  state->filter + state->cursor,
                  cur_len - state->cursor + 1);
          memcpy(state->filter + state->cursor, clip, clip_len);
          state->cursor += clip_len;
        }
        free(clip);

        if (FlintContext_Get()->prompt_callbacks.refresh) {
          FlintContext_Get()->prompt_callbacks.refresh(state->filter, &state->listbox);
        }
        return true;
      }
    if (ctrl_pressed) {
      if (wParam == VK_OEM_MINUS) {
        Platform_AdjustFontSize(-1);
        Config_Save();
      }
      if (wParam == VK_OEM_PLUS) {
        Platform_AdjustFontSize(1);
        Config_Save();
      }
    }

    if (IsInsertMode() && key->text_len > 0) {
        bool swallow = false;
        bool trigger_press = false;
        if (pending_hotkey_vk != 0 && key->text_len == 1) {
            struct timeval tv; gettimeofday(&tv, NULL);
            long now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            if (now - pending_hotkey_timeMs < 800) {
                unsigned char c = (unsigned char)key->text[0];
                int vkc = 0;
                if (c >= 'a' && c <= 'z') vkc = c - 'a' + 'A';
                else if (c >= 'A' && c <= 'Z') vkc = c;
                else if (c == ' ') vkc = VK_SPACE;
                else if (c == '[' || c == '{') vkc = VK_OEM_4;
                else if (c == ']' || c == '}') vkc = VK_OEM_6;
                else if (c == ';' || c == ':') vkc = VK_OEM_1;
                else if (c == '/' || c == '?') vkc = VK_OEM_2;
                if (vkc != 0 && vkc == pending_hotkey_vk) trigger_press = true;
            }
            pending_hotkey_vk = 0;
            pending_hotkey_mod = 0;
        }
        if (!trigger_press && !swallow && (key->ctrl || key->alt)) swallow = true;
        if (!swallow) {
            bool is_printable = true;
            for (int i = 0; i < key->text_len; i++) { unsigned char c = (unsigned char)key->text[i]; if (c < 0x20 || c == 0x7F) { is_printable = false; break; } }
            if (is_printable) {
                size_t n = strlen(state->filter);
                if (n + (size_t)key->text_len < sizeof(state->filter) - 1) {
                    if ((size_t)state->cursor > n) state->cursor = (int)n;
                    memmove(state->filter + state->cursor + key->text_len, state->filter + state->cursor, n - state->cursor + 1);
                    memcpy(state->filter + state->cursor, key->text, key->text_len);
                    state->cursor += key->text_len;
                    History_ResetCursor(FlintContext_Get()->window.current_feature);
                    FlintContext* _c = FlintContext_Get();
                    if (_c->prompt_callbacks.refresh) _c->prompt_callbacks.refresh(state->filter, state->has_listbox ? &state->listbox : NULL);
                }
                PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
                return true;
            }
        }
    }

    PROFILE_END_VAR(timer, "prompt_popup_x11:Platform_PromptPopupOnKey");
    return false;
}

