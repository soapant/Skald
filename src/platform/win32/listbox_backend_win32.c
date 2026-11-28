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

bool Platform_ListboxBackendBegin(int item_height_override,
                                  ListboxBackendInit *out) {
  (void)item_height_override;
  win32_get_draw_context(&b_hdc);
  if (!b_hdc)
    return false;
  out->item_h = prompt_row_height();
  out->list_top = prompt_list_top();
  draw_state_get()->win32_listbox_surface = true;
  return true;
}

void Platform_ListboxBackendDrawSelection(ListboxBackendDrawSelection *argspec) {
  (void)argspec->f_frame_h;
  (void)argspec->f_fw;
  if (RoundedCorners && CornerRadius > 0) {
    int R = PROMPT_SEL_RADIUS;
    if (R > CornerRadius)
      R = CornerRadius;
    int t = PROMPT_SEL_INSET;
    RECT inner = {argspec->f_bx + t, argspec->f_y + PROMPT_SEL_TOP_INSET + t, argspec->f_bx + argspec->f_bw - t,
                  argspec->f_y + argspec->f_item_h - 1 - t};
    int innerR = R - 1;
    if (innerR < 0)
      innerR = 0;
    if (inner.right <= inner.left || inner.bottom <= inner.top) {
      inner = (RECT){argspec->f_bx, argspec->f_y, argspec->f_bx + argspec->f_bw, argspec->f_y + argspec->f_item_h};
      innerR = R;
    }
    if (argspec->f_sel)
      win32_fill_rounded_rect(b_hdc, &inner, COLOR_SELECTED_TEXT, innerR);
  } else if (argspec->f_sel) {
    RECT rc = {argspec->f_bx, argspec->f_y, argspec->f_bx + argspec->f_bw, argspec->f_y + argspec->f_item_h};
    HBRUSH br = CreateSolidBrush(COLOR_SELECTED_TEXT);
    FillRect(b_hdc, &rc, br);
    DeleteObject(br);
  }
}

void Platform_ListboxBackendEnd(void) {
  draw_state_get()->win32_listbox_surface = false;
  b_hdc = NULL;
}

PromptState *Platform_ListboxBackendPromptState(const ListBox *lb) {
  PromptState *ps = prompt_popup_find_state(lb);
  if (ps)
    return ps;
  return prompt_popup_get_top_prompt_state();
}
