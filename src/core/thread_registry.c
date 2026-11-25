#include "core/thread_registry.h"

#include <stdlib.h>
#include <string.h>

#include "core/types/window.h"
#include "core/owner_registry.h"
#include "platform/platform.h"
#include "deps/log.h"
#include "helpers/helpers_misc.h"

typedef struct {
  void *ctx;
  OwnerTag tag;
} ThreadEntry;

static ThreadEntry *threads = NULL;
static int thread_count = 0;
static int thread_cap = 0;
static void *thread_mtx = NULL;
static bool thread_mtx_failed = false;

static void thread_reg_lock(void) {
  if (!thread_mtx && !thread_mtx_failed) {
    thread_mtx = lazy_mutex_get(&thread_mtx);
    if (!thread_mtx) {
      thread_mtx_failed = true;
      log_error("thread_registry: mutex creation failed; registry will run "
                "unsynchronized");
    }
  }
  if (thread_mtx)
    Platform_MutexLock(thread_mtx);
}

static void thread_reg_unlock(void) {
  if (thread_mtx)
    Platform_MutexUnlock(thread_mtx);
}

void thread_registry_add(void *ctx, const char *owner_path) {
  if (!ctx)
    return;
  thread_reg_lock();
  if (thread_count >= thread_cap) {
    int cap = thread_cap > 0 ? thread_cap * 2 : 16;
    ThreadEntry *grown = realloc(threads, (size_t)cap * sizeof(*threads));
    if (!grown) {
      thread_reg_unlock();
      return;
    }
    threads = grown;
    thread_cap = cap;
  }
  memset(&threads[thread_count], 0, sizeof(threads[thread_count]));
  threads[thread_count].ctx = ctx;
  owner_tag_set(&threads[thread_count].tag, owner_path, 0);
  thread_count++;
  thread_reg_unlock();
}

void thread_registry_finish(const void *ctx) {
  if (!ctx)
    return;
  thread_reg_lock();
  for (int i = 0; i < thread_count; i++) {
    if (threads[i].ctx == ctx) {
      owner_tag_clear(&threads[i].tag);
      if (i != thread_count - 1) {
        threads[i] = threads[thread_count - 1];
        threads[thread_count - 1].tag.owner = NULL;
      }
      thread_count--;
      thread_reg_unlock();
      return;
    }
  }
  thread_reg_unlock();
}

bool thread_registry_owner_active(const char *owner_path) {
  if (!owner_path || !owner_path[0])
    return false;
  thread_reg_lock();
  bool active = false;
  for (int i = 0; i < thread_count; i++) {
    if (owner_tag_active(&threads[i].tag, owner_path)) {
      active = true;
      break;
    }
  }
  thread_reg_unlock();
  return active;
}
