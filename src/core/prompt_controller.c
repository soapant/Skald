#include "core/prompt_controller.h"

#include <stddef.h>

#include "core/context.h"
#include "core/window_manager.h"
#include "deps/log.h"
#include "core/types.h"
#include "core/types/keys.h"

bool PromptController_TryVimMotion(int wParam, bool ctrl, bool shift,
                                    bool alt) {
  (void)wParam;
  (void)ctrl;
  (void)shift;
  (void)alt;
  return false;
}

bool PromptController_PollSequenceTimeout(void) {
  return false;
}
