#include "core/cap_registry.h"
#include <string.h>
#include <stdio.h>
#include "deps/log.h"
#include "core/feature_loader.h"
#include "helpers/helpers_owner.h"
#include "platform/platform.h"

typedef struct {
  char key[CAP_KEY_LEN];
  char owner[512];
  unsigned abi;
  void *fn;
} Cap;

static Cap caps[CAP_MAX];
static int cap_count = 0;
static volatile unsigned cap_gen = 1;

static void *cap_mutex = NULL;

static void cap_lock(void) {
  if (!cap_mutex)
    cap_mutex = Platform_MutexCreate();
  if (cap_mutex)
    Platform_MutexLock(cap_mutex);
  else
    log_error(
        "cap registry: could not create mutex, continuing unsynchronized");
}

static void cap_unlock(void) {
  if (cap_mutex)
    Platform_MutexUnlock(cap_mutex);
}

unsigned cap_generation(void) { return cap_gen; }

static void cap_add_provides(const char *owner, const char *key) {
  feature_loader_manifest_add_provides(owner, key);
}

void cap_register_abi(const char *key, unsigned abi, void *fn) {
  if (!key || !key[0] || !fn)
    return;
  if (strlen(key) >= CAP_KEY_LEN) {
    log_error("cap key too long (max %d), refusing registration",
              CAP_KEY_LEN - 1);
    return;
  }
  cap_lock();

  char owner[OWNER_PATH_MAX];
  owner_resolve_for_func(fn, owner, sizeof(owner));

  for (int i = 0; i < cap_count; i++) {
    if (strcmp(caps[i].key, key) != 0)
      continue;
    if (caps[i].fn == fn) {
      cap_unlock();
      return;
    }
    log_info("cap '%s' replaced (was abi=%u now abi=%u)", key, caps[i].abi,
             abi);
    snprintf(caps[i].key, CAP_KEY_LEN, "%s", key);
    snprintf(caps[i].owner, sizeof(caps[i].owner), "%s", owner);
    caps[i].abi = abi;
    caps[i].fn = fn;
    cap_gen++;
    cap_unlock();
    if (owner[0])
      cap_add_provides(owner, key);
    return;
  }

  if (cap_count >= CAP_MAX) {
    log_error("cap table full, dropping '%s'", key);
    cap_unlock();
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
  cap_unlock();
  if (owner[0])
    cap_add_provides(owner, key);
}

void *cap_resolve_abi(const char *key, unsigned abi) {
  if (!key || !key[0])
    return NULL;
  cap_lock();
  for (int i = 0; i < cap_count; i++) {
    if (strcmp(caps[i].key, key) != 0)
      continue;
    if (caps[i].abi != abi) {
      log_warn("cap '%s' abi mismatch want=%u have=%u", key, abi, caps[i].abi);
      cap_unlock();
      return NULL;
    }
    void *fn = caps[i].fn;
    cap_unlock();
    return fn;
  }
  cap_unlock();
  return NULL;
}

void cap_bind_declare(const char *key, void *consumer) {
  if (!key || !key[0] || !consumer) {
    return;
  }

  char consumer_path[OWNER_PATH_MAX];
  owner_resolve_for_func(consumer, consumer_path, sizeof(consumer_path));
  if (!consumer_path[0]) {
    return;
  }

  cap_lock();
  for (int i = 0; i < cap_count; i++) {
    if (strcmp(caps[i].key, key) != 0)
      continue;
    if (caps[i].abi != CAP_ABI_V1) {
      cap_unlock();
      return;
    }
    if (!caps[i].owner[0]) {
      cap_unlock();
      return;
    }
    char provider[64];
    feature_loader_owner_feature_name(caps[i].owner, provider, sizeof(provider));
    cap_unlock();
    if (!provider[0]) {
      return;
    }
    feature_loader_manifest_add_depends(consumer_path, provider);
    return;
  }
  cap_unlock();
}

void cap_purge_owner(const char *path_a, const char *path_b) {
  if (owner_purge_inactive(path_a, path_b)) {
    return;
  }

  int n = 0;
  cap_lock();
  for (int i = 0; i < cap_count; i++) {
    if (owner_module_matches(caps[i].owner, path_a, path_b)) {
      continue;
    }
    caps[n++] = caps[i];
  }
  int dropped = cap_count - n;
  cap_count = n;
  if (dropped > 0) {
    cap_gen++;
  }
  cap_unlock();
}
