#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "test.h"

#include "core/widget_registry.h"

TEST(empty_registry_reports_inactive_owner) {
  ASSERT_FALSE(widget_owner_active("some/bogus-owner.so"));
  ASSERT_FALSE(widget_owner_active(""));
  ASSERT_FALSE(widget_owner_active(NULL));
}

TEST(purge_owner_on_empty_registry_does_not_deadlock) {
  widget_purge_owner("no-owner-a.so", NULL, widget_current_generation());
  widget_purge_owner(NULL, "no-owner-b.so", widget_current_generation());
  widget_purge_owner("no-owner-c.so", "no-owner-d.so",
                    widget_current_generation());
  ASSERT_TRUE(true);
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

int main(void) {
  RUN(empty_registry_reports_inactive_owner);
  RUN(purge_owner_on_empty_registry_does_not_deadlock);
  RUN(generation_protocol_is_monotonic);
  RUN(concurrent_purge_and_lookup_smoke);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
