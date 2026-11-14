#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "helpers/helpers_ui.h"
#include "core/listbox_hover.h"
#include "core/types.h"
#include "core/types/prompt.h"

void UI_SetSelection(ListBoxInfo *lb, int index) {
  if (!lb)
    return;
  if (lb->selected_index == index)
    return;
  lb->text_hscroll = 0;
  lb->selected_index = index;
  UI_RequestPopupRepaint();
}

static int lb_update_depth = 0;
static int lb_visible_count = 0;

void UI_LB_BeginUpdate(void) { lb_update_depth++; }

void UI_LB_EndUpdate(void) {
  if (lb_update_depth > 0)
    lb_update_depth--;
}

void UI_LB_SetCount(ListBoxInfo *lb, int count) {
  if (lb)
    lb->item_count = count;
  UI_RequestPopupRepaint();
}

int UI_LB_GetViewTop(const ListBoxInfo *lb) { return lb ? lb->view_offset : 0; }

int UI_LB_GetVisibleCount(void) { return lb_visible_count; }

void UI_LB_SetVisibleCount(int count) { lb_visible_count = count; }

void UI_LB_ScrollToTop(ListBoxInfo *lb, int top_index) {
  if (!lb)
    return;
  int visible = UI_LB_GetVisibleCount();
  if (top_index < 0)
    top_index = 0;
  if (visible > 0 && top_index + visible > lb->item_count)
    top_index = lb->item_count - visible;
  if (top_index < 0)
    top_index = 0;
  lb->view_offset = top_index;
  UI_RequestPopupRepaint();
}

void UI_NavigateTo(ListBoxInfo *lb, int index) {
  if (!lb)
    return;
  int count = lb->item_count;
  if (count <= 0) {
    lb->selected_index = -1;
    lb->text_hscroll = 0;
    UI_RequestPopupRepaint();
    return;
  }
  if (index >= count)
    index = count - 1;
  if (index == -1) {
    lb->selected_index = -1;
    lb->text_hscroll = 0;
    UI_RequestPopupRepaint();
    return;
  }
  int old_sel = lb->selected_index;
  if (old_sel == index)
    return;
  lb->text_hscroll = 0;
  lb->selected_index = index;
  lb->selected_text = listbox_info_row(lb, index);
  listbox_scroll_to_selection(lb);
  UI_RequestPopupRepaint();
}

void UI_NavigateUp(ListBoxInfo *lb) {
  listbox_navigate_up(lb);
  UI_RequestPopupRepaint();
}

void UI_NavigateDown(ListBoxInfo *lb) {
  listbox_navigate_down(lb);
  UI_RequestPopupRepaint();
}
