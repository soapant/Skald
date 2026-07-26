#ifndef SUBCLASSES_H
#define SUBCLASSES_H
#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <gdiplus.h>
#include <stdio.h>
#include <shellapi.h>
#include <stdbool.h>
#include <process.h>
#include <shlwapi.h> 
#include "helpers.h"
#include "modal.h"
#include "types.h"
#include "config.h"
#include "common_ui.h"
int g_refresh_count = 0;
HashMap* history_indices = NULL;
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    HWND hParent = GetParent(hwnd);
    HWND hListBox = GetDlgItem(hParent, IDC_LIST_BOX);
    HWND hEditControl = hwnd;
    bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_CONTROL || wParam == VK_SHIFT) {
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }

        if (wParam >= '0' && wParam <= '9') {
            if (ctrl_pressed) {
              HandleNumberShortcut(wParam, hListBox, hEditControl,true);
              return 0;
            }
        }

        if (wParam == VK_UP || wParam == VK_DOWN) {
            if (hListBox != NULL) {
                int count = UI_GetCount(hListBox);
                int sel = UI_GetSelection(hListBox);
                
                if (count > 0) {
                    if (sel == LB_ERR) {
                        if (sel != -1 | wParam == VK_DOWN){
                          sel = 0;
                        }else {
                        if (sel == -1 && wParam == VK_UP) {
                            char** HistoryList = (char**)HashMap_Get(feature_history, g_current_feature);
                            if (HistoryList) {
                                int* idx_ptr = (int*)HashMap_Get(history_indices, g_current_feature);
                                int idx = idx_ptr ? *idx_ptr : 0;
                                
                                int count = 0;
                                while (HistoryList[count]) count++;
                                
                                if (count > 0) {
                                    idx = (idx - 1 + count) % count;
                                    
                                    UI_SetText(hEditControl, HistoryList[idx]);
                                    UI_SetCursorPosition(hEditControl,strlen(HistoryList[idx]));
                                    if (!IsNormalMode()) {
                                        if (g_refresh_callback != NULL) {
                                            g_refresh_callback(HistoryList[idx], hListBox);
                                        }
                                    }
                                    if (!idx_ptr) {
                                        idx_ptr = malloc(sizeof(int));
                                        HashMap_Put(history_indices, g_current_feature, idx_ptr);
                                    }
                                    *idx_ptr = idx;
                                    return 0;
                                } 
                            }
                        }
                      }
                    } else if (sel == 0 && wParam == VK_UP) {
                        sel = -1;
                    } else {
                      if (wParam == VK_UP) {
                          sel = (sel > 0) ? sel - 1 : 0;
                        } else {
                          sel = (sel < count - 1) ? sel + 1 : count - 1;
                        }
                    }
                    
                    int old_sel = g_SelectedIndex;
                    g_SelectedIndex = sel;
                    UI_LB_SetCaretIndex(hListBox, sel, TRUE);
                    UI_SetSelection(hListBox, sel, old_sel);
                }
            }
            return 0;
        }

        if (wParam == VK_RETURN) {
            bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000);
            output_buffer[0] = '\0';
            int selected_index = LB_ERR;

            if (hListBox != NULL) {
                selected_index = UI_GetSelection(hListBox);
            }

            if (selected_index != LB_ERR && hListBox != NULL) {
                strncpy(output_buffer, g_populated_cache[g_SelectedIndex], sizeof(output_buffer) - 1);
            } else {
                UI_GetText(hwnd, output_buffer, sizeof(output_buffer));
            }
            char search_string[256] = {0};
            UI_GetCurrentText(hEditControl, search_string);
            SaveFeatureString(search_string);
            if (ctrl_pressed) {
                if (g_ctrl_enter_callback != NULL) {
                    g_ctrl_enter_callback(output_buffer);
                }
                return 0;
            } else {
                if (g_enter_callback != NULL) {
                    g_enter_callback(output_buffer);
                }
                selection_complete = true; 
                return 0;
            }
        }

        if (wParam == VK_ESCAPE) {
            output_buffer[0] = '\0'; 
            selection_complete = true;  
            return 0;
        }
        
        if (wParam == VK_TAB) {
            if (g_tab_callback != NULL) {
                g_tab_callback();
            } else {
              if (g_TotalMatches == 1) {
                UI_SetText(hEditControl,g_populated_cache[0]);
                UI_SetCursorPosition(hEditControl,strlen(g_populated_cache[0]));
              }
            }
            return 0;
        }
    } 
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
} 

LRESULT CALLBACK SimpleEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    HWND hParent = GetParent(hwnd);
    HWND hListBox = GetDlgItem(hParent, IDC_LIST_BOX);
    HWND hEditControl = hwnd;

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU) {
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        }
        if (wParam == VK_RETURN) {
            UI_GetText(hEditControl, output_buffer, sizeof(output_buffer));
            
            selection_complete = true; 
            
            return 0; 
        }

        if (wParam == VK_ESCAPE) {
            output_buffer[0] = '\0'; 
            selection_complete = true;  
            return 0;
        }
        
    } 
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
} 

LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData){
    switch (uMsg) {
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ListBoxSubclassProc, uIdSubclass);
            break;
        case WM_ERASEBKGND: {
                    HDC hdc = (HDC)wParam;
                    RECT rect;
                    GetClientRect(hWnd, &rect);
                    HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
                    FillRect(hdc, &rect, hBrush);
                    DeleteObject(hBrush);
                    return TRUE;
                }
            }
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }
      LRESULT CALLBACK SimpleListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
          switch (uMsg) {
              case WM_NCDESTROY:
                  RemoveWindowSubclass(hWnd, SimpleListBoxSubclassProc, uIdSubclass);
                  break;
      
              case WM_ERASEBKGND:
                  return 1; 
      
              case WM_PAINT: {
                  PAINTSTRUCT ps;
                  HDC hdc = BeginPaint(hWnd, &ps);
      
                  RECT rect;
                  GetClientRect(hWnd, &rect);
                  if (COLORFUL_MODE) {
                      int h = rect.bottom - rect.top;
                      int bandH = max(1, h / 32);
                      for (int y = 0; y < h; y += bandH) {
                          float ft = (float)y / (float)h;
                          COLORREF c = GetRainbowColor(ft, 0.55f, 0.18f);
                          HBRUSH hBrush = CreateSolidBrush(c);
                          RECT band = {rect.left, rect.top + y, rect.right, rect.top + min(y + bandH, h)};
                          FillRect(hdc, &band, hBrush);
                          DeleteObject(hBrush);
                      }
                  } else {
                      HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
                      FillRect(hdc, &rect, hBrush);
                      DeleteObject(hBrush);
                  }
      
                  int textLen = UI_LB_GetTextLen(hWnd);
                  
                  if (textLen > 0 && textLen < 512) {
                      char textBuffer[512] = {0}; 
                      UI_LB_GetText(hWnd, 0, textBuffer);
      
                      HFONT hFont = UI_GetFont(hWnd);
                      HFONT hOldFont = NULL;
                      if (hFont) hOldFont = (HFONT)SelectObject(hdc, hFont);
      
                      SetBkMode(hdc, TRANSPARENT);
                      SetTextColor(hdc, COLOR_TEXT);  
      
                      int itemHeight = UI_LB_GetItemHeight(hWnd);
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

int counter;
LRESULT CALLBACK PopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
      case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT);
            if (COLORFUL_MODE) {
                COLORREF bg = GetRainbowColor(0.02f, 0.55f, 0.20f);
                SetBkColor(hdc, bg);
                return (LRESULT)CreateSolidBrush(bg);
            } else {
                SetBkColor(hdc, COLOR_BG);
                return (LRESULT)CreateSolidBrush(COLOR_BG);
            }
        }
        case WM_CREATE: {
                if (!g_hNumFont) {
                    g_hNumFont = CreateFontA(NumberFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UsedFont);
                }
                if (!g_hMainFont) {
                    g_hMainFont = CreateFontA(FontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UsedFont);
                }
                break;
        }
        case WM_COMMAND: {
            int wmId   = LOWORD(wParam);
            int wmEvent = HIWORD(wParam);

            if (wmId == IDC_INPUT_BOX) {
                if (GetKeyState(VK_UP) & 0x8000 || 
                    GetKeyState(VK_DOWN) & 0x8000 || 
                    GetKeyState(VK_RETURN) & 0x8000 ||
                    GetKeyState(VK_ESCAPE) & 0x8000) 
                {
                    break;
                }

                if (hEditControl && hListBox) {
                    char input_buffer[256] = {0};
                    UI_GetText(hEditControl, input_buffer, sizeof(input_buffer));
                    if (g_refresh_callback != NULL) {
                        g_refresh_callback(input_buffer, hListBox);
                    }
                }
            }
            
            if (wmId == IDC_LIST_BOX && wmEvent == LBN_SELCHANGE) {
                break; 
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
              BOOL isSelected = (pdis->itemID == g_SelectedIndex);
              float t = (float)pdis->itemID / 20.0f;
              
              // === CACHED BRUSHES (created once) ===
              static HBRUSH hBgBrush = NULL;
              static HBRUSH hSelBrush = NULL;
              static HBRUSH hColorfulBrush[100] = {0};
              static bool brushes_initialized = false;
              
              if (!brushes_initialized) {
                  hBgBrush = CreateSolidBrush(COLOR_BG);
                  hSelBrush = CreateSolidBrush(COLOR_SELECTED_TEXT);
                  for (int i = 0; i < 99; i++) {
                      float hue = (float)i / 20.0f * 240.0f;
                      float h = hue / 60.0f;
                      float s = 0.85f;
                      float v = 0.85f;
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
                      hColorfulBrush[i] = CreateSolidBrush(RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)));
                  }
                  brushes_initialized = true;
              }
              static char num_cache[100][8] = {0};
              static bool num_init = false;
              if (!num_init) {
                  for (int i = 0; i < 100; i++) {
                      snprintf(num_cache[i], sizeof(num_cache[i]), "%d.", i + 1);
                  }
                  num_init = true;
              }
              
              static COLORREF rainbow_text_cache[100] = {0};
              static bool rainbow_init = false;
              if (!rainbow_init) {
                  for (int i = 0; i < 100; i++) {
                      float hue = (float)i / 20.0f * 240.0f;
                      float h = hue / 60.0f;
                      float s = 0.85f;
                      float v = 0.85f;
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
                      rainbow_text_cache[i] = RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
                  }
                  rainbow_init = true;
              }
              
              if (COLORFUL_MODE) {
                  int color_index = pdis->itemID % 100;
                  HBRUSH hBrush = isSelected ? hColorfulBrush[color_index] : hColorfulBrush[color_index];
                  FillRect(pdis->hDC, &pdis->rcItem, hBrush);
              } else {
                  FillRect(pdis->hDC, &pdis->rcItem, isSelected ? hSelBrush : hBgBrush);
              }
              
              if (pdis->itemID == -1 || pdis->itemID >= g_populated_count) {
                  return TRUE;
              }
              
              const char* textBuffer = "";
              if (pdis->itemID < g_populated_count) {
                  textBuffer = g_populated_cache[pdis->itemID];
              }
              
              SetBkMode(pdis->hDC, TRANSPARENT);
              RECT textRect = pdis->rcItem;
              
              if (pdis->itemID < 99) {
                  RECT numRect = pdis->rcItem;
                  numRect.left += 5;
                  numRect.right = numRect.left + 25;
                  
                  HFONT hOldFont = (HFONT)SelectObject(pdis->hDC, g_hNumFont);
                  SetTextColor(pdis->hDC, rainbow_text_cache[pdis->itemID]);
                  DrawTextA(pdis->hDC, num_cache[pdis->itemID], -1, &numRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
                  SelectObject(pdis->hDC, hOldFont);
                  textRect.left += 35;
              } else {
                  textRect.left += 10;
              }
              
              // === DRAW MAIN TEXT ===
              if (g_render_callback != NULL) {
                  g_render_callback(pdis->hDC, textBuffer, &textRect, isSelected, pdis->itemID);
              } else {
                  HFONT hOldMainFont = (HFONT)SelectObject(pdis->hDC, g_hMainFont);
                  DrawTextA(pdis->hDC, textBuffer, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
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
          if (COLORFUL_MODE) {
              int h = rect.bottom - rect.top;
              int bandH = max(1, h / 128);
              for (int y = 0; y < h; y += bandH) {
                  float ft = (float)y / (float)h;
                  COLORREF c = GetRainbowColor(ft, 0.55f, 0.18f);
                  HBRUSH hBrush = CreateSolidBrush(c);
                  RECT band = {rect.left, rect.top + y, rect.right, rect.top + min(y + bandH, h)};
                  FillRect(hdc, &band, hBrush);
                  DeleteObject(hBrush);
              }
          } else {
              HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
              FillRect(hdc, &ps.rcPaint, hBrush);
              DeleteObject(hBrush);
          }
          if (g_vim_mode != NULL) {
              wchar_t wmode[256];
              MultiByteToWideChar(CP_UTF8, 0, g_vim_mode, -1, wmode, 256);
              
              HFONT hOldFont = (HFONT)SelectObject(hdc, g_hMainFont);
              
              SetBkMode(hdc, TRANSPARENT);
              
              if (g_current_mode == MODE_NORMAL) {
                  SetTextColor(hdc, RGB(100, 255, 255));
              } else if (g_current_mode == MODE_INSERT) {
                  SetTextColor(hdc, RGB(50, 255, 50));
              } else {
                  SetTextColor(hdc, RGB(200, 3, 222));
              }
              
              SIZE sz;
              GetTextExtentPoint32W(hdc, wmode, (int)wcslen(wmode), &sz);
              
              RECT modeRect;
              modeRect.left = 15;
              modeRect.top = 10;
              modeRect.right = modeRect.left + sz.cx + 10;
              modeRect.bottom = modeRect.top + sz.cy + 10;
              
              DrawTextW(hdc, wmode, -1, &modeRect, DT_LEFT | DT_TOP);
              
              SelectObject(hdc, hOldFont);
          }
          
          if (g_title != NULL && strlen(g_title) > 0) {
              wchar_t wtitle[256];
              MultiByteToWideChar(CP_UTF8, 0, g_title, -1, wtitle, 256);
              
              HFONT hOldFont = (HFONT)SelectObject(hdc, g_hMainFont);
              SetBkMode(hdc, TRANSPARENT);
              
              SIZE sz;
              GetTextExtentPoint32W(hdc, wtitle, (int)wcslen(wtitle), &sz);
              
              RECT titleRect;
              titleRect.left = rect.right - sz.cx - 20;
              titleRect.top = 10;
              titleRect.right = rect.right - 10;
              titleRect.bottom = rect.top + sz.cy + 15;
              
              // === HARDCODED COLORS EVERY 25 ITEMS ===
              COLORREF titleColor;
              if (g_populated_count < 25) {
                  titleColor = RGB(0, 255, 0);       // Green
              } else if (g_populated_count < 50) {
                  titleColor = RGB(100, 255, 0);     // Yellow-green
              } else if (g_populated_count < 75) {
                  titleColor = RGB(200, 255, 0);     // Light yellow
              } else if (g_populated_count < 100) {
                  titleColor = RGB(255, 255, 0);     // Yellow
              } else if (g_populated_count < 125) {
                  titleColor = RGB(255, 200, 0);     // Gold
              } else if (g_populated_count < 150) {
                  titleColor = RGB(255, 150, 0);     // Orange
              } else if (g_populated_count < 175) {
                  titleColor = RGB(255, 100, 0);     // Dark orange
              } else if (g_populated_count < 200) {
                  titleColor = RGB(255, 50, 0);      // Orange-red
              } else {
                  titleColor = RGB(255, 0, 0);       // Pure red (200+ items)
              }
              
              SetTextColor(hdc, titleColor);
              DrawTextW(hdc, wtitle, -1, &titleRect, DT_LEFT | DT_TOP);
              
              SelectObject(hdc, hOldFont);
          }
          
          EndPaint(hwnd, &ps);
          break;
      }
      case WM_TIMER:
        if (wParam == 999) {
            static bool last_ctrl = false;
            bool current_ctrl = IsCtrlPressed();
            if (current_ctrl != last_ctrl) {
                last_ctrl = current_ctrl;
                if (hListBox != NULL && IsWindow(hListBox)) {
                    InvalidateRect(hListBox, NULL, FALSE);
                }
            }
        }
        break;              
        case WM_ERASEBKGND: {
            return TRUE;
        }
      default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
            
    }
    return 0;
}

static int g_currentAlpha = 245; 
static const char* g_displayMessage = NULL;
static HFONT g_hFont = NULL;

typedef struct {
    const char* message;
    HFONT hFont;
    int alpha;
} FadeWindowData;


LRESULT CALLBACK DisplayMessageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FadeWindowData* data = (FadeWindowData*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

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

            HBRUSH hBackgroundBrush = CreateSolidBrush(COLOR_BG); 
            FillRect(hdc, &rect, hBackgroundBrush);
            DeleteObject(hBackgroundBrush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            HFONT hOldFont = NULL;
            if (data->hFont) {
                hOldFont = (HFONT)SelectObject(hdc, data->hFont);
            }

            if (data->message) {
                RECT textRect = rect;
                textRect.left += 15;  
                textRect.right -= 15; 
                DrawTextA(hdc, data->message, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            if (hOldFont) SelectObject(hdc, hOldFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 999 && data) {
                if (g_timeout > 0) {
                    g_timeout--;
                    return 0; 
                }

                data->alpha -= 12; 

                if (data->alpha <= 0) {
                    KillTimer(hwnd, 999);
                    DestroyWindow(hwnd); // Self-destruct when completely invisible
                } else {
                    SetLayeredWindowAttributes(hwnd, 0, (BYTE)data->alpha, LWA_ALPHA);
                }
            }
            return 0;
        }
        case WM_NCDESTROY: {
            // CRITICAL ASYNC CLEANUP: Free allocated heap structures when window dies
            if (data) {
                if (data->message) free((void*)data->message);
                if (data->hFont) DeleteObject(data->hFont);
                free(data);
                SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            }
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
            
            nid.hIcon = (HICON)LoadImage(NULL,
                             TEXT(IconPath), 
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
                AppendMenu(hMenu, MF_STRING, ID_EXIT, "Exit");
                
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
            break;
        }
        case WM_TRIGGER_UNIVERSAL_MACRO: {
            MacroAction macroAction = (MacroAction)wParam;
            int macroDelay = (int)lParam;
            
            printf("[MACRO-WNDPROC] Received action=%d, delay=%d\n", macroAction, macroDelay);
            
            
            for (int i = 0; i < MACRO_BINDING_COUNT; i++) {
                if (G_MACRO_BINDINGS[i].action_id == macroAction && 
                    G_MACRO_BINDINGS[i].handler != NULL) {
                    G_MACRO_BINDINGS[i].handler();  
                    break;
                }
            }
            break;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_EXIT) {
                PostQuitMessage(0);
            }
            break;
        }
        
        case WM_DESTROY: {
            NOTIFYICONDATA nid = {0};
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hwnd;
            nid.uID = ID_TRAY_ICON;
            Shell_NotifyIcon(NIM_DELETE, &nid);
            if (g_hNumFont) {
                DeleteObject(g_hNumFont);
                g_hNumFont = NULL;
            }
            if (g_hMainFont) {
                DeleteObject(g_hMainFont);
                g_hMainFont = NULL;
            }
            PostQuitMessage(0);
            break;
        }
        
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WindowList* myList = (WindowList*)lParam;

    if (!IsWindowVisible(hwnd) || GetWindowTextLengthA(hwnd) == 0) {
        return TRUE;
    }

    if (myList->count >= myList->capacity) {
        return FALSE; 
    }

    WindowData* currentItem = &myList->items[myList->count];
    currentItem->hwnd = hwnd;

    GetWindowTextA(hwnd, currentItem->title, sizeof(currentItem->title));
    
    currentItem->exe[0] = '\0'; 

    myList->count++;
    return TRUE;
}

#endif
