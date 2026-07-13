#ifndef NVIM_ZOXIDE_H
#define NVIM_ZOXIDE_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "helpers.h"
extern HWND hPopupWnd;

static void TriggerNvimZoxide(int version, HWND hPopupWnd) {
    ZoxideOptions options = {
        .prompt_title = "Enter zoxide query (NEOVIDE)...",
        .icon = "📝 Neovide / Nvim",
        .open_neovide = (version == 0),
        .open_yazi = false,
        .open_explorer = false
    };
    TriggerZoxideWithOptions(&options);
}

#endif
