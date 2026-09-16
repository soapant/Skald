#ifndef HELPERS_HELPERS_UI_H
#define HELPERS_HELPERS_UI_H

#include <stdbool.h>
#include "core/types.h"
#include "core/types/window.h"
#include "core/types/color.h"

typedef struct {
    char **items;
    size_t count;
} StringList;

StringList String_Split(const char *string, const char *delimiter);
void StringList_Free(StringList *list);

unsigned long rgb_to_hex(unsigned long win_color);
void eval_window_title_color(PromptState * state,unsigned long *window_title_color);
void eval_xoffset_for_alias(char* alias_text,int main_text_lenght,int *xOffsetAlias);
unsigned short OpacityAlpha16(void);

static bool g_add_alias_to_string;
static char g_alias_display[MAX_ITEMS][512];
static int g_alias_display_count;
static bool g_alias_display_active;
void BuildAliasDisplayItems(const char** items, int count);

void PopulateListBox(ListBoxInfo* lb, WindowHandle hListBox, const char* filter,
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter));

ColorRef GetRainbowColor(float t, float s, float v);

static inline bool UI_IsListBoxSelected(ListBoxInfo* lb) {
  return lb && lb->selected_index != -1;
}

void UpdateTextCaret(WindowHandle hwndEditControl);
void CalculateWindowMetrics(int count, WindowLayout* layout,int height , int width,int item_height);

#endif
