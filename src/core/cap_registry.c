#include "core/cap_registry.h"
#include <string.h>
#include <stdio.h>
#include "deps/log.h"
#include "core/feature_loader.h"
#include "helpers/helpers_owner.h"

typedef struct {
  char key[CAP_KEY_LEN];
  char owner[512];
  unsigned abi;
  void *fn;
} Cap;

static Cap caps[CAP_MAX];
static int cap_count = 0;
static unsigned cap_gen = 1;

unsigned cap_generation(void) { return cap_gen; }

static void cap_add_provides(const char *owner, const char *key) {
  FeatureLoader_ManifestAddProvides(owner, key);
}

void cap_register_abi(const char *key, unsigned abi, void *fn) {
  if (!key || !key[0] || !fn)
    return;
  if (strlen(key) >= CAP_KEY_LEN) {
    log_error("cap key too long (max %d), refusing registration",
              CAP_KEY_LEN - 1);
    return;
  }

  char owner[OWNER_PATH_MAX];
  Owner_ResolveForFunc(fn, owner, sizeof(owner));

  for (int i = 0; i < cap_count; i++) {
    if (strcmp(caps[i].key, key) != 0)
      continue;
    if (caps[i].fn == fn)
      return;
    log_info("cap '%s' replaced (was abi=%u now abi=%u)", key, caps[i].abi,
             abi);
    snprintf(caps[i].key, CAP_KEY_LEN, "%s", key);
    snprintf(caps[i].owner, sizeof(caps[i].owner), "%s", owner);
    caps[i].abi = abi;
    caps[i].fn = fn;
    cap_gen++;
    if (owner[0])
      cap_add_provides(owner, key);
    return;
  }

  if (cap_count >= CAP_MAX) {

    log_error("cap table full, dropping '%s'", key);
    return;
  }
  Cap *c = &caps[cap_count++];
  snprintf(c->key, CAP_KEY_LEN, "%s", key);
  snprintf(c->owner, sizeof(c->owner), "%s", owner);
  c->abi = abi;
  c->fn = fn;
  cap_gen++;
  log_info("cap '%s' registered abi=%u owner='%s'", key, abi,
           owner[0] ? owner : "?");
  if (owner[0])
    cap_add_provides(owner, key);
}

void *cap_resolve_abi(const char *key, unsigned abi) {
  if (!key || !key[0])
    return NULL;
  for (int i = 0; i < cap_count; i++) {
    if (strcmp(caps[i].key, key) != 0)
      continue;
    if (caps[i].abi != abi) {

      log_warn("cap '%s' abi mismatch want=%u have=%u", key, abi, caps[i].abi);
      return NULL;
    }
    return caps[i].fn;
  }
  return NULL;
}

void cap_bind_declare(const char *key, void *consumer) {
  if (!key || !key[0] || !consumer) {
    return;
  }

  char consumer_path[OWNER_PATH_MAX];
  Owner_ResolveForFunc(consumer, consumer_path, sizeof(consumer_path));
  if (!consumer_path[0]) {
    return;
  }

  for (int i = 0; i < cap_count; i++) {
    if (strcmp(caps[i].key, key) != 0)
      continue;
    if (caps[i].abi != CAP_ABI_V1) {
      return;
    }
    if (!caps[i].owner[0]) {
      return;
    }
    char provider[64];
    FeatureLoader_OwnerFeatureName(caps[i].owner, provider, sizeof(provider));
    if (!provider[0]) {
      return;
    }
    FeatureLoader_ManifestAddDepends(consumer_path, provider);
    return;
  }
}

void cap_purge_owner(const char *path_a, const char *path_b) {
  if (Owner_PurgeInactive(path_a, path_b)) {
    return;
  }

  int n = 0;
  for (int i = 0; i < cap_count; i++) {
    if (Owner_ModuleMatches(caps[i].owner, path_a, path_b)) {
      continue;
    }
    caps[n++] = caps[i];
  }
  int dropped = cap_count - n;
  cap_count = n;
  if (dropped > 0) {
    cap_gen++;
  }
}
