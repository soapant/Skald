#include <string.h>

#include "test.h"

#include "core/skapi.h"

TEST(settings_editbox_group_get_and_set) {
  static char first[16] = "one";
  static char second[16] = "two";
  SkSettingsEditboxField fields[] = {
      {.name = "first", .label = "First", .buffer = first,
       .buffer_size = sizeof(first)},
      {.name = "second", .label = "Second", .buffer = second,
       .buffer_size = sizeof(second)},
  };

  SkSettingsEntry *entry = sk_add_settings_entry(.name = "__test_editbox_group",
                                                 .icon = "", .desc = "");
  ASSERT_TRUE(entry != NULL);

  SkSettingsEditboxGroup group =
      sk_settings_entry_add_editbox(entry, .title = "Values", .sub = "Two boxes",
                                    .fields = fields, .field_count = 2);
  ASSERT_TRUE(group.entry >= 0);
  ASSERT_TRUE(group.block >= 0);
  ASSERT_EQ_STR(sk_settings_editbox_get(group, "first"), "one");
  ASSERT_EQ_STR(sk_settings_editbox_get(group, "second"), "two");
  ASSERT_TRUE(sk_settings_editbox_set(group, "first", "three"));
  ASSERT_EQ_STR(first, "three");
  ASSERT_EQ_STR(sk_settings_editbox_get(group, "first"), "three");
  ASSERT_FALSE(sk_settings_editbox_set(group, "missing", "x"));
  ASSERT_TRUE(sk_settings_editbox_get(group, "missing") == NULL);
}

int main(void) {
  RUN(settings_editbox_group_get_and_set);
  return TEST_SUMMARY() != 0 ? 1 : 0;
}
