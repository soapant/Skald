#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "types.h"

extern HWND hPopupWnd;
extern void SaveRegionToPNG(int x1, int y1, int x2, int y2, const char* fullPath);
extern int img_region_step;
extern long img_x1, img_y1, img_x2, img_y2;

#define SCREENSHOT_DIR "C:\\Users\\HP\\Pictures\\Screenshots\\"
#define IMAGE_EDITOR "picview.exe"

static void TriggerScreenshot(HWND hPopupWnd,bool open_screenshot) {
    if (hPopupWnd != NULL && IsWindow(hPopupWnd)) {
        return;
    }

    const char* name_prompt[1] = { "Enter image name..." };
    char* raw_name = PromptUser(name_prompt,"📷 Screenshot", 1, 220, 150,32,NULL,NULL,NULL,NULL,NULL);

    if (raw_name != NULL && strlen(raw_name) > 0) {
        char final_file_path[512];
        snprintf(final_file_path, sizeof(final_file_path), "%s%s.png", SCREENSHOT_DIR, raw_name);
        
        SaveRegionToPNG((int)img_x1, (int)img_y1, (int)img_x2, (int)img_y2, final_file_path);
        printf("[SCREENSHOT] Saved: %s\n", final_file_path);
        if (open_screenshot) {
          char open_cmd[1024];
          snprintf(open_cmd, sizeof(open_cmd), "start %s \"%s\"", IMAGE_EDITOR, final_file_path);
          system(open_cmd);
          printf("[SCREENSHOT] Opening in editor: %s\n", open_cmd);
        }
    } else {
        printf("[SCREENSHOT] Aborted (no name provided)\n");
    }
    
    // Reset region step
    img_region_step = 0;
}

#endif
