#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "test.h"

#include "core/widget_registry.h"

TEST(empty_registry_reports_inactive_owner) {
  ASSERT_FALSE(Widget_OwnerActive("some/bogus-owner.so"));
  ASSERT_FALSE(Widget_OwnerActive(""));
  ASSERT_FALSE(Widget_OwnerActive(NULL));
}

TEST(purge_owner_on_empty_registry_does_not_deadlock) {
  Widget_PurgeOwner("no-owner-a.so", NULL, Widget_CurrentGeneration());
  Widget_PurgeOwner(NULL, "no-owner-b.so", Widget_CurrentGeneration());
  Widget_PurgeOwner("no-owner-c.so", "no-owner-d.so",
                     Widget_CurrentGeneration());
  ASSERT_TRUE(true);
}

TEST(generation_protocol_is_monotonic) {
  unsigned g0 = Widget_CurrentGeneration();
  unsigned g1 = Widget_BumpGeneration();
  unsigned g2 = Widget_CurrentGeneration();
  ASSERT_TRUE(g1 > g0);
  ASSERT_EQ_INT(g1, g2);
  unsigned g3 = Widget_BumpGeneration();
  ASSERT_TRUE(g3 > g1);
}

enum { CONCURRENT_THREADS = 4, CONCURRENT_ITERS = 200 };

static int concurrent_owner_checks = 0;
static pthread_mutex_t concurrent_count_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *concurrent_worker(void *arg) {
  (void)arg;
  for (int i = 0; i < CONCURRENT_ITERS; i++) {
    if (Widget_OwnerActive("concurrent-bogus-owner.so")) {
      pthread_mutex_lock(&concurrent_count_mutex);
      concurrent_owner_checks++;
      pthread_mutex_unlock(&concurrent_count_mutex);
    }
    Widget_PurgeOwner("concurrent-bogus-owner.so", NULL,
                       Widget_CurrentGeneration());
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
