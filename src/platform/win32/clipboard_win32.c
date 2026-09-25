#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "deps/log.h"
#include "config.h"
#include "platform/platform.h"
#include "helpers/helpers_hotkey.h"

int Platform_CopyToClipboard(const char* text) {
    if (text == NULL) return 0;

    size_t len = strlen(text);
    if (len == 0) return 0;

    char* clean_text = (char*)malloc(len + 1);
    if (clean_text == NULL) return 0;

    size_t write_idx = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] != '\r') {
            clean_text[write_idx++] = text[i];
        }
    }
    clean_text[write_idx] = '\0';

    len = write_idx;
    if (len == 0) {
        free(clean_text);
        return 0;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (hMem == NULL) {
        free(clean_text);
        return 0;
    }

    char* pMem = (char*)GlobalLock(hMem);
    if (pMem == NULL) {
        GlobalFree(hMem);
        free(clean_text);
        return 0;
    }
    
    memcpy(pMem, clean_text, len + 1); 
    GlobalUnlock(hMem);
    free(clean_text);

    if (!OpenClipboard(NULL)) {
        GlobalFree(hMem);
        return 0;
    }

    if (!EmptyClipboard()) {
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }

    if (SetClipboardData(CF_TEXT, hMem) == NULL) {
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }
    char buffer[256];
    
    snprintf(buffer,sizeof(buffer),"Copied %s",text);
    SubjectiveMessageDisplay(buffer,140,TimeOutForMessages,COLOR_TEXT);
    CloseClipboard();
    
log_info("Platform_CopyToClipboard text='%s'", text ? text : "");;
    return 1;
}

int Platform_CopyImageToClipboard(const unsigned char* png, size_t len) {
    if (!png || len == 0) return 0;
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    strcat(tmp, "flint_clip_up.png");
    FILE* f = fopen(tmp, "wb");
    if (!f) return 0;
    size_t w = fwrite(png, 1, len, f);
    fclose(f);
    if (w != len) { remove(tmp); return 0; }
    wchar_t wtmp[512];
    MultiByteToWideChar(CP_ACP, 0, tmp, -1, wtmp, 512);
    GpBitmap* bmp = NULL;
    if (GdipCreateBitmapFromFile(wtmp, &bmp) != 0 || !bmp) { remove(tmp); return 0; }
    HBITMAP hbm = NULL;
    if (GdipCreateHBITMAPFromBitmap((GpImage*)bmp, &hbm, 0) != 0 || !hbm) { GdipDisposeImage((GpImage*)bmp); remove(tmp); return 0; }
    GdipDisposeImage((GpImage*)bmp);
    remove(tmp);
    BITMAP bm;
    memset(&bm, 0, sizeof(bm));
    if (GetObjectA(hbm, sizeof(bm), &bm) == 0) { DeleteObject(hbm); return 0; }
    int width = bm.bmWidth;
    int height = abs(bm.bmHeight);
    if (width <= 0 || height <= 0) { DeleteObject(hbm); return 0; }
    int stride = ((width * 32 + 31) / 32) * 4;
    size_t img = (size_t)stride * (size_t)height;
    HGLOBAL hDib = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + img);
    if (!hDib) { DeleteObject(hbm); return 0; }
    BITMAPINFOHEADER* hdr = (BITMAPINFOHEADER*)GlobalLock(hDib);
    if (!hdr) { GlobalFree(hDib); DeleteObject(hbm); return 0; }
    memset(hdr, 0, sizeof(*hdr));
    hdr->biSize = sizeof(*hdr);
    hdr->biWidth = width;
    hdr->biHeight = bm.bmHeight;
    hdr->biPlanes = 1;
    hdr->biBitCount = 32;
    hdr->biCompression = BI_RGB;
    hdr->biSizeImage = (DWORD)img;
    HDC hdc = GetDC(NULL);
    int lines = GetDIBits(hdc, hbm, 0, (UINT)height, (BYTE*)hdr + sizeof(*hdr), (BITMAPINFO*)hdr, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);
    GlobalUnlock(hDib);
    DeleteObject(hbm);
    if (lines == 0) { GlobalFree(hDib); return 0; }
    if (!OpenClipboard(NULL)) { GlobalFree(hDib); return 0; }
    EmptyClipboard();
    if (!SetClipboardData(CF_DIB, hDib)) { CloseClipboard(); GlobalFree(hDib); return 0; }
    CloseClipboard();
    log_info("Platform_CopyImageToClipboard ok %dx%d", width, height);
    return 1;
}
