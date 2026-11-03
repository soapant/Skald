#ifndef CORE_WIDGET_REGISTRY_H
#define CORE_WIDGET_REGISTRY_H

#include <stdbool.h>
#include "core/types.h"
#include "platform/widget.h"
#include "platform/platform.h"

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
  WIDGET_FREE
} WidgetSlot;

typedef struct WidgetState WidgetState;

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
  void (*hover_thread)(void);
  PlatformWidget *win;
  WidgetRenderFn render;
  unsigned long frame;
  PlTimer created_at;
  char *owner;
  unsigned gen;
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
bool widget_valid(Widget *widget);
PlPos widget_get_pos(const char *name);
void widget_show(Widget *widget);
void widget_hide(Widget *widget);
void widget_set_render(Widget *widget, WidgetRenderFn render);

#endif
