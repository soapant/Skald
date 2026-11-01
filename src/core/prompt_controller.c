#include "core/prompt_controller.h"

#include <stddef.h>

#include "core/context.h"
#include "core/vim.h"
#include "core/window_manager.h"
#include "deps/log.h"
#include "bindings/bindings.h"
#include "core/types.h"
#include "core/types/keys.h"

extern int pending_hotkey_vk;
extern int pending_hotkey_mod;
extern long pending_hotkey_timeMs;

bool PromptController_TryVimMotion(int wParam, bool ctrl, bool shift,
                                   bool alt) {
  int mod = HOTKEY_MOD_NONE;
  if (ctrl)
    mod |= HOTKEY_MOD_CTRL;
  if (shift)
    mod |= HOTKEY_MOD_SHIFT;
  if (alt)
    mod |= HOTKEY_MOD_ALT;
  VimMotion motion;
  if (CheckVimMotion(wParam, mod, &motion)) {
    ExecuteMotion(&motion);
    return true;
  }
  return false;
}

bool PromptController_PollSequenceTimeout(void) {
  if (!SequenceHasPendingWait())
    return false;
  if (!SequenceConsumeTimeout())
    return false;
  log_info("PollSequenceTimeout firing refresh");
  FlintContext *ctx = FlintContext_Get();
  if (!ctx || !ctx->prompt_callbacks.refresh)
    return false;
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (top && top->type == POPUP_PROMPT && top->user_data) {
    PromptState *s = (PromptState *)top->user_data;
    ctx->prompt_callbacks.refresh(s);
    return true;
  }
  return false;
}
