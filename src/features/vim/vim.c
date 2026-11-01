#include "core/flapi.h"
#include <stdio.h>

static bool vim_key_interceptor(const PopupKeyEvent *key) {
  if (key && (key->vk == 'K' || key->vk == 'k')) {
    printf("vim: swallowed K\n");
    return true;
  }
  return false;
}

FLINT_FEATURE_INIT {
  fl_register_key_interceptor(vim_key_interceptor);
}
