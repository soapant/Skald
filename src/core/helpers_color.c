#include "config.h"
#include <stdlib.h>
#include "config.h"
#include "helpers/helpers_ui.h"

unsigned long rgb_to_hex(unsigned long win_color) {
#if defined(__linux__)
  unsigned long r = win_color & 0xFF;
  unsigned long g = (win_color >> 8) & 0xFF;
  unsigned long b = (win_color >> 16) & 0xFF;
  return (r << 16) | (g << 8) | b;
#elif defined(_WIN32)
  return win_color;
#endif
}

void eval_window_title_color(PromptState *state,
                             unsigned long *window_title_color) {
  unsigned long final_color;
  if (state->item_count > 1000) {
    final_color = rgb_to_hex(RGB(255, 0, 0));
  } else if (state->item_count > 500) {
    final_color = rgb_to_hex(RGB(255, 165, 0));
  } else if (state->item_count > 200) {
    final_color = rgb_to_hex(RGB(255, 255, 0));
  } else if (state->item_count > 100) {
    final_color = rgb_to_hex(RGB(255, 255, 50));
  } else {
    final_color = rgb_to_hex(RGB(30, 255, 30));
  }
  *window_title_color = final_color;
}

unsigned short OpacityAlpha16(void) {
  float t = WindowOpacity / 255.0f;
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  return (unsigned short)(t * 65535.0f + 0.5f);
}

unsigned int Color_Lerp(unsigned int a, unsigned int b, float t) {
  if (t < 0)
    t = 0;
  if (t > 1)
    t = 1;
  int ar = (int)((a >> 16) & 0xFF), ag = (int)((a >> 8) & 0xFF),
      ab = (int)(a & 0xFF);
  int br = (int)((b >> 16) & 0xFF), bg = (int)((b >> 8) & 0xFF),
      bb = (int)(b & 0xFF);
  unsigned int rr = (unsigned int)(ar + (br - ar) * t + 0.5f);
  unsigned int rg = (unsigned int)(ag + (bg - ag) * t + 0.5f);
  unsigned int rb = (unsigned int)(ab + (bb - ab) * t + 0.5f);
  return (rr << 16) | (rg << 8) | rb;
}
