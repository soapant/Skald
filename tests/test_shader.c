#include "test.h"
#include "config.h"
#include "core/theme.h"
#include "helpers/helpers_ui.h"


#define DARK 0x192419u

/* cfg_intensity is the ShaderIntensity config value; the per-call multiplier is
   always 1.0f in production, so these helpers drive the global directly. */
static void derive_at(unsigned int base, float cfg_intensity, int* r, int* g, int* b) {
    ShaderIntensity = cfg_intensity;
    unsigned char cr, cg, cb;
    Color_DeriveLighter(base, 1, &cr, &cg, &cb);
    *r = cr; *g = cg; *b = cb;
}

static int base_sum(unsigned int base) {
    return (int)((base >> 16) & 0xFF) + (int)((base >> 8) & 0xFF) + (int)(base & 0xFF);
}

static int derive_delta(unsigned int base, float cfg_intensity) {
    int r, g, b;
    derive_at(base, cfg_intensity, &r, &g, &b);
    return (r + g + b) - base_sum(base);
}

TEST(lerp_endpoints) {
    ASSERT_EQ_INT(Color_Lerp(0x000000u, 0x000000u, 0.0f) & 0xFF, 0x00);
    ASSERT_EQ_INT((Color_Lerp(0x000000u, 0xFFFFFFu, 0.5f) >> 16) & 0xFF, 0x80);
    ASSERT_EQ_INT(Color_Lerp(0x102030u, 0x102030u, 0.5f), 0x102030u);
    ASSERT_EQ_INT(Color_Lerp(0x000000u, 0xFFFFFFu, 0.0f), 0x000000u);
    ASSERT_EQ_INT(Color_Lerp(0x000000u, 0xFFFFFFu, 1.0f), 0xFFFFFFu);
}

TEST(lerp_clamps_t) {
    unsigned int a = 0x000000u, b = 0xFFFFFFu;
    ASSERT_EQ_INT(Color_Lerp(a, b, -5.0f), Color_Lerp(a, b, 0.0f));
    ASSERT_EQ_INT(Color_Lerp(a, b, 5.0f), Color_Lerp(a, b, 1.0f));
}

TEST(derive_lighter_brightens_dark_base) {
    int r, g, b;
    derive_at(DARK, 1.0f, &r, &g, &b);
    ASSERT_EQ_INT(r, 25 + 65 + 35);
    ASSERT_EQ_INT(g, 36 + 54 + 35);
    ASSERT_EQ_INT(b, 25 + 115 + 40);
    ASSERT_TRUE(r + g + b > base_sum(DARK));
}

TEST(derive_lighter_zero_intensity_is_identity) {
    int r, g, b;
    derive_at(DARK, 0.0f, &r, &g, &b);
    ASSERT_EQ_INT(r, 25);
    ASSERT_EQ_INT(g, 36);
    ASSERT_EQ_INT(b, 25);
}

TEST(derive_lighter_intensity_is_monotonic) {
    float steps[] = {0.0f, 0.0001f, 0.001f, 0.01f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f};
    int prev = derive_delta(DARK, steps[0]);
    for (size_t i = 1; i < sizeof(steps) / sizeof(steps[0]); i++) {
        int d = derive_delta(DARK, steps[i]);
        ASSERT_TRUE(d >= prev);
        prev = d;
    }
}

TEST(derive_lighter_low_intensity_is_not_clamped_to_a_floor) {
    int quarter = derive_delta(DARK, 0.25f);
    ASSERT_TRUE(derive_delta(DARK, 0.0001f) * 4 < quarter);
    ASSERT_TRUE(derive_delta(DARK, 0.001f) * 4 < quarter);
    ASSERT_TRUE(derive_delta(DARK, 0.01f) * 4 < quarter);
}

TEST(derive_lighter_strong_intensity_scales_gradient) {
    int quarter = derive_delta(DARK, 0.25f);
    int half = derive_delta(DARK, 0.5f);
    int full = derive_delta(DARK, 1.0f);
    ASSERT_TRUE(half > quarter);
    ASSERT_TRUE(full > half);
}

TEST(derive_lighter_darkens_bright_base) {
    unsigned int light = 0xC8C8F0u;
    int r, g, b;
    derive_at(light, 1.0f, &r, &g, &b);
    ASSERT_TRUE(r < 0xC8);
    ASSERT_TRUE(g < 0xC8);
    ASSERT_TRUE(b < 0xF0);
    ASSERT_EQ_INT(r, 90 + 35);
    ASSERT_EQ_INT(g, 90 + 35);
    ASSERT_EQ_INT(b, 140 + 40);
}

TEST(derive_lighter_never_wraps_on_bright_base) {
    unsigned int probes[] = {0xFFFFFFu, 0xC8C8F0u, 0x8090A0u, 0x646464u, 0x000000u};
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        for (int step = 0; step <= 20; step++) {
            int r, g, b;
            derive_at(probes[i], step / 20.0f, &r, &g, &b);
            ASSERT_TRUE(r >= 0 && r <= 255);
            ASSERT_TRUE(g >= 0 && g <= 255);
            ASSERT_TRUE(b >= 0 && b <= 255);
        }
    }
}

TEST(derive_lighter_is_pure_function_of_intensity) {
    int r1, g1, b1, r2, g2, b2;
    derive_at(DARK, 0.37f, &r1, &g1, &b1);
    derive_at(DARK, 0.37f, &r2, &g2, &b2);
    ASSERT_EQ_INT(r1, r2);
    ASSERT_EQ_INT(g1, g2);
    ASSERT_EQ_INT(b1, b2);
}

TEST(gamma_decreases_with_smoothness) {
    ASSERT_TRUE(Color_ShaderGamma(1.0f) > Color_ShaderGamma(10.0f));
    ASSERT_TRUE(Color_ShaderGamma(0.0f) >= 0.45f);
    ASSERT_TRUE(Color_ShaderGamma(1000.0f) <= 2.2f);
    ASSERT_TRUE(Color_ShaderGamma(-5.0f) >= 0.45f);
    ASSERT_TRUE(Color_ShaderGamma(10.0f) > 0.0f);
}

TEST(gamma_is_shared_by_both_platforms) {
    ASSERT_EQ_INT((int)(Color_ShaderGamma(10.0f) * 1000.0f), 600);
    ASSERT_EQ_INT((int)(Color_ShaderGamma(1.0f) * 1000.0f), 1860);
}

TEST(bayer_dither_is_bounded) {
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            float d = Color_BayerDither(x, y);
            ASSERT_TRUE(d >= -0.6f && d <= 0.6f);
        }
}

TEST(bayer_dither_tiles_every_four) {
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            ASSERT_EQ_INT((int)(Color_BayerDither(x + 4, y) * 1000.0f), (int)(Color_BayerDither(x, y) * 1000.0f));
            ASSERT_EQ_INT((int)(Color_BayerDither(x, y + 4) * 1000.0f), (int)(Color_BayerDither(x, y) * 1000.0f));
        }
}

TEST(bayer_dither_mean_is_near_zero) {
    double sum = 0.0;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            sum += Color_BayerDither(x, y);
    double mean = sum / 16.0;
    ASSERT_TRUE(mean > -0.05 && mean < 0.05);
}

TEST(hash_noise_is_deterministic_and_bounded) {
    for (int i = 0; i < 64; i++) {
        ASSERT_EQ_INT(Color_HashNoise(i, i * 3), Color_HashNoise(i, i * 3));
        ASSERT_TRUE(Color_HashNoise(i, i * 3) <= 0xFF);
    }
    ASSERT_TRUE(Color_HashNoise(0, 0) != Color_HashNoise(1, 0));
}

TEST(gradient_endpoints_are_visibly_distinct) {
    ASSERT_TRUE(derive_delta(DARK, 0.55f) > 60);
    ASSERT_TRUE(derive_delta(DARK, 0.25f) > 20);
    ASSERT_TRUE(derive_delta(DARK, 0.1f) > 5);
}

TEST(theme_apply_never_overwrites_shader_settings) {
    static const char* names[] = { "rose-pine", "tokyo-night", "gruvbox", "nord", "dracula" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        Theme = names[i];
        ShaderIntensity = 0.75f;
        GradientBrightness = 0.6f;
        Theme_Apply();
        ASSERT_EQ_INT((int)(ShaderIntensity * 1000.0f), 750);
        ASSERT_EQ_INT((int)(GradientBrightness * 1000.0f), 600);
    }
}

TEST(theme_apply_still_sets_colors) {
    Theme = "rose-pine";
    COLOR_BG = 0;
    Theme_Apply();
    ASSERT_EQ_INT(COLOR_BG, RGB(25, 23, 36));
}

TEST(theme_apply_ignores_unknown_theme) {
    Theme = "no-such-theme";
    ShaderIntensity = 0.42f;
    COLOR_BG = 0x123456;
    Theme_Apply();
    ASSERT_EQ_INT((int)(ShaderIntensity * 1000.0f), 420);
    ASSERT_EQ_INT(COLOR_BG, 0x123456);
}

int main(void) {
    RUN(lerp_endpoints);
    RUN(lerp_clamps_t);
    RUN(derive_lighter_brightens_dark_base);
    RUN(derive_lighter_zero_intensity_is_identity);
    RUN(derive_lighter_intensity_is_monotonic);
    RUN(derive_lighter_low_intensity_is_not_clamped_to_a_floor);
    RUN(derive_lighter_strong_intensity_scales_gradient);
    RUN(derive_lighter_darkens_bright_base);
    RUN(derive_lighter_never_wraps_on_bright_base);
    RUN(derive_lighter_is_pure_function_of_intensity);
    RUN(gamma_decreases_with_smoothness);
    RUN(gamma_is_shared_by_both_platforms);
    RUN(bayer_dither_is_bounded);
    RUN(bayer_dither_tiles_every_four);
    RUN(bayer_dither_mean_is_near_zero);
    RUN(hash_noise_is_deterministic_and_bounded);
    RUN(gradient_endpoints_are_visibly_distinct);
    RUN(theme_apply_never_overwrites_shader_settings);
    RUN(theme_apply_still_sets_colors);
    RUN(theme_apply_ignores_unknown_theme);
    ShaderIntensity = 0.55f;
    return TEST_SUMMARY() != 0 ? 1 : 0;
}

