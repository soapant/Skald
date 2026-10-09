#include "test.h"
#include "core/clipboard_history.h"
#include <stdlib.h>

static void put_be16(unsigned char* p, unsigned v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)(v & 0xFF);
}

static void put_le16(unsigned char* p, unsigned v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)(v >> 8);
}

static void put_le32(unsigned char* p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned char* make_png(unsigned w, unsigned h, size_t* out_len) {
    unsigned char* b = calloc(1, 64);
    memcpy(b, "\x89PNG\r\n\x1a\n", 8);
    memcpy(b + 12, "IHDR", 4);
    b[16] = (unsigned char)(w >> 24); b[17] = (unsigned char)(w >> 16);
    b[18] = (unsigned char)(w >> 8);  b[19] = (unsigned char)w;
    b[20] = (unsigned char)(h >> 24); b[21] = (unsigned char)(h >> 16);
    b[22] = (unsigned char)(h >> 8);  b[23] = (unsigned char)h;
    *out_len = 64;
    return b;
}

static unsigned char* make_gif(unsigned w, unsigned h, size_t* out_len) {
    unsigned char* b = calloc(1, 16);
    memcpy(b, "GIF89a", 6);
    put_le16(b + 6, w);
    put_le16(b + 8, h);
    *out_len = 16;
    return b;
}

static unsigned char* make_bmp(int32_t w, int32_t h, size_t* out_len) {
    unsigned char* b = calloc(1, 40);
    b[0] = 'B'; b[1] = 'M';
    put_le32(b + 18, (uint32_t)w);
    put_le32(b + 22, (uint32_t)h);
    *out_len = 40;
    return b;
}

static unsigned char* make_jpeg(unsigned char sof_marker, unsigned w, unsigned h,
                                int with_app_segment, size_t* out_len) {
    unsigned char* b = calloc(1, 128);
    b[0] = 0xFF; b[1] = 0xD8; b[2] = 0xFF;
    size_t i = 2;
    if (with_app_segment) {
        b[i] = 0xFF; b[i + 1] = 0xE1;
        put_be16(b + i + 2, 20);
        memset(b + i + 4, 'A', 16);
        i += 2 + 20;
    }
    b[i] = 0xFF; b[i + 1] = sof_marker;
    put_be16(b + i + 2, 17);
    b[i + 4] = 8;
    put_be16(b + i + 5, h);
    put_be16(b + i + 7, w);
    *out_len = i + 19;
    return b;
}

TEST(detects_png) {
    size_t n = 0;
    unsigned char* b = make_png(137, 89, &n);
    ASSERT_EQ_INT(clipboard_image_format(b, n), CLIPBOARD_FMT_PNG);
    free(b);
}

TEST(detects_jpeg) {
    size_t n = 0;
    unsigned char* b = make_jpeg(0xC0, 137, 89, 0, &n);
    ASSERT_EQ_INT(clipboard_image_format(b, n), CLIPBOARD_FMT_JPEG);
    free(b);
}

TEST(detects_gif) {
    size_t n = 0;
    unsigned char* b = make_gif(137, 89, &n);
    ASSERT_EQ_INT(clipboard_image_format(b, n), CLIPBOARD_FMT_GIF);
    free(b);
}

TEST(detects_bmp) {
    size_t n = 0;
    unsigned char* b = make_bmp(137, 89, &n);
    ASSERT_EQ_INT(clipboard_image_format(b, n), CLIPBOARD_FMT_BMP);
    free(b);
}

TEST(rejects_non_images) {
    const char* txt = "hello not an image";
    ASSERT_EQ_INT(clipboard_image_format((const unsigned char*)txt, strlen(txt)), CLIPBOARD_FMT_UNKNOWN);
    ASSERT_EQ_INT(clipboard_image_format(NULL, 0), CLIPBOARD_FMT_UNKNOWN);
}

TEST(bare_magic_matches_but_dims_stay_zero) {
    unsigned char* b = (unsigned char*)"BM";
    ASSERT_EQ_INT(clipboard_image_format(b, 2), CLIPBOARD_FMT_BMP);
    uint32_t w = 99, h = 99;
    clipboard_image_dims(b, 2, &w, &h);
    ASSERT_EQ_INT(w, 0);
    ASSERT_EQ_INT(h, 0);
}

TEST(requires_full_magic_for_gif) {
    ASSERT_EQ_INT(clipboard_image_format((const unsigned char*)"GIF8", 4), CLIPBOARD_FMT_UNKNOWN);
    size_t n = 0;
    unsigned char* b = make_gif(1, 1, &n);
    ASSERT_EQ_INT(clipboard_image_format(b, n), CLIPBOARD_FMT_GIF);
    free(b);
}

TEST(mime_strings_match_format) {
    ASSERT_EQ_STR(clipboard_image_mime(CLIPBOARD_FMT_PNG), "image/png");
    ASSERT_EQ_STR(clipboard_image_mime(CLIPBOARD_FMT_JPEG), "image/jpeg");
    ASSERT_EQ_STR(clipboard_image_mime(CLIPBOARD_FMT_GIF), "image/gif");
    ASSERT_EQ_STR(clipboard_image_mime(CLIPBOARD_FMT_BMP), "image/bmp");
    ASSERT_TRUE(clipboard_image_mime(CLIPBOARD_FMT_UNKNOWN) == NULL);
}

TEST(png_dims_unchanged) {
    size_t n = 0;
    unsigned char* b = make_png(640, 480, &n);
    uint32_t w = 0, h = 0;
    clipboard_png_dims(b, n, &w, &h);
    ASSERT_EQ_INT(w, 640);
    ASSERT_EQ_INT(h, 480);
    free(b);
}

TEST(image_dims_dispatch_png) {
    size_t n = 0;
    unsigned char* b = make_png(300, 7, &n);
    uint32_t w = 0, h = 0;
    clipboard_image_dims(b, n, &w, &h);
    ASSERT_EQ_INT(w, 300);
    ASSERT_EQ_INT(h, 7);
    free(b);
}

TEST(image_dims_dispatch_jpeg) {
    size_t n = 0;
    unsigned char* b = make_jpeg(0xC0, 137, 89, 0, &n);
    uint32_t w = 0, h = 0;
    clipboard_image_dims(b, n, &w, &h);
    ASSERT_EQ_INT(w, 137);
    ASSERT_EQ_INT(h, 89);
    free(b);
}

TEST(image_dims_dispatch_gif) {
    size_t n = 0;
    unsigned char* b = make_gif(300, 7, &n);
    uint32_t w = 0, h = 0;
    clipboard_image_dims(b, n, &w, &h);
    ASSERT_EQ_INT(w, 300);
    ASSERT_EQ_INT(h, 7);
    free(b);
}

TEST(image_dims_dispatch_bmp) {
    size_t n = 0;
    unsigned char* b = make_bmp(137, 89, &n);
    uint32_t w = 0, h = 0;
    clipboard_image_dims(b, n, &w, &h);
    ASSERT_EQ_INT(w, 137);
    ASSERT_EQ_INT(h, 89);
    free(b);
}

TEST(jpeg_progressive_sof2_parsed) {
    size_t n = 0;
    unsigned char* b = make_jpeg(0xC2, 137, 89, 0, &n);
    uint32_t w = 0, h = 0;
    clipboard_image_dims(b, n, &w, &h);
    ASSERT_EQ_INT(w, 137);
    ASSERT_EQ_INT(h, 89);
    free(b);
}

TEST(jpeg_skips_app_segment_before_sof) {
    size_t n = 0;
    unsigned char* b = make_jpeg(0xC0, 137, 89, 1, &n);
    uint32_t w = 0, h = 0;
    clipboard_image_dims(b, n, &w, &h);
    ASSERT_EQ_INT(w, 137);
    ASSERT_EQ_INT(h, 89);
    free(b);
}

TEST(bmp_topdown_height_is_absolute) {
    size_t n = 0;
    unsigned char* b = make_bmp(64, -32, &n);
    uint32_t w = 0, h = 0;
    clipboard_image_dims(b, n, &w, &h);
    ASSERT_EQ_INT(w, 64);
    ASSERT_EQ_INT(h, 32);
    free(b);
}

TEST(truncated_files_yield_zero_dims_without_crash) {
    size_t n = 0;
    unsigned char* b = make_jpeg(0xC0, 137, 89, 0, &n);
    uint32_t w = 99, h = 99;
    clipboard_image_dims(b, 4, &w, &h);
    ASSERT_EQ_INT(w, 0);
    ASSERT_EQ_INT(h, 0);
    free(b);

    b = make_bmp(137, 89, &n);
    w = 99; h = 99;
    clipboard_image_dims(b, 10, &w, &h);
    ASSERT_EQ_INT(w, 0);
    ASSERT_EQ_INT(h, 0);
    free(b);

    b = make_gif(137, 89, &n);
    w = 99; h = 99;
    clipboard_image_dims(b, 4, &w, &h);
    ASSERT_EQ_INT(w, 0);
    ASSERT_EQ_INT(h, 0);
    free(b);
}

TEST(image_dims_zeroes_outputs_for_unknown) {
    const char* txt = "not an image at all";
    uint32_t w = 99, h = 99;
    clipboard_image_dims((const unsigned char*)txt, strlen(txt), &w, &h);
    ASSERT_EQ_INT(w, 0);
    ASSERT_EQ_INT(h, 0);
}

TEST(image_dims_tolerates_null_outputs) {
    size_t n = 0;
    unsigned char* b = make_png(10, 10, &n);
    clipboard_image_dims(b, n, NULL, NULL);
    clipboard_png_dims(b, n, NULL, NULL);
    ASSERT_TRUE(1);
    free(b);
}


TEST(uri_list_basic_path) {
    char* u = clipboard_path_to_uri_list("/home/dog/main.c");
    ASSERT_EQ_STR(u, "file:///home/dog/main.c\r\n");
    free(u);
}

TEST(uri_list_escapes_spaces) {
    char* u = clipboard_path_to_uri_list("/tmp/my file.txt");
    ASSERT_EQ_STR(u, "file:///tmp/my%20file.txt\r\n");
    free(u);
}

TEST(uri_list_escapes_reserved_characters) {
    char* u = clipboard_path_to_uri_list("/tmp/a#b?c&d");
    ASSERT_EQ_STR(u, "file:///tmp/a%23b%3Fc%26d\r\n");
    free(u);
}

TEST(uri_list_escapes_percent_and_hash) {
    char* u = clipboard_path_to_uri_list("/tmp/100%.c");
    ASSERT_EQ_STR(u, "file:///tmp/100%25.c\r\n");
    free(u);
}

TEST(uri_list_keeps_unreserved_and_slash) {
    char* u = clipboard_path_to_uri_list("/tmp/a-b_c.d~e/f0");
    ASSERT_EQ_STR(u, "file:///tmp/a-b_c.d~e/f0\r\n");
    free(u);
}

TEST(uri_list_encodes_utf8_bytes) {
    char* u = clipboard_path_to_uri_list("/tmp/\xc3\xa9.c");
    ASSERT_EQ_STR(u, "file:///tmp/%C3%A9.c\r\n");
    free(u);
}

TEST(uri_list_rejects_bad_input) {
    ASSERT_TRUE(clipboard_path_to_uri_list(NULL) == NULL);
    ASSERT_TRUE(clipboard_path_to_uri_list("") == NULL);
}

TEST(uri_list_single_entry_newline_terminated) {
    char* u = clipboard_path_to_uri_list("/x");
    ASSERT_EQ_INT(strlen(u), strlen("file:///x\r\n"));
    ASSERT_EQ_INT(u[strlen(u) - 2], '\r');
    ASSERT_EQ_INT(u[strlen(u) - 1], '\n');
    ASSERT_EQ_INT(strchr(u, '\r') - u, (long)strlen("file:///x"));
    free(u);
}

int main(void) {
    RUN(detects_png);
    RUN(detects_jpeg);
    RUN(detects_gif);
    RUN(detects_bmp);
    RUN(rejects_non_images);
    RUN(bare_magic_matches_but_dims_stay_zero);
    RUN(requires_full_magic_for_gif);
    RUN(mime_strings_match_format);
    RUN(png_dims_unchanged);
    RUN(image_dims_dispatch_png);
    RUN(image_dims_dispatch_jpeg);
    RUN(image_dims_dispatch_gif);
    RUN(image_dims_dispatch_bmp);
    RUN(jpeg_progressive_sof2_parsed);
    RUN(jpeg_skips_app_segment_before_sof);
    RUN(bmp_topdown_height_is_absolute);
    RUN(truncated_files_yield_zero_dims_without_crash);
    RUN(image_dims_zeroes_outputs_for_unknown);
    RUN(image_dims_tolerates_null_outputs);
    RUN(uri_list_single_entry_newline_terminated);
    RUN(uri_list_rejects_bad_input);
    RUN(uri_list_encodes_utf8_bytes);
    RUN(uri_list_keeps_unreserved_and_slash);
    RUN(uri_list_escapes_percent_and_hash);
    RUN(uri_list_escapes_reserved_characters);
    RUN(uri_list_escapes_spaces);
    RUN(uri_list_basic_path);
    return TEST_SUMMARY();
}
