#include "core/thread_registry.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/types/window.h"
#include "core/owner_registry.h"
#include "core/owner_purge.h"
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

bool thread_registry_add(void *ctx, const char *owner_path) {
  if (!ctx)
    return false;
  thread_reg_lock();
  if (thread_count >= thread_cap) {
    int cap = thread_cap > 0 ? thread_cap * 2 : 16;
    ThreadEntry *grown = realloc(threads, (size_t)cap * sizeof(*threads));
    if (!grown) {
      thread_reg_unlock();
      log_error("thread_registry: cannot track thread, refusing to start it");
      return false;
    }
    threads = grown;
    thread_cap = cap;
  }
  memset(&threads[thread_count], 0, sizeof(threads[thread_count]));
  threads[thread_count].ctx = ctx;
  owner_tag_set(&threads[thread_count].tag, owner_path, 0);
  thread_count++;
  thread_reg_unlock();
  return true;
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

typedef struct {
  ThreadFn fn;
  void *arg;
  OwnerTag tag;
} ThreadCtx;

static void *thread_trampoline(void *p) {
  ThreadCtx *ctx = (ThreadCtx *)p;
  ThreadFn fn = ctx->fn;
  void *arg = ctx->arg;
  void *ret = fn ? fn(arg) : NULL;
  thread_registry_finish(ctx);
  owner_tag_clear(&ctx->tag);
  free(ctx);
  return ret;
}

void *thread_create_ex(ThreadFn fn, void *arg, void *owner_src) {
  if (!fn)
    return NULL;
  ThreadCtx *ctx = malloc(sizeof(*ctx));
  if (!ctx)
    return NULL;
  ctx->fn = fn;
  ctx->arg = arg;
  memset(&ctx->tag, 0, sizeof(ctx->tag));
  owner_tag_set_for_func(&ctx->tag,
                         owner_src ? owner_src : (void *)(uintptr_t)fn, 0);
  if (!thread_registry_add(ctx, ctx->tag.owner ? ctx->tag.owner : "")) {
    owner_tag_clear(&ctx->tag);
    free(ctx);
    return NULL;
  }
  void *thread = Platform_ThreadCreate(thread_trampoline, ctx);
  if (!thread) {
    thread_registry_finish(ctx);
    owner_tag_clear(&ctx->tag);
    free(ctx);
    return NULL;
  }
  return thread;
}

typedef struct {
  double seconds;
  void (*callback)(void);
  int stop;
} RunEveryArgs;

typedef struct RunEveryEntry {
  RunEveryArgs *args;
  void *thread;
  OwnerTag tag;
  struct RunEveryEntry *next;
} RunEveryEntry;

static RunEveryEntry *run_every_head = NULL;
static void *run_every_mtx = NULL;

static void run_every_lock(void) {
  lazy_mutex_get(&run_every_mtx);
  if (run_every_mtx)
    Platform_MutexLock(run_every_mtx);
}

static void run_every_unlock(void) {
  if (run_every_mtx)
    Platform_MutexUnlock(run_every_mtx);
}

static void *run_every_thread(void *arg) {
  RunEveryArgs *args = (RunEveryArgs *)arg;
  for (;;) {
    if (__atomic_load_n(&args->stop, __ATOMIC_RELAXED))
      return NULL;
    double remaining = args->seconds;
    while (remaining > 0.0) {
      if (__atomic_load_n(&args->stop, __ATOMIC_RELAXED))
        return NULL;
      double step = remaining >= 0.05 ? 0.05 : remaining;
      Platform_SleepSeconds(step);
      remaining -= step;
    }
    if (__atomic_load_n(&args->stop, __ATOMIC_RELAXED))
      return NULL;
    args->callback();
  }
}

void run_every_start(double seconds, void (*callback)(void)) {
  if (!callback)
    return;
  if (!(seconds > 0.0))
    seconds = 0.01;
  RunEveryArgs *args = malloc(sizeof(RunEveryArgs));
  if (!args) {
    log_error("sk_run_every: malloc failed");
    return;
  }
  args->seconds = seconds;
  args->callback = callback;
  args->stop = 0;
  RunEveryEntry *entry = malloc(sizeof(RunEveryEntry));
  if (!entry) {
    log_error("sk_run_every: malloc failed");
    free(args);
    return;
  }
  entry->args = args;
  memset(&entry->tag, 0, sizeof(entry->tag));
  owner_tag_set_for_func(&entry->tag, (void *)(uintptr_t)callback, 0);
  entry->thread =
      thread_create_ex(run_every_thread, args, (void *)(uintptr_t)callback);
  if (!entry->thread) {
    log_error("sk_run_every: thread creation failed");
    owner_tag_clear(&entry->tag);
    free(entry);
    free(args);
    return;
  }
  run_every_lock();
  entry->next = run_every_head;
  run_every_head = entry;
  run_every_unlock();
}

OWNER_PURGE_REGISTER2(run_every_purge_owner)

void run_every_purge_owner(const char *path_a, const char *path_b) {
  RunEveryEntry *mine = NULL;
  run_every_lock();
  RunEveryEntry **pp = &run_every_head;
  while (*pp) {
    RunEveryEntry *e = *pp;
    if (owner_tag_matches(&e->tag, path_a, path_b)) {
      *pp = e->next;
      e->next = mine;
      mine = e;
    } else {
      pp = &e->next;
    }
  }
  run_every_unlock();
  while (mine) {
    RunEveryEntry *e = mine;
    mine = e->next;
    __atomic_store_n(&e->args->stop, 1, __ATOMIC_RELAXED);
    if (e->thread)
      Platform_ThreadJoin(e->thread);
    owner_tag_clear(&e->tag);
    free(e->args);
    free(e);
  }
}
