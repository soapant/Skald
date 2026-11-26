#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "test.h"

#include "core/widget_registry.h"
#include "helpers/helpers_owner.h"

TEST(empty_registry_reports_inactive_owner) {
  ASSERT_FALSE(widget_owner_active("some/bogus-owner.so"));
  ASSERT_FALSE(widget_owner_active(""));
  ASSERT_FALSE(widget_owner_active(NULL));
}

TEST(purge_owner_on_empty_registry_does_not_deadlock) {
  unsigned gen = widget_current_generation();
  widget_purge_owner("no-owner-a.so", NULL, gen);
  widget_purge_owner(NULL, "no-owner-b.so", gen);
  widget_purge_owner("no-owner-c.so", "no-owner-d.so", gen);
  ASSERT_EQ_INT((int)widget_current_generation(), (int)gen);
  ASSERT_FALSE(widget_owner_active("no-owner-a.so"));
  ASSERT_FALSE(widget_owner_active("no-owner-d.so"));
}

TEST(generation_protocol_is_monotonic) {
  unsigned g0 = widget_current_generation();
  unsigned g1 = widget_bump_generation();
  unsigned g2 = widget_current_generation();
  ASSERT_TRUE(g1 > g0);
  ASSERT_EQ_INT(g1, g2);
  unsigned g3 = widget_bump_generation();
  ASSERT_TRUE(g3 > g1);
}

enum { CONCURRENT_THREADS = 4, CONCURRENT_ITERS = 200 };

static int concurrent_owner_checks = 0;
static pthread_mutex_t concurrent_count_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *concurrent_worker(void *arg) {
  (void)arg;
  for (int i = 0; i < CONCURRENT_ITERS; i++) {
    if (widget_owner_active("concurrent-bogus-owner.so")) {
      pthread_mutex_lock(&concurrent_count_mutex);
      concurrent_owner_checks++;
      pthread_mutex_unlock(&concurrent_count_mutex);
    }
    widget_purge_owner("concurrent-bogus-owner.so", NULL,
                       widget_current_generation());
  }
  return NULL;
}

TEST(concurrent_purge_and_lookup_smoke) {
  pthread_t t[CONCURRENT_THREADS];
  for (int i = 0; i < CONCURRENT_THREADS; i++)
    ASSERT_TRUE(pthread_create(&t[i], NULL, concurrent_worker, NULL) == 0);
  for (int i = 0; i < CONCURRENT_THREADS; i++)
    pthread_join(t[i], NULL);
  ASSERT_EQ_INT(concurrent_owner_checks, 0);
}

static void registry_stub_render(WidgetState *state) { (void)state; }

TEST(create_then_purge_owner_removes_widget) {
  char owner[OWNER_PATH_MAX];
  owner_resolve_for_func((void *)test_create_then_purge_owner_removes_widget,
                         owner, sizeof(owner));
  WidgetSpec spec = {0};
  spec.name = "test_registry_widget";
  spec.w = 40;
  spec.h = 20;
  spec.slot = WIDGET_FREE;
  spec.x = 12;
  spec.y = 12;
  spec.fps = 1;
  spec.render = registry_stub_render;

  Widget *w =
      widget_create(&spec, (void *)test_create_then_purge_owner_removes_widget);
  ASSERT_TRUE(w != NULL);
  ASSERT_TRUE(widget_valid(w));
  if (owner[0]) {
    ASSERT_TRUE(widget_owner_active(owner));
    widget_purge_owner(owner, NULL, widget_current_generation());
    ASSERT_FALSE(widget_owner_active(owner));
  }
}

TEST(key_capture_is_opt_in_and_queue_starts_empty) {
  WidgetSpec spec = {0};
  spec.name = "test_key_capture_widget";
  spec.w = 40;
  spec.h = 20;
  spec.slot = WIDGET_FREE;
  spec.x = 600;
  spec.y = 400;
  spec.fps = 1;
  spec.render = registry_stub_render;

  Widget *w = widget_create(
      &spec, (void *)test_key_capture_is_opt_in_and_queue_starts_empty);
  ASSERT_TRUE(w != NULL);
  ASSERT_FALSE(w->capture_keys);
  WidgetKey key = {0};
  ASSERT_FALSE(widget_poll_key(w, &key));
  ASSERT_FALSE(widget_poll_key(NULL, &key));
  widget_key_capture_clear(w);
  widget_key_capture_release_pin();

  spec.capture_keys = true;
  Widget *opt_in = widget_create(
      &spec, (void *)test_key_capture_is_opt_in_and_queue_starts_empty);
  ASSERT_TRUE(opt_in != NULL);
  ASSERT_TRUE(opt_in->capture_keys);
  ASSERT_FALSE(widget_poll_key(opt_in, &key));

  char owner[OWNER_PATH_MAX];
  owner_resolve_for_func((void *)test_key_capture_is_opt_in_and_queue_starts_empty,
                         owner, sizeof(owner));
  if (owner[0])
    widget_purge_owner(owner, NULL, widget_current_generation());
}

int main(void) {
  RUN(empty_registry_reports_inactive_owner);
  RUN(purge_owner_on_empty_registry_does_not_deadlock);
  RUN(generation_protocol_is_monotonic);
  RUN(concurrent_purge_and_lookup_smoke);
  RUN(create_then_purge_owner_removes_widget);
  RUN(key_capture_is_opt_in_and_queue_starts_empty);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
