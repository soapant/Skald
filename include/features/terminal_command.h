#ifndef TERMINAL_COMMAND_H
#define TERMINAL_COMMAND_H


void RunTerminalCommand(const char* Command, bool save_to_history,bool open_in_terminal);
void TriggerTerminalCommand(void);
void terminal_command_init(void);

#endif
