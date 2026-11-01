
#ifndef BINDINGS_H
#define BINDINGS_H

#include <stdbool.h>
#include "bindings/vim_motions.h"
#include "bindings/vim_bindings.h"

typedef struct {
  EditMotionHandler handler;
  const char *sequence;
  int typed_count;
} EditMotion;

bool CheckEditMotion(int pressed_vk, int modifiers, EditMotion *out);
void ExecuteMotion(const EditMotion *motion);

#endif
