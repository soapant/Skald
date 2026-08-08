#include "common.h"
#include "core/types.h"

KeyCallback g_key_callback = NULL;

SequenceState g_seq_state = {0};

FileEntry G_FILE_ENTRIES[7] = {
    {"TerminalHistory", DATA_DIR "TerminalHistory.txt", NULL},
    {"YaziZoxide",      DATA_DIR "YaziZoxide.txt", NULL},
    {"NvimZoxide",      DATA_DIR "NvimZoxide.txt", NULL},
    {"TerminalZoxide",      DATA_DIR "TerminalZoxide.txt", NULL},
    {"ExplorerZoxide",  DATA_DIR "ExplorerZoxide.txt", NULL},
    {"GoogleZoxide",  DATA_DIR "GoogleZoxide.txt", NULL},
    {"ReminderHistory",  DATA_DIR "ReminderHistory.txt", NULL},
};

const char* GetFilePath(const char* name) {
    for (int i = 0; i < FILE_ENTRY_COUNT; i++) {
        if (strcmp(G_FILE_ENTRIES[i].name, name) == 0) {
            return G_FILE_ENTRIES[i].path;
        }
    }
    return NULL;
}
