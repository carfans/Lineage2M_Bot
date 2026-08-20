/**
 * @file hp_engine.c
 * @brief Lineage2MBot 血条采样计算引擎纯 C 原生实现
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../include/l2m_hp.h"
#include "../../include/l2m_vision.h"

static inline bool match_color_tolerance(L2MRGB c, L2MRGB target, L2MRGB tol) {
    return (abs((int32_t)c.r - (int32_t)target.r) <= tol.r) &&
           (abs((int32_t)c.g - (int32_t)target.g) <= tol.g) &&
           (abs((int32_t)c.b - (int32_t)target.b) <= tol.b);
}

bool l2m_calculate_hp(
    const L2MImageBuffer* crop_rgb,
    const L2MHpConfig* config,
    L2MHpResult* out_result
) {
    if (!crop_rgb || !crop_rgb->data || !config || !out_result || crop_rgb->channels < 3) return false;

    int32_t w = crop_rgb->width;
    int32_t h = crop_rgb->height;
    if (w <= 0 || h <= 0) return false;

    int64_t total_r = 0, total_g = 0, total_b = 0;
    int32_t total_count = w * h;

    int32_t last_valid_x = -1;
    int32_t last_c1_x = -1;

    /* 逐列逐行扫描像素颜色 */
    for (int32_t y = 0; y < h; y++) {
        const uint8_t* row = crop_rgb->data + y * crop_rgb->stride;
        for (int32_t x = 0; x < w; x++) {
            L2MRGB pixel;
            pixel.r = row[x * crop_rgb->channels + 0];
            pixel.g = row[x * crop_rgb->channels + 1];
            pixel.b = row[x * crop_rgb->channels + 2];

            total_r += pixel.r;
            total_g += pixel.g;
            total_b += pixel.b;

            bool m1 = match_color_tolerance(pixel, config->target_color_1, config->tolerance_1);
            bool m2 = match_color_tolerance(pixel, config->target_color_2, config->tolerance_2);

            if (m1) {
                if (x > last_c1_x) last_c1_x = x;
                if (x > last_valid_x) last_valid_x = x;
            } else if (m2) {
                if (x > last_valid_x) last_valid_x = x;
            }
        }
    }

    out_result->mean_rgb.r = (uint8_t)(total_r / total_count);
    out_result->mean_rgb.g = (uint8_t)(total_g / total_count);
    out_result->mean_rgb.b = (uint8_t)(total_b / total_count);
    out_result->sample_hp_end = last_valid_x;
    out_result->sample_red_end = last_c1_x;

    if (last_valid_x >= 0) {
        /* 血量百分比 = ((采样命中端点 + 1) / 采样总宽度) * 100% */
        float pct = ((float)(last_valid_x + 1) / (float)w) * 100.0f;
        if (pct > 100.0f) pct = 100.0f;
        if (pct < 0.0f) pct = 0.0f;
        out_result->hp_percent = (int32_t)roundf(pct);
        out_result->is_valid = true;
    } else {
        out_result->hp_percent = 0;
        out_result->is_valid = false;
    }

    return true;
}

bool l2m_calculate_hp_from_fullscreen(
    const L2MImageBuffer* full_screen_rgb,
    const L2MHpConfig* config,
    L2MHpResult* out_result
) {
    if (!full_screen_rgb || !full_screen_rgb->data || !config || !out_result) return false;

    L2MRect roi;
    roi.x = config->offset_x;
    roi.y = config->offset_y;
    roi.width = config->width;
    roi.height = config->height;

    L2MImageBuffer* crop = l2m_image_create(roi.width, roi.height, full_screen_rgb->format);
    if (!crop) return false;

    bool ok = l2m_image_crop(full_screen_rgb, &roi, crop);
    if (ok) {
        ok = l2m_calculate_hp(crop, config, out_result);
    }
    l2m_image_free(crop);
    return ok;
}
