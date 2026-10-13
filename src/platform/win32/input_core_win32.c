#include <windows.h>
#include <stdbool.h>
#include <string.h>
#include "deps/log.h"
#include "core/context.h"
#include "core/types.h"
#include "core/vim.h"
#include "core/hotkey_manager.h"
#include "platform/platform.h"
#include "core/window_manager.h"
#include "core/snippet_manager.h"
#include "core/remap_manager.h"
#include "core/timer.h"
#include "platform/input_state.h"
#include "bindings/bindings.h"
#include "platform/win32/win32_state.h"
#include "platform/win32/subclasses_win32.h"
#include "platform/win32/input_core_win32.h"
#include "config.h"

static int win32_mouse_button_from_vk(WPARAM wParam) {
        PROFILE_START(_pt);
switch (wParam) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:    return MOUSE_BTN_LEFT;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:    return MOUSE_BTN_RIGHT;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:    return MOUSE_BTN_MIDDLE;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:    return MOUSE_BTN_SIDE1;
        default:              return 0;
    }
}

bool Platform_GetMousePos(int* out_x, int* out_y) {
        PROFILE_START(_pt);
POINT pt;
    if (!GetCursorPos(&pt)) { PROFILE_END_VAR(_pt, "Platform_GetMousePos"); return false; }
    if (out_x) *out_x = (int)pt.x;
    if (out_y) *out_y = (int)pt.y;
    { PROFILE_END_VAR(_pt, "Platform_GetMousePos"); return true; }
}

LRESULT CALLBACK win32_handle_global_mouse(int nCode, WPARAM wParam, LPARAM lParam) {
    PROFILE_START(_pt);
if (nCode < 0) { PROFILE_END_VAR(_pt, "win32_handle_global_mouse"); return CallNextHookEx(NULL, nCode, wParam, lParam); }
  if (nCode != HC_ACTION) { PROFILE_END_VAR(_pt, "win32_handle_global_mouse"); return CallNextHookEx(NULL, nCode, wParam, lParam); }

  MSLLHOOKSTRUCT* _rm = (MSLLHOOKSTRUCT*)lParam;
  if (!_rm) { PROFILE_END_VAR(_pt, "win32_handle_global_mouse"); return CallNextHookEx(NULL, nCode, wParam, lParam); }
  if (_rm->flags & LLMHF_INJECTED) { PROFILE_END_VAR(_pt, "win32_handle_global_mouse"); return CallNextHookEx(NULL, nCode, wParam, lParam); }

  if (wParam == WM_MOUSEMOVE || wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
      PlatformMouseEvent _mev;
      memset(&_mev, 0, sizeof(_mev));
      _mev.button = 0;
      _mev.x = (int)_rm->pt.x;
      _mev.y = (int)_rm->pt.y;
      _mev.mods = 0;
      if (GetKeyState(VK_CONTROL) & 0x8000) _mev.mods |= HOTKEY_MOD_CTRL;
      if (GetKeyState(VK_MENU) & 0x8000) _mev.mods |= HOTKEY_MOD_ALT;
      if (GetKeyState(VK_SHIFT) & 0x8000) _mev.mods |= HOTKEY_MOD_SHIFT;
      _mev.down = false;
      _mev.repeat = false;
      Platform_GlobalMouse_Push(&_mev);
      return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  int _btn = win32_mouse_button_from_vk(wParam);
  if (_btn == 0) { PROFILE_END_VAR(_pt, "win32_handle_global_mouse"); return CallNextHookEx(NULL, nCode, wParam, lParam); }

  bool _isDown = (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                  wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN);
  bool _isUp = !_isDown;
  if (_isUp && wParam == WM_XBUTTONUP) {
      _btn = (_rm->mouseData == XBUTTON2) ? MOUSE_BTN_SIDE2 : MOUSE_BTN_SIDE1;
  }
  Platform_UpdateMouseButton(_btn, _isDown);

  PlatformMouseEvent _mev;
  memset(&_mev, 0, sizeof(_mev));
  _mev.button = _btn;
  _mev.x = (int)_rm->pt.x;
  _mev.y = (int)_rm->pt.y;
  _mev.mods = 0;
  if (GetKeyState(VK_CONTROL) & 0x8000) _mev.mods |= HOTKEY_MOD_CTRL;
  if (GetKeyState(VK_MENU) & 0x8000) _mev.mods |= HOTKEY_MOD_ALT;
  if (GetKeyState(VK_SHIFT) & 0x8000) _mev.mods |= HOTKEY_MOD_SHIFT;
  _mev.down = _isDown;
  _mev.repeat = false;
  Platform_GlobalMouse_Push(&_mev);
  if (_isDown && _btn > 0 && _btn <= FL_MOUSE_BTN_MAX) {
    hotkey_process(FL_MOUSE_VK(_btn), _mev.mods);
  }

  { PROFILE_END_VAR(_pt, "win32_handle_global_mouse"); return CallNextHookEx(NULL, nCode, wParam, lParam); }
}

LRESULT CALLBACK win32_handle_global_key(int nCode, WPARAM wParam, LPARAM lParam) {
    PROFILE_START(_pt);
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
            PlatformKeyEvent _gev;
            _gev.vk = _vk;
            _gev.scancode = (int)_rk->scanCode;
            _gev.mods = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) _gev.mods |= HOTKEY_MOD_CTRL;
            if (GetKeyState(VK_MENU) & 0x8000) _gev.mods |= HOTKEY_MOD_ALT;
            if (GetKeyState(VK_SHIFT) & 0x8000) _gev.mods |= HOTKEY_MOD_SHIFT;
            if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) _gev.mods |= HOTKEY_MOD_SUPER;
            _gev.down = _isDown;
            _gev.repeat = ((_rk->flags & LLKHF_INJECTED) != 0);
            Platform_GlobalKey_Push(&_gev);
            if (remap_should_swallow_vk(_vk, _val)) {
                char* _t = Platform_GetActiveWindowTitle();
                log_info("hook swallow=remap vk=0x%X val=%d title='%s'", _vk, _val, _t ? _t : "");
                return 1;
            }
        }
    }
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
    HWND hMsgWnd = WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage);
    if (hMsgWnd && IsWindow(hMsgWnd)) {
      FadeWindowData* fd = (FadeWindowData*)GetWindowLongPtrA(hMsgWnd, GWLP_USERDATA);
      if (fd && fd->timeout == 6969.0) {
        KillTimer(hMsgWnd, 999);
          DestroyWindow(hMsgWnd);
          FlintContext_Get()->window.hDisplayMessage = WH_NULL;
      }
    }
    KBDLLHOOKSTRUCT* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;

    bool alt_down   = (pKeyInfo->flags & LLKHF_ALTDOWN) || (GetAsyncKeyState(VK_MENU) & 0x8000);
    bool ctrl_down  = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
    bool shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
      (GetAsyncKeyState(VK_LSHIFT) & 0x8000) ||
      (GetAsyncKeyState(VK_RSHIFT) & 0x8000);

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
  { PROFILE_END_VAR(_pt, "win32_handle_global_key"); return CallNextHookEx(NULL, nCode, wParam, lParam); }
}

int Platform_CharForKey(int vk, int scancode, int mods) {
    PROFILE_START(_pt);
BYTE state[256];
  memset(state, 0, sizeof(state));
  if (mods & HOTKEY_MOD_SHIFT) state[VK_SHIFT] = 0x80;
  if (mods & HOTKEY_MOD_CTRL) state[VK_CONTROL] = 0x80;
  if (mods & HOTKEY_MOD_ALT) state[VK_MENU] = 0x80;
  HKL layout = GetKeyboardLayout(0);
  wchar_t buf[8];
  int n = ToUnicodeEx((UINT)vk, (UINT)scancode, state, buf, 8, 0, layout);
  if (n <= 0) { PROFILE_END_VAR(_pt, "Platform_CharForKey"); return 0; }
  if (n == 1) { PROFILE_END_VAR(_pt, "Platform_CharForKey"); return (int)buf[0]; }
  char mb[16];
  int m = WideCharToMultiByte(CP_UTF8, 0, buf, 1, mb, sizeof(mb), NULL, NULL);
  if (m <= 0) { PROFILE_END_VAR(_pt, "Platform_CharForKey"); return 0; }
  { PROFILE_END_VAR(_pt, "Platform_CharForKey"); return (int)(unsigned char)mb[0]; }
}
