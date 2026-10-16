#ifndef fl_window_CONTROLLER_H
#define fl_window_CONTROLLER_H

#include <stdbool.h>

bool PromptController_ShouldSwallowPending(int wParam, int mods);
bool PromptController_TryVimMotion(int wParam, bool ctrl, bool shift, bool alt);
bool PromptController_PollSequenceTimeout(void);

#endif