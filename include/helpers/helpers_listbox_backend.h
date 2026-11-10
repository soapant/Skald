#include <stdbool.h>
#include "core/types/prompt.h"

#ifndef HELPERS_LISTBOX_BACKEND_H
#define HELPERS_LISTBOX_BACKEND_H

typedef struct {
  int item_h;
  int list_top;
} ListboxBackendInit;

bool ListboxBackend_Begin(int item_height_override, ListboxBackendInit *out);
void ListboxBackend_DrawSelection(int bx, int bw, int y, int item_h, int fw,
                                  int frame_h, bool sel);
void ListboxBackend_End(void);
PromptState *ListboxBackend_PromptState(const ListBoxInfo *lb);

#endif
