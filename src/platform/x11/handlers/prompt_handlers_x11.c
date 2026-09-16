#include "core/context.h"
#include "core/modal.h"
#include "config.h"
#include "platform/x11/drawing.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "platform/ui.h"
#include "core/vim.h"
#include "platform/x11/input.h"
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
#include "platform/x11/input_helpers.h"
#include "platform/x11/ui_x11.h"
#include "core/window_manager.h"
#include <stdlib.h>

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

bool Linux_HandlePromptKey(const XEvent* ev,PromptState *state) {
    PROFILE_START(timer);
    if (ev->type != KeyPress) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey"); return false; }

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym((XKeyEvent*)&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey"); return true; }
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey"); return true; }
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->filter[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->filter, &state->cursor);
        }
        History_ResetCursor(FlintContext_Get()->window.current_feature);
        if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
      if (IsInsertMode()) {
        SetMode(MODE_NORMAL);
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      } else {
        FlintContext_Get()->window.selection_complete = true;
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
    }
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->filter));
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->filter));
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_UP) {
      if (ctrl_pressed) {
        SetCursorToStartOfText(FlintContext_Get());
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
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
            if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
            PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
            return true;
          }
        }
        UI_NavigateUp(&state->listbox, FlintContext_Get()->window.hListBox);
      }
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_DOWN) {
      if (ctrl_pressed) {
        SetCursorToEndOfText(FlintContext_Get());
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
      if (state->has_listbox) UI_NavigateDown(&state->listbox, FlintContext_Get()->window.hListBox);
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_DELETE) {
      if (state && EditBufferDeleteChar(state->filter, &state->cursor)) {
        History_ResetCursor(FlintContext_Get()->window.current_feature);
        if (FlintContext_Get()->prompt_callbacks.refresh) FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, state->has_listbox ? &state->listbox : NULL);
      }
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_HOME) {
      SetCursorToStartOfText(FlintContext_Get());
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_END) {
      SetCursorToEndOfText(FlintContext_Get());
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
      return true;
    }
    if (wParam == VK_TAB) {
      if (FlintContext_Get()->prompt_callbacks.tab) FlintContext_Get()->prompt_callbacks.tab(&state->listbox);
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
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
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      } else {
        {
          FlintContext* _c2 = FlintContext_Get();
          bool should_close = true;
          if (_c2->prompt_callbacks.enter) should_close = _c2->prompt_callbacks.enter(sel, &state->listbox);
          _c2->window.selection_complete = should_close;
        }
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
        return true;
      }
    }
    if (wParam == COPY_SELECTION_KEY) {
      if (shift_pressed || ctrl_pressed) {
        const char* sel = "";
        if (state->has_listbox && UI_IsListBoxSelected(&state->listbox)) sel = state->listbox.populated_cache[state->listbox.selected_index];
        else sel = state->filter;
        int type = shift_pressed ? 0 : 1;
        if (FlintContext_Get()->prompt_callbacks.copy) {
          int idx = UI_IsListBoxSelected(&state->listbox) ? state->listbox.selected_index : -1;
          FlintContext_Get()->prompt_callbacks.copy(sel, idx, type);
        } else {
          Platform_CopyToClipboard(sel);
        }
        return true;
      }
    }

    if (wParam == COMMAND_MODE_KEY) {
        WindowManager* wm = WindowManager_Get();
        Popup* popup = EditPopup_Create(wm,"", "Command Mode", 960, 540,command_confirm,state);
        WindowManager_PushPopup(wm, popup);
        return true;
      }
     if (wParam == 'V' && ctrl_pressed) {
        char *clip = Platform_GetLastClipboardEntry();
        if (!clip) return true;
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
          FlintContext_Get()->prompt_callbacks.refresh(state->filter, FlintContext_Get()->window.hListBox, &state->listbox);
        }
        return true;
      }

    PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandlePromptKey");
    return false;
}

bool Linux_HandleSimplePromptKey(const XEvent* ev,SimplePromptState *state) {
    PROFILE_START(timer);
    if (ev->type != KeyPress) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return false; }

    int wParam = Linux_KeysymToWindowsVK(XLookupKeysym((XKeyEvent*)&ev->xkey, 0));
    bool ctrl_pressed, shift_pressed, alt_pressed;
    ReadModifiers(ev, &ctrl_pressed, &shift_pressed, &alt_pressed);
    if (should_swallow_pending(wParam, (ctrl_pressed?HOTKEY_MOD_CTRL:0)|(shift_pressed?HOTKEY_MOD_SHIFT:0)|(alt_pressed?HOTKEY_MOD_ALT:0))) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return true; }
    if (TryVimMotion(wParam, ctrl_pressed, shift_pressed, alt_pressed)) { PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey"); return true; }
    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            state->user_input[0] = '\0';
            state->cursor = 0;
        } else {
            EditBufferBackspace(state->user_input, &state->cursor);
        }
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
    }
    if (wParam == VK_ESCAPE) {
      if (IsInsertMode()) {
        SetMode(MODE_NORMAL);
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
      } else {
        FlintContext_Get()->window.selection_complete = true;
        PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
        return true;
      }
    }
    if (wParam == VK_RIGHT) {
      CursorForward(&state->cursor, strlen(state->user_input));
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }
    if (wParam == VK_LEFT) {
       CursorBack(&state->cursor, strlen(state->user_input));
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }
    if (wParam == VK_RETURN) {
      FlintContext_Get()->window.selection_complete = true;
      PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
      return true;
    }

    if (wParam == 'V' && ctrl_pressed) {
      char *clip = Platform_GetLastClipboardEntry();
      if (!clip) return true;
      clip[strcspn(clip, "\r\n")] = '\0';
      if (clip[0] == '\0') { free(clip); return true; }

      size_t cur_len = strlen(state->user_input);
      size_t clip_len = strlen(clip);
      size_t avail = sizeof(state->user_input) - cur_len - 1;
      if (clip_len > avail) clip_len = avail;
      if (clip_len > 0) {
        memmove(state->user_input + state->cursor + clip_len,
                state->user_input + state->cursor,
                cur_len - state->cursor + 1);
        memcpy(state->user_input + state->cursor, clip, clip_len);
        state->cursor += clip_len;
      }
      free(clip);

      return true;
    }
    PROFILE_END_VAR(timer, "subclasses_x11:Linux_HandleSimplePromptKey");
    return false;
}

