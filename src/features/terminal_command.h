#ifndef TERMINAL_COMMAND_H
#define TERMINAL_COMMAND_H

#include <stdbool.h>
#include "core/types.h"

#define FILE_TERMINAL_HISTORY 0
#define TerminalHistoryFile G_FILE_ENTRIES[FILE_TERMINAL_HISTORY].mf

void RunTerminalCommand(const char* Command, bool save_to_history,bool open_in_terminal);
void TriggerTerminalCommand(void);

#endif
