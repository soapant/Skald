#include "config_values.h"
#include "helpers/helpers_ui.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helpers/helpers_monitor.h"
#include "helpers/helpers_key.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_input_state.h"
#include "core/types/keys.h"
#include "core/thread_registry.h"
#include "platform/lib.h"
#include "deps/log.h"

unsigned long RgbToHex(unsigned long win_color) {
#if defined(__linux__)
  unsigned long r = win_color & 0xFF;
  unsigned long g = (win_color >> 8) & 0xFF;
  unsigned long b = (win_color >> 16) & 0xFF;
  return (r << 16) | (g << 8) | b;
#elif defined(_WIN32)
  return win_color;
#endif
  return 0;
}

unsigned short OpacityAlpha16(void) {
  float t = WindowOpacity / 255.0f;
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  return (unsigned short)(t * 65535.0f + 0.5f);
}

unsigned int Color_Lerp(unsigned int a, unsigned int b, float t) {
  if (t < 0)
    t = 0;
  if (t > 1)
    t = 1;
  int ar = (int)((a >> 16) & 0xFF), ag = (int)((a >> 8) & 0xFF),
      ab = (int)(a & 0xFF);
  int br = (int)((b >> 16) & 0xFF), bg = (int)((b >> 8) & 0xFF),
      bb = (int)(b & 0xFF);
  unsigned int rr = (unsigned int)(ar + (br - ar) * t + 0.5f);
  unsigned int rg = (unsigned int)(ag + (bg - ag) * t + 0.5f);
  unsigned int rb = (unsigned int)(ab + (bb - ab) * t + 0.5f);
  return (rr << 16) | (rg << 8) | rb;
}

int Monitor_Count(void) {
  MonitorEntry mon[16];
  return Monitor_Fill(mon, 16);
}

bool Monitor_GeometryAt(int index, int *x, int *y, int *w, int *h) {
  MonitorEntry mon[16];
  int n = Monitor_Fill(mon, 16);
  if (n <= 0 || index < 0 || index >= n || !mon[index].w)
    return false;
  if (x)
    *x = mon[index].x;
  if (y)
    *y = mon[index].y;
  if (w)
    *w = mon[index].w;
  if (h)
    *h = mon[index].h;
  return true;
}

bool Monitor_Info(int index, MonitorInfo *out) {
  MonitorEntry mon[16];
  int n = Monitor_Fill(mon, 16);
  if (n <= 0 || index < 0 || index >= n || !mon[index].w || !out)
    return false;
  memset(out, 0, sizeof(*out));
  out->index = index;
  out->x = mon[index].x;
  out->y = mon[index].y;
  out->w = mon[index].w;
  out->h = mon[index].h;
  out->is_primary = mon[index].is_primary;
  snprintf(out->name, sizeof(out->name), "%s", mon[index].name);
  return true;
}

bool Key_IsCtrlPressed(void) {
  return Input_IsKeyDown(VK_LCONTROL) || Input_IsKeyDown(VK_RCONTROL) ||
         Input_IsKeyDown(VK_CONTROL);
}

bool Key_IsShiftPressed(void) {
  return Input_IsKeyDown(VK_LSHIFT) || Input_IsKeyDown(VK_RSHIFT) ||
         Input_IsKeyDown(VK_SHIFT);
}

bool Key_IsAltPressed(void) {
  return Input_IsKeyDown(VK_LMENU) || Input_IsKeyDown(VK_RMENU) ||
         Input_IsKeyDown(VK_MENU);
}

bool Key_IsSuperPressed(void) {
  return Input_IsKeyDown(VK_LWIN) || Input_IsKeyDown(VK_RWIN);
}

static KeyInterceptorFn key_interceptor = NULL;
static char key_interceptor_owner[OWNER_PATH_MAX] = {0};

void Key_RegisterInterceptor(KeyInterceptorFn fn) {
  key_interceptor = fn;
  key_interceptor_owner[0] = '\0';
  if (fn)
    Owner_ResolveForFunc((void *)fn, key_interceptor_owner,
                         sizeof(key_interceptor_owner));
}

void Key_PurgeInterceptorOwner(const char *path_a, const char *path_b) {
  if (key_interceptor &&
      Owner_ModuleMatches(key_interceptor_owner, path_a, path_b)) {
    key_interceptor = NULL;
    key_interceptor_owner[0] = '\0';
  }
}

bool Key_InvokeInterceptor(const PopupKeyEvent *key) {
  if (!key_interceptor)
    return false;
  return key_interceptor(key);
}

void Owner_ModuleBasename(const char *p, char *out, size_t n) {
  if (!out || n == 0)
    return;
  if (!p || !p[0]) {
    out[0] = '\0';
    return;
  }
  const char *s = p;
  for (const char *q = p; *q; q++)
    if (*q == '/' || *q == '\\')
      s = q + 1;
  snprintf(out, n, "%s", s);
}

bool Owner_ModuleMatches(const char *owner, const char *a, const char *b) {
  if (!owner || !owner[0])
    return false;
  if (a && a[0] && strcmp(owner, a) == 0)
    return true;
  if (b && b[0] && strcmp(owner, b) == 0)
    return true;
  char ob[OWNER_PATH_MAX], ab[OWNER_PATH_MAX], bb[OWNER_PATH_MAX];
  Owner_ModuleBasename(owner, ob, sizeof(ob));
  if (a && a[0]) {
    Owner_ModuleBasename(a, ab, sizeof(ab));
    if (strcmp(ob, ab) == 0)
      return true;
  }
  if (b && b[0]) {
    Owner_ModuleBasename(b, bb, sizeof(bb));
    if (strcmp(ob, bb) == 0)
      return true;
  }
  return false;
}

void Owner_ResolveForFunc(void *fn, char *out, size_t n) {
  if (!out || n == 0)
    return;
  out[0] = '\0';
  if (!fn)
    return;
  Platform_GetModuleOwnerPath(fn, out, n);
  out[n - 1] = '\0';
}

void *HttpAsyncWorker(void *arg) {
  HttpAsyncJob *job = (HttpAsyncJob *)arg;
  if (!job)
    return NULL;
  char *body = Platform_HttpGet(job->url);
  if (job->cb)
    job->cb(body ? body : "", body != NULL, job->userdata);
  free(body);
  free(job->url);
  ThreadRegistry_Finish(job);
  free(job);
  return NULL;
}

bool Owner_PurgeInactive(const char *path_a, const char *path_b) {
  return (!path_a || !path_a[0]) && (!path_b || !path_b[0]);
}
