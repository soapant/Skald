#include "core/cap_registry.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "deps/log.h"
#include "core/feature_loader.h"
#include "core/owner_registry.h"
#include "helpers/helpers_owner.h"
#include "platform/platform.h"
#include "deps/stb_ds.h"
#include "helpers/helpers_misc.h"
#include "core/owner_purge.h"

typedef struct {
  char *key;
  OwnerTag tag;
  unsigned abi;
  void *fn;
} Cap;

static Cap *caps;
#define cap_count arrlenu(caps)
static _Atomic unsigned cap_gen = 1;

static void *cap_mutex = NULL;

static void cap_lock(void) {
  lazy_mutex_get(&cap_mutex);
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

static void cap_destroy(void *element) {
  Cap *c = (Cap *)element;
  owner_strfree(&c->key);
  owner_tag_clear(&c->tag);
}

unsigned cap_generation(void) {
  return atomic_load_explicit(&cap_gen, memory_order_acquire);
}

void cap_register_abi(const char *key, unsigned abi, void *fn) {
  if (!key || !key[0] || !fn)
    return;
  unsigned gen = atomic_load_explicit(&cap_gen, memory_order_acquire);
  cap_lock();

  char *owner = owner_resolve_for_func_alloc(fn);

  for (size_t i = 0; i < cap_count; i++) {
    if (!caps[i].key || strcmp(caps[i].key, key) != 0)
      continue;
    if (caps[i].fn == fn) {
      cap_unlock();
      owner_strfree(&owner);
      return;
    }
    log_info("cap '%s' replaced (was abi=%u now abi=%u)", key, caps[i].abi,
             abi);
    char *new_key = owner_strdup(key);
    owner_strfree(&caps[i].key);
    caps[i].key = new_key;
    owner_tag_set(&caps[i].tag, owner, gen);
    caps[i].abi = abi;
    caps[i].fn = fn;
    atomic_fetch_add_explicit(&cap_gen, 1, memory_order_acq_rel);
    cap_unlock();
    if (owner && owner[0])
      feature_loader_manifest_add_provides(owner, key);
    owner_strfree(&owner);
    return;
  }

  Cap *c = arraddnptr(caps, 1);
  memset(c, 0, sizeof(*c));
  c->key = owner_strdup(key);
  owner_tag_set_for_func(&c->tag, fn, gen);
  c->abi = abi;
  c->fn = fn;
  atomic_fetch_add_explicit(&cap_gen, 1, memory_order_acq_rel);
  log_info("cap '%s' registered abi=%u owner='%s'", key, abi,
           c->tag.owner ? c->tag.owner : "?");
  cap_unlock();
  if (owner && owner[0])
    feature_loader_manifest_add_provides(owner, key);
  owner_strfree(&owner);
}

void *cap_resolve_abi(const char *key, unsigned abi) {
  if (!key || !key[0])
    return NULL;
  cap_lock();
  for (size_t i = 0; i < cap_count; i++) {
    if (!caps[i].key || strcmp(caps[i].key, key) != 0)
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

void cap_bind_declare(const char *key, void *consumer_fn) {
  if (!key || !key[0] || !consumer_fn) {
    return;
  }

  char *consumer = owner_resolve_for_func_alloc(consumer_fn);
  if (!consumer || !consumer[0]) {
    owner_strfree(&consumer);
    return;
  }

  cap_lock();
  for (size_t i = 0; i < cap_count; i++) {
    if (!caps[i].key || strcmp(caps[i].key, key) != 0)
      continue;
    if (caps[i].abi != CAP_ABI_V1) {
      cap_unlock();
      owner_strfree(&consumer);
      return;
    }
    if (!caps[i].tag.owner || !caps[i].tag.owner[0]) {
      cap_unlock();
      owner_strfree(&consumer);
      return;
    }
    size_t provider_cap = strlen(caps[i].tag.owner) + 1;
    char *provider = (char *)malloc(provider_cap);
    if (!provider) {
      cap_unlock();
      owner_strfree(&consumer);
      return;
    }
    feature_loader_owner_feature_name(caps[i].tag.owner, provider,
                                      provider_cap);
    cap_unlock();
    if (!provider[0]) {
      free(provider);
      owner_strfree(&consumer);
      return;
    }
    feature_loader_manifest_add_depends(consumer, provider);
    free(provider);
    owner_strfree(&consumer);
    return;
  }
  cap_unlock();
  owner_strfree(&consumer);
}

OWNER_PURGE_REGISTER2(cap_purge_owner)

void cap_purge_owner(const char *path_a, const char *path_b) {
  cap_lock();
  size_t removed =
      OWNER_PURGE_STRUCT(caps, Cap, path_a, path_b, 0, cap_destroy);
  if (removed > 0) {
    atomic_fetch_add_explicit(&cap_gen, 1, memory_order_acq_rel);
  }
  cap_unlock();
}
