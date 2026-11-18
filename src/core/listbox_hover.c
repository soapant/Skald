#include "core/listbox_hover.h"

#include <stdint.h>
#include <stdlib.h>

#include "core/context.h"
#include "config_values.h"
#include "core/timer.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "core/window_manager.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_input_state.h"
#include "platform/platform.h"

static bool mouse_valid = false;
static int mouse_x = 0;
static int mouse_y = 0;
static int pendinscroll = 0;
static bool focus_by_keyboard = false;
static bool mouse_moved = false;
static bool hover_pos_set = false;
static int hover_x = 0;
static int hover_y = 0;
static int list_top = 0;
static int item_height = 0;
static int visible_count = 0;
static int row_hscroll = 0;
static bool pub_set = false;
static int pub_x = 0;
static int pub_y = 0;
static int hover_throttle_ms = 0;
static int64_t last_hover_ticks = 0;

static bool listbox_hover_throttled(void) {
  if (hover_throttle_ms <= 0)
    return false;
  int64_t now = timer_now_ticks();
  int64_t interval = (int64_t)hover_throttle_ms * timer_tick_frequency() / 1000;
  if (last_hover_ticks != 0 && (now - last_hover_ticks) < interval) {
    return true;
  }
  last_hover_ticks = now;
  return false;
}

void listbox_set_row_h_scroll(int px) { row_hscroll = px; }
int listbox_get_row_h_scroll(void) { return row_hscroll; }

void listbox_focus_by_keyboard(void) {
  focus_by_keyboard = true;
  mouse_moved = false;
}

void listbox_focus_by_mouse(void) { focus_by_keyboard = false; }

void listbox_publish_mouse_pos(int x, int y) {
  if (pub_set && (pub_x != x || pub_y != y))
    mouse_moved = true;
  pub_x = x;
  pub_y = y;
  pub_set = true;
  mouse_x = x;
  mouse_y = y;
  mouse_valid = true;
}

void listbox_invalidate_mouse_pos(void) {
  mouse_valid = false;
  mouse_moved = false;
  hover_pos_set = false;
}

bool listbox_client_mouse_pos(int *out_x, int *out_y) {
  if (!mouse_valid) {
    if (!Platform_GetMousePos(&mouse_x, &mouse_y)) {
      mouse_valid = false;
      if (out_x)
        *out_x = 0;
      if (out_y)
        *out_y = 0;
      return false;
    }
    mouse_valid = true;
  }
  int mx = mouse_x;
  int my = mouse_y;
  int wx = 0, wy = 0, dx = 0, dy = 0;
  WindowManager *wm = WindowManager_Get();
  Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
  bool have_origin =
      Platform_WindowToRoot(SkaldContext_Get()->window.hPopupWnd, &wx, &wy);
  if (!have_origin && top)
    have_origin = Platform_WindowToRoot(top->handle, &wx, &wy);
  if (have_origin) {
    mx -= wx;
    my -= wy;
  }
  get_draw_offset(&dx, &dy);
  mx -= dx;
  my -= dy;
  int pw = 0, ph = 0;
  if (top) {
    if (top->type == POPUP_PROMPT && top->user_data) {
      const PromptState *s = (PromptState *)top->user_data;
      pw = s->width;
      ph = s->height;
    }
  }
  bool inside =
      (pw <= 0 || ph <= 0) || (mx >= 0 && my >= 0 && mx < pw && my < ph);
  if (out_x)
    *out_x = mx;
  if (out_y)
    *out_y = my;
  if (!inside) {
    return false;
  }
  return true;
}

int listbox_row_at_y(const ListBoxInfo *lb, int top_y, int row_h, int top_index,
                     int rows_visible, int pos_y) {

  if (!lb || row_h <= 0 || rows_visible <= 0) {
    return -1;
  }
  int rel = pos_y - top_y;
  if (rel < 0)
    return -1;
  int row = rel / row_h;
  if (row >= rows_visible)
    return -1;
  int idx = top_index + row;
  if (idx < 0 || idx >= lb->item_count)
    return -1;
  return idx;
}

bool listbox_hover_focus(ListBoxInfo *lb, int top_y, int row_h, int top_index,
                         int rows_visible, int left_x, int right_x) {

  if (!lb || lb->item_count <= 0)
    return false;
  if (!mouse_moved)
    return false;
  if (listbox_hover_throttled())
    return false;
  mouse_moved = false;
  int mx = 0, my = 0;
  if (!listbox_client_mouse_pos(&mx, &my)) {
    hover_pos_set = false;
    return false;
  }
  bool moved = !hover_pos_set || hover_x != mx || hover_y != my;
  hover_pos_set = true;
  hover_x = mx;
  hover_y = my;
  if (!moved)
    return false;
  if (mx < left_x || mx >= right_x)
    return false;
  listbox_focus_by_mouse();
  int idx = listbox_row_at_y(lb, top_y, row_h, top_index, rows_visible, my);
  if (idx < 0 || idx == lb->selected_index)
    return false;
  UI_SetSelection(lb, idx);
  return true;
}

void listbox_queue_scroll(int rows) { pendinscroll += rows; }

int listbox_take_scroll(void) {
  int rows = pendinscroll;
  pendinscroll = 0;
  return rows;
}

void listbox_scroll_by(ListBoxInfo *lb, int rows, int rows_visible) {
  if (!lb || rows == 0) {
    return;
  }
  int max_off = lb->item_count - rows_visible;
  if (max_off < 0)
    max_off = 0;
  int off = lb->view_offset + rows;
  if (off < 0)
    off = 0;
  if (off > max_off)
    off = max_off;
  int delta = off - lb->view_offset;
  if (delta == 0) {
    return;
  }
  int old_sel = lb->selected_index;
  lb->view_offset = off;
  if (!focus_by_keyboard && old_sel >= 0) {
    int sel = old_sel + delta;
    if (sel < 0)
      sel = 0;
    if (sel >= lb->item_count)
      sel = lb->item_count - 1;
    lb->selected_index = sel;
  }
  UI_RequestPopupRepaint();
}

void listbox_apply_pending_scroll(ListBoxInfo *lb, int rows_visible) {
  int rows = listbox_take_scroll();
  if (rows == 0)
    return;
  listbox_scroll_by(lb, rows, rows_visible);
}

int listbox_scroll_top_for(int index, int rows_visible, int count) {
  if (rows_visible <= 0)
    rows_visible = 16;
  int new_top = index - rows_visible / 2;
  if (new_top < 0)
    new_top = 0;
  if (new_top + rows_visible > count)
    new_top = count - rows_visible;
  if (new_top < 0)
    new_top = 0;
  return new_top;
}

void listbox_scroll_to_selection(ListBoxInfo *lb) {
  if (!lb || lb->selected_index < 0 || lb->item_count <= 0)
    return;
  int visible = UI_LB_GetVisibleCount();
  if (visible <= 0)
    visible = 16;
  int old_top = UI_LB_GetViewTop(lb);
  int new_top =
      listbox_scroll_top_for(lb->selected_index, visible, lb->item_count);
  if (new_top != old_top)
    UI_LB_ScrollToTop(lb, new_top);
}

void listbox_set_geometry(int top_y, int row_h, int rows_visible) {
  list_top = top_y;
  item_height = row_h;
  visible_count = rows_visible;
}

void listbox_get_geometry(int *out_list_top, int *out_item_height,
                          int *out_visible_count) {
  if (out_list_top)
    *out_list_top = list_top;
  if (out_item_height)
    *out_item_height = item_height;
  if (out_visible_count)
    *out_visible_count = visible_count;
}

void listbox_navigate_up(ListBoxInfo *lb) {
  if (!lb)
    return;
  listbox_focus_by_keyboard();
  int sel = lb->selected_index;
  if (sel >= 0)
    sel--;
  if (sel == lb->selected_index)
    return;
  UI_NavigateTo(lb, sel);
}

void listbox_navigate_down(ListBoxInfo *lb) {
  if (!lb)
    return;
  listbox_focus_by_keyboard();
  int count = lb->item_count;
  if (count <= 0)
    return;
  int sel = lb->selected_index;
  if (sel < 0)
    sel = -1;
  if (sel >= count - 1)
    sel = count - 1;
  else
    sel++;
  if (sel == lb->selected_index)
    return;
  UI_NavigateTo(lb, sel);
}

void listbox_info_free(ListBoxInfo *lb) {
  if (!lb)
    return;
  free(lb->items);
  lb->items = NULL;
  lb->items_cap = 0;
  lb->item_count = 0;
}

bool listbox_info_ensure_rows(ListBoxInfo *lb, int rows) {
  if (!lb || rows <= 0 || rows > MAX_VISIBLE_ITEMS)
    return false;
  if (lb->items && rows <= lb->items_cap)
    return true;
  int cap = lb->items_cap > 0 ? lb->items_cap : 64;
  while (cap < rows)
    cap *= 2;
  if (cap > MAX_VISIBLE_ITEMS)
    cap = MAX_VISIBLE_ITEMS;
  if (cap < rows)
    return false;
  char (*grown)[LISTBOX_ROW_BYTES] = (char (*)[LISTBOX_ROW_BYTES])realloc(
      lb->items, (size_t)cap * LISTBOX_ROW_BYTES);
  if (!grown)
    return false;
  lb->items = grown;
  lb->items_cap = cap;
  return true;
}

const char *listbox_info_row(const ListBoxInfo *lb, int idx) {
  if (!lb || !lb->items)
    return NULL;
  if (idx < 0 || idx >= lb->item_count)
    return NULL;
  if (idx >= lb->items_cap)
    return NULL;
  return lb->items[idx];
}

void listbox_info_init(ListBoxInfo *lb) {
  if (!lb)
    return;
  listbox_info_free(lb);
  lb->selected_index = -1;
  lb->selected_text = "";
  lb->total_matches = 0;
  lb->view_offset = 0;
  lb->text_hscroll = 0;
}
