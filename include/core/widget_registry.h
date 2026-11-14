#ifndef CORE_WIDGET_REGISTRY_H
#define CORE_WIDGET_REGISTRY_H

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

void Widget_PurgeOwner(const char *path_a, const char *path_b,
                       unsigned generation);
bool Widget_OwnerActive(const char *path);
unsigned Widget_BumpGeneration(void);
unsigned Widget_CurrentGeneration(void);

Widget *Widget_Create(const WidgetSpec *spec);
void Widget_Destroy(Widget *widget);
bool Widget_Valid(const Widget *widget);
PlPos Widget_GetPos(const char *name);
void Widget_Show(Widget *widget);
void Widget_Hide(Widget *widget);
void Widget_SetRender(Widget *widget, WidgetRenderFn render);
void Widget_SetFps(PlatformWidget *window, int fps);

bool Widget_IsValid(const PlatformWidget *widget);
void Widget_SetFrameCallback(PlatformWidget *widget, RenderFrameFn fn,
                             void *ud);
void Widget_Invalidate(PlatformWidget *widget);
bool Widget_BaseCheck(const PlatformWidgetBase *base);
bool Widget_BaseMove(PlatformWidgetBase *base, int x, int y);
bool Widget_BaseResize(PlatformWidgetBase *base, int w, int h);
bool Widget_BaseSetNoBg(PlatformWidgetBase *base, bool no_bg);
bool Widget_BaseSetBgColor(PlatformWidgetBase *base, ColorRef bg_color);

void Widget_InitBase(PlatformWidgetBase *base, int x, int y, int w, int h,
                     int fps, bool no_bg, void (*on_click)(void),
                     const char *name);
#endif
