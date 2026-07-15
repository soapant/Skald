// factorio.h
#ifndef FACTORIO_H
#define FACTORIO_H

#include <fn.h>
#include <windows.h>

void create_tag() {
    if (pixel_equals(1672, 200, 140, 140, 140)) {
        click(1672, 200);
        
        if (wait_for_pixel(1033,562,0,43,2)) {
            click_until_pixel(824, 463, 911, 22, 255, 230, 192, 0, 10);
            click(1198, 23);
        }
    }
}

void search_factory() {
    if (pixel_equals(1690, 75, 227, 227, 227, 10)) {
        click(1519, 1013);
        
        if (wait_for_pixel(1017, 327, 142, 142, 142, 4,10000)) {
            Sleep(200);
            click_until_pixel(861, 405, 965, 18, 49,48,49, 5, 10);
            click(1253, 20);
        }
    }
}

#endif // FACTORIO_H
