#include "helpers/win32/raw_input_core_win32.h"

void win32_register_raw_mouse(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  RAWINPUTDEVICE rid;
  memset(&rid, 0, sizeof(rid));
  rid.usUsagePage = 0x01;
  rid.usUsage = 0x02;
  rid.dwFlags = RIDEV_INPUTSINK;
  rid.hwndTarget = hwnd;
  if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
    log_warn("RegisterRawInputDevices failed err=%lu", GetLastError());
  }
}

void win32_handle_raw_mouse_input(LPARAM lParam) {
  UINT size = 0;
  if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size,
                      sizeof(RAWINPUTHEADER)) != 0) {
    return;
  }
  if (size == 0 || size > 4096) {
    return;
  }
  BYTE *buf = (BYTE *)malloc(size);
  if (!buf) {
    return;
  }
  BOOL got = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buf, &size,
                             sizeof(RAWINPUTHEADER));
  RAWINPUT *raw = (RAWINPUT *)buf;
  if (!got || raw->header.dwType != RIM_TYPEMOUSE) {
    free(buf);
    return;
  }
  const RAWMOUSE *rm = &raw->data.mouse;
  POINT pt;
  if (!GetCursorPos(&pt)) {
    free(buf);
    return;
  }
  unsigned short flags = rm->usButtonFlags;
  int btn = 0;
  bool down = false;
  bool is_btn = true;
  if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
    btn = MOUSE_BTN_LEFT;
    down = true;
  } else if (flags & RI_MOUSE_LEFT_BUTTON_UP) {
    btn = MOUSE_BTN_LEFT;
    down = false;
  } else if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
    btn = MOUSE_BTN_RIGHT;
    down = true;
  } else if (flags & RI_MOUSE_RIGHT_BUTTON_UP) {
    btn = MOUSE_BTN_RIGHT;
    down = false;
  } else if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
    btn = MOUSE_BTN_MIDDLE;
    down = true;
  } else if (flags & RI_MOUSE_MIDDLE_BUTTON_UP) {
    btn = MOUSE_BTN_MIDDLE;
    down = false;
  } else
    is_btn = false;

  PlatformMouseEvent mev;
  memset(&mev, 0, sizeof(mev));
  mev.x = (int)pt.x;
  mev.y = (int)pt.y;
  mev.repeat = false;

  if (is_btn) {
    bool was_down = input_mouse_state_down(btn);
    if (down && was_down) {
      input_update_mouse_button(btn, false);
      PlatformMouseEvent rev;
      memset(&rev, 0, sizeof(rev));
      rev.button = btn;
      rev.x = mev.x;
      rev.y = mev.y;
      rev.down = false;
      rev.repeat = false;
      global_mouse_push(&rev);
    }
    input_update_mouse_button(btn, down);
    if (!down && btn > 0 && btn <= MOUSE_BTN_MAX) {
      hotkey_key_up(MOUSE_VK(btn));
    }
    mev.button = btn;
    mev.down = down;
    if (GetKeyState(VK_CONTROL) & 0x8000)
      mev.mods |= HOTKEY_MOD_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000)
      mev.mods |= HOTKEY_MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000)
      mev.mods |= HOTKEY_MOD_SHIFT;
    global_mouse_push(&mev);
    if (down && btn <= MOUSE_BTN_MAX) {
      static unsigned long long s_last_fire[MOUSE_BTN_MAX + 1] = {0};
      unsigned long long now = GetTickCount64();
      bool rising = !was_down;
      bool stale = (now - s_last_fire[btn]) > 200;
      if (rising || stale) {
        s_last_fire[btn] = now;
        hotkey_process(MOUSE_VK(btn), mev.mods);
      }
    }
  } else {
    static int s_last_mx = -1;
    static int s_last_my = -1;
    if (mev.x != s_last_mx || mev.y != s_last_my) {
      s_last_mx = mev.x;
      s_last_my = mev.y;
      if (!global_mouse_full()) {
        mev.button = 0;
        mev.down = false;
        global_mouse_push(&mev);
      }
    }
  }
  free(buf);
}

LRESULT CALLBACK win32_handle_global_key(int nCode, WPARAM wParam,
                                         LPARAM lParam) {

  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  if (nCode == HC_ACTION) {
    static bool s_key_is_down[256] = {0};
    bool _freshDown = false;
    const KBDLLHOOKSTRUCT *_rk = (KBDLLHOOKSTRUCT *)lParam;
    if (_rk->flags & LLKHF_INJECTED)
      return CallNextHookEx(NULL, nCode, wParam, lParam);
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
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
          _gev.mods |= HOTKEY_MOD_CTRL;
        if (GetAsyncKeyState(VK_MENU) & 0x8000)
          _gev.mods |= HOTKEY_MOD_ALT;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
          _gev.mods |= HOTKEY_MOD_SHIFT;
        if (GetAsyncKeyState(VK_LWIN) & 0x8000 ||
            GetAsyncKeyState(VK_RWIN) & 0x8000)
          _gev.mods |= HOTKEY_MOD_SUPER;
        _gev.down = _isDown;
        _gev.repeat = ((_rk->flags & LLKHF_INJECTED) != 0);
        if (_isDown)
          _freshDown = !s_key_is_down[_vk & 0xFF];
        static unsigned long long s_last_keydown_ms[256] = {0};
        unsigned long long now_ms = GetTickCount64();
        {
          int _hint = 0;
          if (_isDown) {
            if (_vk == 0x20)
              _hint = ' ';
            else if (_vk >= '0' && _vk <= '9') {
              if (_gev.mods & HOTKEY_MOD_SHIFT) {
                const char *sd = ")!@#$%^&*(";
                _hint = sd[_vk - '0'];
              } else
                _hint = _vk;
            } else if (_vk >= 'A' && _vk <= 'Z')
              _hint = (_gev.mods & HOTKEY_MOD_SHIFT) ? _vk : _vk - 'A' + 'a';
            else if (_vk >= 0x60 && _vk <= 0x69)
              _hint = '0' + (_vk - 0x60);
            else if (_vk == 0x6A)
              _hint = '*';
            else if (_vk == 0x6B)
              _hint = '+';
            else if (_vk == 0x6C)
              _hint = ',';
            else if (_vk == 0x6D)
              _hint = '-';
            else if (_vk == 0x6E)
              _hint = '.';
            else if (_vk == 0x6F)
              _hint = '/';
          }
          if (_isDown) {
            if (!s_key_is_down[_vk & 0xFF] ||
                (now_ms - s_last_keydown_ms[_vk & 0xFF]) >=
                    (unsigned long long)HotkeyRepeatDelayMs) {
              s_key_is_down[_vk & 0xFF] = true;
              s_last_keydown_ms[_vk & 0xFF] = now_ms;
              input_state_update(_vk, true, _gev.mods, _hint);
            }
          } else {
            s_key_is_down[_vk & 0xFF] = false;
            input_state_update(_vk, false, _gev.mods, 0);
          }
        }
        global_key_push(&_gev);
        if (_isUp)
          hotkey_key_up(_vk);
        if (remap_should_swallow_vk(_vk, _val)) {
          char *_t = Platform_GetActiveWindowTitle();
          log_info("hook swallow=remap vk=0x%X val=%d title='%s'", _vk, _val,
                   _t ? _t : "");
          return 1;
        }
      }
    }
    if (nCode == HC_ACTION &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
      Platform_DismissIndefiniteMessage();
      const KBDLLHOOKSTRUCT *pKeyInfo = (KBDLLHOOKSTRUCT *)lParam;

      bool alt_down = (pKeyInfo->flags & LLKHF_ALTDOWN) ||
                      (GetAsyncKeyState(VK_MENU) & 0x8000);
      bool ctrl_down = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
      bool shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
                        (GetAsyncKeyState(VK_LSHIFT) & 0x8000) ||
                        (GetAsyncKeyState(VK_RSHIFT) & 0x8000);
      bool super_down = (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                        (GetAsyncKeyState(VK_RWIN) & 0x8000);

      int pressed_vk = (int)pKeyInfo->vkCode;
      int modifiers = HOTKEY_MOD_NONE;
      if (alt_down)
        modifiers |= HOTKEY_MOD_ALT;
      if (shift_down)
        modifiers |= HOTKEY_MOD_SHIFT;
      if (ctrl_down)
        modifiers |= HOTKEY_MOD_CTRL;
      if (super_down)
        modifiers |= HOTKEY_MOD_SUPER;

      if (_freshDown && hotkey_process(pressed_vk, modifiers)) {
        pending_hotkey_vk = pressed_vk;
        pending_hotkey_mod = modifiers;
        pending_hotkey_timeMs = (long)GetTickCount();
        char *_t2 = Platform_GetActiveWindowTitle();
        log_info("hook swallow=hotkey vk=0x%X mods=%d title='%s'", pressed_vk,
                 modifiers, _t2 ? _t2 : "");
        return 1;
      }
    }
  }
  if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
    WindowManager *wmx = WindowManager_Get();
    if (!wmx || !WindowManager_GetTopPopup(wmx)) {
      KBDLLHOOKSTRUCT *rk = (KBDLLHOOKSTRUCT *)lParam;
      if (snippet_feed_vk((int)rk->vkCode, 1, true)) {
        log_info("hook swallow=snippet vk=0x%X", (int)rk->vkCode);
        return 1;
      }
    }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}
