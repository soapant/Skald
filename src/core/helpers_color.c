#include "common.h"
#include "config.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "helpers.h"
#include "core/timer.h"
#include "core/common_ui.h"

unsigned long rgb_to_hex(unsigned long win_color) {
    unsigned long r = win_color & 0xFF;          
    unsigned long g = (win_color >> 8) & 0xFF;  
    unsigned long b = (win_color >> 16) & 0xFF;

    return (r << 16) | (g << 8) | b;
}

void eval_window_title_color(PromptState* state,unsigned long *window_title_color) {
  unsigned long final_color;
  if (state->item_count > 1000) {
    final_color = rgb_to_hex(RGB(255,0,0));
  } else if (state->item_count > 500) {
    final_color = rgb_to_hex(RGB(255,165,0));
  } else if (state->item_count > 200) {
    final_color = rgb_to_hex(RGB(255,255,0));
  } else if (state->item_count > 100) {
    final_color = rgb_to_hex(RGB(255,255,50));
  } else {
    final_color = rgb_to_hex(RGB(30,255,30));
  }
  *window_title_color = final_color;
}

void eval_xoffset_for_alias(char* alias_text,int main_text_lenght,int* xOffsetAlias) {
  int main_text_w = (main_text_lenght * FontSize) / 2;
  *xOffsetAlias = main_text_w;
}

unsigned short OpacityAlpha16(void) {
    float t = WindowOpacity / 255.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (unsigned short)(t * 65535.0f + 0.5f);
}
