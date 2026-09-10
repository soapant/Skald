#include <windows.h>
#include "core/context.h"
#include "core/modal.h"
#include <commctrl.h>
#include <shellapi.h>
#include "config.h"
#include "core/common_ui.h"
#include "core/feature_data.h"
#include "core/number_shortcut.h"
#include "core/types.h"
#include "platform/subclasses.h"
#include "wingdi.h"
#include "config.h"
#include "core/platform.h"
#include "helpers/hotkey.h"
#include "helpers/alias.h"
#include "helpers/flag.h"
#include "helpers/file.h"
#include "helpers/ui.h"
#include "core/platform.h"

static ListBoxInfo* GetLBFromPopup(HWND hPopup) {
  if (hPopup && IsWindow(hPopup)) return (ListBoxInfo*)GetWindowLongPtrA(hPopup, GWLP_USERDATA);
  return NULL;
}
static ListBoxInfo* GetLBFromChild(HWND hChild) {
  HWND hParent = GetParent(hChild);
  if (hParent && IsWindow(hParent)) return GetLBFromPopup(hParent);
  
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) return GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  return NULL;
}
static ListBoxInfo* GetLBForPopupWindow(HWND hPopup) { return GetLBFromPopup(hPopup); }

static bool only_once = true;
static bool rainbow_init = false;

#define WM_PREVIEW_SELECTION (WM_APP + 1)

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  ListBoxInfo* lb = GetLBFromChild(hwnd);
  HWND hParent = GetParent(hwnd);
    HWND hListBox = GetDlgItem(hParent, IDC_LIST_BOX);
    HWND hEditControl = hwnd;
    
    if (msg == WM_KEYDOWN) {
      FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
        bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
        if (wParam == VK_CONTROL || wParam == VK_SHIFT) {
          return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
      
        if (wParam >= '0' && wParam <= '9') {
          if (ctrl_pressed) {
            HandleNumberShortcut(wParam, FlintContext_Get()->window.hListBox, FlintContext_Get()->window.hEditControl, lb);
              return 0;
          }
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
            return 0;
          }
        }
      
        if (wParam == VK_RETURN) {
          bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
            struct feature_data* data = GetFeatureData(FlintContext_Get()->window.current_feature);
            FlintContext_Get()->window.output_buffer[0] = '\0';
            if (UI_IsListBoxSelected(lb)) {
              if (lb && lb->selected_index >=0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
            } else {
              GetWindowTextA(hwnd, FlintContext_Get()->window.output_buffer, sizeof(FlintContext_Get()->window.output_buffer));
            }
            if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
              KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
                FlintContext_Get()->window.hDisplayMessage = WH_NULL;
            }
          if (ctrl_pressed) {
            AddSelectionToData(FlintContext_Get()->window.output_buffer,FlintContext_Get()->window.current_feature,data);
              if (FlintContext_Get()->prompt_callbacks.ctrl_enter != NULL) {
                FlintContext_Get()->prompt_callbacks.ctrl_enter(FlintContext_Get()->window.output_buffer, lb);
              }
            return 0;
          } else {
            AddSelectionToData(FlintContext_Get()->window.output_buffer,FlintContext_Get()->window.current_feature,data);
              if (FlintContext_Get()->prompt_callbacks.enter != NULL) {
                FlintContext_Get()->prompt_callbacks.enter(FlintContext_Get()->window.output_buffer, lb);
              }
            
              FlintContext_Get()->window.selection_complete = true; 
              return 0;
          }
        }
      
        if (wParam == VK_ESCAPE) {
          FlintContext_Get()->window.output_buffer[0] = '\0'; 
            FlintContext_Get()->window.selection_complete = true;  
            if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
              KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
            }
          return 1;
        }
      
        if (wParam == VK_TAB) {
          struct feature_data* data = GetFeatureData(FlintContext_Get()->window.current_feature);
            if (FlintContext_Get()->prompt_callbacks.tab != NULL) {
              FlintContext_Get()->prompt_callbacks.tab(lb);
            }
          for (int i = 0;i < data->selection_count;++i) {
            printf("%s \n",data->last_selections[i]);
          }
          return 0;
        }
      if (wParam == VK_BACK) {
        if (ctrl_pressed) {
          UI_SetText(FlintContext_Get()->window.hEditControl,"");
            UI_Redraw(lb, FlintContext_Get()->window.hListBox);
            return 0;
        }
      }
      if (wParam == COPY_SELECTION_KEY) {
        bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
          bool shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000);
          if (shift_pressed) {
            if (UI_IsListBoxSelected(lb)) {
              if (lb && lb->selected_index >=0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
                FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
            }
            if (FlintContext_Get()->prompt_callbacks.copy != NULL) {
              FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer, UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer), 0);
            } else {
              Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
            }
          } else if (ctrl_pressed) {
            if (UI_IsListBoxSelected(lb)) {
              if (lb && lb->selected_index >=0 && lb->selected_index < lb->populated_count) strncpy(FlintContext_Get()->window.output_buffer, lb->populated_cache[lb->selected_index], sizeof(FlintContext_Get()->window.output_buffer) - 1);
                FlintContext_Get()->window.output_buffer[sizeof(FlintContext_Get()->window.output_buffer) - 1] = '\0';
            }
            
              if (FlintContext_Get()->prompt_callbacks.copy != NULL) {
                FlintContext_Get()->prompt_callbacks.copy(FlintContext_Get()->window.output_buffer, UI_LookupTrueIndex(FlintContext_Get()->window.output_buffer), 1);
              } else {
                Platform_CopyToClipboard(FlintContext_Get()->window.output_buffer);
              }
          }
      }
      if (wParam == VK_DOWN) {
        if (ctrl_pressed) {
          char text[256];
          UI_GetCurrentText(FlintContext_Get()->window.hEditControl, text);
          UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,strlen(text));
        }
      }
      if (wParam == VK_UP) {
        if (ctrl_pressed) {
          UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,1);
        }
      }
    } 
  if (msg == WM_CHAR) {
    if (wParam == 0x7F) {
      return 0; 
    }
  }
  
    return DefSubclassProc(hwnd, msg, wParam, lParam);
} 

LRESULT CALLBACK SimpleEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  HWND hParent = GetParent(hwnd);
    HWND hListBox = GetDlgItem(hParent, IDC_LIST_BOX);
    HWND hEditControl = hwnd;
    bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
    
    if (msg == WM_KEYDOWN) {
      FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
        if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) {
          return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
      if (wParam == VK_RETURN) {
        GetWindowTextA(WH_TO_HWND(FlintContext_Get()->window.hEditControl), FlintContext_Get()->window.output_buffer, sizeof(FlintContext_Get()->window.output_buffer));
          
          FlintContext_Get()->window.selection_complete = true; 
          
          return 0; 
      }
      
        if (wParam == VK_ESCAPE) {
          FlintContext_Get()->window.output_buffer[0] = '\0'; 
            FlintContext_Get()->window.selection_complete = true;  
            if (WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage) != NULL) {
              KillTimer(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage), 999);
                DestroyWindow(WH_TO_HWND(FlintContext_Get()->window.hDisplayMessage));
            }
          return 0;
        }
      
        if (wParam == VK_BACK) {
          if (ctrl_pressed) {
            UI_SetText(FlintContext_Get()->window.hEditControl,"");
              return 0;
          }
        }
    } 
  return DefSubclassProc(hwnd, msg, wParam, lParam);
} 

LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData){
  static HBRUSH s_bg_brush = NULL;
    static HBRUSH s_bg_colorful_brush = NULL;
    switch (uMsg) {
    case WM_NCDESTROY:
                      RemoveWindowSubclass(hWnd, ListBoxSubclassProc, uIdSubclass);
                        if (s_bg_brush) { DeleteObject(s_bg_brush); s_bg_brush = NULL; }
                          if (s_bg_colorful_brush) { DeleteObject(s_bg_colorful_brush); s_bg_colorful_brush = NULL; }
                            break;
    case WM_ERASEBKGND:
                       return TRUE;
    case WM_PAINT: {
      LRESULT lr = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int itemH = (int)SendMessageA(hWnd, LB_GETITEMHEIGHT, 0, 0);
        int count = (int)SendMessageA(hWnd, LB_GETCOUNT, 0, 0);
        if (itemH > 0) {
          LONG itemsBottom = (LONG)count * itemH;
            if (itemsBottom < rc.bottom) {
              HBRUSH hBrush;
                if (ColorfullMode) {
                  if (!s_bg_colorful_brush) {
                    s_bg_colorful_brush = CreateSolidBrush(GetRainbowColor(0.02f, 0.55f, GradientBrightness));
                  }
                  hBrush = s_bg_colorful_brush;
                } else {
                  if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
                    hBrush = s_bg_brush;
                }
              RECT empty = { rc.left, itemsBottom, rc.right, rc.bottom };
                HDC hdc = GetDC(hWnd);
                if (hdc) {
                  FillRect(hdc, &empty, hBrush);
                    ReleaseDC(hWnd, hdc);
                }
            }
        }
      return lr;
    }
    default:
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
  return 0;
}

LRESULT CALLBACK SimpleListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  static HBRUSH s_bg_brush = NULL;
    static HBRUSH s_gradient_brushes[32] = {0};
    static bool s_gradient_ready = false;
    
    switch (uMsg) {
    case WM_NCDESTROY:
                      RemoveWindowSubclass(hWnd, SimpleListBoxSubclassProc, uIdSubclass);
                        if (s_bg_brush) { DeleteObject(s_bg_brush); s_bg_brush = NULL; }
                          for (int i = 0; i < 32; i++) { if (s_gradient_brushes[i]) { DeleteObject(s_gradient_brushes[i]); s_gradient_brushes[i] = NULL; } }
                            s_gradient_ready = false;
                              break;
                              
    case WM_ERASEBKGND:
                       return TRUE;
                         
    case WM_PAINT: {
      PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        
        RECT rect;
        GetClientRect(hWnd, &rect);
        if (ColorfullMode) {
          if (!s_gradient_ready) {
            for (int i = 0; i < GradientItems; i++) {
              float ft = (float)i / GradientItems;
                s_gradient_brushes[i] = CreateSolidBrush(GetRainbowColor(ft, 0.55f, GradientBrightness));
            }
            s_gradient_ready = true;
          }
          int h = rect.bottom - rect.top;
            int bandH = max(1, h / 32);
            for (int y = 0; y < h; y += bandH) {
              int colorIndex = (int)(((float)y / (float)h) * 32.0f);
                if (colorIndex > 31) colorIndex = 31;
                  RECT band = {rect.left, rect.top + y, rect.right, rect.top + min(y + bandH, h)};
                    FillRect(hdc, &band, s_gradient_brushes[colorIndex]);
            }
        } else {
          if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
            FillRect(hdc, &rect, s_bg_brush);
        }
      
        int textLen = UI_LB_GetTextLen(WH_FROM_HWND(hWnd));
        
        if (textLen > 0 && textLen < 512) {
          char textBuffer[512] = {0};
            UI_LB_GetText(WH_FROM_HWND(hWnd), 0, textBuffer);
            
            HFONT hFont = UI_GetFont(WH_FROM_HWND(hWnd));
            HFONT hOldFont = NULL;
            if (hFont) hOldFont = (HFONT)SelectObject(hdc, hFont);
              
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, COLOR_TEXT);
                
                int itemHeight = UI_LB_GetItemHeight(WH_FROM_HWND(hWnd));
                if (itemHeight <= 0) itemHeight = 40;
                  
                    RECT textRect = rect;
                    textRect.left = 0;
                    textRect.right = rect.right;
                    textRect.bottom = textRect.top + itemHeight;
                    
                    DrawTextA(hdc, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
                    
                    if (hOldFont) SelectObject(hdc, hOldFont);
        }
      
        EndPaint(hWnd, &ps);
        return 0;
    }
    }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static int g_last_selected_index = -1;

static int g_pending_navigate = -1;

void RequestInitialNavigation(int index) {
  g_pending_navigate = index;
}

LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  ListBoxInfo* lb = GetLBForPopupWindow(hwnd);
  static HBRUSH s_bg_brush = NULL;
    static HBRUSH s_bg_colorful_brush = NULL;
    static HBRUSH s_bg_gradient_brushes[128] = {0};
    static bool s_bg_gradient_ready = false;
    static HBRUSH s_sel_brush = NULL;
    static HBRUSH s_colorful_brushes[MAX_VISIBLE_ITEMS + 40] = {0};
    static bool s_colorful_ready = false;
    
    switch (msg) {
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
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
      HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT);
        if (ColorfullMode) {
          COLORREF bg = GetRainbowColor(0.02f, 0.55f, GradientBrightness);
            SetBkColor(hdc, bg);
            if (!s_bg_colorful_brush) {
              s_bg_colorful_brush = CreateSolidBrush(bg);
            }
          return (LRESULT)s_bg_colorful_brush;
        } else {
          SetBkColor(hdc, COLOR_BG);
            if (!s_bg_brush) {
              s_bg_brush = CreateSolidBrush(COLOR_BG);
            }
          return (LRESULT)s_bg_brush;
        }
    }
    case WM_CREATE: {
      if (!FlintContext_Get()->g_hMainFont) {
        FlintContext_Get()->g_hMainFont = CreateFontA(FontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UsedFont);
      }
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
        
        if (wmId == IDC_INPUT_BOX || wmId == EN_CHANGE) {
          if (GetKeyState(VK_UP) & 0x8000 || 
              GetKeyState(VK_DOWN) & 0x8000 || 
              GetKeyState(VK_RETURN) & 0x8000 ||
              GetKeyState(VK_ESCAPE) & 0x8000) 
          {
            break;
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
    case WM_TIMER: {
      if (wParam == UI_DEBOUNCE_TIMER_ID) {
        KillTimer(hwnd, UI_DEBOUNCE_TIMER_ID);
          UI_RedrawNow(lb, FlintContext_Get()->window.hListBox);
          return 0;
      }
      break;
    }
    case WM_MEASUREITEM: {
      MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
        if (pmis->CtlID == IDC_LIST_BOX) {
          pmis->itemHeight = 40;
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
           if (!s_colorful_ready) {
            s_sel_brush = CreateSolidBrush(COLOR_SELECTED_TEXT);
              for (int i = 0; i < MAX_VISIBLE_ITEMS + 40; i++) {
                float t = (float)i / (float)(MAX_VISIBLE_ITEMS + 40);
                  float hue = t * 240.0f;
                  float h = hue / 60.0f;
                  float s = 0.85f;
                  float v = GradientBrightness;
                  int j = (int)h;
                  float f = h - j;
                  float p = v * (1 - s);
                  float q = v * (1 - s * f);
                  float t_val = v * (1 - s * (1 - f));
                  float r, g, b;
                  switch (j) {
                  case 0: r = v; g = t_val; b = p; break;
                  case 1: r = q; g = v; b = p; break;
                  case 2: r = p; g = v; b = t_val; break;
                  case 3: r = p; g = q; b = v; break;
                  case 4: r = t_val; g = p; b = v; break;
                  default: r = v; g = p; b = q; break;
                  }
                s_colorful_brushes[i] = CreateSolidBrush(RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)));
              }
            s_colorful_ready = true;
          }
          if (EnableShaders && ShaderPreset != 0) {
            Win32_DrawSelectionBackground(pdis->hDC, &pdis->rcItem, isSelected ? COLOR_SELECTED_TEXT : COLOR_BG);
          } else if (ColorfullMode) {
            int color_index = pdis->itemID % 100;
              HBRUSH hBrush = s_colorful_brushes[color_index];
              FillRect(pdis->hDC, &pdis->rcItem, hBrush);
          } else {
            if (isSelected) {
              FillRect(pdis->hDC, &pdis->rcItem, s_sel_brush);
            } else {
              if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
              FillRect(pdis->hDC, &pdis->rcItem, s_bg_brush);
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
          }
          
            const char* textBuffer = "";
            if (pdis->itemID < (lb ? lb->populated_count : 0)) {
              int raw = lb->matching_indices[pdis->itemID];
                if (g_alias_display_active && g_alias_display_count > 0 && raw >= 0 && raw < g_alias_display_count && g_alias_display[raw][0] != '\0') {
                  textBuffer = g_alias_display[raw];
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
          
            if (FlintContext_Get()->prompt_callbacks.render != NULL) {
              FlintContext_Get()->prompt_callbacks.render(pdis->hDC, textBuffer, &textRect, isSelected, pdis->itemID);
            } else {
              HFONT hOldMainFont = (HFONT)SelectObject(pdis->hDC, FlintContext_Get()->g_hMainFont);
                if (!isSelected) {
                    if (RainbowText) {
                        float _t = (lb && lb->populated_count>1) ? (float)pdis->itemID/(float)(lb->populated_count-1) : 0;
                        SetTextColor(pdis->hDC, GetRainbowColor(_t, 0.85f, GradientBrightness));
                    } else if (AlternatingText && (pdis->itemID % 2 == 0)) {
                        SetTextColor(pdis->hDC, COLOR_ALTERNATE);
                    } else {
                        SetTextColor(pdis->hDC, COLOR_TEXT);
                    }
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
        if (EnableShaders && ShaderPreset != 0) {
          Win32_DrawBackground(hdc, &rect);
        } else if (ColorfullMode) {
          if (!s_bg_gradient_ready) {
            for (int i = 0; i < 128; i++) {
              float ft = (float)i / 128.0f;
                s_bg_gradient_brushes[i] = CreateSolidBrush(GetRainbowColor(ft, 0.55f, GradientBrightness));
            }
            s_bg_gradient_ready = true;
          }
          int h = rect.bottom - rect.top;
            int bandH = max(1, h / 128);
            for (int y = 0; y < h; y += bandH) {
              int colorIndex = (int)(((float)y / (float)h) * 128.0f);
                if (colorIndex > 127) colorIndex = 127;
                  RECT band = {rect.left, rect.top + y, rect.right, rect.top + min(y + bandH, h)};
                    FillRect(hdc, &band, s_bg_gradient_brushes[colorIndex]);
            }
        } else {
          if (!s_bg_brush) s_bg_brush = CreateSolidBrush(COLOR_BG);
            FillRect(hdc, &ps.rcPaint, s_bg_brush);
        }
      if (g_vim_mode != NULL) {
        wchar_t wmode[256];
          MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
          
          HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
          
          SetBkMode(hdc, TRANSPARENT);
          
          if (g_current_mode == MODE_NORMAL) {
            SetTextColor(hdc,COLOR_NORMAL_MODE_TEXT);
          } else if (g_current_mode == MODE_INSERT) {
            SetTextColor(hdc,COLOR_INSERT_MODE_TEXT);
          } else {
            SetTextColor(hdc,COLOR_VISUAL_MODE_TEXT);
          }
        
          SIZE sz;
          GetTextExtentPoint32W(hdc, wmode, (int)wcslen(wmode), &sz);
          
          RECT modeRect;
          modeRect.left = 15;
          modeRect.top = 10;
          modeRect.right = modeRect.left + sz.cx + 10;
          modeRect.bottom = modeRect.top + sz.cy + 10;
          
          DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT);
          
          SelectObject(hdc, hOldFont);
      }
      if (FlintContext_Get()->window.title != NULL && strlen(FlintContext_Get()->window.title) > 0) {
        wchar_t wtitle[256];
          MultiByteToWideChar(CP_UTF8, 0, FlintContext_Get()->window.title, -1, wtitle, 256);
          
          HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
          SetBkMode(hdc, TRANSPARENT);
          
          SIZE sz;
          GetTextExtentPoint32W(hdc, wtitle, (int)wcslen(wtitle), &sz);
          
          RECT titleRect;
          titleRect.left = rect.right - sz.cx - 20;
          titleRect.top = 10;
          titleRect.right = rect.right - 10;
          titleRect.bottom = rect.top + sz.cy + 15;
          
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
        
          SetTextColor(hdc, titleColor);
          DrawTextW(hdc, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);
          
          SelectObject(hdc, hOldFont);
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
        for (int i = 0; i < MAX_VISIBLE_ITEMS + 40; i++) {
          if (s_colorful_brushes[i]) { DeleteObject(s_colorful_brushes[i]); s_colorful_brushes[i] = NULL; }
        }
        s_colorful_ready = false;
        rainbow_init = false;
    }
    default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
              
    }
  return 0;
}

LRESULT CALLBACK DisplayMessageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  FadeWindowData* data = (FadeWindowData*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    FlintContext_Get()->window.hDisplayMessage = WH_FROM_HWND(hwnd);
    
    switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    case WM_PAINT: {
      if (!data) break;
        PAINTSTRUCT ps;
          HDC hdc = BeginPaint(hwnd, &ps);
          
          RECT rect;
          GetClientRect(hwnd, &rect);
          
          if (EnableShaders && ShaderPreset != 0) Win32_DrawBackground(hdc, &rect);
          else { HBRUSH hBackgroundBrush = CreateSolidBrush(COLOR_BG); FillRect(hdc, &rect, hBackgroundBrush); DeleteObject(hBackgroundBrush); }
          
          SetBkMode(hdc, TRANSPARENT);
          SetTextColor(hdc, data->color ? data->color : COLOR_TEXT);
      
        HFONT hOldFont = NULL;
        if (FH_TO_HFONT(data->hFont)) {
          hOldFont = (HFONT)SelectObject(hdc, FH_TO_HFONT(data->hFont));
        }
      
        if (data->message) {
          RECT textRect = rect;
            textRect.left += 15;  
            textRect.right -= 15;
            textRect.top += 10;
            textRect.bottom -= 10;
            
            RECT calcRect = textRect;
            DrawTextA(hdc, data->message, -1, &calcRect, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
            
            int textHeight = calcRect.bottom - calcRect.top;
            int availableHeight = rect.bottom - rect.top;
            textRect.top = (availableHeight - textHeight) / 2;
            textRect.bottom = textRect.top + textHeight;
            
            DrawTextA(hdc, data->message, -1, &textRect, DT_CENTER | DT_WORDBREAK);
        }
      
        if (hOldFont) SelectObject(hdc, hOldFont);
          EndPaint(hwnd, &ps);
            return 0;
    }
    case WM_TIMER: {
      if (wParam == 999 && data) {
        if (FlintContext_Get()->g_timeout == 6969) {
          return 0;
        }
        
          if (FlintContext_Get()->g_timeout > 0) {
            FlintContext_Get()->g_timeout--;
              return 0; 
          }
        
          data->alpha -= 12; 
          
          if (data->alpha <= 0) {
            KillTimer(hwnd, 999);
              FlintContext_Get()->window.hDisplayMessage = WH_NULL; 
              DestroyWindow(hwnd);
          } else {
            SetLayeredWindowAttributes(hwnd, 0, (BYTE)data->alpha, LWA_ALPHA);
          }
      }
      return 0;
    }
    case WM_NCDESTROY: {
      if (data) {
        if (data->message) free((void*)data->message);
          if (FH_TO_HFONT(data->hFont)) Platform_DestroyFont(data->hFont);
            free(data);
              SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
      }
      break;
    }
    }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_NCCREATE: {
    CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
      return DefWindowProcA(hwnd, msg, wParam, lParam);
  }
    
  case WM_CREATE: {
    if (!FlintContext_Get()->g_hMainFont) {
      FlintContext_Get()->g_hMainFont = CreateFontA(FontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UsedFont);
    }
    break;
  }
      
  case WM_ERASEBKGND: {
    return TRUE;
  }
        
  case WM_CTLCOLOREDIT: {
    HDC hdc = (HDC)wParam;
      SetTextColor(hdc, COLOR_TEXT);
      SetBkColor(hdc, COLOR_BG);
      static HBRUSH hBrush = NULL;
      if (!hBrush) hBrush = CreateSolidBrush(COLOR_BG);
        return (LRESULT)hBrush;
  }
          
  case WM_PAINT: {
    PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT rect;
      GetClientRect(hwnd, &rect);
      
      
      HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
      
      HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG);
      FillRect(hdc, &rect, hBgBrush);
      DeleteObject(hBgBrush);
      
      if (g_vim_mode != NULL) {
        wchar_t wmode[256];
          MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
          
          SetBkMode(hdc, TRANSPARENT);
          
          COLORREF modeColor;
          if (g_current_mode == MODE_NORMAL) {
            modeColor = RGB(100, 255, 255);
          } else if (g_current_mode == MODE_INSERT) {
            modeColor = RGB(50, 255, 50);
          } else {
            modeColor = RGB(200, 3, 222);
          }
        SetTextColor(hdc, modeColor);
          
          RECT modeRect;
          modeRect.left = 8;
          modeRect.top = 4;
          modeRect.right = rect.right - 8;
          modeRect.bottom = 28;
          
          DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT | DT_TOP);
      }

    wchar_t wtitle[256];
    MultiByteToWideChar(CP_UTF8, 0, "Settings", -1, wtitle, 256);

    SetBkMode(hdc, TRANSPARENT);

    SIZE sz;
    GetTextExtentPoint32W(hdc, wtitle, (int)wcslen(wtitle), &sz);

    RECT titleRect;
    titleRect.left = rect.right - sz.cx - 20;
    titleRect.top = 10;
    titleRect.right = rect.right - 10;
    titleRect.bottom = rect.top + sz.cy + 15;

    COLORREF titleColor;
    titleColor = RGB(0, 255, 0);

    SetTextColor(hdc, titleColor);
    DrawTextW(hdc, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);

    SelectObject(hdc, hOldFont);
    
      
      SelectObject(hdc, hOldFont);
      
      EndPaint(hwnd, &ps);
      return 0;
  }
  case WM_DESTROY: {
      FlintContext_Get()->window.hSettings = WH_NULL;
      break;
  }
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  ListBoxInfo* lb = GetLBFromChild(hwnd);
  HWND hEditControl = hwnd;
    
    if (msg == WM_KEYDOWN) {
      FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
        bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
        if (wParam == VK_CONTROL || wParam == VK_SHIFT) {
          return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
      
        if (wParam == VK_ESCAPE) {
          if (IsInsertMode()) {
            SetModeWithListBox(MODE_NORMAL, lb);
          }else {
            DestroyWindow(GetParent(hwnd));
              FlintContext_Get()->window.hSettings = WH_NULL;
              return 1;
          }
        }
      if (wParam == VK_BACK) {
        if (ctrl_pressed) {
          UI_SetText(FlintContext_Get()->window.hEditControl,"");
            return 0;
        }
      }
      if (wParam == VK_DOWN) {
        if (ctrl_pressed) {
          char text[256];
          UI_GetCurrentText(FlintContext_Get()->window.hEditControl, text);
          UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,strlen(text));
        }
      }
      if (wParam == VK_UP) {
        if (ctrl_pressed) {
          UI_SetCursorPosition(FlintContext_Get()->window.hEditControl,1);
        }
      }
    } 
  return DefSubclassProc(hwnd, msg, wParam, lParam);
} 

LRESULT CALLBACK EditPopupSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  ListBoxInfo* lb = GetLBFromChild(hWnd);
  
  if (!lb && WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  EditPopupData* data = (EditPopupData*)dwRefData;
    if (!data || !WH_TO_HWND(data->hEdit)) {
      data = (EditPopupData*)GetWindowLongPtrA(hWnd, GWLP_USERDATA);
    }
  if (uMsg == WM_KEYDOWN) {
    if (!data || !WH_TO_HWND(data->hEdit)) {
      return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }
    
      if (wParam == VK_ESCAPE) {
        printf("[EditPopup] Escape pressed\n");
          data->result = NULL;
          DestroyWindow(GetParent(hWnd));
          UI_Redraw(lb, FlintContext_Get()->window.hListBox);
          return 0;
      }
    if (wParam == VK_RETURN) {
      if (GetKeyState(VK_SHIFT) & 0x8000) {
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
      }
      printf("[EditPopup] Enter pressed\n");
        char buffer[2048] = {0};
        GetWindowTextA(WH_TO_HWND(data->hEdit), buffer, sizeof(buffer) - 1);
        data->result = _strdup(buffer);
        DestroyWindow(GetParent(hWnd));
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        
        return 0;
    }
  }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK SearchPopupSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  ListBoxInfo* lb = GetLBFromChild(hWnd);
  if (!lb && WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  if (uMsg == WM_KEYDOWN) {
    if (wParam == VK_ESCAPE) {
      DestroyWindow(GetParent(hWnd));
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        return 0;
    }
    if (wParam == VK_RETURN) {
      if (GetKeyState(VK_SHIFT) & 0x8000) {
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
      }
      printf("[SearchPopup] Enter pressed\n");
        char buffer[2048] = {0};
        DestroyWindow(GetParent(hWnd));
        UI_Redraw(lb, FlintContext_Get()->window.hListBox);
        return 0;
    }
  }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK EditPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  ListBoxInfo* lb = NULL;
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  EditPopupData* data = (EditPopupData*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    
    switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
      
    case WM_CREATE: {
      if (!data) break;
        HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE);
          RECT rect;
          GetClientRect(hwnd, &rect);
          
          int modeHeight = 30;
          data->hEdit = WH_FROM_HWND(CreateWindowExA(
                                        0, "EDIT", data->initial_text,
                                        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE,
                                        5, modeHeight + 5,
                                        rect.right - 10, rect.bottom - modeHeight - 10,
                                        hwnd, (HMENU)1001, hInstance, NULL
                                       ));
          
          if (WH_TO_HWND(data->hEdit)) {
            UI_Edit_SetFont(data->hEdit,data->hFont);
              UI_Edit_SetSel(data->hEdit,0,0);
              SetFocus(WH_TO_HWND(data->hEdit));
              FlintContext_Get()->window.g_ActiveEditControl = data->hEdit;
              
              SetWindowLongPtrA(WH_TO_HWND(data->hEdit), GWLP_USERDATA, (LONG_PTR)data);
              SetWindowSubclass(WH_TO_HWND(data->hEdit), EditPopupSubclassProc, 0, (DWORD_PTR)data);
          }
      break;
    }
        
    case WM_CTLCOLOREDIT: {
      HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        static HBRUSH hBrush = NULL;
        if (!hBrush) hBrush = CreateSolidBrush(COLOR_BG);
          return (LRESULT)hBrush;
    }
          
    case WM_PAINT: {
      PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        if (EnableShaders && ShaderPreset != 0) Win32_DrawBackground(hdc, &rect);
        else { HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG); FillRect(hdc, &rect, hBgBrush); DeleteObject(hBgBrush); }
        
        if (g_vim_mode != NULL) {
          wchar_t wmode[256];
            MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
            
            HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
            SetBkMode(hdc, TRANSPARENT);
            
            COLORREF modeColor;
            if (g_current_mode == MODE_NORMAL) {
              modeColor = RGB(100, 255, 255);
            } else if (g_current_mode == MODE_INSERT) {
              modeColor = RGB(50, 255, 50);
            } else {
              modeColor = RGB(200, 3, 222);
            }
          SetTextColor(hdc, modeColor);
            
            RECT modeRect;
            modeRect.left = 8;
            modeRect.top = 4;
            modeRect.right = rect.right - 8;
            modeRect.bottom = 28;
            
            DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT | DT_TOP);
            
            SelectObject(hdc, hOldFont);
            
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 65, 75));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, 5, 30, NULL);
            LineTo(hdc, rect.right - 5, 30);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
        }
      
        EndPaint(hwnd, &ps);
        return 0;
    }
            
    case WM_DESTROY: {
      FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
        if (data) {
          if (data->initial_text) { free((void*)data->initial_text); data->initial_text = NULL; }
            if (data->hFont) { DeleteObject(data->hFont); data->hFont = NULL; }
              SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        }
        ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
        last_index = lb ? lb->selected_index : -1;
        SetModeWithListBox(MODE_NORMAL, lb);
        UpdateTextCaret(FlintContext_Get()->window.hEditControl);
        break;
    }
    }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SearchPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  ListBoxInfo* lb = NULL;
  if (WH_TO_HWND(FlintContext_Get()->window.hPopupWnd) && IsWindow(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd))) lb = GetLBFromPopup(WH_TO_HWND(FlintContext_Get()->window.hPopupWnd));
  HFONT hFont = CreateCustomNerdFont(FontSize, UsedFont);
    switch (msg) {
    case WM_NCCREATE: {
      CREATESTRUCTA* cs = (CREATESTRUCTA*)lParam;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
      
    case WM_CREATE: {
      HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE);
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        int modeHeight = 30;
        
        FlintContext_Get()->window.g_hSearchPopup = WH_FROM_HWND(CreateWindowExA(
                                         0, "EDIT","",
                                         WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE,
                                         5, modeHeight + 5,
                                         rect.right - 10, rect.bottom - modeHeight - 10,
                                         hwnd, (HMENU)1001, hInstance, NULL
                                        ));
        if (WH_TO_HWND(FlintContext_Get()->window.g_hSearchPopup) != NULL) {
            UI_Edit_SetFont(FlintContext_Get()->window.g_hSearchPopup,hFont);
            UI_Edit_SetSel(FlintContext_Get()->window.g_hSearchPopup,0,0);
            SetFocus(WH_TO_HWND(FlintContext_Get()->window.g_hSearchPopup));
            FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.g_hSearchPopup;
            
            SetWindowLongPtrA(WH_TO_HWND(FlintContext_Get()->window.g_hSearchPopup), GWLP_USERDATA,1);
            SetWindowSubclass(WH_TO_HWND(FlintContext_Get()->window.g_hSearchPopup), SearchPopupSubclassProc, 0,1);
        }
      break;
    }
        
    case WM_CTLCOLOREDIT: {
      HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        static HBRUSH hBrush = NULL;
        if (!hBrush) hBrush = CreateSolidBrush(COLOR_BG);
          return (LRESULT)hBrush;
    }
          
    case WM_PAINT: {
      PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        if (EnableShaders && ShaderPreset != 0) Win32_DrawBackground(hdc, &rect);
        else { HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG); FillRect(hdc, &rect, hBgBrush); DeleteObject(hBgBrush); }
        
        if (g_vim_mode != NULL) {
          wchar_t wmode[256];
            MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
            
            HFONT hOldFont = (HFONT)SelectObject(hdc, FlintContext_Get()->g_hMainFont);
            SetBkMode(hdc, TRANSPARENT);
            
            COLORREF modeColor;
            if (g_current_mode == MODE_NORMAL) {
              modeColor = RGB(100, 255, 255);
            } else if (g_current_mode == MODE_INSERT) {
              modeColor = RGB(50, 255, 50);
            } else {
              modeColor = RGB(200, 3, 222);
            }
          SetTextColor(hdc, modeColor);
            
            RECT modeRect;
            modeRect.left = 8;
            modeRect.top = 4;
            modeRect.right = rect.right - 8;
            modeRect.bottom = 28;
            
            DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT | DT_TOP);
            
            SelectObject(hdc, hOldFont);
            
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 65, 75));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, 5, 30, NULL);
            LineTo(hdc, rect.right - 5, 30);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
        }
      
        EndPaint(hwnd, &ps);
        return 0;
    }
            
    case WM_DESTROY: {
      FlintContext_Get()->window.g_ActiveEditControl = FlintContext_Get()->window.hEditControl;
        DeleteObject(hFont);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
        ForceWindowFocus(FlintContext_Get()->window.hPopupWnd, FlintContext_Get()->window.hEditControl);
        SetModeWithListBox(MODE_NORMAL, lb);
        UpdateTextCaret(FlintContext_Get()->window.hEditControl);
        break;
    }
    }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
    
  case WM_TRAYICON: {
    if (lParam == WM_RBUTTONUP) {
      POINT pt;
        GetCursorPos(&pt);
        
        HMENU hMenu = CreatePopupMenu();
        
        AppendMenu(hMenu, MF_STRING, ID_SETTINGS, "Settings");
        
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
    if (LOWORD(wParam) == ID_SETTINGS) {
      Platform_SettingsPopUp();
    }
    
      break;
  }
      
  case WM_DISPLAY_MESSAGE: {
    DisplayMessageParams* p = (DisplayMessageParams*)lParam;
      if (p != NULL) {
        Platform_CreateDisplayMessageWindow(p->message, p->centerX, p->centerY, p->maxWidth, p->maxHeight, p->timeout, p->color);
      }
    return 0;
  }
        
  case WM_DESTROY: {
    NOTIFYICONDATA nid = {0};
      nid.cbSize = sizeof(NOTIFYICONDATA);
      nid.hWnd = hwnd;
      nid.uID = ID_TRAY_ICON;
      Shell_NotifyIcon(NIM_DELETE, &nid);
      if (FlintContext_Get()->g_hNumFont) {
        DeleteObject(FlintContext_Get()->g_hNumFont);
          FlintContext_Get()->g_hNumFont = NULL;
      }
    if (FlintContext_Get()->g_hMainFont) {
      DeleteObject(FlintContext_Get()->g_hMainFont);
        FlintContext_Get()->g_hMainFont = NULL;
    }
    PostQuitMessage(0);
      break;
  }
          
  default:
          return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}
