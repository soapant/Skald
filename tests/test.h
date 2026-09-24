#ifndef FLINT_TEST_H
#define FLINT_TEST_H

#include <stdio.h>
#include <string.h>

static int flint_test_passes = 0;
static int flint_test_fails = 0;
static const char* flint_test_current = "";

#define TEST(name) static void test_##name(void); \
    static void test_##name(void)

#define RUN(name) do { \
    flint_test_current = #name; \
    test_##name(); \
} while (0)

#define ASSERT_TRUE(cond) do { \
    if (cond) { flint_test_passes++; } \
    else { flint_test_fails++; printf("  FAIL %s:%d [%s] expected true: %s\n", __FILE__, __LINE__, flint_test_current, #cond); } \
} while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ_INT(a, b) do { \
    long long _a = (long long)(a); long long _b = (long long)(b); \
    if (_a == _b) { flint_test_passes++; } \
    else { flint_test_fails++; printf("  FAIL %s:%d [%s] %s (%lld) != %s (%lld)\n", __FILE__, __LINE__, flint_test_current, #a, _a, #b, _b); } \
} while (0)

#define ASSERT_EQ_STR(a, b) do { \
    const char* _a = (a); const char* _b = (b); \
    if (_a && _b && strcmp(_a, _b) == 0) { flint_test_passes++; } \
    else { flint_test_fails++; printf("  FAIL %s:%d [%s] \"%s\" != \"%s\"\n", __FILE__, __LINE__, flint_test_current, _a ? _a : "(null)", _b ? _b : "(null)"); } \
} while (0)

#define TEST_SUMMARY() (printf("%s: %d passed, %d failed\n", __FILE__, flint_test_passes, flint_test_fails), flint_test_fails)

#endif
