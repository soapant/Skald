#include <windows.h>
#include <stdbool.h>
#include "deps/log.h"
#include "core/context.h"
#include "core/types.h"
#include "core/vim.h"
#include "core/hotkey_manager.h"
#include "core/platform.h"
#include "core/window_manager.h"
#include "core/snippet_manager.h"
#include "core/remap_manager.h"
#include "core/timer.h"
#include "bindings/bindings.h"
#include "platform/win32/win32_state.h"
#include "platform/win32/input_core_win32.h"

LRESULT CALLBACK win32_handle_global_key(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  if (nCode == HC_ACTION) {
    bool _popupActive = false;
    WindowManager* _wm = WindowManager_Get();
    if (_wm && WindowManager_GetTopPopup(_wm)) _popupActive = true;
    if (! _popupActive && WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) _popupActive = true;
    KBDLLHOOKSTRUCT* _rk = (KBDLLHOOKSTRUCT*)lParam;
    if (_rk->flags & LLKHF_INJECTED) return CallNextHookEx(NULL, nCode, wParam, lParam);
    {
        bool _isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool _isUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
        if (_isDown || _isUp) {
            int _vk = (int)_rk->vkCode;
            int _val = _isDown ? 1 : 0;
            if (remap_should_swallow_vk(_vk, _val)) {
                char* _t = Platform_GetActiveWindowTitle();
                log_info("hook swallow=remap vk=0x%X val=%d title='%s'", _vk, _val, _t ? _t : "");
                return 1;
            }
        }
    }
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
    if (g_win32_timeout == 6969 && WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage))) {
      KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
      DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
      FlintContext_Get()->window.hDisplayMessage = WH_NULL;
    }
    KBDLLHOOKSTRUCT* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;

    bool alt_down   = (pKeyInfo->flags & LLKHF_ALTDOWN) || (GetAsyncKeyState(VK_MENU) & 0x8000);
    bool ctrl_down  = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
    bool shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
      (GetAsyncKeyState(VK_LSHIFT) & 0x8000) ||
      (GetAsyncKeyState(VK_RSHIFT) & 0x8000);

    double seconds = Platform_GetUniversalTimeSeconds();
    if (seconds >= 300) {
      Platform_ResetUniversalTimer();
      log_info("SUPPOSED TO CLEAR FILE");
    }


    int pressed_vk = (int)pKeyInfo->vkCode;
    int modifiers = HOTKEY_MOD_NONE;
    if (alt_down) modifiers |= HOTKEY_MOD_ALT;
    if (shift_down) modifiers |= HOTKEY_MOD_SHIFT;
    if (ctrl_down) modifiers |= HOTKEY_MOD_CTRL;

    PROFILE_START(timer);
    if (! _popupActive && hotkey_process(pressed_vk, modifiers)) {
      char* _t2 = Platform_GetActiveWindowTitle();
      log_info("hook swallow=hotkey vk=0x%X mods=%d title='%s'", pressed_vk, modifiers, _t2 ? _t2 : "");
      return 1;
    }
    if (alt_down && wParam == SETTINGS_KEY) {
      Platform_SettingsPopUp();
      return true;
    }
    PROFILE_END_VAR(timer, "ProcessHotkey");

    PROFILE_START(timer3);

    if (IsNormalMode()) {
      if (g_win32_waiting_for_key) {
        g_win32_waiting_for_key = false;
        if (g_win32_key_callback) {
          g_win32_key_callback(pKeyInfo->vkCode);
          PROFILE_END_VAR(timer3, "KeyCallback");
        }
        log_info("hook swallow=waiting_for_key vk=0x%X", pKeyInfo->vkCode);
        return 1;
      }
    }
    PROFILE_END_VAR(timer3, "NormalModeCheck");


  }

  }
  if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
    WindowManager* wmx = WindowManager_Get();
    if (!wmx || !WindowManager_GetTopPopup(wmx)) {
      KBDLLHOOKSTRUCT* rk = (KBDLLHOOKSTRUCT*)lParam;
      if (snippet_feed_vk((int)rk->vkCode, 1, true)) {
        log_info("hook swallow=snippet vk=0x%X", (int)rk->vkCode);
        return 1;
      }
    }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}
