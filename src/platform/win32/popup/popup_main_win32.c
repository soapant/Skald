#include <windows.h>
#include <string.h>
#include "platform/win32/win32_state.h"
#include "deps/log.h"
#include "core/context.h"
#include "core/modal.h"
#include "core/window_manager.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "platform/draw.h"
#include "platform/ui.h"
#include "core/feature_data.h"
#include "platform/win32/subclasses_win32.h"
#include "platform/input_state.h"
#include "wingdi.h"
#include "platform/platform.h"
#include "helpers/helpers_alias.h"
#include "helpers/helpers_ui.h"
#include "platform/win32/popup_helpers_win32.h"
#include "platform/flapi_platform.h"
#include "core/prompt_controller.h"

static bool s_swallow_next_char = false;

static bool PromptWindow_OnKey(HWND hwnd, const PopupKeyEvent* key) {
    int wParam = key->vk;
    bool ctrl_pressed = key->ctrl;
    bool shift_pressed = key->shift;

    FlintContext_Get()->window.hActiveEditControl = FlintContext_Get()->window.hEditControl;
    if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) return false;

    ListBoxInfo* lb = GetLBFromChild(hwnd);

    if (PromptController_TryVimMotion(wParam, ctrl_pressed, shift_pressed, key->alt)) {
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
            HWND hPar = GetParent(hwnd);
            if (hPar && IsWindow(hPar)) InvalidateRect(hPar, NULL, FALSE);
        }
        return true;
    }

    if (!ctrl_pressed) {
        if (wParam == VK_UP || wParam == VK_DOWN) {
            if (WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL) {
                if (wParam == VK_UP) {
                    UI_NavigateUp(lb, FlintContext_Get()->window.hListBox);
                } else {
                    UI_NavigateDown(lb, FlintContext_Get()->window.hListBox);
                }
            }
            return true;
        }
    }

    if (wParam == VK_RETURN) {
        bool ctrl_down = key->ctrl;
        FlintContext_Get()->window.output_buffer[0] = '\0';
        if (UI_IsListBoxSelected(lb)) {
            if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
            FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
        } else {
            GetWindowTextA(hwnd, FlintContext_Get()->window.output_buffer, sizeof(FlintContext_Get()->window.output_buffer));
        }
        if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
            KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
            DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
            FlintContext_Get()->window.hDisplayMessage = WH_NULL;
        }
        if (ctrl_down) {
            bool should_close = false;
            if (FlintContext_Get()->prompt_callbacks.ctrl_enter != NULL) {
                char sel_copy[256];
                strncpy(sel_copy, FlintContext_Get()->window.output_buffer, sizeof(sel_copy) - 1);
                sel_copy[sizeof(sel_copy) - 1] = '\0';
                should_close = FlintContext_Get()->prompt_callbacks.ctrl_enter(sel_copy, lb);
            } else {
                should_close = false;
            }
            if (should_close) FlintContext_Get()->window.selection_complete = true;
            else if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL) ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
            return true;
        } else {
            bool should_close = false;
            if (FlintContext_Get()->prompt_callbacks.enter != NULL) {
                char sel_copy[256];
                strncpy(sel_copy, FlintContext_Get()->window.output_buffer, sizeof(sel_copy) - 1);
                sel_copy[sizeof(sel_copy) - 1] = '\0';
                should_close = FlintContext_Get()->prompt_callbacks.enter(sel_copy, lb);
            } else {
                should_close = true;
            }
            if (should_close) FlintContext_Get()->window.selection_complete = true;
            else if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) != NULL) ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
            return true;
        }
    }

    if (wParam == VK_ESCAPE) {
        if (FlintContext_Get()->prompt_callbacks.refresh) {
            if (IsInsertMode()) {
                SetModeWithListBox(MODE_NORMAL, lb);
            } else {
                FlintContext_Get()->window.output_buffer[0] = '\0';
                FlintContext_Get()->window.selection_complete = true;
                if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
                    KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                    DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
                }
            }
        } else {
            FlintContext_Get()->window.output_buffer[0] = '\0';
            FlintContext_Get()->window.selection_complete = true;
            if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
                KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
            }
        }
        return true;
    }

    if (wParam == VK_TAB) {
        struct feature_data* data = GetFeatureData(FlintContext_Get()->window.current_feature);
        if (FlintContext_Get()->prompt_callbacks.tab != NULL) {
            FlintContext_Get()->prompt_callbacks.tab(lb);
        }
        for (int i = 0; i < data->selection_count; ++i) {
            log_info("%s ", data->last_selections[i]);
        }
        return true;
    }

    if (wParam == VK_BACK) {
        if (ctrl_pressed) {
            UI_SetText(FlintContext_Get()->window.hEditControl, "");
            UI_Redraw(lb, FlintContext_Get()->window.hListBox);
            InvalidateRect(hwnd, NULL, TRUE);
            return true;
        } else {
            char buf[512] = {0};
            GetWindowTextA(hwnd, buf, sizeof(buf));
            DWORD selStart = 0, selEnd = 0;
            SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (WPARAM)&selEnd);
            int len = (int)strlen(buf);
            if (selStart != selEnd) {
                memmove(buf + selStart, buf + selEnd, len - selEnd + 1);
                SetWindowTextA(hwnd, buf);
                SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
            } else if (selStart > 0) {
                memmove(buf + selStart - 1, buf + selStart, len - selStart + 1);
                SetWindowTextA(hwnd, buf);
                SendMessageA(hwnd, EM_SETSEL, selStart - 1, selStart - 1);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return true;
        }
    }

    if (wParam == VK_DELETE) {
        char buf[512] = {0};
        GetWindowTextA(hwnd, buf, sizeof(buf));
        DWORD selStart = 0, selEnd = 0;
        SendMessageA(hwnd, EM_GETSEL, (WPARAM)&selStart, (WPARAM)&selEnd);
        int len = (int)strlen(buf);
        if (selStart != selEnd) {
            memmove(buf + selStart, buf + selEnd, len - selEnd + 1);
            SetWindowTextA(hwnd, buf);
            SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
        } else if (selStart < (DWORD)len) {
            memmove(buf + selStart, buf + selStart + 1, len - selStart);
            SetWindowTextA(hwnd, buf);
            SendMessageA(hwnd, EM_SETSEL, selStart, selStart);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return true;
    }

    if (wParam == COPY_SELECTION_KEY) {
        bool copy_ctrl = key->ctrl;
        if (shift_pressed) {
            if (UI_IsListBoxSelected(lb)) {
                if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
                FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
            }
            int true_idx = -1;
            if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) true_idx = lb->matching_indices[lb->selected_index];
            else true_idx = UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer);
            if (FlintContext_Get()->prompt_callbacks.copy != NULL) {
                FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer, true_idx, 0);
            } else {
                Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
            }
            return true;
        } else if (copy_ctrl) {
            if (UI_IsListBoxSelected(lb)) {
                if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
                FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
            }
            int true_idx = -1;
            if (lb && lb->selected_index >= 0 && lb->selected_index < lb->populated_count) true_idx = lb->matching_indices[lb->selected_index];
            else true_idx = UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer);
            if (FlintContext_Get()->prompt_callbacks.copy != NULL) {
                FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer, true_idx, 1);
            } else {
                Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
            }
            return true;
        }
        return false;
    }

    if (wParam == HELP_SCREEN_KEY && ctrl_pressed) {
      if (FlintContext_Get()->prompt_callbacks.help) FlintContext_Get()->prompt_callbacks.help();
      return true;
    }

    if (wParam == VK_DOWN) {
        if (ctrl_pressed) {
            char text[256];
            UI_GetCurrentTextEdit(FlintContext_Get()->window.hEditControl, text, sizeof(text));
            UI_SetCursorPosition(FlintContext_Get()->window.hEditControl, strlen(text));
        }
        return true;
    }

    if (wParam == VK_UP) {
        if (ctrl_pressed) {
            UI_SetCursorPosition(FlintContext_Get()->window.hEditControl, 1);
        }
        return true;
    }

    if (ctrl_pressed) {
        bool is_minus = (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT);
        bool is_plus  = (wParam == VK_OEM_PLUS  || wParam == VK_ADD || wParam == 0xBB || wParam == 0xBD);
        if (is_minus || is_plus) {
            int old = FontSize;
            if (is_minus) Platform_AdjustFontSize(-1); else Platform_AdjustFontSize(1);
            if (FontSize != old) {
                if (g_win32_hMainFont) { DeleteObject(g_win32_hMainFont); g_win32_hMainFont = NULL; }
                g_win32_hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
                if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hEditControl))) {
                    UI_Edit_SetFont(FlintContext_Get()->window.hEditControl, g_win32_hMainFont);
                }
                if (WH_TO_HWND(FlintContext_Get()->window.hListBox) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox))) {
                    UI_LB_SetFont(FlintContext_Get()->window.hListBox, g_win32_hMainFont);
                    int newItemH = FontSize + 14;
                    if (newItemH < 24) newItemH = 24;
                    if (newItemH > 64) newItemH = 64;
                    UI_LB_SetItemHeight(FlintContext_Get()->window.hListBox, newItemH);
                }
                HWND hPopup = WH_TO_HWND(FlintContext_Get()->window.hPopupWnd);
                if (hPopup && IsWindow(hPopup)) {
                    InvalidateRect(hPopup, NULL, TRUE);
                    if (WH_TO_HWND(FlintContext_Get()->window.hListBox)) InvalidateRect(WH_TO_HWND(FlintContext_Get()->window.hListBox), NULL, TRUE);
                    if (WH_TO_HWND(FlintContext_Get()->window.hEditControl)) InvalidateRect(WH_TO_HWND(FlintContext_Get()->window.hEditControl), NULL, TRUE);
                    UpdateWindow(hPopup);
                }
            }
            Config_Save();
            return true;
        }
    }

    return false;
}

static void Win32_PaintEditCaret(HDC memDC, HWND hwnd) {
  if (GetFocus() != hwnd) return;
  DWORD s = 0, e = 0;
  SendMessageA(hwnd, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
  if (s != e) return;
  HFONT hFont = (HFONT)SendMessageA(hwnd, WM_GETFONT, 0, 0);
  HFONT oldFont = hFont ? (HFONT)SelectObject(memDC, hFont) : NULL;
  TEXTMETRIC tm;
  memset(&tm, 0, sizeof(tm));
  GetTextMetrics(memDC, &tm);
  int emSize = tm.tmAscent - tm.tmInternalLeading + tm.tmDescent;
  if (emSize <= 0) emSize = FontSize;
  int inkTop = 0;
  int inkOk = 0;
  {
    GLYPHMETRICS gm;
    memset(&gm, 0, sizeof(gm));
    MAT2 mat;
    memset(&mat, 0, sizeof(mat));
    mat.eM11.value = 1;
    mat.eM22.value = 1;
    if (GetGlyphOutline(memDC, 'x', GGO_METRICS, &gm, 0, NULL, &mat) != GDI_ERROR && gm.gmptGlyphOrigin.y > 0) {
      inkTop = tm.tmAscent - (int)gm.gmptGlyphOrigin.y;
      inkOk = 1;
    } else {
      inkTop = (emSize * 48 + 50) / 100;
    }
    if (inkTop < 0) inkTop = 0;
  }
  int xHeight = tm.tmAscent - inkTop;
  {
    static HWND loggedHwnd = NULL;
    static int loggedTick = 0;
    if (hwnd != loggedHwnd) { loggedHwnd = hwnd; loggedTick = 0; }
    if (loggedTick < 3) {
      loggedTick++;
      log_info("PaintCaret tmH=%d asc=%d desc=%d il=%d em=%d inkOk=%d ink=%d xh=%d", tm.tmHeight, tm.tmAscent, tm.tmDescent, tm.tmInternalLeading, emSize, inkOk, inkTop, xHeight);
    }
  }
  if (oldFont) SelectObject(memDC, oldFont);
  RECT rc;
  GetClientRect(hwnd, &rc);
  char buf[512];
  GetWindowTextA(hwnd, buf, sizeof(buf));
  int len = (int)strlen(buf);
  DWORD q = s;
  if ((int)q > len) q = (DWORD)len;
  int cx;
  int lineTop;
  if (len <= 0) {
    cx = 2;
    lineTop = (rc.bottom - tm.tmHeight) / 2;
  } else {
    LRESULT coords = SendMessageA(hwnd, EM_POSFROMCHAR, q, 0);
    if (coords == (LRESULT)-1) {
      coords = SendMessageA(hwnd, EM_POSFROMCHAR, (WPARAM)(len - 1), 0);
      if (coords == (LRESULT)-1) return;
      HFONT mf = (HFONT)SendMessageA(hwnd, WM_GETFONT, 0, 0);
      HFONT mo = mf ? (HFONT)SelectObject(memDC, mf) : NULL;
      char lastc[2] = { buf[len - 1], '\0' };
      SIZE wsz = {0, 0};
      GetTextExtentPoint32A(memDC, lastc, 1, &wsz);
      if (mo) SelectObject(memDC, mo);
      cx = (short)LOWORD(coords) + wsz.cx;
      lineTop = (short)HIWORD(coords);
    } else {
      int emX = (short)LOWORD(coords);
      HFONT qf = (HFONT)SendMessageA(hwnd, WM_GETFONT, 0, 0);
      HFONT qo = qf ? (HFONT)SelectObject(memDC, qf) : NULL;
      SIZE qs = {0, 0};
      int qq = (int)q < (int)sizeof(buf)-1 ? (int)q : (int)sizeof(buf)-1;
      char qtmp[512];
      memcpy(qtmp, buf, qq);
      qtmp[qq] = '\0';
      GetTextExtentPoint32A(memDC, qtmp, qq, &qs);
      if (qo) SelectObject(memDC, qo);
      int measX = (int)qs.cx + 2;
      int slack = tm.tmAveCharWidth > 0 ? tm.tmAveCharWidth : 8;
      cx = emX;
      if (cx < 0 || cx > measX + 2 + slack) {
        log_info("PaintCaret reject emX=%d measX=%d q=%d len=%d", emX, measX, (int)q, len);
        cx = measX;
      }
      lineTop = (short)HIWORD(coords);
    }
  }
  int caretH = FontSize;
  if (caretH > rc.bottom && rc.bottom > 0) caretH = rc.bottom;
  if (caretH <= 0) return;
  int caretY = (lineTop + inkTop + lineTop + tm.tmAscent - caretH) / 2;
  if (caretY < lineTop) caretY = lineTop;
  if (rc.bottom > 0 && caretY + caretH > rc.bottom) caretY = rc.bottom - caretH;
  int caretW = 2;
  if (!IsInsertMode()) caretW = tm.tmAveCharWidth > 0 ? tm.tmAveCharWidth : 8;
  if (cx < 0) cx = 0;
  if (cx + caretW > rc.right && rc.right > caretW) cx = rc.right - caretW;
  HBRUSH cbr = CreateSolidBrush(COLOR_CURSOR);
  RECT cRc = {cx, caretY, cx + caretW, caretY + caretH};
  FillRect(memDC, &cRc, cbr);
  DeleteObject(cbr);
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  if(msg==WM_KEYDOWN){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,true,mods); }
  if(msg==WM_KEYUP){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,false,mods); }
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (GetFocus() == hwnd) DestroyCaret();
    RECT rc; GetClientRect(hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(hwnd, &ps); return 0; }
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    if (!memDC || !memBmp) {
      if (memDC) DeleteDC(memDC);
      if (memBmp) DeleteObject(memBmp);
      EndPaint(hwnd, &ps);
      return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
    HWND hPar = GetParent(hwnd);
    if (hPar && IsWindow(hPar)) {
      RECT winRc; GetClientRect(hPar, &winRc);
      int winW = winRc.right - winRc.left;
      int winH = winRc.bottom - winRc.top;
      POINT pt = {0,0};
      ClientToScreen(hwnd, &pt);
      ScreenToClient(hPar, &pt);
      if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
        win32_draw_background_slice(memDC, &rc, COLOR_BG, winW, winH, pt.x, pt.y);
      } else {
        HBRUSH br = CreateSolidBrush(COLOR_BG);
        FillRect(memDC, &rc, br);
        DeleteObject(br);
      }
    } else {
      HBRUSH br = CreateSolidBrush(COLOR_BG);
      FillRect(memDC, &rc, br);
      DeleteObject(br);
    }
    SendMessage(hwnd, WM_PRINTCLIENT, (WPARAM)memDC, PRF_CLIENT);
    Win32_PaintEditCaret(memDC, hwnd);
    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) return 1;
  if (msg == WM_SETFOCUS) {
    LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
    DestroyCaret();
    return res;
  }
    if (msg == WM_CHAR) {
      if (s_swallow_next_char) { s_swallow_next_char = false; return 0; }
      if (IsNormalMode()) return 0;
      if (wParam == 0x08 || wParam == 0x7F) return 0;
      return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
    if (msg == WM_KEYDOWN) {
      s_swallow_next_char = false;
      PopupKeyEvent key;
      memset(&key, 0, sizeof(key));
      key.vk = (int)wParam;
      key.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      key.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      key.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
      bool was_normal = IsNormalMode();
      if (PromptWindow_OnKey(hwnd, &key)) {
        (void)was_normal;
        s_swallow_next_char = true;
        if (FlintContext_Get()->prompt_callbacks.render != NULL) {
          HWND hPar = GetParent(hwnd);
          if (hPar && IsWindow(hPar)) InvalidateRect(hPar, NULL, FALSE);
        }
        return 0;
      }
      if (FlintContext_Get()->prompt_callbacks.render != NULL) {
        HWND hPar = GetParent(hwnd);
        if (hPar && IsWindow(hPar)) InvalidateRect(hPar, NULL, FALSE);
      }
    }

    LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
    if (msg == WM_KEYDOWN || msg == WM_CHAR || msg == WM_LBUTTONDOWN) {
      HWND hPar = GetParent(hwnd);
      if (hPar && IsWindow(hPar)) PostMessageA(hPar, WM_SYNC_TEXT_CARET, 0, 0);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return res;
} 

LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData){
  static HBRUSH s_bg_brush = NULL;
    switch (uMsg) {
    case WM_NCDESTROY:
                      RemoveWindowSubclass(hWnd, ListBoxSubclassProc, uIdSubclass);
                        if (s_bg_brush) { DeleteObject(s_bg_brush); s_bg_brush = NULL; }
                            break;
    case WM_ERASEBKGND:
                       return TRUE;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      RECT rc; GetClientRect(hWnd, &rc);
      if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(hWnd, &ps); return 0; }
      HDC memDC = CreateCompatibleDC(hdc);
      HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
      if (!memDC || !memBmp) {
        if (memDC) DeleteDC(memDC);
        if (memBmp) DeleteObject(memBmp);
        EndPaint(hWnd, &ps);
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
      }
      HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
      HWND hParent = GetParent(hWnd);
      RECT winRc; GetClientRect(hParent ? hParent : hWnd, &winRc);
      int winW = winRc.right - winRc.left;
      int winH = winRc.bottom - winRc.top;
      POINT lbOrigin = {0,0};
      if (hParent) { ClientToScreen(hWnd, &lbOrigin); ScreenToClient(hParent, &lbOrigin); }
      if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
        RECT full = rc;
        win32_draw_background_slice(memDC, &full, COLOR_BG, winW, winH, lbOrigin.x, lbOrigin.y);
      } else {
        if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
        FillRect(memDC, &rc, s_bg_brush);
      }
      SendMessage(hWnd, WM_PRINTCLIENT, (WPARAM)memDC, PRF_CLIENT);
      {
        int itemH = (int)SendMessageA(hWnd, LB_GETITEMHEIGHT, 0, 0);
        int count = (int)SendMessageA(hWnd, LB_GETCOUNT, 0, 0);
        if (itemH > 0 && count >= 0) {
          LONG itemsBottom = (LONG)count * itemH;
          if (itemsBottom < 0) itemsBottom = 0;
          if (itemsBottom < rc.bottom) {
            RECT empty = { rc.left, itemsBottom, rc.right, rc.bottom };
            RECT inter;
            if (empty.top < ps.rcPaint.bottom && empty.bottom > ps.rcPaint.top) {
              if (!IntersectRect(&inter, &empty, &ps.rcPaint)) inter = empty;
              if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
                int gx = lbOrigin.x + empty.left;
                int gy = lbOrigin.y + empty.top;
                win32_draw_background_slice(memDC, &empty, COLOR_BG, winW, winH, gx, gy);
              } else {
                if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
                FillRect(memDC, &empty, s_bg_brush);
              }
            }
          }
        } else if (count == 0) {
          if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
            RECT full = rc;
            win32_draw_background_slice(memDC, &full, COLOR_BG, winW, winH, lbOrigin.x, lbOrigin.y);
          } else {
            if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
            FillRect(memDC, &rc, s_bg_brush);
          }
        }
      }
      BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
      SelectObject(memDC, oldBmp);
      DeleteObject(memBmp);
      DeleteDC(memDC);
      EndPaint(hWnd, &ps);
      return 0;
    }
    default:
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
  return 0;
}

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if(msg==WM_KEYDOWN){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,true,mods); }
  if(msg==WM_KEYUP){ int mods=0; if(GetKeyState(VK_CONTROL)&0x8000) mods|=HOTKEY_MOD_CTRL; if(GetKeyState(VK_MENU)&0x8000) mods|=HOTKEY_MOD_ALT; if(GetKeyState(VK_SHIFT)&0x8000) mods|=HOTKEY_MOD_SHIFT; if(GetKeyState(VK_LWIN)&0x8000||GetKeyState(VK_RWIN)&0x8000) mods|=HOTKEY_MOD_SUPER; Platform_InputState_Update((int)wParam,false,mods); }
  ListBoxInfo* lb = GetLBForPopupWindow(hwnd);
  static HBRUSH s_bg_brush = NULL;
    static HBRUSH s_bg_gradient_brushes[128] = {0};
    static bool s_bg_gradient_ready = false;
    static HBRUSH s_sel_brush = NULL;
    (void)s_bg_gradient_brushes; (void)s_bg_gradient_ready;

    
    switch (msg) {
    case WM_KEYDOWN: {
      s_swallow_next_char = false;
      HWND hEdit = WH_TO_HWND(FlintContext_Get()->window.hEditControl);
      HWND hKey = (hEdit && IsWindow(hEdit)) ? hEdit : hwnd;
      PopupKeyEvent pkey;
      memset(&pkey, 0, sizeof(pkey));
      pkey.vk = (int)wParam;
      pkey.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      pkey.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      pkey.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
      if (PromptWindow_OnKey(hKey, &pkey)) {
        s_swallow_next_char = true;
        if (FlintContext_Get()->prompt_callbacks.render != NULL) InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
      if (FlintContext_Get()->prompt_callbacks.render != NULL) InvalidateRect(hwnd, NULL, FALSE);
      break;
    }
    case WM_CHAR: {
      if (s_swallow_next_char) { s_swallow_next_char = false; return 0; }
      if (wParam >= 32 && FlintContext_Get()->prompt_callbacks.render != NULL && IsInsertMode()) {
        HWND hEdit = WH_TO_HWND(FlintContext_Get()->window.hEditControl);
        if (hEdit && IsWindow(hEdit)) {
          char buf[1024] = {0};
          GetWindowTextA(hEdit, buf, sizeof(buf));
          DWORD s = 0, e = 0;
          SendMessageA(hEdit, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
          int len = (int)strlen(buf);
          if ((int)s > len) s = (DWORD)len;
          if ((int)e > len) e = (DWORD)len;
          if (s != e) {
            memmove(buf + s, buf + e, len - e + 1);
            len = (int)strlen(buf);
          }
          if (len < (int)sizeof(buf) - 2) {
            memmove(buf + s + 1, buf + s, len - (int)s + 1);
            buf[s] = (char)wParam;
            SetWindowTextA(hEdit, buf);
            SendMessageA(hEdit, EM_SETSEL, s + 1, s + 1);
            extern PromptState* Platform_GetWin32PromptState(void);
            PromptState* st = Platform_GetWin32PromptState();
            if (st) {
              strncpy(st->filter, buf, sizeof(st->filter) - 1);
              st->filter[sizeof(st->filter) - 1] = '\0';
              st->cursor = (size_t)(s + 1);
            }
            strncpy(FlintContext_Get()->window.filter, buf, sizeof(FlintContext_Get()->window.filter) - 1);
            FlintContext_Get()->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] = '\0';
            FlintContext_Get()->window.filter_cursor = (int)(s + 1);
          }
          InvalidateRect(hwnd, NULL, FALSE);
          return 0;
        }
      }
      break;
    }
    case WM_ACTIVATE: {
        if (CloseOnLossOfFocus && LOWORD(wParam) == WA_INACTIVE) {
            FlintContext_Get()->window.output_buffer[0] = '\0';
            FlintContext_Get()->window.selection_complete = true;
        }
        break;
    }
    case WM_KILLFOCUS: {
        if (CloseOnLossOfFocus) {
            FlintContext_Get()->window.output_buffer[0] = '\0';
            FlintContext_Get()->window.selection_complete = true;
        }
        break;
    }
    case WM_CTLCOLOREDIT: {
      HDC hdc = (HDC)wParam;
      if (EnableShaders && ShaderPreset != 0) {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      } else {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
        return (LRESULT)s_bg_brush;
      }
    }
    case WM_CTLCOLORLISTBOX: {
      HDC hdc = (HDC)wParam;
      if (EnableShaders && ShaderPreset != 0) {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      } else {
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
        return (LRESULT)s_bg_brush;
      }
    }
    case WM_CREATE: {
      if (g_win32_hMainFont) {
        DeleteObject(g_win32_hMainFont);
        g_win32_hMainFont = NULL;
      }
      g_win32_hMainFont = FH_TO_HFONT(Platform_CreateFont(FontSize, UsedFont));
      if (only_once) selection_feature = FlintContext_Get()->window.current_feature;
        only_once = false;
          break;
    }
    case WM_SHOWWINDOW: {
      if (wParam && g_pending_navigate >= 0) {
        int idx = g_pending_navigate;
          g_pending_navigate = -1;
          if (WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox))) {
            UI_NavigateTo(lb, FlintContext_Get()->window.hListBox, idx);
              UpdateWindow(WH_TO_HWND(FlintContext_Get()->window.hListBox));
          }
      }
      break;
    }
    case WM_COMMAND: {
      int wmId   = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        if (wmId == IDC_INPUT_BOX && wmEvent == EN_CHANGE) {
          if (GetKeyState(VK_UP) & 0x8000 ||
              GetKeyState(VK_DOWN) & 0x8000 ||
              GetKeyState(VK_RETURN) & 0x8000 ||
              GetKeyState(VK_ESCAPE) & 0x8000)
          {
            break;
          }
            if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) != NULL && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hEditControl))) {
              char buf[sizeof(FlintContext_Get()->window.filter)];
              GetWindowTextA(WH_TO_HWND(FlintContext_Get()->window.hEditControl), buf, (int)sizeof(buf));
              strncpy(FlintContext_Get()->window.filter, buf, sizeof(FlintContext_Get()->window.filter) - 1);
              FlintContext_Get()->window.filter[sizeof(FlintContext_Get()->window.filter) - 1] = '\0';
              DWORD s = 0, e = 0;
              SendMessageA(WH_TO_HWND(FlintContext_Get()->window.hEditControl), EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
              FlintContext_Get()->window.filter_cursor = (int)s;
              PostMessageA(hwnd, WM_SYNC_TEXT_CARET, 0, 0);
              InvalidateRect(WH_TO_HWND(FlintContext_Get()->window.hEditControl), NULL, TRUE);
              UpdateWindow(WH_TO_HWND(FlintContext_Get()->window.hEditControl));
            }
            if (WH_TO_HWND(FlintContext_Get()->window.hEditControl) != NULL && WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL) {
              UI_Redraw(lb, FlintContext_Get()->window.hListBox);
            }
        }
      
        if (wmId == IDC_LIST_BOX && wmEvent == LBN_SELCHANGE) {
          break; 
        }
      
        break;
    }
    case WM_SYNC_TEXT_CARET: {
      HWND hEdit = WH_TO_HWND(FlintContext_Get()->window.hEditControl);
      if (hEdit && IsWindow(hEdit) && GetFocus() == hEdit) DestroyCaret();
      return 0;
    }
    case WM_TIMER: {
      if (wParam == UI_DEBOUNCE_TIMER_ID) {
        KillTimer(hwnd, UI_DEBOUNCE_TIMER_ID);
          UI_RedrawNow(lb, FlintContext_Get()->window.hListBox);
          if (FlintContext_Get()->prompt_callbacks.render != NULL) InvalidateRect(hwnd, NULL, FALSE);
          return 0;
      }
      if (wParam == UI_TICK_TIMER_ID) {
          if (PromptController_PollSequenceTimeout()) {
              if (lb && WH_TO_HWND(FlintContext_Get()->window.hListBox) != NULL) UI_Redraw(lb, FlintContext_Get()->window.hListBox);
          }
          if (FlintContext_Get()->prompt_callbacks.render != NULL) InvalidateRect(hwnd, NULL, FALSE);
          return 0;
      }
      break;
    }
    case WM_MEASUREITEM: {
      MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
        if (pmis->CtlID == IDC_LIST_BOX) {
          int h = FontSize + 14;
          if (h < 24) h = 24;
          if (h > 64) h = 64;
          pmis->itemHeight = h;
            return TRUE;
        }
      break;
    }
    case WM_DRAWITEM: {
      DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis->CtlID == IDC_LIST_BOX && pdis->itemID != -1) {
          BOOL isSelected = (pdis->itemID == (lb ? lb->selected_index : -1));
            if (pdis->itemID >= (lb ? lb->populated_count : 0)) {
              return TRUE;
            }
          if (RoundedCorners && CornerRadius > 0) {
            int R = 6;
            if (R > CornerRadius) R = CornerRadius;
            int t = 1;
            RECT inner = pdis->rcItem;
            inner.left += t; inner.top += t + 1; inner.right -= t; inner.bottom -= t + 1;
            if (inner.right <= inner.left || inner.bottom <= inner.top) { inner = pdis->rcItem; t = 0; }
            int innerR = R - 1; if (innerR < 0) innerR = 0;
            {
              RECT winRc; GetClientRect(hwnd, &winRc);
              int winW = winRc.right - winRc.left;
              int winH = winRc.bottom - winRc.top;
              POINT lbOrigin2 = {0,0};
              ClientToScreen(pdis->hwndItem, &lbOrigin2);
              ScreenToClient(hwnd, &lbOrigin2);
              int gx = lbOrigin2.x + pdis->rcItem.left;
              int gy = lbOrigin2.y + pdis->rcItem.top;
              if (EnableShaders && ShaderPreset != 0 && winW > 0 && winH > 0) {
                win32_draw_background_slice(pdis->hDC, &pdis->rcItem, COLOR_BG, winW, winH, gx, gy);
              }
            }
            if (isSelected) {
              win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_SELECTED_TEXT, innerR);
            } else {
              if (EnableShaders && ShaderPreset != 0) {
                RECT winRc; GetClientRect(hwnd, &winRc);
                int winW = winRc.right - winRc.left;
                int winH = winRc.bottom - winRc.top;
                POINT lbOrigin = {0,0};
                ClientToScreen(pdis->hwndItem, &lbOrigin);
                ScreenToClient(hwnd, &lbOrigin);
                int globalX = lbOrigin.x + inner.left;
                int globalY = lbOrigin.y + inner.top;
                if (winW <= 0) winW = pdis->rcItem.right - pdis->rcItem.left;
                if (winH <= 0) { RECT tr; GetClientRect(hwnd,&tr); winH = tr.bottom - tr.top; if(winH<=0) winH = pdis->rcItem.bottom - pdis->rcItem.top + 600; }
                win32_fill_rounded_rect_slice(pdis->hDC, &inner, COLOR_BG, winW, winH, globalX, globalY, innerR);
              } else {
                win32_fill_rounded_rect(pdis->hDC, &inner, COLOR_BG, innerR);
              }
            }
          } else if (EnableShaders && ShaderPreset != 0) {
            if (isSelected) {
              if (!s_sel_brush) s_sel_brush = CreateSolidBrush(COLOR_SELECTED_TEXT);
              FillRect(pdis->hDC, &pdis->rcItem, s_sel_brush);
            } else {
              RECT winRc; GetClientRect(hwnd, &winRc);
              int winW = winRc.right - winRc.left;
              int winH = winRc.bottom - winRc.top;
              POINT lbOrigin = {0,0};
              ClientToScreen(pdis->hwndItem, &lbOrigin);
              ScreenToClient(hwnd, &lbOrigin);
              int globalX = lbOrigin.x + pdis->rcItem.left;
              int globalY = lbOrigin.y + pdis->rcItem.top;
              if (winW <= 0) winW = pdis->rcItem.right - pdis->rcItem.left;
              if (winH <= 0) { RECT tr2; GetClientRect(hwnd,&tr2); winH = tr2.bottom - tr2.top; if(winH<=0) winH = pdis->rcItem.bottom - pdis->rcItem.top + 600; }
              win32_draw_background_slice(pdis->hDC, &pdis->rcItem, COLOR_BG, winW, winH, globalX, globalY);
            }
          } else {
            if (isSelected) {
              if (!s_sel_brush) s_sel_brush = CreateSolidBrush(COLOR_SELECTED_TEXT);
              FillRect(pdis->hDC, &pdis->rcItem, s_sel_brush);
            } else {
              if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
              FillRect(pdis->hDC, &pdis->rcItem, s_bg_brush);
            }
          }
          if (UI_IsListBoxSelected(lb)) {
                  char measureBuffer[512];
                    strncpy(measureBuffer, lb->populated_cache[pdis->itemID], sizeof(measureBuffer) - 1);
                    measureBuffer[sizeof(measureBuffer) - 1] = '\0';
                    char* pos = strchr(measureBuffer, '|');
                    if (pos) {
                      *pos = '\0';
                    }
                  if (!FullTextNeedsKeyPress) {
                    if (isSelected && strlen(measureBuffer) >= 70) {
                      if (g_last_selected_index != pdis->itemID || WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) == NULL) {
                        g_last_selected_index = pdis->itemID;
                          PostMessageA(hwnd, WM_PREVIEW_SELECTION, 0, (LPARAM)pdis->itemID);
                      }
                    } else {
                      if (g_last_selected_index == pdis->itemID) {
                        g_last_selected_index = -1;
                          PostMessageA(hwnd, WM_PREVIEW_SELECTION, 0, (LPARAM)-1);
                      }
                    }
                  } else {
                    if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
                      KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                        DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
                        FlintContext_Get()->window.hDisplayMessage = WH_NULL;
                    }
                  } 
                } 
           
            char composed[1024];
            const char* textBuffer = "";
            if (pdis->itemID < (lb ? lb->populated_count : 0)) {
              int raw = lb->matching_indices[pdis->itemID];
                if (lb && lb->alias_suffix_active && lb->alias_suffix_count > 0 && raw >= 0 && raw < lb->alias_suffix_count && lb->alias_suffix[raw]) {
                  snprintf(composed, sizeof(composed), "%s [%s]", lb->populated_cache[pdis->itemID], lb->alias_suffix[raw]);
                  textBuffer = composed;
                } else {
                  textBuffer = lb->populated_cache[pdis->itemID];
                }
            }
          
            if (pdis->itemID == -1 || pdis->itemID >= (lb ? lb->populated_count : 0)) {
              return TRUE;
            }
          
            
            SetBkMode(pdis->hDC, TRANSPARENT);
            RECT textRect = pdis->rcItem;
            textRect.left += 10;
          
            Platform_SetPreRenderStateWin32(textBuffer, &textRect);
            Platform_SetDrawContextWin32(pdis->hDC);
            // temporary
            // if (!fl_prerender()) return TRUE;
            if (FlintContext_Get()->prompt_callbacks.render != NULL) {
              return TRUE;
            } else {
              HFONT hOldMainFont = (HFONT)SelectObject(pdis->hDC, g_win32_hMainFont);
                if (!isSelected) {
                    unsigned long ncol, bcol, pcol;
                    int orig_idx = (int)pdis->itemID;
                    int orig_cnt = lb ? lb->populated_count : 0;
                    if (lb && (int)pdis->itemID >= 0 && pdis->itemID < (UINT)lb->populated_count) {
                      orig_idx = lb->matching_indices[pdis->itemID];
                      orig_cnt = lb->last_item_count > 0 ? lb->last_item_count : lb->populated_count;
                    }
                    Platform_GetItemColors(orig_idx, orig_cnt, &ncol, &bcol, &pcol);
                    SetTextColor(pdis->hDC, ncol);
                } else {
                    SetTextColor(pdis->hDC, COLOR_TEXT);
                }
                DrawTextA(pdis->hDC, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
                const char* alias_pos = strstr(textBuffer, " [");
                if (alias_pos) {
                  SIZE sz;
                    GetTextExtentPoint32A(pdis->hDC, textBuffer, (int)(alias_pos - textBuffer), &sz);
                    RECT aliasRect = textRect;
                    aliasRect.left += sz.cx;
                    SetTextColor(pdis->hDC, COLOR_ALIAS_TEXT);
                    DrawTextA(pdis->hDC, alias_pos, -1, &aliasRect, DT_SINGLELINE | DT_VCENTER);
                }
              SelectObject(pdis->hDC, hOldMainFont);
            }
          
            return TRUE;
        }
      break;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        HDC paintDC = hdc;
        HDC memDC = NULL;
        HBITMAP memBmp = NULL;
        HBITMAP oldBmp = NULL;
        bool useRenderBuf = (FlintContext_Get()->prompt_callbacks.render != NULL && rect.right > 0 && rect.bottom > 0);
        if (useRenderBuf) {
          memDC = CreateCompatibleDC(hdc);
          memBmp = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
          if (memDC && memBmp) {
            oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
            paintDC = memDC;
          } else {
            if (memDC) DeleteDC(memDC);
            if (memBmp) DeleteObject(memBmp);
            memDC = NULL;
            useRenderBuf = false;
          }
        }
        if (EnableShaders && ShaderPreset != 0) {
          win32_draw_background(paintDC, &rect);
        } else {
          if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
            FillRect(paintDC, &ps.rcPaint, s_bg_brush);
        }
        if (FlintContext_Get()->prompt_callbacks.render) {
            extern PromptState* Platform_GetWin32PromptState(void);
            PromptState *state = Platform_GetWin32PromptState();
            if (state) {
                fl_editbox_reset_placed();
                Platform_SetDrawContextWin32(paintDC);
                FlintContext_Get()->prompt_callbacks.render(state);
            }
        }
      if (g_vim_mode != NULL && VimModeText) {
        wchar_t wmode[256];
          MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
          HFONT hOldFont = (HFONT)SelectObject(paintDC, g_win32_hMainFont);
          SetBkMode(paintDC, TRANSPARENT);
          if (g_current_mode == MODE_NORMAL) {
            SetTextColor(paintDC,COLOR_NORMAL_MODE_TEXT);
          } else if (g_current_mode == MODE_INSERT) {
            SetTextColor(paintDC,COLOR_INSERT_MODE_TEXT);
          } else {
            SetTextColor(paintDC,COLOR_VISUAL_MODE_TEXT);
          }
          SIZE sz;
          GetTextExtentPoint32W(paintDC, wmode, (int)wcslen(wmode), &sz);
          RECT modeRect;
          modeRect.left = rect.right - sz.cx - 20;
          modeRect.top = rect.bottom - sz.cy - 10;
          modeRect.right = rect.right - 10;
          modeRect.bottom = modeRect.top + sz.cy;
          DrawTextW(paintDC, wmode, -1, &modeRect, DT_LEFT);
          SelectObject(paintDC, hOldFont);
      }
      if (FlintContext_Get()->window.title != NULL && strlen(FlintContext_Get()->window.title) > 0 && WindowTitleText) {
        wchar_t wtitle[256];
          MultiByteToWideChar(CP_UTF8, 0, FlintContext_Get()->window.title, -1, wtitle, 256);
          HFONT hOldFont = (HFONT)SelectObject(paintDC, g_win32_hMainFont);
          SetBkMode(paintDC, TRANSPARENT);
          SIZE sz;
          GetTextExtentPoint32W(paintDC, wtitle, (int)wcslen(wtitle), &sz);
          RECT titleRect;
          titleRect.left = 15;
          titleRect.top = rect.bottom - sz.cy - 10;
          titleRect.right = titleRect.left + sz.cx + 10;
          titleRect.bottom = titleRect.top + sz.cy;
          COLORREF titleColor;
          if ((lb ? lb->populated_count : 0) < 25) {
            titleColor = RGB(0, 255, 0);
          } else if ((lb ? lb->populated_count : 0) < 50) {
            titleColor = RGB(100, 255, 0);
          } else if ((lb ? lb->populated_count : 0) < 75) {
            titleColor = RGB(200, 255, 0);
          } else if ((lb ? lb->populated_count : 0) < 100) {
            titleColor = RGB(255, 255, 0);
          } else if ((lb ? lb->populated_count : 0) < 125) {
            titleColor = RGB(255, 200, 0);
          } else if ((lb ? lb->populated_count : 0) < 150) {
            titleColor = RGB(255, 150, 0);
          } else if ((lb ? lb->populated_count : 0) < 175) {
            titleColor = RGB(255, 100, 0);
          } else if ((lb ? lb->populated_count : 0) < 200) {
            titleColor = RGB(255, 50, 0);
          } else {
            titleColor = RGB(255, 0, 0);
          }
          SetTextColor(paintDC, titleColor);
          DrawTextW(paintDC, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);
          SelectObject(paintDC, hOldFont);
      }
        if (useRenderBuf) {
          BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
          SelectObject(memDC, oldBmp);
          DeleteObject(memBmp);
          DeleteDC(memDC);
        }
        EndPaint(hwnd, &ps);
        break;
    }
                         break;              
    case WM_ERASEBKGND: {
      return TRUE;
    }
    case WM_DESTROY: {
      static const WORD held_modifiers[] = { VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL,
        VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN };
        if (!KeepSelectionAfterClose)  {
          last_index = 0;
            if (lb) lb->selected_index = 0;
        }
      
        extern EditorMode g_last_mode;
        g_last_mode = g_current_mode;
        selection_feature = FlintContext_Get()->window.current_feature;
        for (int i = 0; i < (int)ARRAY_LEN(held_modifiers); i++) {
          if (GetAsyncKeyState(held_modifiers[i]) & 0x8000) {
            INPUT up = {0};
              up.type = INPUT_KEYBOARD;
              up.ki.wVk = held_modifiers[i];
              up.ki.dwFlags = KEYEVENTF_KEYUP;
              SendInput(1, &up, sizeof(INPUT));
          }
        }
      if (s_sel_brush) { DeleteObject(s_sel_brush); s_sel_brush = NULL; }
        rainbow_init = false;
        break;
    }
    default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
              
    }
  return 0;
}
