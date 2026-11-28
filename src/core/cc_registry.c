#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/cc_registry.h"
#include "core/feature_loader.h"
#include "core/owner_purge.h"
#include "core/owner_registry.h"
#include "core/types/file.h"
#include "deps/stb_ds.h"
#include "helpers/helpers_misc.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_text.h"

typedef struct {
  CcEntrySpec pub;
  char display_name[1024];
  char internal_name[1024];
  char icon[1024];
  char type[128];
  OwnerTag tag;
} CcEntry;

static CcEntry *cc_entries;
#define cc_entry_count arrlenu(cc_entries)

static char **cc_dep_owners;
#define cc_dep_owner_count arrlenu(cc_dep_owners)

bool cc_add_dep(const char *dep_name, const char *owner_file) {
  if (!dep_name || !dep_name[0] || !owner_file || !owner_file[0])
    return false;
  char dep[64];
  safe_copy(dep, sizeof(dep), dep_name);
  char *dot = strrchr(dep, '.');
  if (dot)
    *dot = '\0';
  if (!dep[0])
    return false;
  feature_loader_manifest_add_depends(owner_file, dep);
  return true;
}

static void cc_rebind_all(void) {
  for (size_t i = 0; i < cc_entry_count; i++) {
    CcEntry *e = &cc_entries[i];
    e->pub.display_name = e->display_name;
    e->pub.internal_name = e->internal_name;
    e->pub.icon = e->icon;
    e->pub.type = e->type;
  }
}

static bool cc_dep_seen(const char *owner) {
  if (!owner)
    return false;
  for (size_t i = 0; i < cc_dep_owner_count; i++)
    if (cc_dep_owners[i] && strcmp(cc_dep_owners[i], owner) == 0)
      return true;
  return false;
}

static void cc_dep_mark(const char *owner) {
  char *copy = owner_strdup(owner);
  if (!copy)
    return;
  arrput(cc_dep_owners, copy);
}

static void cc_dep_unmark(const char *owner) {
  if (!owner)
    return;
  for (size_t i = 0; i < cc_dep_owner_count;) {
    if (cc_dep_owners[i] && strcmp(cc_dep_owners[i], owner) == 0) {
      owner_strfree(&cc_dep_owners[i]);
      arrdel(cc_dep_owners, i);
      continue;
    }
    i++;
  }
}

static void cc_entry_destroy(void *element) {
  CcEntry *e = (CcEntry *)element;
  if (!e)
    return;
  cc_dep_unmark(e->tag.owner);
  owner_tag_clear(&e->tag);
}

bool cc_add_from_spec(const CcEntrySpec *spec, void *caller) {
  if (!spec)
    return false;
  int slot = -1;
  for (size_t i = 0; i < cc_entry_count; i++) {
    if (spec->display_name && cc_entries[i].display_name[0] &&
        strcmp(cc_entries[i].display_name, spec->display_name) == 0) {
      slot = (int)i;
      break;
    }
  }
  if (slot < 0) {
    slot = (int)cc_entry_count;
    arrput(cc_entries, (CcEntry){0});
  }
  CcEntry *e = &cc_entries[slot];
  owner_tag_clear(&e->tag);
  memset(e, 0, sizeof(*e));
  e->pub.on_enter = spec->on_enter;
  e->pub.on_ctrl_enter = spec->on_ctrl_enter;
  e->pub.on_shift_enter = spec->on_shift_enter;
  e->pub.on_ctrl_shift_enter = spec->on_ctrl_shift_enter;
  e->pub.on_ctrl_alt_enter = spec->on_ctrl_alt_enter;
  e->pub.on_shift_alt_enter = spec->on_shift_alt_enter;
  e->pub.on_tab = spec->on_tab;
  e->pub.on_action = spec->on_action;
  safe_copy(e->display_name, sizeof(e->display_name), spec->display_name);
  e->pub.display_name = e->display_name;
  char icon_asset[MAX_PATH];
  if (resolve_icon_path(caller, spec->icon, icon_asset, sizeof(icon_asset))) {
    safe_copy(e->icon, sizeof(e->icon), icon_asset);
  } else {
    safe_copy(e->icon, sizeof(e->icon), spec->icon);
  }
  e->pub.icon = e->icon;
  safe_copy(e->type, sizeof(e->type), spec->type);
  e->pub.type = e->type;
  if (spec->internal_name)
    safe_copy(e->internal_name, sizeof(e->internal_name), spec->internal_name);
  else
    safe_copy(e->internal_name, sizeof(e->internal_name), spec->display_name);
  e->pub.internal_name = e->internal_name;
  void *anchor = (void *)(uintptr_t)spec->on_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_ctrl_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_shift_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_ctrl_shift_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_ctrl_alt_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_shift_alt_enter;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_tab;
  if (!anchor)
    anchor = (void *)(uintptr_t)spec->on_action;
  owner_tag_set_for_func(&e->tag, anchor, 0);
  const char *owner = e->tag.owner ? e->tag.owner : "";
  if (owner[0] && !cc_dep_seen(owner)) {
    cc_dep_mark(owner);
    cc_add_dep("control_center.c", owner);
  }
  if (spec->type && strcmp(spec->type, "Feature") == 0)
    feature_list_add(spec->display_name, spec->on_action, e->pub.icon, owner);
  cc_rebind_all();
  return true;
}

int cc_registry_count(void) { return (int)cc_entry_count; }

const CcEntrySpec *cc_registry_at(int index) {
  if (index < 0 || (size_t)index >= cc_entry_count)
    return NULL;
  return &cc_entries[index].pub;
}

OWNER_PURGE_REGISTER2(cc_registry_purge_owner)

void cc_registry_purge_owner(const char *path_a, const char *path_b) {
  if (!path_a && !path_b)
    return;
  OWNER_PURGE_STRUCT(cc_entries, CcEntry, path_a, path_b, 0, cc_entry_destroy);
  cc_rebind_all();
}
