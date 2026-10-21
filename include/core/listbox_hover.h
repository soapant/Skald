#ifndef CORE_LISTBOX_HOVER_H
#define CORE_LISTBOX_HOVER_H

#include <stdbool.h>
#include "core/types.h"

#define LISTBOX_WHEEL_ROWS 3

bool ListBox_ClientMousePos(int* out_x, int* out_y);
void ListBox_PublishMousePos(int x, int y);
void ListBox_InvalidateMousePos(void);
int ListBox_RowAtY(const ListBoxInfo* lb, int list_top, int item_height,
                   int top_index, int visible_count, int mouse_y);
bool ListBox_HoverFocus(ListBoxInfo* lb, int list_top, int item_height,
                        int top_index, int visible_count);
void ListBox_FocusByKeyboard(void);
void ListBox_FocusByMouse(void);
void ListBox_ListRebuilt(void);
void ListBox_QueueScroll(int rows);
int ListBox_TakeScroll(void);
void ListBox_ScrollBy(ListBoxInfo* lb, int rows, int visible_count);
void ListBox_ApplyPendingScroll(ListBoxInfo* lb, int visible_count);
int ListBox_ScrollTopFor(int index, int visible_count, int count);
void ListBox_ScrollToSelection(ListBoxInfo* lb);

#endif
