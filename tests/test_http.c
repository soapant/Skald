#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"
#include "test_tmp.h"

#include "core/http.h"

static char PATH[512];
static char LAST_URL[256];
static char LAST_METHOD[32];
static char LAST_CONTENT_TYPE[64];
static char LAST_BODY[256];
static size_t LAST_LIMIT;
static int COLLECT_CALLS;
static bool SAVE_FAILS;

static char *fake_collect(PlatformHttpCollect *argspec) {
  COLLECT_CALLS++;
  snprintf(LAST_URL, sizeof(LAST_URL), "%s",
           argspec->url ? argspec->url : "");
  snprintf(LAST_METHOD, sizeof(LAST_METHOD), "%s",
           argspec->method ? argspec->method : "");
  snprintf(LAST_CONTENT_TYPE, sizeof(LAST_CONTENT_TYPE), "%s",
           argspec->content_type ? argspec->content_type : "");
  snprintf(LAST_BODY, sizeof(LAST_BODY), "%s",
           argspec->body ? argspec->body : "");
  LAST_LIMIT = argspec->max_bytes;
  if (argspec->out_len)
    *argspec->out_len = 7;
  return strdup("payload");
}

static bool fake_save(const char *url, const char *path) {
  (void)url;
  FILE *f = fopen(path, "wb");
  if (f) {
    fputs("saved", f);
    fclose(f);
  }
  if (SAVE_FAILS)
    return false;
  return f != NULL;
}

static bool fake_rename(const char *old_path, const char *new_path) {
  return rename(old_path, new_path) == 0;
}

static HttpTransport fake_transport(void) {
  HttpTransport t = {0};
  t.collect = fake_collect;
  t.save_to_file = fake_save;
  t.atomic_rename = fake_rename;
  return t;
}

static void http_cleanup(void) {
  http_set_transport(NULL);
  file_remove_path(PATH);
}

static void reset_counters(void) {
  LAST_URL[0] = '\0';
  LAST_METHOD[0] = '\0';
  LAST_CONTENT_TYPE[0] = '\0';
  LAST_BODY[0] = '\0';
  LAST_LIMIT = 0;
  COLLECT_CALLS = 0;
  SAVE_FAILS = false;
}

TEST(get_uses_get_method_and_default_limit) {
  reset_counters();
  HttpTransport t = fake_transport();
  http_set_transport(&t);
  char *body = http_get("http://example.invalid/x");
  ASSERT_EQ_STR(body, "payload");
  ASSERT_EQ_INT(COLLECT_CALLS, 1);
  ASSERT_EQ_STR(LAST_METHOD, "GET");
  ASSERT_EQ_INT((int)LAST_LIMIT, (int)HTTP_DEFAULT_LIMIT);
  free(body);
}

TEST(get_limited_passes_length_and_limit) {
  reset_counters();
  HttpTransport t = fake_transport();
  http_set_transport(&t);
  size_t len = 0;
  char *body = http_get_limited("http://example.invalid/y", 17, &len);
  ASSERT_EQ_STR(body, "payload");
  ASSERT_EQ_INT((int)len, 7);
  ASSERT_EQ_INT((int)LAST_LIMIT, 17);
  free(body);
}

TEST(post_normalizes_null_body) {
  reset_counters();
  HttpTransport t = fake_transport();
  http_set_transport(&t);
  char *body = http_post("http://example.invalid/z", "text/plain", NULL);
  ASSERT_EQ_STR(body, "payload");
  ASSERT_EQ_STR(LAST_METHOD, "POST");
  ASSERT_EQ_STR(LAST_CONTENT_TYPE, "text/plain");
  ASSERT_EQ_STR(LAST_BODY, "");
  free(body);
}

TEST(invalid_urls_do_not_reach_transport) {
  reset_counters();
  HttpTransport t = fake_transport();
  http_set_transport(&t);
  ASSERT_TRUE(http_get(NULL) == NULL);
  ASSERT_TRUE(http_get("") == NULL);
  ASSERT_TRUE(http_post(NULL, "text/plain", "x") == NULL);
  ASSERT_EQ_INT(COLLECT_CALLS, 0);
}

TEST(download_renames_part_file) {
  reset_counters();
  HttpTransport t = fake_transport();
  http_set_transport(&t);
  ASSERT_TRUE(http_download("http://example.invalid/f", PATH));
  FILE *f = fopen(PATH, "rb");
  ASSERT_TRUE(f != NULL);
  if (f) {
    char buf[16] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    ASSERT_EQ_STR(buf, "saved");
  }
}

TEST(download_failure_removes_part_file) {
  reset_counters();
  SAVE_FAILS = true;
  HttpTransport t = fake_transport();
  http_set_transport(&t);
  ASSERT_FALSE(http_download("http://example.invalid/f", PATH));
  char part[600];
  snprintf(part, sizeof(part), "%s.part", PATH);
  FILE *f = fopen(part, "rb");
  if (f)
    fclose(f);
  ASSERT_TRUE(f == NULL);
}

static atomic_bool ASYNC_DONE = false;
static atomic_bool ASYNC_OK = false;
static char ASYNC_BODY[64];

static void async_cb(const char *body, bool ok, void *ud) {
  (void)ud;
  snprintf(ASYNC_BODY, sizeof(ASYNC_BODY), "%s", body ? body : "");
  atomic_store_explicit(&ASYNC_OK, ok, memory_order_relaxed);
  atomic_store_explicit(&ASYNC_DONE, true, memory_order_release);
}

TEST(async_get_delivers_transport_result) {
  reset_counters();
  atomic_store_explicit(&ASYNC_DONE, false, memory_order_relaxed);
  ASYNC_BODY[0] = '\0';
  HttpTransport t = fake_transport();
  http_set_transport(&t);
  http_get_async("http://example.invalid/a", async_cb, NULL);
  for (int i = 0;
       i < 2000 && !atomic_load_explicit(&ASYNC_DONE, memory_order_acquire);
       i++)
    usleep(1000);
  ASSERT_TRUE(atomic_load_explicit(&ASYNC_DONE, memory_order_acquire));
  ASSERT_TRUE(atomic_load_explicit(&ASYNC_OK, memory_order_relaxed));
  ASSERT_EQ_STR(ASYNC_BODY, "payload");
  ASSERT_EQ_INT(COLLECT_CALLS, 1);
}

int main(void) {
  test_tmp_path(PATH, sizeof(PATH), "test_http_download.bin");
  TEST_SUITE_TEARDOWN(http_cleanup);
  RUN(get_uses_get_method_and_default_limit);
  RUN(get_limited_passes_length_and_limit);
  RUN(post_normalizes_null_body);
  RUN(invalid_urls_do_not_reach_transport);
  RUN(download_renames_part_file);
  RUN(download_failure_removes_part_file);
  RUN(async_get_delivers_transport_result);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}


