#ifndef INPUT_STATE_H
#define INPUT_STATE_H
#include <stdbool.h>

void Platform_InputState_Update(int vk, bool down, int mods);
bool Platform_IsKeyDown(int vk);
bool Platform_WasKeyPressed(int vk);
bool Platform_WasKeyReleased(int vk);
void Platform_SetKeyCallback(void (*cb)(int vk, bool down, int mods, void *ud), void *ud);
void Platform_InputState_Clear(void);
int Platform_GetPressedKey(void);
char Platform_GetPressedChar(void);
#endif
