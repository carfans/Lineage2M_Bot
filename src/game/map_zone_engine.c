/**
 * @file map_zone_engine.c
 * @brief Lineage2MBot 左上角地图框识别与安全/普通区域判断引擎 (100% 纯 C
 * 原生实现) 支持蓝色 Safe、浅咖色
 * Common、红色不可记忆网格与正中间橙圈白箭头扇形视角朝向分析
 */

#include "../../include/l2m_cbt.h"
#include "../../include/l2m_vision.h"
#include "../../include/l2m_zone.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool l2m_detect_map_box_with_config(const L2MImageBuffer *crop_rgb,
                                    int32_t base_x, int32_t base_y,
                                    const L2MMapZoneConfig *cfg,
                                    L2MMapBoxResult *out_result) {
  if (!crop_rgb || !crop_rgb->data || !out_result)
    return false;
  memset(out_result, 0, sizeof(L2MMapBoxResult));

  int32_t w = crop_rgb->width;
  int32_t h = crop_rgb->height;
  if (w <= 10 || h <= 10)
    return false;

  out_result->map_rect = (L2MRect){base_x, base_y, w, h};

  /* 1. 统计小地图全局像素特征 (暗色雷达底板 + 全局亮度 + 红色网格) */
  int64_t total_r = 0, total_g = 0, total_b = 0;
  int32_t dark_map_bg_pixels = 0;
  int32_t red_grid_pixels = 0;
  int32_t total_pixels = w * h;

  for (int32_t y = 0; y < h; y++) {
    const uint8_t *row = crop_rgb->data + y * crop_rgb->stride;
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

      /* A. 小地图深灰半透雷达底板像素特征 (考虑半透明透光) */
      if (r <= 125 && g <= 130 && b <= 140 && chroma <= 48) {
        dark_map_bg_pixels++;
      }

      /* B. 红色不可记忆网格像素特征 (纯正高饱和红网格线: R 显著高且 G
       * 较低，绝非浅咖色) */
      if (r >= 130 && r >= (int)(g * 1.5f) && (r - g) >= 45 && (r - b) >= 45 &&
          chroma >= 35) {
        red_grid_pixels++;
      }
    }
  }

  float mean_r = (float)total_r / (float)total_pixels;
  float mean_g = (float)total_g / (float)total_pixels;
  float mean_b = (float)total_b / (float)total_pixels;
  float mean_brightness = (mean_r + mean_g + mean_b) / 3.0f;
  out_result->mean_brightness = mean_brightness;

  float red_grid_ratio = (float)red_grid_pixels / (float)total_pixels;
  out_result->red_grid_ratio = red_grid_ratio;
  out_result->has_red_grid = (red_grid_ratio >= 0.015f);

  /* 2. 提取左上角区域状态徽章 ROI (Zone Badge ROI)
        在 Lineage 2M 中，左上角显示蓝色 Safe 徽标或浅咖色 Common 徽标
   */
  int32_t badge_x = 2;
  int32_t badge_y = 2;
  int32_t badge_w = (int32_t)(w * 0.38f);
  int32_t badge_h = (int32_t)(h * 0.24f);

  if (cfg && cfg->badge_width > 5 && cfg->badge_height > 5) {
    badge_x = cfg->badge_offset_x;
    badge_y = cfg->badge_offset_y;
    badge_w = cfg->badge_width;
    badge_h = cfg->badge_height;
  }

  if (badge_x < 0)
    badge_x = 0;
  if (badge_y < 0)
    badge_y = 0;
  if (badge_x + badge_w > w)
    badge_w = w - badge_x;
  if (badge_y + badge_h > h)
    badge_h = h - badge_y;

  if (badge_w < 5)
    badge_w = w;
  if (badge_h < 5)
    badge_h = h;

  int64_t b_total_r = 0, b_total_g = 0, b_total_b = 0;
  int32_t blue_safe_count = 0;
  int32_t brown_common_count = 0;
  int32_t green_safety_count = 0;
  int32_t white_normal_count = 0;
  int32_t red_combat_count = 0;
  int32_t badge_total_pixels = 0;

  for (int32_t y = badge_y; y < badge_y + badge_h; y++) {
    if (y >= h)
      break;
    const uint8_t *row = crop_rgb->data + y * crop_rgb->stride;
    for (int32_t x = badge_x; x < badge_x + badge_w; x++) {
      if (x >= w)
        break;
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

      /* A. 蓝色 Safe 安全区域文字/徽标特征 (高饱和蓝色/浅蓝/青蓝) */
      if (b >= 85 && b >= r + 20 && b >= g - 15 && (b - r) >= 15 &&
          chroma >= 18) {
        blue_safe_count++;
      }
      /* B. 浅咖色 Common 普通区域文字/徽标特征 (R > G > B
         咖啡色/黄褐色/暖褐色，G 较高) */
      else if (r >= 105 && g >= 65 && g <= 175 && b <= 115 && r >= g &&
               (r - b) >= 25 && g >= b + 10) {
        brown_common_count++;
      }
      /* C. 备用亮绿色安全区域特征 */
      else if (g >= 90 && g >= r + 20 && g >= b + 15 && chroma >= 22) {
        green_safety_count++;
      }
      /* D. 纯红色/危险战斗特征 (R 显著高且 G 较低) */
      else if (r >= 130 && r >= (int)(g * 1.5f) && (r - g) >= 45 &&
               (r - b) >= 45 && chroma >= 35) {
        red_combat_count++;
      }
      /* E. 备用白色/浅灰色常规野外文字 */
      else if ((chroma <= 25 && r >= 130 && g >= 130 && b >= 130) ||
               (r >= 140 && g >= 130 && b <= 120 && chroma <= 45)) {
        white_normal_count++;
      }
    }
  }

  if (badge_total_pixels <= 0)
    badge_total_pixels = 1;

  float blue_ratio = (float)blue_safe_count / (float)badge_total_pixels;
  float brown_ratio = (float)brown_common_count / (float)badge_total_pixels;
  float green_ratio = (float)green_safety_count / (float)badge_total_pixels;
  float white_ratio = (float)white_normal_count / (float)badge_total_pixels;
  float red_ratio = (float)red_combat_count / (float)badge_total_pixels;

  out_result->blue_safe_ratio = blue_ratio;
  out_result->brown_common_ratio = brown_ratio;
  out_result->green_ratio = green_ratio;
  out_result->white_gray_ratio = white_ratio;
  out_result->red_ratio = red_ratio;

  out_result->badge_mean_rgb.r = (uint8_t)(b_total_r / badge_total_pixels);
  out_result->badge_mean_rgb.g = (uint8_t)(b_total_g / badge_total_pixels);
  out_result->badge_mean_rgb.b = (uint8_t)(b_total_b / badge_total_pixels);

  /* 3. 中心玩家指示器与扇形视角朝向分析 (Center Player Indicator & Heading
   * Angle) */
  bool do_player_detect = (cfg == NULL || cfg->detect_player_indicator);
  if (do_player_detect) {
    int32_t cx = w / 2;
    int32_t cy = h / 2;
    int32_t search_r = (int32_t)(w < h ? w * 0.28f : h * 0.28f);
    if (search_r < 15)
      search_r = 15;

    int32_t white_arrow_pixels = 0;
    int32_t orange_cone_pixels = 0;
    float cone_sum_vx = 0.0f;
    float cone_sum_vy = 0.0f;

    for (int32_t dy = -search_r; dy <= search_r; dy++) {
      int32_t py = cy + dy;
      if (py < 0 || py >= h)
        continue;
      const uint8_t *row = crop_rgb->data + py * crop_rgb->stride;
      for (int32_t dx = -search_r; dx <= search_r; dx++) {
        int32_t px = cx + dx;
        if (px < 0 || px >= w)
          continue;
        if (dx * dx + dy * dy > search_r * search_r)
          continue;

        int32_t r = row[px * crop_rgb->channels + 0];
        int32_t g = row[px * crop_rgb->channels + 1];
        int32_t b = row[px * crop_rgb->channels + 2];

        /* 1. 中心高亮白色箭头 */
        if (r >= 200 && g >= 200 && b >= 200 && abs(r - g) <= 25 &&
            abs(r - b) <= 25) {
          white_arrow_pixels++;
        }

        /* 2. 橙色圆圈与扇形橙色渐变视角 (R 高, G 居中, B 极低) */
        if (r >= 160 && g >= 70 && g <= 185 && b <= 75 && (r - b) >= 80) {
          orange_cone_pixels++;
          /* 累加向量计算视角扇形重心朝向 */
          float dist = sqrtf((float)(dx * dx + dy * dy));
          if (dist > 2.0f) {
            cone_sum_vx += ((float)dx / dist);
            cone_sum_vy += ((float)dy / dist);
          }
        }
      }
    }

    if (white_arrow_pixels >= 2 || orange_cone_pixels >= 6) {
      out_result->has_player_indicator = true;
      out_result->player_center_pos = (L2MPoint){base_x + cx, base_y + cy};

      if (orange_cone_pixels >= 4 &&
          (fabsf(cone_sum_vx) > 0.1f || fabsf(cone_sum_vy) > 0.1f)) {
        out_result->has_view_cone = true;
        /* 计算数学方向角 (以正东为 0°, 逆时针为正 -> 转为正北 0° 顺时针) */
        float rad = atan2f(-cone_sum_vy, cone_sum_vx);
        float deg = (90.0f - (rad * 180.0f / 3.14159265f));
        while (deg < 0.0f)
          deg += 360.0f;
        while (deg >= 360.0f)
          deg -= 360.0f;
        out_result->player_heading_angle = deg;
      }
    }
  }

  /* 4. 综合判定地图框存在性与区域类型决策 */
  float th_blue =
      (cfg && cfg->min_blue_ratio > 0.0001f) ? cfg->min_blue_ratio : 0.015f;
  float th_brown =
      (cfg && cfg->min_brown_ratio > 0.0001f) ? cfg->min_brown_ratio : 0.015f;
  float th_green =
      (cfg && cfg->min_green_ratio > 0.0001f) ? cfg->min_green_ratio : 0.015f;
  float th_white =
      (cfg && cfg->min_white_ratio > 0.0001f) ? cfg->min_white_ratio : 0.015f;
  float max_bg_br =
      (cfg && cfg->max_bg_brightness > 10.0f) ? cfg->max_bg_brightness : 140.0f;

  float dark_ratio = (float)dark_map_bg_pixels / (float)total_pixels;
  bool has_map_bg = (dark_ratio >= 0.12f || mean_brightness <= max_bg_br ||
                     out_result->has_player_indicator);

  out_result->detected = has_map_bg;

  /* A. 安全区域 (Safe - 单词蓝色字体 / 备用绿色) */
  if ((blue_ratio >= th_blue && blue_ratio > brown_ratio * 1.3f) ||
      (green_ratio >= th_green && green_ratio > red_ratio * 1.5f)) {
    out_result->zone_type = L2M_ZONE_SAFETY;
    float score_ratio = blue_ratio > green_ratio ? blue_ratio : green_ratio;
    float sc = 78.0f + (score_ratio * 120.0f);
    if (sc > 99.0f)
      sc = 99.0f;
    out_result->confidence = sc;
    snprintf(out_result->zone_name, sizeof(out_result->zone_name),
             "安全区域(Safe蓝色字)");
    snprintf(out_result->desc, sizeof(out_result->desc),
             "检测到左上角地图 [安全区域/Safe蓝色单词]: 蓝色文字占比 %.1f%% | "
             "平均RGB(%d,%d,%d)",
             blue_ratio * 100.0f, out_result->badge_mean_rgb.r,
             out_result->badge_mean_rgb.g, out_result->badge_mean_rgb.b);
  }
  /* B. 战斗区域 (Combat - 红色特征 / 不可记忆红网格) */
  else if (out_result->has_red_grid || (red_ratio >= 0.012f && red_ratio > brown_ratio)) {
    out_result->zone_type = L2M_ZONE_COMBAT;
    float sc = 85.0f + (red_ratio * 80.0f);
    if (sc > 98.0f)
      sc = 98.0f;
    out_result->confidence = sc;
    snprintf(out_result->zone_name, sizeof(out_result->zone_name),
             "战斗区域(Combat/不可记忆)");
    snprintf(out_result->desc, sizeof(out_result->desc),
             "检测到左上角地图 [战斗区域/不可记忆]: 红色占比 %.1f%%",
             red_ratio * 100.0f);
  }
  /* C. 普通区域 (Common - 单词浅咖色字体 / 备用白灰) */
  else if (brown_ratio >= th_brown || white_ratio >= th_white || has_map_bg) {
    out_result->zone_type = L2M_ZONE_NORMAL;
    float sc = 72.0f + (brown_ratio * 90.0f);
    if (sc > 98.0f)
      sc = 98.0f;
    out_result->confidence = sc;
    snprintf(out_result->zone_name, sizeof(out_result->zone_name),
             "普通区域(Common浅咖字)");
    snprintf(
        out_result->desc, sizeof(out_result->desc),
        "检测到左上角地图 [普通区域/Common浅咖色单词]: 浅咖色文字占比 %.1f%%",
        brown_ratio * 100.0f);
  } else {
    out_result->zone_type = L2M_ZONE_UNKNOWN;
    out_result->confidence = 30.0f;
    snprintf(out_result->zone_name, sizeof(out_result->zone_name), "未知区域");
    snprintf(out_result->desc, sizeof(out_result->desc),
             "未识别到明显地图与区域特征");
  }

  return out_result->detected;
}

bool l2m_detect_map_box(const L2MImageBuffer *crop_rgb, int32_t base_x,
                        int32_t base_y, L2MMapBoxResult *out_result) {
  return l2m_detect_map_box_with_config(crop_rgb, base_x, base_y, NULL,
                                        out_result);
}

bool l2m_detect_map_zone(const L2MImageBuffer *full_frame_rgb,
                         const void *cbt_cfg_ptr, L2MMapBoxResult *out_result) {
  if (!full_frame_rgb || !full_frame_rgb->data || !out_result)
    return false;
  memset(out_result, 0, sizeof(L2MMapBoxResult));

  L2MMapZoneConfig map_cfg;
  memset(&map_cfg, 0, sizeof(map_cfg));
  map_cfg.enabled = true;
  snprintf(map_cfg.desc, sizeof(map_cfg.desc),
           "左上角深灰半透小地图");
  map_cfg.x = 25;
  map_cfg.y = 59;
  map_cfg.width = 145;
  map_cfg.height = 95;
  map_cfg.badge_offset_x = 2;
  map_cfg.badge_offset_y = 2;
  map_cfg.badge_width = 50;
  map_cfg.badge_height = 20;
  map_cfg.min_blue_ratio = 0.012f;
  map_cfg.min_brown_ratio = 0.012f;
  map_cfg.min_green_ratio = 0.012f;
  map_cfg.min_red_ratio = 0.015f;
  map_cfg.min_white_ratio = 0.012f;
  map_cfg.max_bg_brightness = 150.0f;
  map_cfg.detect_player_indicator = true;

  if (cbt_cfg_ptr) {
    const L2MCbtConfig *cfg = (const L2MCbtConfig *)cbt_cfg_ptr;
    if (cfg->map_zone_cfg.width > 10 && cfg->map_zone_cfg.height > 10) {
      map_cfg = cfg->map_zone_cfg;
    } else if (cfg->map_box_roi.width > 10 && cfg->map_box_roi.height > 10) {
      map_cfg.x = cfg->map_box_roi.x;
      map_cfg.y = cfg->map_box_roi.y;
      map_cfg.width = cfg->map_box_roi.width;
      map_cfg.height = cfg->map_box_roi.height;
    }
  }

  if (!map_cfg.enabled) {
    out_result->detected = false;
    snprintf(out_result->desc, sizeof(out_result->desc),
             "地图区域识别已在配置中禁用");
    return false;
  }

  L2MRect map_roi = {map_cfg.x, map_cfg.y, map_cfg.width, map_cfg.height};

  /* 安全越界保护 */
  if (map_roi.x < 0)
    map_roi.x = 0;
  if (map_roi.y < 0)
    map_roi.y = 0;
  if (map_roi.x + map_roi.width > full_frame_rgb->width) {
    map_roi.width = full_frame_rgb->width - map_roi.x;
  }
  if (map_roi.y + map_roi.height > full_frame_rgb->height) {
    map_roi.height = full_frame_rgb->height - map_roi.y;
  }

  if (map_roi.width <= 10 || map_roi.height <= 10)
    return false;

  L2MImageBuffer *crop =
      l2m_image_create(map_roi.width, map_roi.height, L2M_FMT_RGB888);
  if (!crop || !l2m_image_crop(full_frame_rgb, &map_roi, crop)) {
    if (crop)
      l2m_image_free(crop);
    return false;
  }

  bool ok = l2m_detect_map_box_with_config(crop, map_roi.x, map_roi.y, &map_cfg,
                                           out_result);
  l2m_image_free(crop);
  return ok;
}
