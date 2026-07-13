#ifndef EXPLORER_H
#define EXPLORER_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <shellapi.h>
#include "types.h"
#include "helpers.h"
extern HWND hPopupWnd;

static void TriggerExplorer(HWND hPopupWnd) {
    ZoxideOptions options = {
        .prompt_title = "Enter zoxide query (EXPLORER)...",
        .icon = "📁 Zoxide",
        .open_neovide = false,
        .open_yazi = false,
        .open_explorer = true
    };
    TriggerZoxideWithOptions(&options);
}

#endif
