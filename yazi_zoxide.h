#ifndef YAZI_ZOXIDE_H
#define YAZI_ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "helpers.h"
extern HWND hPopupWnd;

static void TriggerYaziZoxide(HWND hPopupWnd) {
    ZoxideOptions options = {
        .prompt_title = "Enter zoxide query (YAZI)...",
        .icon = "📂 Zoxide",
        .open_neovide = false,
        .open_yazi = true,
        .open_explorer = false
    };
    TriggerZoxideWithOptions(&options);
}

#endif
