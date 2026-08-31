#ifndef PROCESS_KILLER_H
#define PROCESS_KILLER_H

void terminate_process_by_name(const char* exe_name);
bool HandleBulkKillProcess(int vk_code, int modifiers, const char* current_title);
void TriggerProcessKiller(void);

#endif
