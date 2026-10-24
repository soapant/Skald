#ifndef CORE_CHECKBOX_H
#define CORE_CHECKBOX_H

#include <stdbool.h>

typedef struct {
    const char* name;
    int x;
    int y;
    int w;
    int h;
    int scale;
    bool checked;
    bool button;
    bool checkbox;
    bool hovered;
    bool was_down;
    bool clicked;
    int thickness;
    unsigned long hovered_color;
    unsigned long clicked_color;
    bool border;
    bool no_bg;
    void (*on_click)(const char* name, bool checked);
    void (*on_hover)(const char* name, bool hovered);
} CheckboxSpec;

void Checkbox_FrameBegin(void);
bool Checkbox_IsChecked(const char* name);
bool Checkbox_IsHovered(const char* name);
bool Checkbox_Place(const CheckboxSpec* spec);
void Checkbox_Draw(const CheckboxSpec* spec);

#endif
