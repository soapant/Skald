#include "test.h"

#include <stdatomic.h>

#include "helpers/helpers_render_thread.h"

static atomic_int g_post_calls;
static atomic_int g_frame_calls;
static int g_frame_marker;

static void count_post(void *ud) {
  (void)ud;
  atomic_fetch_add_explicit(&g_post_calls, 1, memory_order_relaxed);
}

static void count_frame(void *ud) {
  (void)ud;
  atomic_fetch_add_explicit(&g_frame_calls, 1, memory_order_relaxed);
}

static void sync_from_thread(void *ud) {
  atomic_bool *done = (atomic_bool *)ud;
  render_thread_sync();
  atomic_store_explicit(done, true, memory_order_release);
}

TEST(post_before_start_fails_and_sync_is_noop) {
  ASSERT_FALSE(render_thread_is_running());
  atomic_store_explicit(&g_post_calls, 0, memory_order_relaxed);
  ASSERT_FALSE(render_thread_post(count_post, NULL));
  render_thread_sync();
  ASSERT_EQ_INT(atomic_load_explicit(&g_post_calls, memory_order_relaxed), 0);
}

TEST(post_and_sync_delivers_callback) {
  if (!render_thread_ensure_started(60))
    return;
  atomic_store_explicit(&g_post_calls, 0, memory_order_relaxed);
  ASSERT_TRUE(render_thread_post(count_post, NULL));
  render_thread_sync();
  ASSERT_EQ_INT(atomic_load_explicit(&g_post_calls, memory_order_relaxed), 1);
}

TEST(sync_from_render_thread_does_not_deadlock) {
  if (!render_thread_is_running())
    return;
  atomic_bool done;
  atomic_store_explicit(&done, false, memory_order_relaxed);
  ASSERT_TRUE(render_thread_post(sync_from_thread, &done));
  render_thread_sync();
  ASSERT_TRUE(atomic_load_explicit(&done, memory_order_acquire));
}

TEST(frame_callback_add_remove) {
  if (!render_thread_is_running())
    return;
  while (render_thread_remove_frame_callback(count_frame, &g_frame_marker)) {
  }
  ASSERT_TRUE(render_thread_add_frame_callback(count_frame, &g_frame_marker));
  ASSERT_TRUE(
      render_thread_remove_frame_callback(count_frame, &g_frame_marker));
  ASSERT_FALSE(
      render_thread_remove_frame_callback(count_frame, &g_frame_marker));
}

TEST(stop_flushes_pending_and_post_then_fails) {
  if (!render_thread_is_running())
    return;
  atomic_store_explicit(&g_post_calls, 0, memory_order_relaxed);
  ASSERT_TRUE(render_thread_post(count_post, NULL));
  render_thread_stop();
  ASSERT_FALSE(render_thread_is_running());
  ASSERT_EQ_INT(atomic_load_explicit(&g_post_calls, memory_order_relaxed), 1);
  ASSERT_FALSE(render_thread_post(count_post, NULL));
  render_thread_sync();
}

int main(void) {
  RUN(post_before_start_fails_and_sync_is_noop);
  RUN(post_and_sync_delivers_callback);
  RUN(sync_from_render_thread_does_not_deadlock);
  RUN(frame_callback_add_remove);
  RUN(stop_flushes_pending_and_post_then_fails);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
