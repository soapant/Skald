#include <windows.h>

#define ID_TRAY_ICON 1001
#define WM_TRAYICON (WM_USER + 1)
#define ID_EXIT 2001

#include "helpers/helpers_fs.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "config_values.h"
#include "core/types.h"
#include "core/types/window.h"
#include "helpers/win32/subclasses_win32.h"
#include "platform/platform.h"
#include "helpers/win32/raw_input_core_win32.h"
#include "deps/log.h"

static HICON s_tray_icon = NULL;
static bool s_tray_icon_fallback = false;

LRESULT CALLBACK win32_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
  switch (msg) {
  case WM_CREATE: {
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;

    char expanded_icon[MAX_PATH];
    if (!expand_path(IconPath, expanded_icon, sizeof(expanded_icon)))
      expanded_icon[0] = '\0';

    nid.hIcon = (HICON)LoadImageA(NULL, expanded_icon, IMAGE_ICON, 0, 0,
                                  LR_LOADFROMFILE | LR_DEFAULTSIZE);
    if (!nid.hIcon) {
      log_error("tray: LoadImageA failed for '%s', using generic icon",
                expanded_icon[0] ? expanded_icon : IconPath);
      nid.hIcon = LoadIconA(NULL, IDI_APPLICATION);
      s_tray_icon_fallback = true;
    } else {
      s_tray_icon_fallback = false;
    }
    s_tray_icon = nid.hIcon;
    lstrcpy(nid.szTip, "Skald");

    if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
      log_error("tray: Shell_NotifyIcon(NIM_ADD) failed (%lu)", GetLastError());
    }
    break;
  }

  case WM_INPUT: {
    win32_handle_raw_mouse_input(lParam);
    break;
  }

  case WM_TRAYICON: {
    if (lParam == WM_RBUTTONUP) {
      POINT pt;
      GetCursorPos(&pt);

      HMENU hMenu = CreatePopupMenu();

      AppendMenu(hMenu, MF_STRING, ID_EXIT, "Exit");

      SetForegroundWindow(hwnd);
      TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0,
                     hwnd, NULL);
      DestroyMenu(hMenu);
    }
    break;
  }
  case WM_COMMAND: {
    if (LOWORD(wParam) == ID_EXIT) {
      PostQuitMessage(0);
    }

    break;
  }

  case WM_APP + 0x42: {
    config_reload();
    return 0;
  }
  case WM_DISPLAY_MESSAGE: {
    DisplayMessageParams *p = (DisplayMessageParams *)lParam;
    if (p != NULL)
      Platform_CreateDisplayMessageWindow(*p);
    return 0;
  }
  case WM_DISMISS_MESSAGE: {
    if (lParam != 0) {
      DismissMessageParams *p = (DismissMessageParams *)lParam;
      return Platform_DismissMessage(p ? p->name : NULL) ? 1 : 0;
    }
    Platform_DismissDisplayMessage();
    return 1;
  }

  case WM_DESTROY: {
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_ICON;
    Shell_NotifyIcon(NIM_DELETE, &nid);
    if (s_tray_icon && !s_tray_icon_fallback) {
      DestroyIcon(s_tray_icon);
    }
    s_tray_icon = NULL;

    PostQuitMessage(0);
    break;
  }

  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}
