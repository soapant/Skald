#include "helpers/helpers_http.h"

#include <stdlib.h>
#include <string.h>

#include "core/thread_registry.h"
#include "deps/log.h"
#include "helpers/helpers_owner.h"

typedef struct {
  char *url;
  PlatformHttpCallback cb;
  void *userdata;
  char owner[OWNER_PATH_MAX];
} HttpAsyncJob;

static void *http_async_worker(void *arg) {
  HttpAsyncJob *job = (HttpAsyncJob *)arg;
  if (!job)
    return NULL;
  char *body = Platform_HttpGet(job->url);
  if (job->cb)
    job->cb(body ? body : "", body != NULL, job->userdata);
  free(body);
  free(job->url);
  ThreadRegistry_Finish(job);
  free(job);
  return NULL;
}

void Http_GetAsync(const char *url, PlatformHttpCallback cb, void *userdata) {
  if (!url || !url[0] || !cb)
    return;
  HttpAsyncJob *job = (HttpAsyncJob *)calloc(1, sizeof(*job));
  if (!job)
    return;
  job->url = strdup(url);
  job->cb = cb;
  job->userdata = userdata;
  if (!job->url) {
    free(job);
    return;
  }
  job->owner[0] = '\0';
  Owner_ResolveForFunc((void *)cb, job->owner, sizeof(job->owner));
  ThreadRegistry_Add(job, job->owner);
  if (!HttpBackend_ThreadRun(http_async_worker, job)) {
    ThreadRegistry_Finish(job);
    free(job->url);
    free(job);
    log_warn("http async thread create failed");
  }
}
