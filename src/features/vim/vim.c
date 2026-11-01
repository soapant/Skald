#include "core/flapi.h"
#include "core/types/keys.h"

typedef enum { VIM_MODE_INSERT, VIM_MODE_NORMAL } VimMode;

static VimMode g_vim_mode = VIM_MODE_INSERT;

static bool vim_key_interceptor(const PopupKeyEvent *key) {
  if (!key)
    return false;
  if (key->vk == FL_KEY_ESCAPE) {
    if (g_vim_mode == VIM_MODE_INSERT) {
      g_vim_mode = VIM_MODE_NORMAL;
      return true;
    }
    return false;
  }
  return false;
}

FLINT_FEATURE_INIT {
  fl_register_key_interceptor(vim_key_interceptor);
}
