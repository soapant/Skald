#include <stdbool.h>
#include <stdio.h>

#include "helpers/helpers_key.h"
#include "core/types/keys.h"
#include "platform/input_state.h"
#include "helpers/helpers_owner.h"

bool Key_IsCtrlPressed(void) {
  return Platform_IsKeyDown(VK_LCONTROL) || Platform_IsKeyDown(VK_RCONTROL) ||
         Platform_IsKeyDown(VK_CONTROL);
}

bool Key_IsShiftPressed(void) {
  return Platform_IsKeyDown(VK_LSHIFT) || Platform_IsKeyDown(VK_RSHIFT) ||
         Platform_IsKeyDown(VK_SHIFT);
}

bool Key_IsAltPressed(void) {
  return Platform_IsKeyDown(VK_LMENU) || Platform_IsKeyDown(VK_RMENU) ||
         Platform_IsKeyDown(VK_MENU);
}

bool Key_IsSuperPressed(void) {
  return Platform_IsKeyDown(VK_LWIN) || Platform_IsKeyDown(VK_RWIN);
}

static KeyInterceptorFn g_key_interceptor = NULL;
static char g_key_interceptor_owner[OWNER_PATH_MAX] = {0};

void Key_RegisterInterceptor(KeyInterceptorFn fn) {
  g_key_interceptor = fn;
  g_key_interceptor_owner[0] = '\0';
  if (fn)
    Owner_ResolveForFunc((void *)fn, g_key_interceptor_owner,
                         sizeof(g_key_interceptor_owner));
}

void Key_PurgeInterceptorOwner(const char *path_a, const char *path_b) {
  if (g_key_interceptor &&
      Owner_ModuleMatches(g_key_interceptor_owner, path_a, path_b)) {
    g_key_interceptor = NULL;
    g_key_interceptor_owner[0] = '\0';
  }
}

bool Key_InvokeInterceptor(const PopupKeyEvent *key) {
  if (!g_key_interceptor)
    return false;
  return g_key_interceptor(key);
}
