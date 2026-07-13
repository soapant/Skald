#ifndef ZOXIDE_H
#define ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "helpers.h"

extern HWND hPopupWnd;

static void TriggerZoxide(HWND hPopupWnd) {
    ZoxideOptions options = {
        .prompt_title = "Enter zoxide query (TERMINAL)...",
        .icon = "⌨️ Zoxide",
        .open_neovide = false,
        .open_yazi = false,
        .open_explorer = false
    };
    TriggerZoxideWithOptions(&options);
}

#endif
