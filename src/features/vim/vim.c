#include "core/flapi.h"
#include <stdio.h>

static bool vim_key_interceptor(int vk, bool ctrl, bool shift, bool alt) {
  if (vk == 'K' || vk == 'k') {
    printf("vim: swallowed K\n");
    return true;
  }
  return false;
}

FLINT_FEATURE_INIT {
  fl_register_key_interceptor(vim_key_interceptor);
}
