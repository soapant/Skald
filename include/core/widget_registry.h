#ifndef WIDGET_REGISTRY_H
#define WIDGET_REGISTRY_H

#include <stdbool.h>
#include "core/types.h"
#include "platform/widget.h"

#define WIDGET_MAX 32
#define WIDGET_NAME_MAX 64
#define WIDGET_DEFAULT_PAD 12
#define WIDGET_PROVISIONAL_W 160
#define WIDGET_PROVISIONAL_H 48

typedef enum {
  WIDGET_SLOT_TOP_LEFT,
  WIDGET_SLOT_TOP_RIGHT,
  WIDGET_SLOT_BOTTOM_LEFT,
  WIDGET_SLOT_BOTTOM_RIGHT,
  WIDGET_SLOT_COUNT,
  WIDGET_FREE = 99,
  WIDGET_MAX_MONITORS = 16
} WidgetSlot;

typedef struct WidgetState WidgetState;
typedef void (*WidgetRenderFn)(WidgetState *state);

typedef struct {
  const char *name;
  int w;
  int h;
  int slot;
  int order;
  int gap;
  int pad;
  int fps;
  ColorRef color;
  bool no_bg;
  void (*on_click)(void);
  void (*on_hover)(void);
  WidgetRenderFn render;
  int x;
  int y;
  int monitor;
  const char *monitor_name;
} WidgetSpec;

typedef struct WidgetState {
  int width;
  int height;
  unsigned long frame;
  double elapsed;
  bool hovered;
} WidgetState;

typedef void (*WidgetRenderFn)(WidgetState *state);

typedef struct Widget Widget;
struct Widget {
  char name[WIDGET_NAME_MAX];
  int w;
  int h;
  int gap_override;
  int fps;
  int slot;
  int order;
  int seq;
  bool order_set;
  bool free_pos;
  int x;
  int y;
  bool auto_w;
  bool auto_h;
  int origin_x;
  int origin_y;
  ColorRef bg_color;
  bool has_bg;
  bool no_bg;
  int pad;
  bool mapped;
  volatile bool dead;
  bool hover_running;
  void *hover_thread;
  int hover_epoch;
  PlatformWidget *win;
  WidgetRenderFn render;
  unsigned long frame;
  PlTimer created_at;
  char *owner;
  unsigned gen;
  int monitor;
  void (*on_click)(void);
  void (*on_hover)(void);
  bool clickable;
  bool hovered;
};

void widget_purge_owner(const char *path_a, const char *path_b,
                       unsigned generation);
bool widget_owner_active(const char *path);
unsigned widget_bump_generation(void);
unsigned widget_current_generation(void);

Widget *widget_create(const WidgetSpec *spec);
void widget_destroy(Widget *widget);
bool widget_valid(const Widget *widget);
PlPos Widget_GetPos(const char *name);
void widget_show(Widget *widget);
void widget_hide(Widget *widget);
void widget_set_render(Widget *widget, WidgetRenderFn render);
void widget_set_fps(PlatformWidget *window, int fps);

bool widget_is_valid(const PlatformWidget *widget);
void widget_set_frame_callback(PlatformWidget *widget, RenderFrameFn fn,
                             void *ud);
void widget_invalidate(PlatformWidget *widget);
bool widget_base_ready(const PlatformWidgetBase *base, bool mapped);
bool widget_base_move(PlatformWidgetBase *base, int x, int y);
bool widget_base_resize(PlatformWidgetBase *base, int w, int h);
bool widget_base_set_no_bg(PlatformWidgetBase *base, bool no_bg);
bool widget_base_set_bg_color(PlatformWidgetBase *base, ColorRef bg_color);

void widget_init_base(PlatformWidgetBase *base, int x, int y, int w, int h,
                     int fps, bool no_bg, void (*on_click)(void),
                     const char *name);
#endif
