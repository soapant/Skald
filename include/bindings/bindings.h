
#ifndef BINDINGS_H
#define BINDINGS_H

#include <stdbool.h>
#include "bindings/vim_motions.h"

bool CheckVimMotion(int pressed_vk, int modifiers, MotionAction* out_action, const char** out_sequence, int* out_typed_count);
void ExecuteMotion(MotionAction action, const char* sequence, int typed_count);

#endif