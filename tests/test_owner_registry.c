#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "core/owner_registry.h"
#include "deps/stb_ds.h"

typedef struct {
  int id;
  OwnerTag tag;
} TestRecord;

static void record_destroy(void *element) {
  TestRecord *r = (TestRecord *)element;
  owner_tag_clear(&r->tag);
}

static void record_ptr_destroy(void *element) {
  TestRecord *r = (TestRecord *)element;
  record_destroy(r);
  free(r);
}

TEST(owner_owned_strings_and_matching) {
  ASSERT_TRUE(owner_strdup(NULL) == NULL);
  ASSERT_TRUE(owner_strdup("") == NULL);
  char *p = owner_strdup("/home/u/features/a.so");
  ASSERT_TRUE(p != NULL);
  ASSERT_EQ_STR(p, "/home/u/features/a.so");

  char *base = owner_module_basename_alloc("/home/u/features/a.so");
  ASSERT_TRUE(base != NULL);
  ASSERT_EQ_STR(base, "a.so");
  ASSERT_TRUE(owner_module_matches(p, "/home/u/features/a.so", NULL));
  ASSERT_TRUE(owner_module_matches(p, NULL, "a.so"));
  ASSERT_FALSE(owner_module_matches(p, "b.so", "c.so"));
  ASSERT_FALSE(owner_module_matches(NULL, "a.so", NULL));

  owner_strfree(&p);
  ASSERT_TRUE(p == NULL);
  owner_strfree(&p);
  owner_strfree(&base);
}

TEST(owner_tag_lifecycle) {
  OwnerTag tag = {0};
  owner_tag_set(&tag, "/x/first.so", 4);
  ASSERT_TRUE(tag.owner != NULL);
  ASSERT_EQ_INT(tag.gen, 4);
  ASSERT_TRUE(owner_tag_matches(&tag, "first.so", NULL));
  ASSERT_TRUE(owner_tag_active(&tag, "first.so"));
  ASSERT_FALSE(owner_tag_active(&tag, "other.so"));

  char *replacement = owner_strdup("/x/second.so");
  owner_tag_set_owned(&tag, replacement, 9);
  ASSERT_EQ_STR(tag.owner, "/x/second.so");
  ASSERT_EQ_INT(tag.gen, 9);
  ASSERT_FALSE(owner_tag_matches(&tag, "first.so", NULL));

  owner_tag_clear(&tag);
  ASSERT_TRUE(tag.owner == NULL);
  ASSERT_EQ_INT(tag.gen, 0);
  ASSERT_FALSE(owner_tag_active(&tag, "second.so"));
}

TEST(owner_purge_array_respects_generation_and_order) {
  TestRecord *records = NULL;
  int ids[] = {0, 1, 2, 3, 4};
  const char *owners[] = {"a.so", "b.so", "a.so", "a.so", "c.so"};
  unsigned gens[] = {1, 1, 1, 9, 1};
  for (int i = 0; i < 5; i++) {
    TestRecord r = {0};
    r.id = ids[i];
    owner_tag_set(&r.tag, owners[i], gens[i]);
    arrput(records, r);
  }

  size_t removed = OWNER_PURGE_STRUCT(records, TestRecord, "/x/a.so", NULL, 2,
                                      record_destroy);
  ASSERT_EQ_INT((int)removed, 2);
  ASSERT_EQ_INT((int)arrlenu(records), 3);
  ASSERT_EQ_INT(records[0].id, 1);
  ASSERT_EQ_INT(records[1].id, 3);
  ASSERT_EQ_INT(records[2].id, 4);
  ASSERT_TRUE(
      OWNER_STRUCT_HAS_ACTIVE(records, TestRecord, arrlenu(records), "c.so"));
  ASSERT_TRUE(
      OWNER_STRUCT_HAS_ACTIVE(records, TestRecord, arrlenu(records), "a.so"));

  for (size_t i = 0; i < arrlenu(records); i++)
    record_destroy(&records[i]);
  arrfree(records);
}

TEST(owner_purge_array_survives_destroy_freeing_path) {
  TestRecord *records = NULL;
  for (int i = 0; i < 3; i++) {
    TestRecord r = {0};
    r.id = i;
    owner_tag_set(&r.tag, "/x/target.so", 1);
    arrput(records, r);
  }

  const char *borrowed = records[0].tag.owner;
  size_t removed = OWNER_PURGE_STRUCT(records, TestRecord, borrowed, NULL, 0,
                                      record_destroy);
  ASSERT_EQ_INT((int)removed, 3);
  ASSERT_EQ_INT((int)arrlenu(records), 0);
  arrfree(records);
}

TEST(owner_purge_pointers_compacts_and_destroys) {
  TestRecord **records = NULL;
  for (int i = 0; i < 4; i++) {
    TestRecord *r = (TestRecord *)calloc(1, sizeof(TestRecord));
    r->id = i;
    owner_tag_set(&r->tag, i == 1 || i == 3 ? "keep.so" : "drop.so", 0);
    arrput(records, r);
  }

  size_t removed = OWNER_PURGE_PTRS(records, TestRecord, "drop.so", NULL, 0,
                                    record_ptr_destroy);
  ASSERT_EQ_INT((int)removed, 2);
  ASSERT_EQ_INT((int)arrlenu(records), 2);
  ASSERT_EQ_INT(records[0]->id, 1);
  ASSERT_EQ_INT(records[1]->id, 3);
  ASSERT_TRUE(
      OWNER_PTRS_HAVE_ACTIVE(records, TestRecord, arrlenu(records), "keep.so"));

  for (size_t i = 0; i < arrlenu(records); i++) {
    record_destroy(records[i]);
    free(records[i]);
  }
  arrfree(records);
}

TEST(owner_purge_ignores_empty_paths) {
  TestRecord *records = NULL;
  TestRecord r = {0};
  owner_tag_set(&r.tag, "a.so", 0);
  arrput(records, r);
  size_t removed =
      OWNER_PURGE_STRUCT(records, TestRecord, NULL, NULL, 0, record_destroy);
  ASSERT_EQ_INT((int)removed, 0);
  ASSERT_EQ_INT((int)arrlenu(records), 1);
  record_destroy(&records[0]);
  arrfree(records);
}

int main(void) {
  RUN(owner_owned_strings_and_matching);
  RUN(owner_tag_lifecycle);
  RUN(owner_purge_array_respects_generation_and_order);
  RUN(owner_purge_array_survives_destroy_freeing_path);
  RUN(owner_purge_pointers_compacts_and_destroys);
  RUN(owner_purge_ignores_empty_paths);
  return TEST_SUMMARY();
}
