#ifndef SKALD_TEST_H
#define SKALD_TEST_H

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int skald_test_passes = 0;
static int skald_test_fails = 0;
static const char *skald_test_current = "";
static jmp_buf skald_test_jmp;
static void (*skald_test_teardown)(void);
static void (*skald_test_global_teardown)(void);

#define TEST(name)                                                             \
  static void test_##name(void);                                               \
  static void test_##name(void)

#define TEST_TEARDOWN(fn) (skald_test_teardown = (fn))

#define TEST_SUITE_TEARDOWN(fn) (skald_test_global_teardown = (fn))
#define TEST_GLOBAL_TEARDOWN(fn) TEST_SUITE_TEARDOWN(fn)

#define RUN(name)                                                              \
  do {                                                                         \
    skald_test_current = #name;                                                \
    skald_test_teardown = NULL;                                                \
    if (setjmp(skald_test_jmp) == 0) {                                         \
      test_##name();                                                           \
    }                                                                          \
    if (skald_test_teardown) {                                                 \
      skald_test_teardown();                                                   \
      skald_test_teardown = NULL;                                              \
    }                                                                          \
  } while (0)

#define SKALD_TEST_ABORT() longjmp(skald_test_jmp, 1)

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (cond) {                                                                \
      skald_test_passes++;                                                     \
    } else {                                                                   \
      skald_test_fails++;                                                      \
      printf("  FAIL %s:%d [%s] expected true: %s\n", __FILE__, __LINE__,      \
             skald_test_current, #cond);                                       \
      SKALD_TEST_ABORT();                                                      \
    }                                                                          \
  } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ_INT(a, b)                                                    \
  do {                                                                         \
    long long _a = (long long)(a);                                             \
    long long _b = (long long)(b);                                             \
    if (_a == _b) {                                                            \
      skald_test_passes++;                                                     \
    } else {                                                                   \
      skald_test_fails++;                                                      \
      printf("  FAIL %s:%d [%s] %s (%lld) != %s (%lld)\n", __FILE__, __LINE__, \
             skald_test_current, #a, _a, #b, _b);                              \
      SKALD_TEST_ABORT();                                                      \
    }                                                                          \
  } while (0)

#define ASSERT_EQ_STR(a, b)                                                    \
  do {                                                                         \
    const char *_a = (a);                                                      \
    const char *_b = (b);                                                      \
    if ((_a == NULL && _b == NULL) || (_a && _b && strcmp(_a, _b) == 0)) {     \
      skald_test_passes++;                                                     \
    } else {                                                                   \
      skald_test_fails++;                                                      \
      printf("  FAIL %s:%d [%s] \"%s\" != \"%s\"\n", __FILE__, __LINE__,       \
             skald_test_current, _a ? _a : "(null)", _b ? _b : "(null)");      \
      SKALD_TEST_ABORT();                                                      \
    }                                                                          \
  } while (0)

#define TEST_SUMMARY()                                                         \
  ((skald_test_global_teardown ? (skald_test_global_teardown(), 0) : 0),       \
   printf("%s: %d passed, %d failed\n", __FILE__, skald_test_passes,           \
          skald_test_fails),                                                   \
   skald_test_fails)

#endif
