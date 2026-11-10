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

void InputState_Update(int vk, bool down, int mods, int char_hint);
bool Input_IsKeyDown(int vk);
bool Input_WasKeyPressed(int vk);
bool Input_WasKeyReleased(int vk);
void Input_SetKeyCallback(void (*cb)(int vk, bool down, int mods, void *ud),
                             void *ud);
void KeyCallback_PurgeOwner(const char *path_a, const char *path_b);
void InputState_Clear(void);
void InputState_ClearQueue(void);
void InputState_InhibitMods(void);
int Input_GetPressedKey(void);
char Input_GetPressedChar(void);

void GlobalKey_Push(const PlatformKeyEvent *ev);
bool GlobalKey_Pop(PlatformKeyEvent *out);
void GlobalKey_Clear(void);
int GlobalKey_Pending(void);
unsigned long GlobalKey_Dropped(void);
#define MOUSE_BTN_LEFT 0x01
#define MOUSE_BTN_RIGHT 0x02
#define MOUSE_BTN_MIDDLE 0x03
#define MOUSE_BTN_SIDE1 0x04
#define MOUSE_BTN_SIDE2 0x05
#define MOUSE_BTN_BACK 0x06
#define MOUSE_BTN_FORWARD 0x07
#define MOUSE_BTN_TASK 0x08
#define MOUSE_BTN_EXTRA1 0x09

typedef struct {
  int button;
  int x;
  int y;
  int mods;
  bool down;
  bool repeat;
  unsigned long long t_us;
} PlatformMouseEvent;

void GlobalMouse_Push(const PlatformMouseEvent *ev);
int GlobalMouse_Full(void);
bool GlobalMouse_Pop(PlatformMouseEvent *out);
void GlobalMouse_Clear(void);
int GlobalMouse_Pending(void);
unsigned long GlobalMouse_Dropped(void);
bool Input_MouseStateDown(int button);
void Input_UpdateMouseButton(int button, bool down);
#endif
