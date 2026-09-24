#include "test.h"
#include <stdio.h>
#include "core/flapi.h"

TEST(temp_roundtrip) {
    char path[512] = {0};
    const char data[] = "P6\n2 2\n255\n";
    bool ok = fl_write_temp_file("flinttest.ppm", data, sizeof(data) - 1, path, sizeof(path));
    ASSERT_TRUE(ok);
    ASSERT_TRUE(path[0] != '\0');
    FILE* f = fopen(path, "rb");
    ASSERT_TRUE(f != NULL);
    if (f) {
        char buf[64] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        ASSERT_EQ_INT((int)n, (int)(sizeof(data) - 1));
        ASSERT_EQ_STR(buf, data);
        remove(path);
    }
}

TEST(temp_rejects_traversal) {
    char path[512] = {0};
    ASSERT_FALSE(fl_write_temp_file("../evil.txt", "x", 1, path, sizeof(path)));
    ASSERT_FALSE(fl_write_temp_file("a/b.txt", "x", 1, path, sizeof(path)));
    ASSERT_FALSE(fl_write_temp_file(NULL, "x", 1, path, sizeof(path)));
}

int main(void) {
    RUN(temp_roundtrip);
    RUN(temp_rejects_traversal);
    return TEST_SUMMARY() != 0 ? 1 : 0;
}
