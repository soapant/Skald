#include "core/http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/thread_registry.h"
#include "deps/log.h"
#include "helpers/helpers_owner.h"
#include "helpers/helpers_ui.h"
#include "platform/platform.h"

char *http_get_limited(const char *url, size_t max_bytes, size_t *out_len) {
  if (!url || !url[0]) {
    if (out_len)
      *out_len = 0;
    return NULL;
  }
  return Platform_HttpCollect(url, "GET", NULL, NULL, max_bytes, out_len);
}

char *http_get(const char *url) {
  return http_get_limited(url, HTTP_DEFAULT_LIMIT, NULL);
}

char *http_post(const char *url, const char *content_type, const char *body) {
  if (!url || !url[0])
    return NULL;
  return Platform_HttpCollect(url, "POST", content_type, body ? body : "",
                             HTTP_DEFAULT_LIMIT, NULL);
}

bool http_download(const char *url, const char *save_path) {
  if (!url || !url[0] || !save_path || !save_path[0])
    return false;
  char tmp[4096];
  int rc = snprintf(tmp, sizeof(tmp), "%s.part", save_path);
  if (rc <= 0 || (size_t)rc >= sizeof(tmp)) {
    log_warn("http download path too long for %s.part", save_path);
    return false;
  }
  if (!Platform_HttpSaveToFile(url, tmp)) {
    log_warn("http download failed for %s", url);
    remove(tmp);
    return false;
  }
  if (!Platform_AtomicRename(tmp, save_path)) {
    log_warn("http download rename failed for %s", save_path);
    remove(tmp);
    return false;
  }
  return true;
}

static void *http_async_worker(void *arg) {
  HttpAsyncJob *job = (HttpAsyncJob *)arg;
  if (!job)
    return NULL;
  char *body = http_get(job->url);
  if (job->cb)
    job->cb(body ? body : "", body != NULL, job->userdata);
  free(body);
  free(job->url);
  thread_registry_finish(job);
  free(job);
  return NULL;
}

void http_get_async(const char *url, PlatformHttpCallback cb,
                    void *userdata) {
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
  owner_resolve_for_func((void *)cb, job->owner, sizeof(job->owner));
  thread_registry_add(job, job->owner);
  if (!Platform_HttpThreadStart(http_async_worker, job)) {
    thread_registry_finish(job);
    free(job->url);
    free(job);
    log_warn("http async thread create failed");
  }
}
