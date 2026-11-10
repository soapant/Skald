#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "core/types/file.h"
#include "platform/platform.h"
#include "helpers/helpers_text.h"
#include "config.h"
#include "helpers/win32/default_apps_win32.h"

const char *win32_terminal_name_from_guid(const char *console_guid,
                                          const char *terminal_guid) {

  if (strstr(console_guid, "2EACA947-7F5F-4CFA-BA87-8F7FBEEFBE69") ||
      strstr(terminal_guid, "E12CFF52-A866-4C77-9A90-F570A7AA2C6B")) {
    return "Windows Terminal";
  }
  if (strstr(console_guid, "B23D10C0-E52E-411E-9D5B-C09FDF709C7D")) {
    return "Legacy Console Host";
  }
  return NULL;
}

#define SM_SLOTS 8

static ListOfFiles g_sm_files;

static bool g_sm_scanned = false;

static char g_sm_match[SM_SLOTS][128];

static bool g_sm_slot_done[SM_SLOTS];

const char *sm_lookup(int slot, const char *const possibilities[],
                             size_t count) {

  if (slot < 0 || slot >= SM_SLOTS)
    return NULL;
  if (g_sm_slot_done[slot])
    return g_sm_match[slot][0] ? g_sm_match[slot] : NULL;
  g_sm_slot_done[slot] = true;

  if (!g_sm_scanned) {
    g_sm_scanned = true;
    FileWalk_AllFilesFromDir(
        "C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs", NULL,
        &g_sm_files, NULL, true, 0);
  }
  for (size_t j = 0; j < count; j++) {
    for (size_t i = 0; i < g_sm_files.count; i++) {
      if (String_ContainsCI(g_sm_files.files[i].name,
                                                 possibilities[j])) {
        char *name = g_sm_files.files[i].name;
        name[0] = (char)tolower((unsigned char)name[0]);
        snprintf(g_sm_match[slot], sizeof(g_sm_match[slot]), "%s", name);
        return g_sm_match[slot];
      }
    }
  }
  return NULL;
}

void win32_default_apps_free_cache(void) {
  if (g_sm_files.files) {
    free(g_sm_files.files);
    g_sm_files.files = NULL;
  }
  g_sm_files.count = 0;
  g_sm_files.capacity = 0;
  g_sm_scanned = false;
  for (int i = 0; i < SM_SLOTS; i++) {
    g_sm_match[i][0] = '\0';
    g_sm_slot_done[i] = false;
  }
}

