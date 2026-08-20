/**
 * @file color_mask.c
 * @brief 纯 C 色彩范围与容差掩码提取实现 (支持高并发与向量化优化)
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../include/l2m_vision.h"

bool l2m_color_mask_range(
    const L2MImageBuffer* src_rgb,
    L2MRGB min_rgb,
    L2MRGB max_rgb,
    L2MImageBuffer* dst_bin
) {
    if (!src_rgb || !src_rgb->data || !dst_bin || src_rgb->channels < 3) return false;

    int32_t w = src_rgb->width;
    int32_t h = src_rgb->height;

    for (int32_t y = 0; y < h; y++) {
        const uint8_t* s_row = src_rgb->data + y * src_rgb->stride;
        uint8_t* d_row = dst_bin->data + y * dst_bin->stride;

        for (int32_t x = 0; x < w; x++) {
            uint8_t r = s_row[x * src_rgb->channels + 0];
            uint8_t g = s_row[x * src_rgb->channels + 1];
            uint8_t b = s_row[x * src_rgb->channels + 2];

            if (r >= min_rgb.r && r <= max_rgb.r &&
                g >= min_rgb.g && g <= max_rgb.g &&
                b >= min_rgb.b && b <= max_rgb.b) {
                d_row[x] = 255;
            } else {
                d_row[x] = 0;
            }
        }
    }
    return true;
}

bool l2m_color_mask_tolerance(
    const L2MImageBuffer* src_rgb,
    L2MRGB target_rgb,
    L2MRGB tolerance,
    L2MImageBuffer* dst_bin
) {
    if (!src_rgb || !src_rgb->data || !dst_bin || src_rgb->channels < 3) return false;

    int32_t min_r = target_rgb.r - tolerance.r; if (min_r < 0) min_r = 0;
    int32_t max_r = target_rgb.r + tolerance.r; if (max_r > 255) max_r = 255;

    int32_t min_g = target_rgb.g - tolerance.g; if (min_g < 0) min_g = 0;
    int32_t max_g = target_rgb.g + tolerance.g; if (max_g > 255) max_g = 255;

    int32_t min_b = target_rgb.b - tolerance.b; if (min_b < 0) min_b = 0;
    int32_t max_b = target_rgb.b + tolerance.b; if (max_b > 255) max_b = 255;

    L2MRGB min_c = {(uint8_t)min_r, (uint8_t)min_g, (uint8_t)min_b};
    L2MRGB max_c = {(uint8_t)max_r, (uint8_t)max_g, (uint8_t)max_b};

    return l2m_color_mask_range(src_rgb, min_c, max_c, dst_bin);
}

bool l2m_mask_orange_popup_button(
    const L2MImageBuffer* src_rgb,
    L2MImageBuffer* dst_bin
) {
    if (!src_rgb || !src_rgb->data || !dst_bin || src_rgb->channels < 3) return false;

    int32_t w = src_rgb->width;
    int32_t h = src_rgb->height;

    for (int32_t y = 0; y < h; y++) {
        const uint8_t* s_row = src_rgb->data + y * src_rgb->stride;
        uint8_t* d_row = dst_bin->data + y * dst_bin->stride;

        for (int32_t x = 0; x < w; x++) {
            int32_t r = s_row[x * src_rgb->channels + 0];
            int32_t g = s_row[x * src_rgb->channels + 1];
            int32_t b = s_row[x * src_rgb->channels + 2];

            /* Lineage 2M 高亮橙黄色确认按钮特征:
               R >= 155, G >= 45 && G <= 175, B <= 75,
               (R - G) >= 30, (G - B) >= 10 */
            if (r >= 155 && r <= 255 &&
                g >= 45 && g <= 175 &&
                b <= 75 &&
                (r - g) >= 30 &&
                (g - b) >= 10) {
                d_row[x] = 255;
            } else {
                d_row[x] = 0;
            }
        }
    }
    return true;
}

bool l2m_mask_gray_button(
    const L2MImageBuffer* src_rgb,
    L2MImageBuffer* dst_bin
) {
    if (!src_rgb || !src_rgb->data || !dst_bin || src_rgb->channels < 3) return false;

    int32_t w = src_rgb->width;
    int32_t h = src_rgb->height;

    for (int32_t y = 0; y < h; y++) {
        const uint8_t* s_row = src_rgb->data + y * src_rgb->stride;
        uint8_t* d_row = dst_bin->data + y * dst_bin->stride;

        for (int32_t x = 0; x < w; x++) {
            int32_t r = s_row[x * src_rgb->channels + 0];
            int32_t g = s_row[x * src_rgb->channels + 1];
            int32_t b = s_row[x * src_rgb->channels + 2];

            int32_t diff_rg = abs(r - g);
            int32_t diff_gb = abs(g - b);

            /* 灰色确认/取消按钮特征:
               低饱和度 (|R-G|<=25, |G-B|<=25), 中等亮度 (40 <= R <= 165) */
            if (diff_rg <= 25 && diff_gb <= 25 && r >= 35 && r <= 165) {
                d_row[x] = 255;
            } else {
                d_row[x] = 0;
            }
        }
    }
    return true;
}
