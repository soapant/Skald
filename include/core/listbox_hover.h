#ifndef LISTBOX_HOVER_H
#define LISTBOX_HOVER_H

#include <stdbool.h>
#include "core/types.h"

typedef struct {
  int idx;
  bool selected;
  int x;
  int y;
  int w;
  int h;
  const char *text;
} ListItemCtx;

bool listbox_begin_common(const ListSpec *spec);
bool listbox_next_common(ListItemCtx *out);
void listbox_common_end(void);
int listbox_common_item_height(void);

#define LISTBOX_WHEEL_ROWS 3
#define MOUSE_RECHECK_INTERVAL_MS 80

bool listbox_client_mouse_pos(int *out_x, int *out_y);
bool listbox_window_origin(int *out_x, int *out_y);
void listbox_publish_mouse_pos(int x, int y);
void listbox_invalidate_mouse_pos(void);
int listbox_row_at_y(const ListBox *lb, int list_top, int item_height,
                     int top_index, int visible_count, int mouse_y);
bool listbox_hover_focus(ListBox *lb, int list_top, int item_height,
                         int top_index, int visible_count, int left_x,
                         int right_x);
void listbox_focus_by_keyboard(void);
void listbox_set_row_h_scroll(int px);
int listbox_get_row_h_scroll(void);
void listbox_focus_by_mouse(void);
void listbox_queue_scroll(int rows);
int listbox_take_scroll(void);
void listbox_scroll_by(ListBox *lb, int rows, int visible_count);
void listbox_apply_pending_scroll(ListBox *lb, int visible_count);
int listbox_scroll_top_for(int index, int visible_count, int count);
void listbox_scroll_to_selection(ListBox *lb);
void listbox_set_geometry(int list_top, int item_height, int visible_count);
void listbox_get_geometry(int *out_list_top, int *out_item_height,
                          int *out_visible_count);
void listbox_navigate_up(ListBox *lb);
void listbox_navigate_down(ListBox *lb);

#endif
