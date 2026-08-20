/**
 * @file map_zone_engine.c
 * @brief Lineage2MBot 左上角地图框识别与安全/普通区域判断引擎 (100% 纯 C 原生实现)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "../../include/l2m_zone.h"
#include "../../include/l2m_vision.h"
#include "../../include/l2m_cbt.h"

bool l2m_detect_map_box(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MMapBoxResult* out_result
) {
    if (!crop_rgb || !crop_rgb->data || !out_result) return false;
    memset(out_result, 0, sizeof(L2MMapBoxResult));

    int32_t w = crop_rgb->width;
    int32_t h = crop_rgb->height;
    if (w < 20 || h < 20) {
        out_result->detected = false;
        out_result->zone_type = L2M_ZONE_UNKNOWN;
        snprintf(out_result->desc, sizeof(out_result->desc), "地图切片尺寸过小 (%dx%d)", w, h);
        return false;
    }

    out_result->map_rect.x = base_x;
    out_result->map_rect.y = base_y;
    out_result->map_rect.width = w;
    out_result->map_rect.height = h;

    int64_t total_r = 0, total_g = 0, total_b = 0;
    int32_t total_pixels = w * h;

    /* 1. 全切片像素统计与小地图特征底色分析 */
    int32_t dark_map_bg_pixels = 0;
    for (int32_t y = 0; y < h; y++) {
        const uint8_t* row = crop_rgb->data + y * crop_rgb->stride;
        for (int32_t x = 0; x < w; x++) {
            int32_t r = row[x * crop_rgb->channels + 0];
            int32_t g = row[x * crop_rgb->channels + 1];
            int32_t b = row[x * crop_rgb->channels + 2];

            total_r += r;
            total_g += g;
            total_b += b;

            int32_t c_max = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
            int32_t c_min = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
            int32_t chroma = c_max - c_min;

            /* 小地图暗色雷达底板像素特征 */
            if (r <= 90 && g <= 95 && b <= 105 && chroma <= 40) {
                dark_map_bg_pixels++;
            }
        }
    }

    float mean_r = (float)total_r / (float)total_pixels;
    float mean_g = (float)total_g / (float)total_pixels;
    float mean_b = (float)total_b / (float)total_pixels;
    float mean_brightness = (mean_r + mean_g + mean_b) / 3.0f;
    out_result->mean_brightness = mean_brightness;

    /* 2. 提取区域状态标签 ROI (Zone Badge / Text ROI)
       在 Lineage 2M 中，区域名称与安全/战斗状态文字通常位于小地图右侧或下半部分 */
    int32_t badge_x = (int32_t)(w * 0.15f);
    int32_t badge_y = (int32_t)(h * 0.10f);
    int32_t badge_w = w - badge_x;
    int32_t badge_h = h - badge_y;
    if (badge_w < 10) badge_w = w;
    if (badge_h < 10) badge_h = h;

    int64_t b_total_r = 0, b_total_g = 0, b_total_b = 0;
    int32_t green_safety_count = 0;
    int32_t white_normal_count = 0;
    int32_t red_combat_count = 0;
    int32_t badge_total_pixels = 0;

    for (int32_t y = badge_y; y < badge_y + badge_h; y++) {
        if (y >= h) break;
        const uint8_t* row = crop_rgb->data + y * crop_rgb->stride;
        for (int32_t x = badge_x; x < badge_x + badge_w; x++) {
            if (x >= w) break;
            int32_t r = row[x * crop_rgb->channels + 0];
            int32_t g = row[x * crop_rgb->channels + 1];
            int32_t b = row[x * crop_rgb->channels + 2];

            b_total_r += r;
            b_total_g += g;
            b_total_b += b;
            badge_total_pixels++;

            int32_t c_max = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
            int32_t c_min = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
            int32_t chroma = c_max - c_min;

            /* A. 安全区域 (Safety / Peace Zone): 显著的亮绿色文字与盾牌特征 */
            if (g >= 90 && (g >= r + 20) && (g >= b + 15) && chroma >= 22) {
                green_safety_count++;
            }
            /* B. 危险/自由战斗区域 (Combat / Danger Zone): 显著的亮红色文字与战斗标记 */
            else if (r >= 125 && (r >= g + 30) && (r >= b + 30) && chroma >= 30) {
                red_combat_count++;
            }
            /* C. 普通区域 (Normal Zone): 白色、浅灰色或淡金黄色常规野外文字 */
            else if ((chroma <= 25 && r >= 130 && g >= 130 && b >= 130) ||
                     (r >= 140 && g >= 130 && b <= 120 && chroma <= 45)) {
                white_normal_count++;
            }
        }
    }

    if (badge_total_pixels <= 0) badge_total_pixels = 1;

    float green_ratio = (float)green_safety_count / (float)badge_total_pixels;
    float white_ratio = (float)white_normal_count / (float)badge_total_pixels;
    float red_ratio = (float)red_combat_count / (float)badge_total_pixels;

    out_result->green_ratio = green_ratio;
    out_result->white_gray_ratio = white_ratio;
    out_result->red_ratio = red_ratio;

    out_result->badge_mean_rgb.r = (uint8_t)(b_total_r / badge_total_pixels);
    out_result->badge_mean_rgb.g = (uint8_t)(b_total_g / badge_total_pixels);
    out_result->badge_mean_rgb.b = (uint8_t)(b_total_b / badge_total_pixels);

    /* 3. 综合判定地图框存在性与区域类型决策 */
    float dark_ratio = (float)dark_map_bg_pixels / (float)total_pixels;
    bool has_map_bg = (dark_ratio >= 0.12f || mean_brightness <= 140.0f);

    out_result->detected = has_map_bg;

    if (green_ratio >= 0.015f && green_ratio > red_ratio * 1.5f) {
        /* 判定为【安全区域】 (Safety Zone / 村庄 / 和平区) */
        out_result->zone_type = L2M_ZONE_SAFETY;
        float sc = 75.0f + (green_ratio * 120.0f);
        if (sc > 99.0f) sc = 99.0f;
        out_result->confidence = sc;
        snprintf(out_result->zone_name, sizeof(out_result->zone_name), "安全区域(村庄)");
        snprintf(out_result->desc, sizeof(out_result->desc),
                 "检测到左上角地图 [安全区域/村庄]: 绿色安全特征占比 %.1f%% (RGB: %d,%d,%d)",
                 green_ratio * 100.0f, out_result->badge_mean_rgb.r, out_result->badge_mean_rgb.g, out_result->badge_mean_rgb.b);
    } else if (red_ratio >= 0.020f && red_ratio > green_ratio * 1.5f) {
        /* 判定为【危险/战斗区域】 (Combat / Danger Zone / 攻城战) */
        out_result->zone_type = L2M_ZONE_COMBAT;
        float sc = 75.0f + (red_ratio * 120.0f);
        if (sc > 99.0f) sc = 99.0f;
        out_result->confidence = sc;
        snprintf(out_result->zone_name, sizeof(out_result->zone_name), "战斗区域(PVP)");
        snprintf(out_result->desc, sizeof(out_result->desc),
                 "检测到左上角地图 [自由战斗区域/PVP]: 红色战斗特征占比 %.1f%%",
                 red_ratio * 100.0f);
    } else if (white_ratio >= 0.015f || has_map_bg) {
        /* 判定为【普通区域】 (Normal Zone / 野外常规战斗刷怪区) */
        out_result->zone_type = L2M_ZONE_NORMAL;
        float sc = 70.0f + (white_ratio * 80.0f);
        if (sc > 98.0f) sc = 98.0f;
        out_result->confidence = sc;
        snprintf(out_result->zone_name, sizeof(out_result->zone_name), "普通区域(野外)");
        snprintf(out_result->desc, sizeof(out_result->desc),
                 "检测到左上角地图 [普通区域/野外]: 常规野外特征占比 %.1f%%",
                 white_ratio * 100.0f);
    } else {
        out_result->zone_type = L2M_ZONE_UNKNOWN;
        out_result->confidence = 30.0f;
        snprintf(out_result->zone_name, sizeof(out_result->zone_name), "未知区域");
        snprintf(out_result->desc, sizeof(out_result->desc), "未识别到明显地图与区域特征");
    }

    return out_result->detected;
}

bool l2m_detect_map_zone(
    const L2MImageBuffer* full_frame_rgb,
    const void* cbt_cfg_ptr,
    L2MMapBoxResult* out_result
) {
    if (!full_frame_rgb || !full_frame_rgb->data || !out_result) return false;
    memset(out_result, 0, sizeof(L2MMapBoxResult));

    L2MRect map_roi = {10, 10, 135, 95}; /* 960x540 标准默认小地图 ROI */

    if (cbt_cfg_ptr) {
        const L2MCbtConfig* cfg = (const L2MCbtConfig*)cbt_cfg_ptr;
        if (cfg->map_box_roi.width > 20 && cfg->map_box_roi.height > 20) {
            map_roi = cfg->map_box_roi;
        }
    }

    /* 安全越界保护 */
    if (map_roi.x < 0) map_roi.x = 0;
    if (map_roi.y < 0) map_roi.y = 0;
    if (map_roi.x + map_roi.width > full_frame_rgb->width) {
        map_roi.width = full_frame_rgb->width - map_roi.x;
    }
    if (map_roi.y + map_roi.height > full_frame_rgb->height) {
        map_roi.height = full_frame_rgb->height - map_roi.y;
    }

    if (map_roi.width <= 10 || map_roi.height <= 10) return false;

    L2MImageBuffer* crop = l2m_image_create(map_roi.width, map_roi.height, L2M_FMT_RGB888);
    if (!crop || !l2m_image_crop(full_frame_rgb, &map_roi, crop)) {
        if (crop) l2m_image_free(crop);
        return false;
    }

    bool ok = l2m_detect_map_box(crop, map_roi.x, map_roi.y, out_result);
    l2m_image_free(crop);
    return ok;
}
