
#ifndef BINDINGS_H
#define BINDINGS_H

#include <stdbool.h>
#include "bindings/vim_motions.h"
#include "bindings/vim_bindings.h"

typedef struct {
    VimMotionHandler handler;
    const char* sequence;
    int typed_count;
} VimMotion;

bool CheckVimMotion(int pressed_vk, int modifiers, VimMotion* out);
void ExecuteMotion(const VimMotion* motion);

#endif
