#ifndef CORE_CHECKBOX_H
#define CORE_CHECKBOX_H

#include <stdbool.h>

typedef struct {
    const char* name;
    char* text;
    int fontsize;
    unsigned long text_color;
    int x;
    int y;
    int w;
    int h;
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
    void (*on_click)(void);
    void (*on_hover)(void);
} CheckboxSpec;

void Checkbox_FrameBegin(void);
bool Checkbox_IsChecked(const char* name);
bool Checkbox_IsHovered(const char* name);
void Checkbox_ResetAll(void);
bool Checkbox_Place(const CheckboxSpec* spec);
void Checkbox_Draw(const CheckboxSpec* spec);

#endif
