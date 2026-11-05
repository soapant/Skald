#include "core/cc_registry.h"

#include <string.h>
#include <stdio.h>

#include "core/feature_loader.h"

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
