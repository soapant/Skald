#include "core/listbox_hover.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/context.h"
#include "core/focus.h"
#include "core/draw_state.h"
#include "core/prompt_geometry.h"
#include "config_values.h"
#include "core/timer.h"
#include "core/types/window.h"
#include "core/types/prompt.h"
#include "core/window_manager.h"
#include "platform/draw.h"
#include "helpers/helpers_ui.h"
#include "helpers/helpers_input_state.h"
#include "platform/platform.h"
#include "deps/log.h"

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
static bool window_origin_valid = false;
static int window_origin_x = 0;
static int window_origin_y = 0;

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

bool listbox_window_origin(int *out_x, int *out_y) {
  if (!window_origin_valid) {
    WindowManager *wm = WindowManager_Get();
    Popup *top = wm ? WindowManager_GetTopPopup(wm) : NULL;
    bool ok = Platform_WindowToRoot(SkaldContext_Get()->window.hPopupWnd,
                                    &window_origin_x, &window_origin_y);
    if (!ok && top)
      ok = Platform_WindowToRoot(top->handle, &window_origin_x,
                                 &window_origin_y);
    if (!ok)
      return false;
    window_origin_valid = true;
  }
  if (out_x)
    *out_x = window_origin_x;
  if (out_y)
    *out_y = window_origin_y;
  return true;
}

void listbox_publish_mouse_pos(int x, int y) {
  window_origin_valid = false;
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
  window_origin_valid = false;
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
  if (listbox_window_origin(&wx, &wy)) {
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

int listbox_row_at_y(ListboxRowAtY *argspec) {

  if (!argspec->lb || argspec->item_height <= 0 ||
      argspec->visible_count <= 0) {
    return -1;
  }
  int rel = argspec->mouse_y - argspec->list_top;
  if (rel < 0)
    return -1;
  int row = rel / argspec->item_height;
  if (row >= argspec->visible_count)
    return -1;
  int idx = argspec->top_index + row;
  if (idx < 0 || idx >= argspec->lb->item_count)
    return -1;
  return idx;
}

bool listbox_hover_focus(ListboxHoverFocus *argspec) {

  if (!argspec->lb || argspec->lb->item_count <= 0)
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
  if (mx < argspec->left_x || mx >= argspec->right_x)
    return false;
  listbox_focus_by_mouse();
  focus_set_pseudo_passive(FOCUS_KIND_LISTBOX);
  int idx =
      listbox_row_at_y(&(ListboxRowAtY){.lb = argspec->lb,
                                        .list_top = argspec->list_top,
                                        .item_height = argspec->item_height,
                                        .top_index = argspec->top_index,
                                        .visible_count = argspec->visible_count,
                                        .mouse_y = my});
  if (idx < 0 || idx == argspec->lb->selected_index)
    return false;
  UI_SetSelection(argspec->lb, idx);
  return true;
}

void listbox_queue_scroll(int rows) { pendinscroll += rows; }

int listbox_take_scroll(void) {
  int rows = pendinscroll;
  pendinscroll = 0;
  return rows;
}

void listbox_scroll_by(ListBox *lb, int rows, int rows_visible) {
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
}

void listbox_apply_pending_scroll(ListBox *lb, int rows_visible) {
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

void listbox_scroll_to_selection(ListBox *lb) {
  if (!lb || lb->selected_index < 0 || lb->item_count <= 0)
    return;
  int geom_visible = 0;
  listbox_get_geometry(NULL, NULL, &geom_visible);
  int visible = geom_visible > 0 ? geom_visible : UI_LB_GetVisibleCount();
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

void listbox_navigate_up(ListBox *lb) {
  if (!lb)
    return;
  listbox_focus_by_keyboard();
  focus_set_pseudo_passive(FOCUS_KIND_LISTBOX);
  int sel = lb->selected_index;
  if (sel >= 0)
    sel--;
  if (sel == lb->selected_index)
    return;
  UI_NavigateTo(lb, sel);
}

void listbox_navigate_down(ListBox *lb) {
  if (!lb)
    return;
  listbox_focus_by_keyboard();
  focus_set_pseudo_passive(FOCUS_KIND_LISTBOX);
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

void listbox_info_free(ListBox *lb) {
  if (!lb)
    return;
  for (int i = 0; i < lb->items_cap; i++)
    free(lb->items[i]);
  free(lb->items);
  lb->items = NULL;
  lb->items_cap = 0;
  lb->item_count = 0;
  free(lb->matching_indices);
  lb->matching_indices = NULL;
  lb->matches_cap = 0;
  lb->total_matches = 0;
}

bool listbox_info_ensure_rows(ListBox *lb, int rows) {
  if (!lb || rows <= 0)
    return false;
  if (lb->items && rows <= lb->items_cap)
    return true;
  int cap = lb->items_cap > 0 ? lb->items_cap : 64;
  while (cap < rows)
    cap *= 2;
  char **grown = (char **)realloc(lb->items, (size_t)cap * sizeof(*grown));
  if (!grown)
    return false;
  lb->items = grown;
  memset(lb->items + lb->items_cap, 0,
         (size_t)(cap - lb->items_cap) * sizeof(*grown));
  lb->items_cap = cap;
  return true;
}

bool listbox_info_set_row(ListBox *lb, int idx, const char *text) {
  if (!lb || idx < 0 || idx >= lb->items_cap)
    return false;
  size_t n = text ? strcspn(text, "\r\n") : 0;
  char *row = (char *)malloc(n + 1);
  if (!row)
    return false;
  memcpy(row, text ? text : "", n);
  row[n] = '\0';
  free(lb->items[idx]);
  lb->items[idx] = row;
  return true;
}

bool listbox_info_ensure_matches(ListBox *lb, int n) {
  if (!lb || n < 0)
    return false;
  if (lb->matching_indices && n <= lb->matches_cap)
    return true;
  int cap = lb->matches_cap > 0 ? lb->matches_cap : 64;
  while (cap < n)
    cap *= 2;
  int *grown =
      (int *)realloc(lb->matching_indices, (size_t)cap * sizeof(*grown));
  if (!grown)
    return false;
  lb->matching_indices = grown;
  lb->matches_cap = cap;
  return true;
}

const char *listbox_info_row(const ListBox *lb, int idx) {
  if (!lb || !lb->items)
    return NULL;
  if (idx < 0 || idx >= lb->item_count)
    return NULL;
  if (idx >= lb->items_cap)
    return NULL;
  return lb->items[idx];
}

void listbox_info_init(ListBox *lb) {
  if (!lb)
    return;
  listbox_info_free(lb);
  lb->selected_index = -1;
  lb->selected_text = "";
  lb->total_matches = 0;
  lb->view_offset = 0;
  lb->text_hscroll = 0;
}

static __thread ListBox *common_lb;
static __thread List common_spec;
static __thread int common_y0, common_item_h, common_visible, common_cur,
    common_h;
static __thread int common_bx, common_bw, common_fw, common_fh;
static __thread bool common_separators;

bool listbox_begin_common(const List *spec) {
  if (!spec)
    return false;
  ListBox *lb = spec->lb;
  if (!lb) {
    return false;
  }
  int item_h = spec->item_height ? spec->item_height : 0;
  ListboxBackendInit bi;
  if (!Platform_ListboxBackendBegin(item_h, &bi))
    return false;
  item_h = item_h > 0 ? item_h : bi.item_h;
  PromptState *st = Platform_ListboxBackendPromptState(lb);
  ListboxLayout lay;
  listbox_compute_layout(
      &(ListboxComputeLayout){.spec = spec,
                              .default_y0 = bi.list_top,
                              .item_h = item_h,
                              .popup_w = st ? st->width : PromptW,
                              .popup_h = st ? st->height : PromptH,
                              .count = lb->item_count,
                              .out = &lay});
  common_spec = *spec;
  common_lb = lb;
  common_item_h = item_h;
  common_y0 = lay.y0;
  common_bx = lay.bx;
  common_bw = lay.bw;
  common_fw = lay.fw;
  common_fh = lay.fh;
  common_h = lay.list_h;
  common_separators = lay.list_h >= LISTBOX_SEPARATOR_MIN_LIST_H;
  common_visible = lay.visible;
  common_cur = 0;
  UI_LB_SetVisibleCount(lay.visible);
  listbox_apply_pending_scroll(lb, lay.visible);
  listbox_set_geometry(common_y0, common_item_h, lay.visible);
  listbox_hover_focus(&(ListboxHoverFocus){.lb = lb,
                                           .list_top = common_y0,
                                           .item_height = common_item_h,
                                           .top_index = lb->view_offset,
                                           .visible_count = lay.visible,
                                           .left_x = common_bx,
                                           .right_x = common_bx + common_bw});
  draw_state_get()->listbox_rows = true;
  draw_state_set_clip(common_y0, common_y0 + common_h);
  SkaldContext_Get()->window.listbox_placed = true;
  return true;
}

bool listbox_next_common(ListItemCtx *out) {
  if (!out || !common_lb)
    return false;
  if (common_cur >= common_visible)
    return false;
  int idx = common_lb->view_offset + common_cur;
  if (idx < 0 || idx >= common_lb->item_count)
    return false;
  int y = common_y0 + common_cur * common_item_h;
  int row_h = common_item_h;
  int clip_bottom = common_y0 + common_h;
  if (y + row_h > clip_bottom)
    row_h = clip_bottom - y;
  if (row_h < 1)
    return false;
  if (common_separators && common_cur == 0)
    draw_prompt_separator(
        (DrawPromptSeparator){.y = common_y0, .width = common_fw});
  bool sel = (idx == common_lb->selected_index);
  Platform_ListboxBackendDrawSelection(
      &(ListboxBackendDrawSelection){.bx = common_bx,
                                     .bw = common_bw,
                                     .y = y,
                                     .item_h = row_h,
                                     .fw = common_fw,
                                     .frame_h = common_fh,
                                     .sel = sel});
  listbox_set_row_h_scroll(sel ? common_lb->text_hscroll : 0);
  out->idx = idx;
  out->selected = sel;
  out->x = common_spec.x;
  out->y = y;
  out->w = common_spec.w;
  out->h = common_item_h;
  out->text = common_lb->items[idx];
  common_cur++;
  return true;
}

void listbox_common_end(void) {
  listbox_set_row_h_scroll(0);
  draw_state_clear_clip();
  if (common_separators && prompt_list_bottom_margin() > 0 && common_y0 >= 0) {
    draw_prompt_separator(
        (DrawPromptSeparator){.y = common_y0 + common_h, .width = common_fw});
  }
  Platform_ListboxBackendEnd();
  common_lb = NULL;
  common_visible = 0;
  common_cur = 0;
  common_y0 = -1;
  draw_state_get()->listbox_rows = false;
}

int listbox_common_item_height(void) { return common_item_h; }
