#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "core/types.h"
#include "platform/win32/subclasses_win32.h"
#include "wingdi.h"
#include "platform/platform.h"
#include "platform/win32/win32_state.h"
#include "platform/win32/input_core_win32.h"

#include "core/timer.h"
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PROFILE_START(_pt);
switch (msg) {
  case WM_CREATE: {
    NOTIFYICONDATA nid = {0};
      nid.cbSize = sizeof(NOTIFYICONDATA);
      nid.hWnd = hwnd;
      nid.uID = ID_TRAY_ICON;
      nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
      nid.uCallbackMessage = WM_TRAYICON;
      
      char expanded_icon[MAX_PATH];
      if (!Platform_ExpandPath(IconPath, expanded_icon, sizeof(expanded_icon))) expanded_icon[0] = '\0';
      
      nid.hIcon = (HICON)LoadImageA(NULL,
                                   expanded_icon, 
                                   IMAGE_ICON, 
                                   0, 0, 
                                   LR_LOADFROMFILE | LR_DEFAULTSIZE);
      lstrcpy(nid.szTip, "Switch Window");
      
      Shell_NotifyIcon(NIM_ADD, &nid);
      break;
  }
    
  case WM_INPUT: {
    Platform_HandleRawMouseInput(lParam);
    break;
  }

  case WM_TRAYICON: {
    if (lParam == WM_RBUTTONUP) {
      POINT pt;
        GetCursorPos(&pt);
        
        HMENU hMenu = CreatePopupMenu();
        
        AppendMenu(hMenu, MF_STRING, ID_EXIT, "Exit");
        
        SetForegroundWindow(hwnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
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
    Config_Reload();
    return 0;
  }
  case WM_DISPLAY_MESSAGE: {
    DisplayMessageParams* p = (DisplayMessageParams*)lParam;
      if (p != NULL) {
        Platform_CreateDisplayMessageWindow(p->message, p->centerX, p->centerY, p->maxWidth, p->maxHeight, p->timeout, p->color, p->no_bg, p->render);
      }
    return 0;
  }
        
  case WM_DESTROY: {
    NOTIFYICONDATA nid = {0};
      nid.cbSize = sizeof(NOTIFYICONDATA);
      nid.hWnd = hwnd;
      nid.uID = ID_TRAY_ICON;
      Shell_NotifyIcon(NIM_DELETE, &nid);
    if (g_win32_hMainFont) {
      DeleteObject(g_win32_hMainFont);
        g_win32_hMainFont = NULL;
    }
    PostQuitMessage(0);
      break;
  }
          
  default:
          return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  { PROFILE_END_VAR(_pt, "WndProc"); return 0; }
}
