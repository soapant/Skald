#ifndef TERMINAL_COMMAND_H
#define TERMINAL_COMMAND_H

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"

extern HWND hPopupWnd;

static void TriggerTerminalCommand(HWND hPopupWnd) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    const char* prompt[1] = { "Enter Terminal Command ..." };
    char* query = PromptUser(prompt,"Terminal", 1, 220, 150,32, NULL, NULL, NULL, NULL,NULL);

    if (query != NULL && strlen(query) > 0) {
        query[strcspn(query, "\r\n")] = '\0';

        // Write command to a temp file
        char temp_file[MAX_PATH];
        GetTempPathA(MAX_PATH, temp_file);
        strcat(temp_file, "temp_cmd.sh");
        
        FILE* f = fopen(temp_file, "w");
        if (f) {
            fprintf(f, "#!/bin/bash\n");
            fprintf(f, "eval \"$(zoxide init bash)\"\n"); 
            fprintf(f, "%s\nexec bash\n", query);
            fclose(f);
        }
        
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), 
                 "start alacritty -e \"C:\\Program Files\\Git\\bin\\bash.exe\" %s", temp_file);
        system(cmd);
        printf("[TERMINAL COMMAND] Running: %s\n", query);
    }
}

#endif
