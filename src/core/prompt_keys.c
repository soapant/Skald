#include "core/edit_state.h"
#include "core/prompt_keys.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "core/context.h"
#include "core/types/prompt.h"
#include "core/types/window.h"
#include "core/window_manager.h"
#include "platform/wm_platform.h"
#include "deps/log.h"
#include "core/listbox_hover.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "platform/ui.h"
#include "helpers/helpers_ui.h"
#include "config_values.h"
#include "core/types/keys.h"

void Prompt_Refresh(PromptState *state) {
  if (!state) {
    return;
  }
  FlintContext *ctx = FlintContext_Get();
  if (ctx && ctx->window.prompt_callbacks.refresh) {
    ctx->window.prompt_callbacks.refresh(state);
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
  int len = (int)strlen(state->eb->filter);
  if (ss < 0)
    ss = 0;
  if (se > len)
    se = len;
  if (se > ss) {
    memmove(state->eb->filter + ss, state->eb->filter + se, len - se + 1);
    state->eb->cursor = ss;
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
  size_t fcur = (size_t)state->eb->cursor;
  EditBufferInsertText(state->eb->filter, sizeof(state->eb->filter), &fcur,
                       text, (size_t)len);
  state->eb->cursor = (int)fcur;
  Prompt_Refresh(state);
}

void Prompt_TriggerEnter(PromptState *state, bool ctrl, bool shift, bool alt) {
  FlintContext *ctx = FlintContext_Get();
  if (ctx) ctx->window.subwindow.in_enter = true;
  const char *sel = NULL;
  char filter_copy[sizeof(state->eb->filter)];
  if (state->lb && UI_IsListBoxSelected(state->lb)) {
    sel = ListBoxInfo_Row(state->lb, state->lb->selected_index);
  } else {
    strncpy(filter_copy, state->eb->filter, sizeof(filter_copy) - 1);
    filter_copy[sizeof(filter_copy) - 1] = '\0';
    sel = filter_copy;
  }
  bool should_close = true;
  if (ctrl && shift) {
    should_close = false;
    if (ctx->window.prompt_callbacks.ctrl_shift_enter)
      should_close = ctx->window.prompt_callbacks.ctrl_shift_enter(state);
    else if (ctx->window.prompt_callbacks.enter)
      should_close = ctx->window.prompt_callbacks.enter(state);
  } else if (ctrl && alt) {
    should_close = false;
    if (ctx->window.prompt_callbacks.ctrl_alt_enter)
      should_close = ctx->window.prompt_callbacks.ctrl_alt_enter(state);
    else if (ctx->window.prompt_callbacks.enter)
      should_close = ctx->window.prompt_callbacks.enter(state);
  } else if (shift && alt) {
    should_close = false;
    if (ctx->window.prompt_callbacks.shift_alt_enter)
      should_close = ctx->window.prompt_callbacks.shift_alt_enter(state);
    else if (ctx->window.prompt_callbacks.enter)
      should_close = ctx->window.prompt_callbacks.enter(state);
  } else if (shift) {
    should_close = false;
    if (ctx->window.prompt_callbacks.shift_enter)
      should_close = ctx->window.prompt_callbacks.shift_enter(state);
    else if (ctx->window.prompt_callbacks.enter)
      should_close = ctx->window.prompt_callbacks.enter(state);
  } else if (ctrl) {
    should_close = false;
    log_info("CTRL_ENTER_BRANCH: ctrl_enter=%p sel='%s'",
             (void *)ctx->window.prompt_callbacks.ctrl_enter,
             sel ? sel : "(null)");
    if (ctx->window.prompt_callbacks.ctrl_enter) {
      log_info("CTRL_ENTER_CALLBACK_CALLING");
      should_close = ctx->window.prompt_callbacks.ctrl_enter(state);
      log_info("CTRL_ENTER_CALLBACK_RETURNED should_close=%d", should_close);
    } else if (ctx->window.prompt_callbacks.enter) {
      log_info("ENTER_FALLBACK_CALLBACK_CALLING");
      should_close = ctx->window.prompt_callbacks.enter(state);
      log_info("ENTER_FALLBACK_CALLBACK_RETURNED should_close=%d",
               should_close);
    } else {
      log_info("CTRL_ENTER_CALLBACK_IS_NULL");
    }
  } else {
    should_close = true;

    log_info("ENTER_BRANCH: enter=%p sel='%s'",
             (void *)ctx->window.prompt_callbacks.enter, sel ? sel : "(null)");

    if (ctx->window.prompt_callbacks.enter) {
      log_info("ENTER_CALLBACK_CALLING");

      should_close = ctx->window.prompt_callbacks.enter(state);

      log_info("ENTER_CALLBACK_RETURNED should_close=%d", should_close);
    } else {
      log_info("ENTER_CALLBACK_IS_NULL");
    }
  }
  if (should_close) {
    if (sel) {
      strncpy(ctx->window.output_buffer, sel,
              sizeof(ctx->window.output_buffer) - 1);
      ctx->window.output_buffer[sizeof(ctx->window.output_buffer) - 1] = '\0';
    } else {
      ctx->window.output_buffer[0] = '\0';
    }
  }
  ctx->window.selection_complete = should_close;
  if (ctx) ctx->window.subwindow.in_enter = false;
  Platform_ProcessPendingSubwindow();
}

bool Prompt_HandleListClick(PromptState *state, bool ctrl, bool shift,
                            bool alt) {

  if (!state || !state->lb)
    return false;
  int list_top = 0, item_height = 0, visible_count = 0;
  ListBox_GetGeometry(&list_top, &item_height, &visible_count);
  if (item_height <= 0 || visible_count <= 0)
    return false;
  ListBoxInfo *lb = state->lb;
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
  Popup *top = WindowManager_GetTopPopup(NULL);
  bool invoke_interceptor = false;
  invoke_interceptor = ctx->window.editbox_placed;

  if (invoke_interceptor && Platform_InvokeKeyInterceptor(key)) {
    Platform_InvalidatePrompt();
    return true;
  }

  if (ctrl && (vk == 'A' || vk == 'a')) {
    int len = (int)strlen(state->eb->filter);
    EditState_BeginSelection(0);
    EditState_UpdateSelection(len);
    state->eb->cursor = len;
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
      int len = (int)strlen(state->eb->filter);
      if (ss < 0)
        ss = 0;
      if (se > len)
        se = len;
      if (se > ss) {
        char buf[512];
        int n = se - ss;
        if (n >= (int)sizeof(buf))
          n = (int)sizeof(buf) - 1;
        memcpy(buf, state->eb->filter + ss, n);
        buf[n] = '\0';
        Platform_CopyToClipboard(buf);
      }
    }
    return true;
  }
  if (vk == VK_BACK) {
    if (Prompt_DeleteSelection(state)) {
      Prompt_Refresh(state);
      return true;
    }
    if (ctrl) {
      state->eb->filter[0] = '\0';
      state->eb->cursor = 0;
    } else {
      size_t fcur = (size_t)state->eb->cursor;
      if (!EditBufferBackspacePair(state->eb->filter, &fcur) && true) {
        EditBufferBackspace(state->eb->filter, &fcur);
      }
      state->eb->cursor = (int)fcur;
    }
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
    CursorForward(&state->eb->cursor, strlen(state->eb->filter));
    return true;
  }
  if (vk == VK_LEFT) {
    EditState_EndSelection();
    CursorBack(&state->eb->cursor, strlen(state->eb->filter));
    return true;
  }
  if (vk == VK_HOME) {
    EditState_EndSelection();
    state->eb->cursor = 0;
    return true;
  }
  if (vk == VK_END) {
    EditState_EndSelection();
    state->eb->cursor = (int)strlen(state->eb->filter);
    return true;
  }
  if (vk == VK_UP) {
    if (ctrl) {
      state->eb->cursor = 0;
      return true;
    }
    if (state->lb) {
      UI_NavigateUp(state->lb);
    }
    return true;
  }
  if (vk == VK_DOWN) {
    if (ctrl) {
      state->eb->cursor = (int)strlen(state->eb->filter);
      return true;
    }
    if (state->lb)
      UI_NavigateDown(state->lb);
    return true;
  }
  if (vk == VK_DELETE) {
    if (Prompt_DeleteSelection(state)) {
      Prompt_Refresh(state);
      return true;
    }
    if (EditBufferDeleteChar(state->eb->filter, &state->eb->cursor)) {
      Prompt_Refresh(state);
    }
    return true;
  }
  if (vk == VK_TAB) {
    if (ctx->window.prompt_callbacks.tab)
      ctx->window.prompt_callbacks.tab(state);
    return true;
  }
  if (vk == VK_RETURN) {
    Prompt_TriggerEnter(state, ctrl, shift, alt);
    return true;
  }
  if ((shift || ctrl) && vk == COPY_SELECTION_KEY) {
    Platform_CopyToClipboard(ctx->window.output_buffer);
    return true;
  }
  if (vk == HELP_SCREEN_KEY && ctrl) {
    if (ctx->window.prompt_callbacks.help)
      ctx->window.prompt_callbacks.help();
    return true;
  }
  if (vk == EDIT_TEXT_KEY && ctrl) {
    if (ctx->window.prompt_callbacks.edit)
      ctx->window.prompt_callbacks.edit(state);
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
    size_t cur_len = strlen(state->eb->filter);
    size_t clip_len = strlen(clip);
    size_t avail = sizeof(state->eb->filter) - cur_len - 1;
    if (clip_len > avail)
      clip_len = avail;
    if (clip_len > 0) {
      memmove(state->eb->filter + state->eb->cursor + clip_len,
              state->eb->filter + state->eb->cursor,
              cur_len - state->eb->cursor + 1);
      memcpy(state->eb->filter + state->eb->cursor, clip, clip_len);
      state->eb->cursor += (int)clip_len;
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
