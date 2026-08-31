// bindings.h
#ifndef BINDINGS_H
#define BINDINGS_H

#include <stdbool.h>
#include "bindings/vim_motions.h"

bool CheckVimMotion(int pressed_vk, int modifiers, MotionAction* out_action, const char** out_sequence, int* out_typed_count);
void ExecuteMotion(MotionAction action, const char* sequence, int typed_count);
// Per-feature Check* bindings moved to feature headers (using flapi helpers):
// CheckAppHotkeyBindings -> features/app_launcher.h
// CheckMiscHotkeyBindings / RunMiscApplication -> features/misc_hotkeys.h
// CheckQuickLinkHotkeyBindings / open_browser -> features/quicklink.h
// CheckTextSnippetBindings -> features/text_snippets.h
// CheckTerminalCommandHotkeyBindings -> features/terminal_command.h
// CheckMacroBindings -> bindings/macro_bindings.h

#endif
