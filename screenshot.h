#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "types.h"

void SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath) {
    int width = abs(x2 - x1);
    int height = abs(y2 - y1);
    if (width == 0 || height == 0) return;

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, (x1 < x2 ? x1 : x2), (y1 < y2 ? y1 : y2), SRCCOPY);

    GpBitmap* pBitmap = NULL;
    GdipCreateBitmapFromHBITMAP(hBitmap, NULL, &pBitmap);

    if (pBitmap != NULL) {
        CLSID pngClsid;
        UINT num = 0, size = 0;
        GdipGetImageEncodersSize(&num, &size); 
        
        ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
        if (pImageCodecInfo != NULL) {
            GdipGetImageEncoders(num, size, pImageCodecInfo); 
            for (UINT j = 0; j < num; ++j) {
                if (wcscmp(pImageCodecInfo[j].MimeType, L"image/png") == 0) {
                    pngClsid = pImageCodecInfo[j].Clsid;
                    break;
                }
            }
            free(pImageCodecInfo);
        }

        wchar_t wPath[512];
        MultiByteToWideChar(CP_ACP, 0, fullPath, -1, wPath, 512);

        GdipSaveImageToFile((GpImage*)pBitmap, wPath, &pngClsid, NULL);
        
        GdipDisposeImage((GpImage*)pBitmap);
    }

    
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}

static void TriggerScreenshot(HWND hPopupWnd,bool open_screenshot) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    const char* name_prompt = "Enter image name...";
    char* raw_name = SimplePromptUser(name_prompt,"📷 Screenshot",920, 650,32);

    if (raw_name != NULL && strlen(raw_name) > 0) {
        char final_file_path[512];
        snprintf(final_file_path, sizeof(final_file_path), "%s%s.png", BASE_SCREENSHOT_FOLDER_PATH, raw_name);
        
        SaveRegionToPNG((int)img_x1, (int)img_y1, (int)img_x2, (int)img_y2, final_file_path);
        printf("[SCREENSHOT] Saved: %s\n", final_file_path);
        if (open_screenshot) {
          char open_cmd[1024];
          snprintf(open_cmd, sizeof(open_cmd), "start %s \"%s\"", UsedImageEditor, final_file_path);
          system(open_cmd);
          printf("[SCREENSHOT] Opening in editor: %s\n", open_cmd);
        }
    } else {
        printf("[SCREENSHOT] Aborted (no name provided)\n");
    }
    
    img_region_step = 0;
}

#endif
