#include "core/edit_popup_keys.h"

#include <stdlib.h>
#include <string.h>

#include "core/context.h"
#include "core/modal.h"
#include "core/prompt_controller.h"
#include "bindings/vim_state.h"
#include "config.h"
#include "platform/input.h"
#include "platform/platform.h"
#include "config_values.h"
#include "core/types/keys.h"

bool EditPopup_DeleteSelection(WMEditPopupData *ed) {
  if (!ed || !ed->text)
    return false;
  int ss = 0, se = 0;
  if (!VimState_GetSelection(&ss, &se)) {
    return false;
  }
  if (se < ss) {
    int t = ss;
    ss = se;
    se = t;
  }
  size_t n = strlen(ed->text);
  if (ss < 0)
    ss = 0;
  if ((size_t)se > n)
    se = (int)n;
  if (se > ss) {
    memmove(ed->text + ss, ed->text + se, n - se + 1);
    ed->cursor = ss;
    VimState_EndSelection();
    return true;
  }
  VimState_EndSelection();
  return false;
}

void EditPopup_InsertText(WMEditPopupData *ed, const char *text, int len) {
  if (!ed || !ed->text || !text || len <= 0) {
    return;
  }
  EditPopup_DeleteSelection(ed);
  size_t cap = ed->text_capacity > 0 ? (size_t)ed->text_capacity : 1024;
  size_t ecur = (size_t)(ed->cursor < 0 ? 0 : ed->cursor);
  EditBufferInsertText(ed->text, cap, &ecur, text, (size_t)len);
  ed->cursor = (int)ecur;
  size_t tlen = strlen(ed->text);
  if (tlen >= 2) {
    if ((ed->text[tlen - 2] == 'j' || ed->text[tlen - 2] == 'J') &&
        (ed->text[tlen - 1] == 'q' || ed->text[tlen - 1] == 'Q')) {
      ed->text[tlen - 2] = '\0';
      ed->cursor -= 2;
      if (ed->cursor < 0)
        ed->cursor = 0;
      ed->confirmed = false;
      FlintContext_Get()->window.selection_complete = true;
    }
  }
}

bool EditPopup_SelectAll(WMEditPopupData *ed) {
  if (!ed || !ed->text)
    return false;
  int len = (int)strlen(ed->text);
  VimState_BeginSelection(0);
  VimState_UpdateSelection(len);
  ed->cursor = len;
  return true;
}

bool EditPopup_CopySelection(WMEditPopupData *ed) {
  if (!ed || !ed->text)
    return false;
  int ss = 0, se = 0;
  if (!VimState_GetSelection(&ss, &se))
    return false;
  if (se < ss) {
    int t = ss;
    ss = se;
    se = t;
  }
  int len = (int)strlen(ed->text);
  if (ss < 0)
    ss = 0;
  if (se > len)
    se = len;
  if (se <= ss)
    return false;
  char buf[1024];
  int n = se - ss;
  if (n >= (int)sizeof(buf))
    n = (int)sizeof(buf) - 1;
  memcpy(buf, ed->text + ss, n);
  buf[n] = '\0';
  Platform_CopyToClipboard(buf);
  return true;
}

bool EditPopup_HandleKey(WMEditPopupData *ed, const PopupKeyEvent *key) {
  if (!ed || !ed->text || !key)
    return false;
  int vk = key->vk;
  bool ctrl = key->ctrl;

  if (PromptController_TryVimMotion(vk, ctrl, key->shift, key->alt)) {
    return true;
  }

  size_t cap = ed->text_capacity > 0 ? (size_t)ed->text_capacity : 1024;
  size_t n = strlen(ed->text);
  if (ed->cursor < 0)
    ed->cursor = 0;
  if ((size_t)ed->cursor > n)
    ed->cursor = (int)n;

  if (vk == VK_BACK) {
    if (EditPopup_DeleteSelection(ed)) {
      return true;
    }
    if (ctrl) {
      ed->text[0] = '\0';
      ed->cursor = 0;
    } else if (ed->cursor > 0) {
      size_t ecur = (size_t)ed->cursor;
      if (!EditBufferBackspacePair(ed->text, &ecur)) {
        size_t del = ecur - 1;
        memmove(ed->text + del, ed->text + del + 1, n - del);
        ecur--;
      }
      ed->cursor = (int)ecur;
    }
    return true;
  }
  if (vk == VK_ESCAPE) {
    if (EnableVimMode && IsInsertMode()) {
      SetMode(MODE_NORMAL);
    } else {
      FlintContext_Get()->window.selection_complete = true;
    }
    return true;
  }
  if (vk == VK_LEFT) {
    VimState_EndSelection();
    if (ed->cursor > 0)
      ed->cursor--;
    return true;
  }
  if (vk == VK_RIGHT) {
    VimState_EndSelection();
    if ((size_t)ed->cursor < n)
      ed->cursor++;
    return true;
  }
  if (vk == VK_HOME) {
    VimState_EndSelection();
    ed->cursor = 0;
    return true;
  }
  if (vk == VK_END) {
    VimState_EndSelection();
    ed->cursor = (int)n;
    return true;
  }
  if (vk == VK_DELETE) {
    if (EditPopup_DeleteSelection(ed)) {
      return true;
    }
    if ((size_t)ed->cursor < n) {
      memmove(ed->text + ed->cursor, ed->text + ed->cursor + 1,
              n - (size_t)ed->cursor);
    }
    return true;
  }
  if (vk == VK_RETURN) {
    if (ed->on_confirm)
      ed->on_confirm(ed->text, ed->user_data);
    ed->confirmed = true;
    FlintContext_Get()->window.selection_complete = true;
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
    size_t clip_len = strlen(clip);
    size_t avail = cap > n + 1 ? cap - n - 1 : 0;
    if (clip_len > avail)
      clip_len = avail;
    if (clip_len > 0) {
      memmove(ed->text + ed->cursor + clip_len, ed->text + ed->cursor,
              n - (size_t)ed->cursor + 1);
      memcpy(ed->text + ed->cursor, clip, clip_len);
      ed->cursor += (int)clip_len;
    }
    free(clip);
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
