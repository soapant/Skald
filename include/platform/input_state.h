#ifndef INPUT_STATE_H
#define INPUT_STATE_H
#include <stdbool.h>

typedef struct {
    int vk;
    int scancode;
    int mods;
    bool down;
    bool repeat;
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
#endif
