#ifndef HELPERS_INPUT_STATE_H
#define HELPERS_INPUT_STATE_H
#include <stdbool.h>

typedef struct {
  int vk;
  int scancode;
  int mods;
  bool down;
  bool repeat;
  unsigned long long t_us;
} PlatformKeyEvent;

void input_state_update(int vk, bool down, int mods, int char_hint);
bool input_is_key_down(int vk);
bool input_was_key_pressed(int vk);
bool input_was_key_released(int vk);
void input_set_key_callback(void (*cb)(int vk, bool down, int mods, void *ud),
                            void *ud);
void key_callback_purge_owner(const char *path_a, const char *path_b);
void input_state_clear(void);
void input_state_clear_queue(void);
void input_state_inhibit_mods(void);
int input_get_pressed_key(void);
char input_get_pressed_char(void);

void global_key_push(const PlatformKeyEvent *ev);
bool global_key_pop(PlatformKeyEvent *out);
void global_key_clear(void);
int global_key_pending(void);
unsigned long global_key_dropped(void);
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

void global_mouse_push(const PlatformMouseEvent *ev);
int global_mouse_full(void);
bool global_mouse_pop(PlatformMouseEvent *out);
void global_mouse_clear(void);
int global_mouse_pending(void);
unsigned long global_mouse_dropped(void);
bool input_mouse_state_down(int button);
void input_update_mouse_button(int button, bool down);
#endif
