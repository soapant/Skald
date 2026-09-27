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

#endif
