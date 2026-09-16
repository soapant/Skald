#ifndef COMMON_UI_H
#define COMMON_UI_H

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <sys/time.h>
#endif
#include "core/context.h"
#include "core/modal.h"
#include "core/types.h"
#include "config.h"
#include "core/timer.h"
#include "platform/ui.h"


extern bool g_add_alias_to_string;
extern char g_alias_display[MAX_ITEMS][512];
extern int g_alias_display_count;
extern bool g_alias_display_active;
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
