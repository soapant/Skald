// bindings.h
#ifndef BINDINGS_H
#define BINDINGS_H

#include <stdbool.h>
#include "bindings/vim_motions.h"

bool CheckVimMotion(int pressed_vk, int modifiers, MotionAction* out_action, const char** out_sequence, int* out_typed_count);
void RemoveSequenceFromEdit(int typed_count);
void ExecuteMotion(MotionAction action, const char* sequence, int typed_count);
bool CheckAppHotkeyBindings(int pressed_vk, int modifiers);
void RunMiscApplication(const char* path);
bool CheckMiscHotkeyBindings(int pressed_vk, int modifiers);
void open_browser(const char *url);
bool CheckQuickLinkHotkeyBindings(int pressed_vk, int modifiers);
bool CheckMacroBindings(int pressed_vk, int modifiers);
bool CheckTextSnippetBindings(int pressed_vk, int modifiers);

#endif
