#include "common.h"
#include "core/types.h"

KeyCallback g_key_callback = NULL;

FileEntry G_FILE_ENTRIES[7] = {
    {"TerminalHistory", DATA_DIR "TerminalHistory.txt", NULL},
    {"YaziZoxide",      DATA_DIR "YaziZoxide.txt", NULL},
    {"NvimZoxide",      DATA_DIR "NvimZoxide.txt", NULL},
    {"TerminalZoxide",      DATA_DIR "TerminalZoxide.txt", NULL},
    {"ExplorerZoxide",  DATA_DIR "ExplorerZoxide.txt", NULL},
    {"GoogleZoxide",  DATA_DIR "GoogleZoxide.txt", NULL},
    {"ReminderHistory",  DATA_DIR "ReminderHistory.txt", NULL},
};

