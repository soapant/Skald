#include "config.h"
#include <stdlib.h>
#include "config.h"
#include "helpers/helpers_ui.h"

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

float Color_ShaderGamma(float smoothness) {
    float s = smoothness;
    if (s < 0.1f) s = 0.1f; if (s > 10.0f) s = 10.0f;
    float g = 2.0f - s * 0.14f;
    if (g < 0.45f) g = 0.45f; if (g > 2.2f) g = 2.2f;
    return g;
}

float Color_BayerDither(int x, int y) {
    static const int bayer[16] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    return ((bayer[(y & 3)*4 + (x & 3)] / 16.0f) - 0.5f) * 1.2f;
}

unsigned int Color_HashNoise(int x, int y) {
    unsigned int h = (unsigned int)(x * 374761393U + y * 668265263U);
    h = (h ^ (h >> 13)) * 1274126177U;
    return h & 0xFF;
}

unsigned int Color_Lerp(unsigned int a, unsigned int b, float t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    int ar = (int)((a >> 16) & 0xFF), ag = (int)((a >> 8) & 0xFF), ab = (int)(a & 0xFF);
    int br = (int)((b >> 16) & 0xFF), bg = (int)((b >> 8) & 0xFF), bb = (int)(b & 0xFF);
    unsigned int rr = (unsigned int)(ar + (br - ar) * t + 0.5f);
    unsigned int rg = (unsigned int)(ag + (bg - ag) * t + 0.5f);
    unsigned int rb = (unsigned int)(ab + (bb - ab) * t + 0.5f);
    return (rr << 16) | (rg << 8) | rb;
}

void Color_DeriveLighter(unsigned int base, float intensity, unsigned char* out_r, unsigned char* out_g, unsigned char* out_b) {
    float f = ShaderIntensity;
    if (f < 0) f = 0; if (f > 1) f = 1;
    float eff = f * intensity;
    if (eff < 0.25f) eff = 0.25f;
    if (eff > 1) eff = 1;
    int r = (int)((base >> 16) & 0xFF), g = (int)((base >> 8) & 0xFF), b = (int)(base & 0xFF);
    int nr = r + (int)((90 - r) * eff + 35 * eff);
    int ng = g + (int)((90 - g) * eff + 35 * eff);
    int nb = b + (int)((140 - b) * eff + 40 * eff);
    if (nr < 0) nr = 0; if (nr > 255) nr = 255;
    if (ng < 0) ng = 0; if (ng > 255) ng = 255;
    if (nb < 0) nb = 0; if (nb > 255) nb = 255;
    *out_r = (unsigned char)nr;
    *out_g = (unsigned char)ng;
    *out_b = (unsigned char)nb;
}
