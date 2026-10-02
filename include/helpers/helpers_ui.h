#ifndef HELPERS_UI_H
#define HELPERS_UI_H

#include <stdbool.h>
#include "core/types.h"
#include "core/types.h"

unsigned long rgb_to_hex(unsigned long win_color);
void eval_window_title_color(PromptState * state,unsigned long *window_title_color);
void eval_xoffset_for_alias(char* alias_text,int main_text_lenght,int *xOffsetAlias);
unsigned short OpacityAlpha16(void);

float Color_ShaderGamma(float smoothness);
float Color_BayerDither(int x, int y);
unsigned int Color_HashNoise(int x, int y);
unsigned int Color_Lerp(unsigned int a, unsigned int b, float t);
void Color_DeriveLighter(unsigned int base, float intensity, unsigned char* out_r, unsigned char* out_g, unsigned char* out_b);

void BuildAliasDisplayItems(ListBoxInfo* lb, const char** items, int count);
void PopulateListBox(ListBoxInfo* lb, const char* filter,
    const char** items, int item_count,
    bool (*filter_match)(const char* item, const char* filter));




ColorRef GetRainbowColor(float t, float s, float v);
bool UI_IsListBoxSelected(ListBoxInfo* lb);
void UpdateTextCaret(WindowHandle hwndEditControl);
void CalculateWindowMetrics(WindowLayout* layout,int height , int width,int item_height);

#define MSG_PROMPT_GAP_DEFAULT 0
#define MSG_SCREEN_GAP_DEFAULT 20

typedef enum {
    FL_AUTO = 0,
    FL_TOP_LEFT,
    FL_TOP_CENTER,
    FL_TOP_RIGHT,
    FL_LEFT,
    FL_CENTER,
    FL_RIGHT,
    FL_BOTTOM_LEFT,
    FL_BOTTOM_CENTER,
    FL_BOTTOM_RIGHT,

    FL_TOP_LEFT_RELATIVE,
    FL_TOP_CENTER_RELATIVE,
    FL_TOP_RIGHT_RELATIVE,
    FL_LEFT_RELATIVE,
    FL_CENTER_RELATIVE,
    FL_RIGHT_RELATIVE,
    FL_BOTTOM_LEFT_RELATIVE,
    FL_BOTTOM_CENTER_RELATIVE,
    FL_BOTTOM_RIGHT_RELATIVE,

    FL_FREE
} FlMessageSlot;

extern int MessagePromptGap;
extern int MessageScreenGap;

void MessageSlotToRect(FlMessageSlot slot, int prompt_gap, int* out_x, int* out_y, int* out_w, int* out_h, int* out_h_slot, int* out_v_slot, int* out_gap, int* out_outward);
void DisplayMessageSlot(const char* message, FlMessageSlot slot, int max_w, int max_h, int prompt_gap, int timeout, ColorRef color);

#endif
