#ifndef PROMPT_CONTROLLER_H
#define PROMPT_CONTROLLER_H

#include <stdbool.h>

// Helpers for prompt input handling - extracted from WindowManager to keep WM focused on popup stack
bool PromptController_ShouldSwallowPending(int wParam, int mods);
bool PromptController_TryVimMotion(int wParam, bool ctrl, bool shift, bool alt);

#endif
