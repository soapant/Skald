#ifndef INPUT_STATE_H
#define INPUT_STATE_H
#include <stdbool.h>

typedef struct {
    int vk;
    int scancode;
    int mods;
    bool down;
    bool repeat;
    unsigned long long t_us;
} PlatformKeyEvent;

void Platform_InputState_Update(int vk, bool down, int mods);
bool Platform_IsKeyDown(int vk);
bool Platform_WasKeyPressed(int vk);
bool Platform_WasKeyReleased(int vk);
void Platform_SetKeyCallback(void (*cb)(int vk, bool down, int mods, void *ud), void *ud);
void Platform_InputState_Clear(void);
int Platform_GetPressedKey(void);
char Platform_GetPressedChar(void);

void Platform_GlobalKey_Push(const PlatformKeyEvent* ev);
bool Platform_GlobalKey_Pop(PlatformKeyEvent* out);
void Platform_GlobalKey_Clear(void);
int Platform_GlobalKey_Pending(void);
unsigned long Platform_GlobalKey_Dropped(void);
int Platform_CharForKey(int vk, int scancode, int mods);

#define MOUSE_BTN_LEFT    0x01
#define MOUSE_BTN_RIGHT   0x02
#define MOUSE_BTN_MIDDLE  0x03
#define MOUSE_BTN_SIDE1   0x04
#define MOUSE_BTN_SIDE2   0x05
#define MOUSE_BTN_BACK    0x06
#define MOUSE_BTN_FORWARD 0x07
#define MOUSE_BTN_TASK    0x08
#define MOUSE_BTN_EXTRA1  0x09

typedef struct {
    int button;
    int x;
    int y;
    int mods;
    bool down;
    bool repeat;
    unsigned long long t_us;
} PlatformMouseEvent;

void Platform_GlobalMouse_Push(const PlatformMouseEvent* ev);
int Platform_GlobalMouse_Full(void);
bool Platform_GlobalMouse_Pop(PlatformMouseEvent* out);
void Platform_GlobalMouse_Clear(void);
int Platform_GlobalMouse_Pending(void);
unsigned long Platform_GlobalMouse_Dropped(void);
bool Platform_MouseStateDown(int button);
void Platform_UpdateMouseButton(int button, bool down);
bool Platform_GetMousePos(int* out_x, int* out_y);
#endif
