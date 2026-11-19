#include "core/edit_state.h"
#include "core/prompt_keys.h"
#include "helpers/helpers_key.h"
#include "helpers/helpers_text.h"

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
#include "config_values.h"
#include "platform/draw.h"
#include "core/prompt_geometry.h"
#include "helpers/helpers_edit_buffer.h"
#include "platform/platform.h"
#include "helpers/helpers_ui.h"
#include "core/types/keys.h"

void prompt_refresh(PromptState *state) {
  if (!state) {
    return;
  }
  SkaldContext *ctx = SkaldContext_Get();
  if (ctx && ctx->window.prompt_callbacks.refresh) {
    ctx->window.prompt_callbacks.refresh(state);
  }
}

static bool prompt_keys_selection(const PromptState *state, int *out_ss,
                                  int *out_se) {
  int ss = 0, se = 0;
  if (!edit_state_get_selection(&ss, &se))
    return false;
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
  *out_ss = ss;
  *out_se = se;
  return true;
}

bool prompt_delete_selection(PromptState *state) {
  if (!state)
    return false;
  int ss = 0, se = 0;
  int len = (int)strlen(state->eb->filter);
  if (!prompt_keys_selection(state, &ss, &se))
    return false;
  if (se > ss) {
    memmove(state->eb->filter + ss, state->eb->filter + se, len - se + 1);
    state->eb->cursor = ss;
    edit_state_end_selection();
    listbox_focus_by_mouse();
    return true;
  }
  edit_state_end_selection();
  listbox_focus_by_mouse();
  return false;
}

void prompt_insert_text(PromptState *state, const char *text, int len) {
  if (!state || !text || len <= 0) {
    return;
  }
  prompt_delete_selection(state);
  size_t fcur = (size_t)state->eb->cursor;
  edit_buffer_insert_text(state->eb->filter, sizeof(state->eb->filter), &fcur,
                          text, (size_t)len);
  state->eb->cursor = (int)fcur;
  listbox_focus_by_mouse();
  prompt_refresh(state);
}

typedef bool (*PromptEnterFn)(PromptState *state);

static PromptEnterFn prompt_enter_callback_for(const PromptCallbacks *cb,
                                               bool ctrl, bool shift, bool alt,
                                               bool *out_combo) {
  *out_combo = true;
  if (ctrl && shift)
    return cb->ctrl_shift_enter ? cb->ctrl_shift_enter : cb->enter;
  if (ctrl && alt)
    return cb->ctrl_alt_enter ? cb->ctrl_alt_enter : cb->enter;
  if (shift && alt)
    return cb->shift_alt_enter ? cb->shift_alt_enter : cb->enter;
  if (shift)
    return cb->shift_enter ? cb->shift_enter : cb->enter;
  if (ctrl)
    return cb->ctrl_enter ? cb->ctrl_enter : cb->enter;
  *out_combo = false;
  return cb->enter;
}

void prompt_trigger_enter(PromptState *state, bool ctrl, bool shift, bool alt) {
  SkaldContext *ctx = SkaldContext_Get();
  if (!ctx)
    return;
  ctx->window.subwindow.in_enter = true;
  const char *sel = NULL;
  char filter_copy[sizeof(state->eb->filter)];
  if (state->lb && state->lb->selected_index != -1) {
    sel = listbox_info_row(state->lb, state->lb->selected_index);
  } else {
    safe_copy(filter_copy, sizeof(filter_copy), state->eb->filter);
    sel = filter_copy;
  }
  bool combo = false;
  PromptEnterFn fn = prompt_enter_callback_for(&ctx->window.prompt_callbacks,
                                               ctrl, shift, alt, &combo);
  bool should_close = !combo;
  if (fn)
    should_close = fn(state);
  if (should_close) {
    safe_copy(ctx->window.output_buffer, sizeof(ctx->window.output_buffer),
              sel);
  }
  ctx->window.selection_complete = should_close;
  ctx->window.subwindow.in_enter = false;
  prompt_process_pending_subwindow();
}

bool prompt_handle_list_click(PromptState *state, bool ctrl, bool shift,
                              bool alt) {

  if (!state || !state->lb)
    return false;
  int list_top = 0, item_height = 0, visible_count = 0;
  listbox_get_geometry(&list_top, &item_height, &visible_count);
  if (item_height <= 0 || visible_count <= 0)
    return false;
  ListBox *lb = state->lb;
  int mx = 0, my = 0;
  if (!listbox_client_mouse_pos(&mx, &my))
    return false;
  int idx = listbox_row_at_y(lb, list_top, item_height, lb->view_offset,
                             visible_count, my);
  if (idx < 0)
    return false;
  UI_SetSelection(lb, idx);
  prompt_trigger_enter(state, ctrl, shift, alt);
  return true;
}

bool prompt_handle_key(PromptState *state, const PopupKeyEvent *key) {
  if (!state || !key)
    return false;
  int vk = key->vk;
  bool ctrl = key->ctrl;
  bool shift = key->shift;
  bool alt = key->alt;

  SkaldContext *ctx = SkaldContext_Get();
  if (!ctx)
    return false;

  if (ctx->window.editbox_placed && key_invoke_interceptor(key)) {
    Platform_InvalidatePrompt();
    return true;
  }

  if (ctrl && (vk == 'A' || vk == 'a')) {
    int len = (int)strlen(state->eb->filter);
    edit_state_begin_selection(0);
    edit_state_update_selection(len);
    state->eb->cursor = len;
    prompt_refresh(state);
    return true;
  }
  if (ctrl && (vk == 'C' || vk == 'c')) {
    int ss = 0, se = 0;
    if (prompt_keys_selection(state, &ss, &se)) {
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
    if (prompt_delete_selection(state)) {
      prompt_refresh(state);
      return true;
    }
    listbox_focus_by_mouse();
    if (ctrl) {
      state->eb->filter[0] = '\0';
      state->eb->cursor = 0;
    } else {
      size_t fcur = (size_t)state->eb->cursor;
      if (!edit_buffer_backspace_pair(state->eb->filter, &fcur))
        edit_buffer_backspace(state->eb->filter, &fcur);
      state->eb->cursor = (int)fcur;
    }
    listbox_focus_by_mouse();
    prompt_refresh(state);
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
    if (state->lb && state->lb->item_count > 0 &&
        state->lb->selected_index >= 0) {
      int idx = state->lb->selected_index;
      const char *text = listbox_info_row(state->lb, idx);
      if (text && text[0]) {
        int tw = Platform_MeasureTextWidth((MeasureTextWidthSpec){
            .text = text, .len = (int)strlen(text), .font_size = FontSize});
        int avail = state->width > 2 * PROMPT_ITEM_TEXT_X
                        ? state->width - 2 * PROMPT_ITEM_TEXT_X
                        : state->width;
        int max_scroll = tw > avail ? tw - avail : 0;
        if (max_scroll > 0) {
          state->lb->text_hscroll += 24;
          if (state->lb->text_hscroll > max_scroll)
            state->lb->text_hscroll = max_scroll;
          prompt_refresh(state);
          return true;
        }
      }
    }
    edit_state_end_selection();
    cursor_forward(&state->eb->cursor, strlen(state->eb->filter));
    return true;
  }
  if (vk == VK_LEFT) {
    if (state->lb && state->lb->item_count > 0 && state->lb->text_hscroll > 0) {
      state->lb->text_hscroll -= 24;
      if (state->lb->text_hscroll < 0)
        state->lb->text_hscroll = 0;
      prompt_refresh(state);
      return true;
    }
    edit_state_end_selection();
    cursor_back(&state->eb->cursor);
    return true;
  }
  if (vk == VK_HOME) {
    edit_state_end_selection();
    state->eb->cursor = 0;
    return true;
  }
  if (vk == VK_END) {
    edit_state_end_selection();
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
    if (prompt_delete_selection(state)) {
      prompt_refresh(state);
      return true;
    }
    listbox_focus_by_mouse();
    if (edit_buffer_delete_char(state->eb->filter, &state->eb->cursor)) {
      prompt_refresh(state);
    }
    return true;
  }
  if (vk == VK_TAB) {
    if (ctx->window.prompt_callbacks.tab)
      ctx->window.prompt_callbacks.tab(state);
    return true;
  }
  if (vk == VK_RETURN) {
    prompt_trigger_enter(state, ctrl, shift, alt);
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
    prompt_delete_selection(state);
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
    prompt_refresh(state);
    return true;
  }
  if (ctrl) {
    if (vk == VK_OEM_MINUS) {
      FontSize -= 1;
      Platform_ResetUIScale();
      config_save();
      return true;
    }
    if (vk == VK_OEM_PLUS) {
      FontSize += 1;
      Platform_ResetUIScale();
      config_save();
      return true;
    }
  }
  return false;
}
