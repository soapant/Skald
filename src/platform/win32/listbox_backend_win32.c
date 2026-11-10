#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "platform/platform.h"
#include "platform/draw.h"
#include "helpers/win32/common_ui_win32.h"
#include "helpers/win32/win32_state.h"
#include "core/prompt_geometry.h"
#include "core/types.h"
#include "core/types/window.h"
#include "core/window_manager.h"
#include "core/draw_state.h"
#include "helpers/helpers_ui.h"
#include "config_values.h"

static __thread HDC b_hdc = NULL;

bool Platform_ListboxBackendBegin(int item_height_override, ListboxBackendInit *out) {
  (void)item_height_override;
  win32_get_draw_context(&b_hdc);
  if (!b_hdc)
    return false;
  out->item_h = PromptRowHeight();
  out->list_top = PromptListTop();
  PlDrawState_Get()->win32_listbox_surface = true;
  return true;
}

void Platform_ListboxBackendDrawSelection(int bx, int bw, int y, int item_h, int fw,
                                  int frame_h, bool sel) {
  (void)frame_h;
  (void)fw;
  if (RoundedCorners && CornerRadius > 0) {
    int R = PROMPT_SEL_RADIUS;
    if (R > CornerRadius)
      R = CornerRadius;
    int t = PROMPT_SEL_INSET;
    RECT inner = {bx + t, y + PROMPT_SEL_TOP_INSET + t, bx + bw - t,
                  y + item_h - 1 - t};
    int innerR = R - 1;
    if (innerR < 0)
      innerR = 0;
    if (inner.right <= inner.left || inner.bottom <= inner.top) {
      inner = (RECT){bx, y, bx + bw, y + item_h};
      innerR = R;
    }
    if (sel)
      win32_fill_rounded_rect(b_hdc, &inner, COLOR_SELECTED_TEXT, innerR);
  } else if (sel) {
    RECT rc = {bx, y, bx + bw, y + item_h};
    HBRUSH br = CreateSolidBrush(COLOR_SELECTED_TEXT);
    FillRect(b_hdc, &rc, br);
    DeleteObject(br);
  }
}

void Platform_ListboxBackendEnd(void) {
  PlDrawState_Get()->win32_listbox_surface = false;
  b_hdc = NULL;
}

PromptState *Platform_ListboxBackendPromptState(const ListBoxInfo *lb) {
  PromptState *ps = PromptPopup_FindState(lb);
  if (ps)
    return ps;
  return PromptPopup_GetTopPromptState();
}
