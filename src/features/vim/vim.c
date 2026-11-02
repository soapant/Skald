#include "core/flapi.h"
#include "core/types/keys.h"

typedef enum { VIM_MODE_INSERT, VIM_MODE_NORMAL } VimMode;

static VimMode g_vim_mode = VIM_MODE_INSERT;

static bool vim_key_interceptor(const PopupKeyEvent *key) {
  if (!key)
    return false;
  if (key->vk == VK_ESCAPE) {
    if (g_vim_mode == VIM_MODE_INSERT) {
      g_vim_mode = VIM_MODE_NORMAL;
      fl_get_metrics()->edit_cursor_width = 12;
      fl_invalidate_prompt();
      return true;
    }
    return false;
  }
  if (g_vim_mode == VIM_MODE_NORMAL) {
    g_vim_mode = VIM_MODE_INSERT;
    fl_get_metrics()->edit_cursor_width = 0;
    fl_invalidate_prompt();
  }
  return false;
}

FLINT_FEATURE_INIT {
  fl_get_metrics()->edit_cursor_width = 0;
  fl_register_key_interceptor(vim_key_interceptor);
}
