#include "test.h"

#include <stdlib.h>

#include "core/cap_registry.h"
#include "helpers/helpers_owner.h"

#define TEST_CAP_KEY "test.registry.cap"

static void cap_fn_a(void) {}
static void cap_fn_b(void) {}

TEST(cap_resolve_requires_exact_key_and_abi) {
  cap_register_abi(TEST_CAP_KEY, CAP_ABI_V1, (void *)cap_fn_a);
  ASSERT_TRUE(cap_resolve_abi(TEST_CAP_KEY, CAP_ABI_V1) == (void *)cap_fn_a);
  ASSERT_TRUE(cap_resolve_abi(TEST_CAP_KEY, CAP_ABI_V1 + 99) == NULL);
  ASSERT_TRUE(cap_resolve_abi("test.registry.missing", CAP_ABI_V1) == NULL);
  ASSERT_TRUE(cap_resolve_abi(NULL, CAP_ABI_V1) == NULL);
  ASSERT_TRUE(cap_resolve_abi(TEST_CAP_KEY, CAP_ABI_V1 + 99) == NULL);
}

TEST(cap_replacement_bumps_generation_and_swaps_fn) {
  unsigned before = cap_generation();
  cap_register_abi(TEST_CAP_KEY, CAP_ABI_V1, (void *)cap_fn_b);
  ASSERT_TRUE(cap_resolve_abi(TEST_CAP_KEY, CAP_ABI_V1) == (void *)cap_fn_b);
  ASSERT_TRUE(cap_generation() > before);

  unsigned after_swap = cap_generation();
  cap_register_abi(TEST_CAP_KEY, CAP_ABI_V1, (void *)cap_fn_b);
  ASSERT_EQ_INT((int)cap_generation(), (int)after_swap);
}

TEST(cap_purge_owner_invalidates_resolution_and_bumps_generation) {
  cap_register_abi(TEST_CAP_KEY, CAP_ABI_V1, (void *)cap_fn_a);
  ASSERT_TRUE(cap_resolve_abi(TEST_CAP_KEY, CAP_ABI_V1) == (void *)cap_fn_a);
  char *owner = owner_resolve_for_func_alloc((void *)cap_fn_a);
  ASSERT_TRUE(owner != NULL);

  unsigned before = cap_generation();
  cap_purge_owner(owner, NULL);
  ASSERT_TRUE(cap_resolve_abi(TEST_CAP_KEY, CAP_ABI_V1) == NULL);
  ASSERT_TRUE(cap_generation() > before);
  free(owner);
}

TEST(cap_bind_declare_tolerates_missing_and_present_providers) {
  unsigned before = cap_generation();
  cap_bind_declare("test.registry.missing", (void *)cap_fn_a);
  ASSERT_EQ_INT((int)cap_generation(), (int)before);

  cap_register_abi(TEST_CAP_KEY, CAP_ABI_V1, (void *)cap_fn_b);
  cap_bind_declare(TEST_CAP_KEY, (void *)cap_fn_a);
  ASSERT_TRUE(cap_resolve_abi(TEST_CAP_KEY, CAP_ABI_V1) == (void *)cap_fn_b);
}

TEST(cap_register_rejects_null_key_and_fn) {
  unsigned before = cap_generation();
  cap_register_abi(NULL, CAP_ABI_V1, (void *)cap_fn_a);
  cap_register_abi("", CAP_ABI_V1, (void *)cap_fn_a);
  cap_register_abi(TEST_CAP_KEY, CAP_ABI_V1, NULL);
  ASSERT_EQ_INT((int)cap_generation(), (int)before);
}

int main(void) {
  RUN(cap_resolve_requires_exact_key_and_abi);
  RUN(cap_replacement_bumps_generation_and_swaps_fn);
  RUN(cap_purge_owner_invalidates_resolution_and_bumps_generation);
  RUN(cap_bind_declare_tolerates_missing_and_present_providers);
  RUN(cap_register_rejects_null_key_and_fn);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
