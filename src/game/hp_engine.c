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

/* 检查像素是否符合配置的主目标色或辅目标色 */
static inline bool is_pixel_matching_config(L2MRGB pixel, const L2MHpConfig* config) {
    if (match_color_tolerance(pixel, config->target_color_1, config->tolerance_1)) {
        return true;
    }
    if (config->target_color_2.r > 0 || config->target_color_2.g > 0 || config->target_color_2.b > 0) {
        if (match_color_tolerance(pixel, config->target_color_2, config->tolerance_2)) {
            return true;
        }
    }
    return false;
}

/* 核心血量计算逻辑：严格依据配置目标色、容差与切片宽度进行纯配置驱动计算 */
static inline void compute_hp_core(
    const uint8_t* data,
    int32_t w,
    int32_t h,
    int32_t stride,
    int32_t channels,
    bool is_bgr,
    const L2MHpConfig* config,
    L2MHpResult* out_result
) {
    int64_t total_r = 0, total_g = 0, total_b = 0;
    int32_t total_count = w * h;
    int32_t last_c1_x = -1;

    /* 逐列逐行计算颜色均值与配置匹配 */
    int32_t continuous_end = -1;
    int32_t missed_gap = 0;

    for (int32_t x = 0; x < w; x++) {
        bool col_matched = false;

        for (int32_t y = 0; y < h; y++) {
            const uint8_t* row = data + y * stride;
            L2MRGB pixel;
            if (is_bgr) {
                pixel.b = row[x * channels + 0];
                pixel.g = row[x * channels + 1];
                pixel.r = row[x * channels + 2];
            } else {
                pixel.r = row[x * channels + 0];
                pixel.g = row[x * channels + 1];
                pixel.b = row[x * channels + 2];
            }

            total_r += pixel.r;
            total_g += pixel.g;
            total_b += pixel.b;

            if (match_color_tolerance(pixel, config->target_color_1, config->tolerance_1)) {
                if (x > last_c1_x) last_c1_x = x;
            }

            if (!col_matched && is_pixel_matching_config(pixel, config)) {
                col_matched = true;
            }
        }

        /* 连续性追踪：从左至右延伸有效血量 */
        if (col_matched) {
            continuous_end = x;
            missed_gap = 0;
        } else {
            missed_gap++;
            /* 若连续两列均不匹配配置颜色，则确定到达当前血量物理端点 */
            if (missed_gap >= 2) {
                break;
            }
        }
    }

    out_result->mean_rgb.r = (uint8_t)(total_r / total_count);
    out_result->mean_rgb.g = (uint8_t)(total_g / total_count);
    out_result->mean_rgb.b = (uint8_t)(total_b / total_count);
    out_result->sample_red_end = last_c1_x;
    out_result->sample_hp_end = continuous_end;

    if (continuous_end < 0) {
        out_result->hp_percent = 0;
        out_result->is_valid = false;
        return;
    }

    /* 纯配置驱动线性百分比计算：(有效命中列数 / 配置总宽度) * 100% */
    float pct = ((float)(continuous_end + 1) / (float)w) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f) pct = 0.0f;

    out_result->hp_percent = (int32_t)roundf(pct);
    out_result->is_valid = true;
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

    compute_hp_core(crop_rgb->data, w, h, crop_rgb->stride, crop_rgb->channels, false, config, out_result);
    return true;
}

bool l2m_calculate_hp_bgr(
    const L2MImageBuffer* crop_bgr,
    const L2MHpConfig* config,
    L2MHpResult* out_result
) {
    if (!crop_bgr || !crop_bgr->data || !config || !out_result || crop_bgr->channels < 3) return false;

    int32_t w = crop_bgr->width;
    int32_t h = crop_bgr->height;
    if (w <= 0 || h <= 0) return false;

    compute_hp_core(crop_bgr->data, w, h, crop_bgr->stride, crop_bgr->channels, true, config, out_result);
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
