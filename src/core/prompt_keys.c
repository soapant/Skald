#include "core/edit_state.h"
#include "core/prompt_keys.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "core/context.h"
#include "deps/log.h"
#include "core/history.h"
#include "core/modal.h"
#include "core/listbox_hover.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "platform/ui.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_alias.h"
#include "config_values.h"
#include "core/types/keys.h"

void Prompt_Refresh(PromptState *state) {
  if (!state) {
    return;
  }
  FlintContext *ctx = FlintContext_Get();
  if (ctx && ctx->prompt_callbacks.refresh) {
    ctx->prompt_callbacks.refresh(state);
  }
}

bool Prompt_DeleteSelection(PromptState *state) {
  if (!state)
    return false;
  int ss = 0, se = 0;
  if (!EditState_GetSelection(&ss, &se)) {
    return false;
  }
  if (se < ss) {
    int t = ss;
    ss = se;
    se = t;
  }
  int len = (int)strlen(state->editbox->filter);
  if (ss < 0)
    ss = 0;
  if (se > len)
    se = len;
  if (se > ss) {
    memmove(state->editbox->filter + ss, state->editbox->filter + se, len - se + 1);
    state->editbox->cursor = ss;
    EditState_EndSelection();
    return true;
  }
  EditState_EndSelection();
  return false;
}

void Prompt_InsertText(PromptState *state, const char *text, int len) {
  if (!state || !text || len <= 0) {
    return;
  }
  Prompt_DeleteSelection(state);
  size_t fcur = (size_t)state->editbox->cursor;
  EditBufferInsertText(state->editbox->filter, sizeof(state->editbox->filter), &fcur, text,
                       (size_t)len);
  state->editbox->cursor = (int)fcur;
  History_ResetCursor(FlintContext_Get()->window.current_feature);
  Prompt_Refresh(state);
}

void Prompt_TriggerEnter(PromptState *state, bool ctrl, bool shift, bool alt) {
  const char *sel = NULL;
  char filter_copy[sizeof(state->editbox->filter)];
  if (state->listbox && UI_IsListBoxSelected(state->listbox)) {
    sel = ListBoxInfo_Row(state->listbox, state->listbox->selected_index);
  } else {
    strncpy(filter_copy, state->editbox->filter, sizeof(filter_copy) - 1);
    filter_copy[sizeof(filter_copy) - 1] = '\0';
    sel = filter_copy;
  }
  const char *feat = FlintContext_Get()->window.current_feature;
  if (feat && sel && sel[0])
    History_Add(feat, sel);
  FlintContext *ctx = FlintContext_Get();
  log_info("ENTER_DISPATCH: ctx=%p current_feature=%s "
           "enter=%p ctrl_enter=%p window_enter=%p",
           (void *)ctx,
           ctx->window.current_feature ? ctx->window.current_feature : "(null)",
           (void *)ctx->prompt_callbacks.enter,
           (void *)ctx->prompt_callbacks.ctrl_enter,
           (void *)ctx->window.callbacks.enter);
  bool should_close = true;
  if (ctrl && shift) {
    should_close = false;
    if (ctx->prompt_callbacks.ctrl_shift_enter)
      should_close = ctx->prompt_callbacks.ctrl_shift_enter(state);
    else if (ctx->prompt_callbacks.enter)
      should_close = ctx->prompt_callbacks.enter(state);
  } else if (ctrl && alt) {
    should_close = false;
    if (ctx->prompt_callbacks.ctrl_alt_enter)
      should_close = ctx->prompt_callbacks.ctrl_alt_enter(state);
    else if (ctx->prompt_callbacks.enter)
      should_close = ctx->prompt_callbacks.enter(state);
  } else if (shift && alt) {
    should_close = false;
    if (ctx->prompt_callbacks.shift_alt_enter)
      should_close = ctx->prompt_callbacks.shift_alt_enter(state);
    else if (ctx->prompt_callbacks.enter)
      should_close = ctx->prompt_callbacks.enter(state);
  } else if (shift) {
    should_close = false;
    if (ctx->prompt_callbacks.shift_enter)
      should_close = ctx->prompt_callbacks.shift_enter(state);
    else if (ctx->prompt_callbacks.enter)
      should_close = ctx->prompt_callbacks.enter(state);
  } else if (ctrl) {
    should_close = false;
    log_info("CTRL_ENTER_BRANCH: ctrl_enter=%p sel='%s'",
             (void *)ctx->prompt_callbacks.ctrl_enter, sel ? sel : "(null)");
    if (ctx->prompt_callbacks.ctrl_enter) {
      log_info("CTRL_ENTER_CALLBACK_CALLING");
      should_close = ctx->prompt_callbacks.ctrl_enter(state);
      log_info("CTRL_ENTER_CALLBACK_RETURNED should_close=%d", should_close);
    } else if (ctx->prompt_callbacks.enter) {
      log_info("ENTER_FALLBACK_CALLBACK_CALLING");
      should_close = ctx->prompt_callbacks.enter(state);
      log_info("ENTER_FALLBACK_CALLBACK_RETURNED should_close=%d",
               should_close);
    } else {
      log_info("CTRL_ENTER_CALLBACK_IS_NULL");
    }
  } else {
    should_close = true;

    log_info("ENTER_BRANCH: enter=%p sel='%s'",
             (void *)ctx->prompt_callbacks.enter, sel ? sel : "(null)");

    if (ctx->prompt_callbacks.enter) {
      log_info("ENTER_CALLBACK_CALLING");

      should_close = ctx->prompt_callbacks.enter(state);

      log_info("ENTER_CALLBACK_RETURNED should_close=%d", should_close);
    } else {
      log_info("ENTER_CALLBACK_IS_NULL");
    }
  }
  ctx->window.selection_complete = should_close;
}

bool Prompt_HandleListClick(PromptState *state, bool ctrl, bool shift,
                            bool alt) {

  if (!state || !state->listbox)
    return false;
  int list_top = 0, item_height = 0, visible_count = 0;
  ListBox_GetGeometry(&list_top, &item_height, &visible_count);
  if (item_height <= 0 || visible_count <= 0)
    return false;
  ListBoxInfo *lb = state->listbox;
  int mx = 0, my = 0;
  if (!ListBox_ClientMousePos(&mx, &my))
    return false;
  int idx = ListBox_RowAtY(lb, list_top, item_height, lb->view_offset,
                           visible_count, my);
  if (idx < 0)
    return false;
  UI_SetSelection(lb, idx);
  Prompt_TriggerEnter(state, ctrl, shift, alt);
  return true;
}

bool Prompt_HandleKey(PromptState *state, const PopupKeyEvent *key) {
  if (!state || !key)
    return false;
  int vk = key->vk;
  bool ctrl = key->ctrl;
  bool shift = key->shift;
  bool alt = key->alt;

  FlintContext *ctx = FlintContext_Get();
  bool invoke_interceptor = (ctx->prompt_callbacks.render == NULL ||
                             ctx->window.editbox_placed);
  if (invoke_interceptor && Platform_InvokeKeyInterceptor(key)) {
    Platform_InvalidatePrompt();
    return true;
  }

  if (ctrl && (vk == 'A' || vk == 'a')) {
    int len = (int)strlen(state->editbox->filter);
    EditState_BeginSelection(0);
    EditState_UpdateSelection(len);
    state->editbox->cursor = len;
    Prompt_Refresh(state);
    return true;
  }
  if (ctrl && (vk == 'C' || vk == 'c')) {
    int ss = 0, se = 0;
    if (EditState_GetSelection(&ss, &se)) {
      if (se < ss) {
        int t = ss;
        ss = se;
        se = t;
      }
      int len = (int)strlen(state->editbox->filter);
      if (ss < 0)
        ss = 0;
      if (se > len)
        se = len;
      if (se > ss) {
        char buf[512];
        int n = se - ss;
        if (n >= (int)sizeof(buf))
          n = (int)sizeof(buf) - 1;
        memcpy(buf, state->editbox->filter + ss, n);
        buf[n] = '\0';
        Platform_CopyToClipboard(buf);
      }
    }
    return true;
  }
  if (false) {
    return true;
  }
  if (vk == VK_BACK) {
    if (Prompt_DeleteSelection(state)) {
      Prompt_Refresh(state);
      return true;
    }
    if (ctrl) {
      state->editbox->filter[0] = '\0';
      state->editbox->cursor = 0;
    } else {
      size_t fcur = (size_t)state->editbox->cursor;
      if (!EditBufferBackspacePair(state->editbox->filter, &fcur) && true) {
        EditBufferBackspace(state->editbox->filter, &fcur);
      }
      state->editbox->cursor = (int)fcur;
    }
    History_ResetCursor(ctx->window.current_feature);
    Prompt_Refresh(state);
    return true;
  }
  if (vk == VK_ESCAPE) {
    WindowManager *wm = WindowManager_Get();
    if (wm && wm->popup_count > 1) {
      WindowManager_PopPopup(wm);
      return true;
    }
    ctx->window.selection_complete = true;
    return true;
  }
  if (vk == VK_RIGHT) {
    EditState_EndSelection();
    CursorForward(&state->editbox->cursor, strlen(state->editbox->filter));
    return true;
  }
  if (vk == VK_LEFT) {
    EditState_EndSelection();
    CursorBack(&state->editbox->cursor, strlen(state->editbox->filter));
    return true;
  }
  if (vk == VK_HOME) {
    EditState_EndSelection();
    state->editbox->cursor = 0;
    return true;
  }
  if (vk == VK_END) {
    EditState_EndSelection();
    state->editbox->cursor = (int)strlen(state->editbox->filter);
    return true;
  }
  if (vk == VK_UP) {
    if (ctrl) {
      state->editbox->cursor = 0;
      return true;
    }
    if (state->listbox) {
      if (state->listbox->selected_index == -1) {
        char hist[512];
        const char *feat = ctx->window.current_feature;
        if (feat && History_CycleUp(feat, hist, sizeof(hist))) {
          strncpy(state->editbox->filter, hist, sizeof(state->editbox->filter) - 1);
          state->editbox->filter[sizeof(state->editbox->filter) - 1] = '\0';
          state->editbox->cursor = (int)strlen(state->editbox->filter);
          Prompt_Refresh(state);
          return true;
        }
      }
      UI_NavigateUp(state->listbox);
    }
    return true;
  }
  if (vk == VK_DOWN) {
    if (ctrl) {
      state->editbox->cursor = (int)strlen(state->editbox->filter);
      return true;
    }
    if (state->listbox)
      UI_NavigateDown(state->listbox);
    return true;
  }
  if (vk == VK_DELETE) {
    if (Prompt_DeleteSelection(state)) {
      Prompt_Refresh(state);
      return true;
    }
    if (EditBufferDeleteChar(state->editbox->filter, &state->editbox->cursor)) {
      History_ResetCursor(ctx->window.current_feature);
      Prompt_Refresh(state);
    }
    return true;
  }
  if (vk == VK_TAB) {
    if (ctx->prompt_callbacks.tab)
      ctx->prompt_callbacks.tab(state);
    return true;
  }
  if (vk == VK_RETURN) {
    Prompt_TriggerEnter(state, ctrl, shift, alt);
    return true;
  }
  if (vk == COPY_SELECTION_KEY && !true) {
    if (shift || ctrl) {
      int type = shift ? 0 : 1;
      int idx = -1;
      if (state->listbox && state->listbox->selected_index >= 0 &&
          state->listbox->selected_index < state->listbox->item_count) {
        idx = state->listbox->matching_indices[state->listbox->selected_index];
      } else {
        idx = UI_LookupTrueIndex(ctx->window.output_buffer);
      }
      Platform_CopyToClipboard(ctx->window.output_buffer);
    }
    return true;
  }
  if (vk == HELP_SCREEN_KEY && ctrl) {
    if (ctx->prompt_callbacks.help)
      ctx->prompt_callbacks.help();
    return true;
  }
  if (vk == 'V' && ctrl) {
    char *clip = Platform_GetClipboardEntry();
    if (!clip) {
      return true;
    }
    clip[strcspn(clip, "\r\n")] = '\0';
    if (clip[0] == '\0') {
      free(clip);
      return true;
    }
    size_t cur_len = strlen(state->editbox->filter);
    size_t clip_len = strlen(clip);
    size_t avail = sizeof(state->editbox->filter) - cur_len - 1;
    if (clip_len > avail)
      clip_len = avail;
    if (clip_len > 0) {
      memmove(state->editbox->filter + state->editbox->cursor + clip_len,
              state->editbox->filter + state->editbox->cursor, cur_len - state->editbox->cursor + 1);
      memcpy(state->editbox->filter + state->editbox->cursor, clip, clip_len);
      state->editbox->cursor += (int)clip_len;
    }
    free(clip);
    Prompt_Refresh(state);
    return true;
  }
  if (ctrl) {
    if (vk == VK_OEM_MINUS) {
      Platform_AdjustFontSize(-1);
      Config_Save();
      return true;
    }
    if (vk == VK_OEM_PLUS) {
      Platform_AdjustFontSize(1);
      Config_Save();
      return true;
    }
  }
  return false;
}
