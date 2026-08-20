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
               R >= 145, G >= 40 && G <= 185, B <= 85,
               (R - G) >= 25, (G - B) >= 5, 且 R 占主导 */
            if (r >= 145 && r <= 255 &&
                g >= 40 && g <= 185 &&
                b <= 85 &&
                (r - g) >= 25 &&
                (g - b) >= 5) {
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

            int32_t max_val = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
            int32_t min_val = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
            int32_t chroma = max_val - min_val;

            /* 灰色确认/取消按钮特征:
               低彩度 (chroma <= 25), 中等灰度亮度 (30 <= r <= 170) */
            if (chroma <= 25 && r >= 30 && r <= 170) {
                d_row[x] = 255;
            } else {
                d_row[x] = 0;
            }
        }
    }
    return true;
}

bool l2m_analyze_region_color(
    const L2MImageBuffer* src_rgb,
    const L2MRect* roi,
    L2MRGB* out_mean_rgb,
    float* out_mean_brightness,
    float* out_max_chroma
) {
    if (!src_rgb || !src_rgb->data || !roi || src_rgb->channels < 3) return false;

    int32_t rx = (roi->x < 0) ? 0 : roi->x;
    int32_t ry = (roi->y < 0) ? 0 : roi->y;
    int32_t rw = roi->width;
    int32_t rh = roi->height;

    if (rx + rw > src_rgb->width) rw = src_rgb->width - rx;
    if (ry + rh > src_rgb->height) rh = src_rgb->height - ry;
    if (rw <= 0 || rh <= 0) return false;

    int64_t total_r = 0, total_g = 0, total_b = 0;
    int32_t max_chroma_found = 0;
    int32_t total_pixels = rw * rh;

    for (int32_t y = ry; y < ry + rh; y++) {
        const uint8_t* row = src_rgb->data + y * src_rgb->stride;
        for (int32_t x = rx; x < rx + rw; x++) {
            int32_t r = row[x * src_rgb->channels + 0];
            int32_t g = row[x * src_rgb->channels + 1];
            int32_t b = row[x * src_rgb->channels + 2];

            total_r += r;
            total_g += g;
            total_b += b;

            int32_t c_max = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
            int32_t c_min = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
            int32_t chroma = c_max - c_min;
            if (chroma > max_chroma_found) max_chroma_found = chroma;
        }
    }

    float mr = (float)total_r / (float)total_pixels;
    float mg = (float)total_g / (float)total_pixels;
    float mb = (float)total_b / (float)total_pixels;

    if (out_mean_rgb) {
        out_mean_rgb->r = (uint8_t)mr;
        out_mean_rgb->g = (uint8_t)mg;
        out_mean_rgb->b = (uint8_t)mb;
    }
    if (out_mean_brightness) {
        *out_mean_brightness = (mr + mg + mb) / 3.0f;
    }
    if (out_max_chroma) {
        *out_max_chroma = (float)max_chroma_found;
    }
    return true;
}

bool l2m_verify_button_color(
    const L2MImageBuffer* src_rgb,
    const L2MRect* roi,
    bool is_orange,
    L2MRGB* out_mean_rgb,
    float* out_fill_ratio,
    float* out_color_score
) {
    if (!src_rgb || !src_rgb->data || !roi || src_rgb->channels < 3) return false;

    /* 内部缩进 1~2 像素以避开边缘外框噪点 */
    int32_t margin_x = (roi->width >= 16) ? 2 : 0;
    int32_t margin_y = (roi->height >= 12) ? 2 : 0;

    int32_t rx = roi->x + margin_x;
    int32_t ry = roi->y + margin_y;
    int32_t rw = roi->width - margin_x * 2;
    int32_t rh = roi->height - margin_y * 2;

    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx + rw > src_rgb->width) rw = src_rgb->width - rx;
    if (ry + rh > src_rgb->height) rh = src_rgb->height - ry;
    if (rw <= 0 || rh <= 0) return false;

    int64_t total_r = 0, total_g = 0, total_b = 0;
    int32_t match_pixels = 0;
    int32_t total_pixels = rw * rh;

    for (int32_t y = ry; y < ry + rh; y++) {
        const uint8_t* row = src_rgb->data + y * src_rgb->stride;
        for (int32_t x = rx; x < rx + rw; x++) {
            int32_t r = row[x * src_rgb->channels + 0];
            int32_t g = row[x * src_rgb->channels + 1];
            int32_t b = row[x * src_rgb->channels + 2];

            total_r += r;
            total_g += g;
            total_b += b;

            if (is_orange) {
                /* 橙黄色按钮像素判别 */
                if (r >= 140 && g >= 35 && g <= 190 && b <= 95 && (r - g) >= 20 && (g - b) >= 5) {
                    match_pixels++;
                }
            } else {
                /* 灰色按钮像素判别 */
                int32_t c_max = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
                int32_t c_min = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
                if ((c_max - c_min) <= 28 && r >= 30 && r <= 180) {
                    match_pixels++;
                }
            }
        }
    }

    float mean_r = (float)total_r / (float)total_pixels;
    float mean_g = (float)total_g / (float)total_pixels;
    float mean_b = (float)total_b / (float)total_pixels;
    float fill_ratio = (float)match_pixels / (float)total_pixels;

    if (out_mean_rgb) {
        out_mean_rgb->r = (uint8_t)mean_r;
        out_mean_rgb->g = (uint8_t)mean_g;
        out_mean_rgb->b = (uint8_t)mean_b;
    }
    if (out_fill_ratio) {
        *out_fill_ratio = fill_ratio;
    }

    /* 综合色彩匹配得分 (0 ~ 100) */
    float color_score = 0.0f;
    if (is_orange) {
        /* 标准橙色目标: (215, 105, 12) */
        float dr = mean_r - 215.0f;
        float dg = mean_g - 105.0f;
        float db = mean_b - 12.0f;
        float dist = sqrtf(dr * dr + dg * dg + db * db);
        float dist_score = 100.0f - (dist * 0.45f);
        if (dist_score < 0.0f) dist_score = 0.0f;

        color_score = (fill_ratio * 60.0f) + (dist_score * 0.40f);
        if (mean_r < 120.0f || mean_b > 110.0f) color_score *= 0.5f;
    } else {
        /* 灰色标准: 彩度极低，亮度在 [40, 160] */
        float c_max = (mean_r > mean_g) ? (mean_r > mean_b ? mean_r : mean_b) : (mean_g > mean_b ? mean_g : mean_b);
        float c_min = (mean_r < mean_g) ? (mean_r < mean_b ? mean_r : mean_b) : (mean_g < mean_b ? mean_g : mean_b);
        float chroma = c_max - c_min;
        float chroma_score = 100.0f - (chroma * 2.5f);
        if (chroma_score < 0.0f) chroma_score = 0.0f;

        color_score = (fill_ratio * 55.0f) + (chroma_score * 0.45f);
    }

    if (color_score > 100.0f) color_score = 100.0f;
    if (color_score < 0.0f) color_score = 0.0f;

    if (out_color_score) {
        *out_color_score = color_score;
    }

    /* 最低填充纯度门限判定 */
    float min_fill = is_orange ? 0.30f : 0.35f;
    return (fill_ratio >= min_fill && color_score >= 35.0f);
}

bool l2m_evaluate_button_size(
    const L2MRect* bbox,
    L2MPopupType ptype,
    float* out_size_score
) {
    if (!bbox) return false;

    int32_t w = bbox->width;
    int32_t h = bbox->height;
    int32_t area = w * h;
    if (w <= 0 || h <= 0) return false;

    float aspect = (float)w / (float)h;

    int32_t min_w = 30, max_w = 260;
    int32_t min_h = 15, max_h = 70;
    int32_t min_area = 400, max_area = 15000;
    float min_aspect = 1.0f, max_aspect = 6.0f;
    float ideal_aspect = 2.8f;

    if (ptype == L2M_POPUP_TOP_LEFT) {
        min_w = 28; max_w = 140;
        min_h = 14; max_h = 55;
        min_area = 350; max_area = 6500;
        min_aspect = 1.2f; max_aspect = 4.8f;
        ideal_aspect = 2.5f;
    } else if (ptype == L2M_POPUP_CENTER) {
        min_w = 40; max_w = 220;
        min_h = 18; max_h = 65;
        min_area = 600; max_area = 12000;
        min_aspect = 1.4f; max_aspect = 5.5f;
        ideal_aspect = 3.0f;
    }

    if (w < min_w || w > max_w || h < min_h || h > max_h ||
        area < min_area || area > max_area ||
        aspect < min_aspect || aspect > max_aspect) {
        if (out_size_score) *out_size_score = 0.0f;
        return false;
    }

    /* 几何形态得分计算 */
    float aspect_diff = fabsf(aspect - ideal_aspect);
    float aspect_score = 100.0f - (aspect_diff * 22.0f);
    if (aspect_score < 20.0f) aspect_score = 20.0f;

    float size_score = aspect_score;
    if (out_size_score) *out_size_score = size_score;
    return true;
}
