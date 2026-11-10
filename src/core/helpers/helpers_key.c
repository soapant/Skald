#include <stdbool.h>
#include <stdio.h>

#include "helpers/helpers_key.h"
#include "core/types/keys.h"
#include "helpers/helpers_input_state.h"
#include "helpers/helpers_owner.h"

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
