#include "core/cc_registry.h"
#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_owner.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/feature_loader.h"

typedef struct {
  CcEntrySpec pub;
  char display_name[1024];
  char internal_name[1024];
  char icon[1024];
  char type[128];
  char owner[OWNER_PATH_MAX];
} CcEntry;

static int g_cc_entry_count = 0;
static CcEntry g_cc_entries[4096];

#define FL_CC_DEP_OWNERS 256
static char g_cc_dep_owners[FL_CC_DEP_OWNERS][OWNER_PATH_MAX];
static int g_cc_dep_owner_count = 0;

bool add_dep(const char *dep_name, const char *owner_file) {
  if (!dep_name || !dep_name[0] || !owner_file || !owner_file[0])
    return false;
  char dep[64];
  snprintf(dep, sizeof(dep), "%s", dep_name);
  char *dot = strrchr(dep, '.');
  if (dot)
    *dot = '\0';
  if (!dep[0])
    return false;
  FeatureLoader_ManifestAddDepends(owner_file, dep);
  return true;
}

void cc_registry_purge_owner(const char *path_a, const char *path_b) {
  if (!path_a && !path_b)
    return;
  int out = 0;
  for (int i = 0; i < g_cc_entry_count; i++) {
    if (Owner_ModuleMatches(g_cc_entries[i].owner, path_a, path_b))
      continue;
    if (out != i)
      g_cc_entries[out] = g_cc_entries[i];
    out++;
  }
  g_cc_entry_count = out;
}
