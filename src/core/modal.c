#include <stdbool.h>
#include <string.h>

#include "core/context.h"
#include "core/modal.h"
#include "platform/ui.h"
#include "core/window_manager.h"
#include "core/vim.h"
#include "helpers/helpers_ui.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/color.h"
#include "core/types/window.h"

static const char *g_mode_names[] = {"Normal", "Insert", "Visual"};
int last_index = 0;

static EditorMode ActiveMode(void) {
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  if (top && top->type == POPUP_EDIT) {
    WMEditPopupData *e = (WMEditPopupData *)top;
    return e->mode;
  }
  return FlintContext_Get()->vim_mode;
}

static void clamp_cursor_for_mode(EditorMode mode) {
  if (mode != MODE_NORMAL)
    return;
  FlintContext *ctx = FlintContext_Get();
  if (!ctx)
    return;
  char text[1024];
  Vim_GetText(ctx, text, sizeof(text));
  int len = (int)strlen(text);
  if (len <= 0)
    return;
  if (Vim_GetCursor(ctx) > len - 1)
    Vim_SetCursor(ctx, len - 1);
}

