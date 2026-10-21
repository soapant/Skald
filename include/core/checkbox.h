#ifndef CORE_CHECKBOX_H
#define CORE_CHECKBOX_H

#include <stdbool.h>

typedef struct {
    const char* name;
    int x;
    int y;
    int scale;
    bool checked;
    void (*on_click)(const char* name, bool checked);
    void (*on_hover)(const char* name, bool hovered);
} FlCheckboxSpec;

void Checkbox_FrameBegin(void);
bool Checkbox_IsChecked(const char* name);
bool Checkbox_IsHovered(const char* name);
bool Checkbox_Place(const FlCheckboxSpec* spec);
void Checkbox_Draw(int x, int y, int w, int h, bool checked, bool hovered);

#endif
