/**
 * @file cbt_manager.c
 * @brief Lineage2MBot 多语言 CBT 采样特征点与弹窗扫描配置管理实现
 */

#include "../../include/l2m_cbt.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* 递归确保文件父目录存在 */
static void ensure_parent_dir_exists(const char *filepath) {
  if (!filepath)
    return;
  char temp[MAX_PATH];
  strncpy(temp, filepath, sizeof(temp) - 1);
  temp[sizeof(temp) - 1] = '\0';

  for (char *p = temp; *p; p++) {
    if (*p == '/' || *p == '\\') {
      char old = *p;
      *p = '\0';
      if (strlen(temp) > 0 && temp[strlen(temp) - 1] != ':') {
#ifdef _WIN32
        CreateDirectoryA(temp, NULL);
#else
        mkdir(temp, 0755);
#endif
      }
      *p = old;
    }
  }
}

/* 鲁棒寻找项目中的 data/cbt/<REGION>.json 绝对/相对路径 */
static bool get_cbt_file_path(const char *region, char *out_path,
                              size_t max_len) {
  if (!region || !out_path || max_len == 0)
    return false;

  /* 1. 优先基于当前进程模块 (EXE) 所在物理目录探测绝对路径 */
#ifdef _WIN32
  char exe_dir[MAX_PATH] = {0};
  if (GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir))) {
    char *last_slash = strrchr(exe_dir, '\\');
    if (!last_slash)
      last_slash = strrchr(exe_dir, '/');
    if (last_slash) {
      *last_slash = '\0';

      char candidate[MAX_PATH];

      /* 探测 exe 同级 data/cbt/<REGION>.json */
      snprintf(candidate, sizeof(candidate), "%s/data/cbt/%s.json", exe_dir,
               region);
      FILE *fp = fopen(candidate, "rb");
      if (fp) {
        fclose(fp);
        strncpy(out_path, candidate, max_len);
        return true;
      }

      /* 探测 exe 上一级 ../data/cbt/<REGION>.json (如 bin/../data/cbt/CN.json)
       */
      snprintf(candidate, sizeof(candidate), "%s/../data/cbt/%s.json", exe_dir,
               region);
      fp = fopen(candidate, "rb");
      if (fp) {
        fclose(fp);
        strncpy(out_path, candidate, max_len);
        return true;
      }

      /* 探测 exe 同级 bot/data/cbt/<REGION>.json */
      snprintf(candidate, sizeof(candidate), "%s/bot/data/cbt/%s.json", exe_dir,
               region);
      fp = fopen(candidate, "rb");
      if (fp) {
        fclose(fp);
        strncpy(out_path, candidate, max_len);
        return true;
      }

      /* 探测 exe 上一级 ../bot/data/cbt/<REGION>.json */
      snprintf(candidate, sizeof(candidate), "%s/../bot/data/cbt/%s.json",
               exe_dir, region);
      fp = fopen(candidate, "rb");
      if (fp) {
        fclose(fp);
        strncpy(out_path, candidate, max_len);
        return true;
      }
    }
  }
#endif

  /* 2. 基于当前工作目录相对路径探测 */
  const char *candidates[] = {"data/cbt/%s.json", "../data/cbt/%s.json",
                              "bot/data/cbt/%s.json",
                              "../bot/data/cbt/%s.json"};
  int num_candidates = (int)(sizeof(candidates) / sizeof(candidates[0]));

  for (int i = 0; i < num_candidates; i++) {
    char temp[MAX_PATH];
    snprintf(temp, sizeof(temp), candidates[i], region);
    FILE *fp = fopen(temp, "rb");
    if (fp) {
      fclose(fp);
      strncpy(out_path, temp, max_len);
      return true;
    }
  }

  /* 3. 最终回退：若在 bin 目录下运行，优先规范保存至
   * exe_dir/../data/cbt/<REGION>.json */
#ifdef _WIN32
  if (exe_dir[0]) {
    snprintf(out_path, max_len, "%s/../data/cbt/%s.json", exe_dir, region);
    return true;
  }
#endif

  snprintf(out_path, max_len, "data/cbt/%s.json", region);
  return true;
}

/* 提取 JSON 对象中的浮点字段 */
static bool parse_json_float(const char *json_obj_str, const char *key,
                             float *out_val) {
  if (!json_obj_str || !key || !out_val)
    return false;
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  char *pos = strstr(json_obj_str, pattern);
  if (pos) {
    char *colon = strchr(pos, ':');
    if (colon) {
      *out_val = (float)atof(colon + 1);
      return true;
    }
  }
  return false;
}

/* 提取 JSON 对象中的整型字段 */
static bool parse_json_int(const char *json_obj_str, const char *key,
                           int *out_val) {
  if (!json_obj_str || !key || !out_val)
    return false;
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  char *pos = strstr(json_obj_str, pattern);
  if (pos) {
    char *colon = strchr(pos, ':');
    if (colon) {
      *out_val = atoi(colon + 1);
      return true;
    }
  }
  return false;
}

/* 提取 JSON 对象中的布尔字段 */
static bool parse_json_bool(const char *json_obj_str, const char *key,
                            bool *out_val) {
  if (!json_obj_str || !key || !out_val)
    return false;
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  char *pos = strstr(json_obj_str, pattern);
  if (pos) {
    char *colon = strchr(pos, ':');
    if (colon) {
      while (*colon && (*colon == ':' || *colon == ' ' || *colon == '\t' ||
                        *colon == '\r' || *colon == '\n')) {
        colon++;
      }
      if (strncmp(colon, "true", 4) == 0) {
        *out_val = true;
        return true;
      } else if (strncmp(colon, "false", 5) == 0) {
        *out_val = false;
        return true;
      }
    }
  }
  return false;
}

/* 提取 JSON 对象中的 RGB 数组字段: [r, g, b] */
static bool parse_json_rgb_array(const char *json_obj_str, const char *key,
                                 L2MRGB *out_rgb) {
  if (!json_obj_str || !key || !out_rgb)
    return false;
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  char *pos = strstr(json_obj_str, pattern);
  if (pos) {
    char *bracket = strchr(pos, '[');
    if (bracket) {
      int r = 0, g = 0, b = 0;
      char *comma1 = strchr(bracket, ',');
      if (comma1) {
        char *comma2 = strchr(comma1 + 1, ',');
        if (comma2) {
          r = atoi(bracket + 1);
          g = atoi(comma1 + 1);
          b = atoi(comma2 + 1);
          out_rgb->r = (uint8_t)r;
          out_rgb->g = (uint8_t)g;
          out_rgb->b = (uint8_t)b;
          return true;
        }
      }
    }
  }
  return false;
}

/* 提取 JSON 对象中的字符串字段 */
static bool parse_json_string(const char *json_obj_str, const char *key,
                              char *out_str, size_t max_len) {
  if (!json_obj_str || !key || !out_str || max_len == 0)
    return false;
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  char *pos = strstr(json_obj_str, pattern);
  if (pos) {
    char *colon = strchr(pos, ':');
    if (colon) {
      char *q1 = strchr(colon, '"');
      if (q1) {
        char *q2 = strchr(q1 + 1, '"');
        if (q2) {
          size_t len = q2 - (q1 + 1);
          if (len >= max_len)
            len = max_len - 1;
          strncpy(out_str, q1 + 1, len);
          out_str[len] = '\0';
          return true;
        }
      }
    }
  }
  return false;
}

/* 提取 JSON 中的弹窗特征配置节点 */
static bool parse_json_popup_param(const char *json_obj_str,
                                   L2MPopupItem *param) {
  if (!json_obj_str || !param)
    return false;

  /* 描述与启用状态 */
  parse_json_string(json_obj_str, "desc", param->desc, sizeof(param->desc));
  parse_json_bool(json_obj_str, "enabled", &param->enabled);

  /* 关联链接的 CBT 采样点 Key */
  parse_json_string(json_obj_str, "linked_cbt_key", param->linked_cbt_key,
                    sizeof(param->linked_cbt_key));

  /* 基础 ROI 坐标 */
  parse_json_int(json_obj_str, "x", &param->x);
  parse_json_int(json_obj_str, "y", &param->y);
  parse_json_int(json_obj_str, "width", &param->width);
  parse_json_int(json_obj_str, "height", &param->height);

  /* 背景特征 */
  parse_json_float(json_obj_str, "min_dark_ratio", &param->min_dark_ratio);
  parse_json_float(json_obj_str, "max_brightness", &param->max_brightness);
  parse_json_float(json_obj_str, "max_high_chroma", &param->max_high_chroma);

  /* 按钮尺寸几何 */
  parse_json_int(json_obj_str, "btn_min_w", &param->btn_min_w);
  parse_json_int(json_obj_str, "btn_max_w", &param->btn_max_w);
  parse_json_int(json_obj_str, "btn_min_h", &param->btn_min_h);
  parse_json_int(json_obj_str, "btn_max_h", &param->btn_max_h);
  parse_json_float(json_obj_str, "btn_ideal_aspect", &param->btn_ideal_aspect);

  /* 按钮色彩 */
  if (parse_json_rgb_array(json_obj_str, "btn_target_rgb",
                           &param->btn_target_rgb)) {
    param->has_btn_rgb = true;
  }
  parse_json_float(json_obj_str, "btn_min_fill_ratio",
                   &param->btn_min_fill_ratio);

  /* 结构特征开关 */
  parse_json_bool(json_obj_str, "check_panel", &param->check_panel);
  parse_json_bool(json_obj_str, "check_title", &param->check_title);
  parse_json_bool(json_obj_str, "check_text_lines", &param->check_text_lines);
  parse_json_bool(json_obj_str, "check_close_cross", &param->check_close_cross);
  parse_json_bool(json_obj_str, "has_checkbox", &param->has_checkbox);

  return true;
}

static void init_default_popup_params(L2MPopupScanConfig *pop_cfg) {
  if (!pop_cfg)
    return;

  pop_cfg->count = 0;

  /* Top-Left 默认特征配置 */
  pop_cfg->top_left = (L2MPopupItem){.name = "top_left_tip",
                                     .desc = "左上角提示弹窗(带不再显示)",
                                     .enabled = true,
                                     .x = 25,
                                     .y = 54,
                                     .width = 235,
                                     .height = 390,
                                     .min_dark_ratio = 0.16f,
                                     .max_brightness = 125.0f,
                                     .max_high_chroma = 0.35f,
                                     .btn_min_w = 28,
                                     .btn_max_w = 140,
                                     .btn_min_h = 14,
                                     .btn_max_h = 55,
                                     .btn_ideal_aspect = 2.5f,
                                     .has_btn_rgb = true,
                                     .btn_target_rgb = {215, 105, 12},
                                     .btn_min_fill_ratio = 0.30f,
                                     .check_panel = true,
                                     .check_title = true,
                                     .check_text_lines = true,
                                     .check_close_cross = false,
                                     .has_checkbox = true};

  /* Center 默认特征配置 */
  pop_cfg->center = (L2MPopupItem){.name = "center_modal",
                                   .desc = "中间标准模态确认弹窗",
                                   .enabled = true,
                                   .x = 280,
                                   .y = 150,
                                   .width = 400,
                                   .height = 240,
                                   .min_dark_ratio = 0.20f,
                                   .max_brightness = 115.0f,
                                   .max_high_chroma = 0.35f,
                                   .btn_min_w = 40,
                                   .btn_max_w = 220,
                                   .btn_min_h = 18,
                                   .btn_max_h = 65,
                                   .btn_ideal_aspect = 3.0f,
                                   .has_btn_rgb = true,
                                   .btn_target_rgb = {215, 105, 12},
                                   .btn_min_fill_ratio = 0.35f,
                                   .check_panel = true,
                                   .check_title = true,
                                   .check_text_lines = true,
                                   .check_close_cross = true,
                                   .has_checkbox = false};

  /* Fullscreen 默认特征配置 */
  pop_cfg->fullscreen = (L2MPopupItem){.name = "fullscreen_event",
                                       .desc = "全屏活动与公告弹窗",
                                       .enabled = true,
                                       .x = 0,
                                       .y = 0,
                                       .width = 960,
                                       .height = 540,
                                       .min_dark_ratio = 0.14f,
                                       .max_brightness = 135.0f,
                                       .max_high_chroma = 0.35f,
                                       .btn_min_w = 30,
                                       .btn_max_w = 260,
                                       .btn_min_h = 16,
                                       .btn_max_h = 70,
                                       .btn_ideal_aspect = 2.8f,
                                       .has_btn_rgb = true,
                                       .btn_target_rgb = {215, 105, 12},
                                       .btn_min_fill_ratio = 0.30f,
                                       .check_panel = false,
                                       .check_title = true,
                                       .check_text_lines = true,
                                       .check_close_cross = true,
                                       .has_checkbox = false};

  pop_cfg->items[pop_cfg->count++] = pop_cfg->top_left;
  pop_cfg->items[pop_cfg->count++] = pop_cfg->center;
  pop_cfg->items[pop_cfg->count++] = pop_cfg->fullscreen;
}

/* 查找与开括号匹配的闭大括号，支持任意层级嵌套和字符串转义 */
static char *find_matching_brace(char *start) {
  if (!start || *start != '{')
    return NULL;
  int depth = 0;
  char *p = start;
  bool in_string = false;
  while (*p) {
    if (*p == '"' && (p == start || *(p - 1) != '\\')) {
      in_string = !in_string;
    } else if (!in_string) {
      if (*p == '{') {
        depth++;
      } else if (*p == '}') {
        depth--;
        if (depth == 0)
          return p;
      }
    }
    p++;
  }
  return NULL;
}

static void init_default_map_zone_config(L2MMapZoneConfig *cfg) {
  if (!cfg)
    return;
  memset(cfg, 0, sizeof(L2MMapZoneConfig));
  cfg->enabled = true;
  snprintf(cfg->desc, sizeof(cfg->desc),
           "左上角深灰半透小地图(Safe蓝色/Common浅咖色/红网格/中心朝向)");
  cfg->x = 25;
  cfg->y = 59;
  cfg->width = 145;
  cfg->height = 95;
  cfg->badge_offset_x = 2;
  cfg->badge_offset_y = 2;
  cfg->badge_width = 50;
  cfg->badge_height = 20;
  cfg->min_blue_ratio = 0.012f;
  cfg->min_brown_ratio = 0.012f;
  cfg->min_green_ratio = 0.012f;
  cfg->min_red_ratio = 0.015f;
  cfg->min_white_ratio = 0.012f;
  cfg->max_bg_brightness = 150.0f;
  cfg->detect_player_indicator = true;
}

static void init_default_hp_config(L2MHpConfig *cfg) {
  if (!cfg)
    return;
  memset(cfg, 0, sizeof(L2MHpConfig));
  cfg->offset_x = 64;
  cfg->offset_y = 21;
  cfg->width = 103;
  cfg->height = 2;
  cfg->target_color_1 = (L2MRGB){230, 48, 48};
  cfg->tolerance_1 = (L2MRGB){25, 25, 25};
  cfg->target_color_2 = (L2MRGB){255, 157, 57};
  cfg->tolerance_2 = (L2MRGB){10, 10, 10};
  cfg->mean_threshold = 0.0f;
}

bool l2m_cbt_load(const char *region, L2MCbtConfig *cfg) {
  if (!region || !cfg)
    return false;

  memset(cfg, 0, sizeof(L2MCbtConfig));
  strncpy(cfg->region, region, sizeof(cfg->region) - 1);
  get_cbt_file_path(region, cfg->file_path, sizeof(cfg->file_path));

  /* 初始化弹窗全特征默认参数、地图区域默认参数与血条默认参数 (960x540 标准参考系) */
  init_default_popup_params(&cfg->popup_cfg);
  init_default_map_zone_config(&cfg->map_zone_cfg);
  init_default_hp_config(&cfg->hp_bar_cfg);
  cfg->map_box_roi =
      (L2MRect){cfg->map_zone_cfg.x, cfg->map_zone_cfg.y,
                cfg->map_zone_cfg.width, cfg->map_zone_cfg.height};

  FILE *fp = fopen(cfg->file_path, "rb");
  if (!fp)
    return false;

  fseek(fp, 0, SEEK_END);
  long fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (fsize <= 0 || fsize > 10 * 1024 * 1024) {
    fclose(fp);
    return false;
  }

  char *content = (char *)malloc(fsize + 1);
  if (!content) {
    fclose(fp);
    return false;
  }

  size_t read_bytes = fread(content, 1, fsize, fp);
  content[read_bytes] = '\0';
  fclose(fp);

  /* 检查并解析 "popup_scan_config" 动态命名弹窗集合 */
  char *pop_tag = strstr(content, "\"popup_scan_config\"");
  if (pop_tag) {
    char *pop_brace_start = strchr(pop_tag, '{');
    if (pop_brace_start) {
      char *pop_brace_end = find_matching_brace(pop_brace_start);
      if (pop_brace_end) {
        cfg->popup_cfg.count = 0; /* 重置以读取 JSON 中的自定义命名列表 */
        char *sub_p = pop_brace_start + 1;
        while (sub_p < pop_brace_end &&
               cfg->popup_cfg.count < MAX_POPUP_ITEMS) {
          char *k_start = strchr(sub_p, '"');
          if (!k_start || k_start >= pop_brace_end)
            break;
          k_start++;
          char *k_end = strchr(k_start, '"');
          if (!k_end || k_end >= pop_brace_end)
            break;

          char item_name[64];
          size_t name_len = k_end - k_start;
          if (name_len >= sizeof(item_name))
            name_len = sizeof(item_name) - 1;
          strncpy(item_name, k_start, name_len);
          item_name[name_len] = '\0';

          char *colon = strchr(k_end, ':');
          if (!colon || colon >= pop_brace_end)
            break;
          char *item_obj_start = strchr(colon, '{');
          if (!item_obj_start || item_obj_start >= pop_brace_end)
            break;

          char *item_obj_end = find_matching_brace(item_obj_start);
          if (!item_obj_end || item_obj_end > pop_brace_end)
            break;

          /* 初始化并解析命名项 */
          L2MPopupItem p_item;
          memset(&p_item, 0, sizeof(p_item));
          snprintf(p_item.name, sizeof(p_item.name), "%s", item_name);
          p_item.enabled = true;
          p_item.btn_ideal_aspect = 2.8f;
          p_item.min_dark_ratio = 0.16f;
          p_item.max_brightness = 125.0f;
          p_item.max_high_chroma = 0.35f;
          p_item.btn_min_w = 28;
          p_item.btn_max_w = 220;
          p_item.btn_min_h = 14;
          p_item.btn_max_h = 65;
          p_item.has_btn_rgb = true;
          p_item.btn_target_rgb = (L2MRGB){215, 105, 12};
          p_item.btn_min_fill_ratio = 0.30f;
          p_item.check_panel = true;
          p_item.check_title = true;
          p_item.check_text_lines = true;
          p_item.check_close_cross = true;

          parse_json_popup_param(item_obj_start, &p_item);
          cfg->popup_cfg.items[cfg->popup_cfg.count++] = p_item;

          /* 同步映射历史别名 */
          if (strcmp(item_name, "top_left") == 0 ||
              strcmp(item_name, "top_left_tip") == 0) {
            cfg->popup_cfg.top_left = p_item;
          } else if (strcmp(item_name, "center") == 0 ||
                     strcmp(item_name, "center_modal") == 0) {
            cfg->popup_cfg.center = p_item;
          } else if (strcmp(item_name, "fullscreen") == 0 ||
                     strcmp(item_name, "fullscreen_event") == 0) {
            cfg->popup_cfg.fullscreen = p_item;
          }

          sub_p = item_obj_end + 1;
        }
      }
    }
  }

  /* 检查并解析 "map_box_config" 地图扫描区域节点 */
  char *map_tag = strstr(content, "\"map_box_config\"");
  if (map_tag) {
    char *map_brace_start = strchr(map_tag, '{');
    if (map_brace_start) {
      char *map_brace_end = find_matching_brace(map_brace_start);
      if (map_brace_end) {
        parse_json_bool(map_brace_start, "enabled", &cfg->map_zone_cfg.enabled);
        parse_json_string(map_brace_start, "desc", cfg->map_zone_cfg.desc,
                          sizeof(cfg->map_zone_cfg.desc));
        parse_json_int(map_brace_start, "x", &cfg->map_zone_cfg.x);
        parse_json_int(map_brace_start, "y", &cfg->map_zone_cfg.y);
        parse_json_int(map_brace_start, "width", &cfg->map_zone_cfg.width);
        parse_json_int(map_brace_start, "height", &cfg->map_zone_cfg.height);
        parse_json_int(map_brace_start, "badge_offset_x",
                       &cfg->map_zone_cfg.badge_offset_x);
        parse_json_int(map_brace_start, "badge_offset_y",
                       &cfg->map_zone_cfg.badge_offset_y);
        parse_json_int(map_brace_start, "badge_width",
                       &cfg->map_zone_cfg.badge_width);
        parse_json_int(map_brace_start, "badge_height",
                       &cfg->map_zone_cfg.badge_height);
        parse_json_float(map_brace_start, "min_blue_ratio",
                         &cfg->map_zone_cfg.min_blue_ratio);
        parse_json_float(map_brace_start, "min_brown_ratio",
                         &cfg->map_zone_cfg.min_brown_ratio);
        parse_json_float(map_brace_start, "min_green_ratio",
                         &cfg->map_zone_cfg.min_green_ratio);
        parse_json_float(map_brace_start, "min_red_ratio",
                         &cfg->map_zone_cfg.min_red_ratio);
        parse_json_float(map_brace_start, "min_white_ratio",
                         &cfg->map_zone_cfg.min_white_ratio);
        parse_json_float(map_brace_start, "max_bg_brightness",
                         &cfg->map_zone_cfg.max_bg_brightness);
        parse_json_bool(map_brace_start, "detect_player_indicator",
                        &cfg->map_zone_cfg.detect_player_indicator);

        cfg->map_box_roi =
            (L2MRect){cfg->map_zone_cfg.x, cfg->map_zone_cfg.y,
                      cfg->map_zone_cfg.width, cfg->map_zone_cfg.height};
      }
    }
  }

  /* 检查并解析 "hp_bar_config" 血条参数节点 */
  char *hp_tag = strstr(content, "\"hp_bar_config\"");
  if (hp_tag) {
    char *hp_brace_start = strchr(hp_tag, '{');
    if (hp_brace_start) {
      char *hp_brace_end = find_matching_brace(hp_brace_start);
      if (hp_brace_end) {
        parse_json_int(hp_brace_start, "offset_x", &cfg->hp_bar_cfg.offset_x);
        parse_json_int(hp_brace_start, "offset_y", &cfg->hp_bar_cfg.offset_y);
        parse_json_int(hp_brace_start, "width", &cfg->hp_bar_cfg.width);
        parse_json_int(hp_brace_start, "height", &cfg->hp_bar_cfg.height);
        parse_json_rgb_array(hp_brace_start, "target_color_1", &cfg->hp_bar_cfg.target_color_1);
        parse_json_rgb_array(hp_brace_start, "tolerance_1", &cfg->hp_bar_cfg.tolerance_1);
        parse_json_rgb_array(hp_brace_start, "target_color_2", &cfg->hp_bar_cfg.target_color_2);
        parse_json_rgb_array(hp_brace_start, "tolerance_2", &cfg->hp_bar_cfg.tolerance_2);
      }
    }
  }

  /* 解析普通特征采样点 */
  char *p = content;
  while (*p && cfg->count < MAX_CBT_POINTS) {
    char *key_start = strchr(p, '"');
    if (!key_start)
      break;
    key_start++;
    char *key_end = strchr(key_start, '"');
    if (!key_end)
      break;

    char key_name[64];
    size_t klen = key_end - key_start;
    if (klen >= sizeof(key_name))
      klen = sizeof(key_name) - 1;
    strncpy(key_name, key_start, klen);
    key_name[klen] = '\0';

    char *colon = strchr(key_end, ':');
    if (!colon)
      break;
    char *obj_start = strchr(colon, '{');
    if (!obj_start)
      break;

    char *obj_end = find_matching_brace(obj_start);
    if (!obj_end)
      break;

    if (strcmp(key_name, "popup_scan_config") == 0 ||
        strcmp(key_name, "map_box_config") == 0 ||
        strcmp(key_name, "hp_bar_config") == 0 ||
        strcmp(key_name, "top_left") == 0 || strcmp(key_name, "center") == 0 ||
        strcmp(key_name, "fullscreen") == 0) {
      p = obj_end + 1;
      continue;
    }

    L2MCbtPoint pt;
    memset(&pt, 0, sizeof(L2MCbtPoint));
    snprintf(pt.key, sizeof(pt.key), "%s", key_name);
    pt.tolerance = 12;

    /* 提取 "pos": [ x, y ] */
    bool has_pos = false;
    char *pos_tag = strstr(obj_start, "\"pos\"");
    if (pos_tag && pos_tag < obj_end) {
      char *bracket = strchr(pos_tag, '[');
      if (bracket && bracket < obj_end) {
        int px = 0, py = 0;
        if (sscanf(bracket + 1, "%d , %d", &px, &py) == 2 ||
            sscanf(bracket + 1, "%d ,%d", &px, &py) == 2 ||
            sscanf(bracket + 1, "%d\n , %d", &px, &py) == 2) {
          pt.x = px;
          pt.y = py;
          has_pos = true;
        } else {
          char *comma = strchr(bracket, ',');
          if (comma && comma < obj_end) {
            pt.x = atoi(bracket + 1);
            pt.y = atoi(comma + 1);
            has_pos = true;
          }
        }
      }
    }

    if (!has_pos) {
      p = obj_end + 1;
      continue;
    }

    /* 提取 "rgb": [ r, g, b ] */
    char *rgb_tag = strstr(obj_start, "\"rgb\"");
    if (rgb_tag && rgb_tag < obj_end) {
      char *bracket = strchr(rgb_tag, '[');
      if (bracket && bracket < obj_end) {
        int r = 0, g = 0, b = 0;
        char *comma1 = strchr(bracket, ',');
        if (comma1 && comma1 < obj_end) {
          char *comma2 = strchr(comma1 + 1, ',');
          if (comma2 && comma2 < obj_end) {
            r = atoi(bracket + 1);
            g = atoi(comma1 + 1);
            b = atoi(comma2 + 1);
            pt.has_rgb = true;
            pt.r = (uint8_t)r;
            pt.g = (uint8_t)g;
            pt.b = (uint8_t)b;
          }
        }
      }
    }

    /* 提取 "tolerance": val */
    char *tol_tag = strstr(obj_start, "\"tolerance\"");
    if (tol_tag && tol_tag < obj_end) {
      char *colon_tol = strchr(tol_tag, ':');
      if (colon_tol && colon_tol < obj_end) {
        pt.tolerance = atoi(colon_tol + 1);
      }
    }

    cfg->points[cfg->count++] = pt;
    p = obj_end + 1;
  }

  free(content);
  return true;
}

static void write_json_popup_item(FILE *fp, const L2MPopupItem *p,
                                  bool is_last) {
  fprintf(fp, "    \"%s\": {\n", p->name);
  if (strlen(p->desc) > 0) {
    fprintf(fp, "      \"desc\": \"%s\",\n", p->desc);
  }
  fprintf(fp, "      \"enabled\": %s,\n", p->enabled ? "true" : "false");
  if (strlen(p->linked_cbt_key) > 0) {
    fprintf(fp, "      \"linked_cbt_key\": \"%s\",\n", p->linked_cbt_key);
  }
  fprintf(fp, "      \"x\": %d,\n", p->x);
  fprintf(fp, "      \"y\": %d,\n", p->y);
  fprintf(fp, "      \"width\": %d,\n", p->width);
  fprintf(fp, "      \"height\": %d,\n", p->height);
  fprintf(fp, "      \"min_dark_ratio\": %.2f,\n", p->min_dark_ratio);
  fprintf(fp, "      \"max_brightness\": %.1f,\n", p->max_brightness);
  fprintf(fp, "      \"max_high_chroma\": %.2f,\n", p->max_high_chroma);
  fprintf(fp, "      \"btn_min_w\": %d,\n", p->btn_min_w);
  fprintf(fp, "      \"btn_max_w\": %d,\n", p->btn_max_w);
  fprintf(fp, "      \"btn_min_h\": %d,\n", p->btn_min_h);
  fprintf(fp, "      \"btn_max_h\": %d,\n", p->btn_max_h);
  fprintf(fp, "      \"btn_ideal_aspect\": %.2f,\n", p->btn_ideal_aspect);
  if (p->has_btn_rgb) {
    fprintf(fp, "      \"btn_target_rgb\": [%d, %d, %d],\n",
            p->btn_target_rgb.r, p->btn_target_rgb.g, p->btn_target_rgb.b);
  } else {
    fprintf(fp, "      \"btn_target_rgb\": null,\n");
  }
  fprintf(fp, "      \"btn_min_fill_ratio\": %.2f,\n", p->btn_min_fill_ratio);
  fprintf(fp, "      \"check_panel\": %s,\n",
          p->check_panel ? "true" : "false");
  fprintf(fp, "      \"check_title\": %s,\n",
          p->check_title ? "true" : "false");
  fprintf(fp, "      \"check_text_lines\": %s,\n",
          p->check_text_lines ? "true" : "false");
  fprintf(fp, "      \"check_close_cross\": %s,\n",
          p->check_close_cross ? "true" : "false");
  fprintf(fp, "      \"has_checkbox\": %s\n",
          p->has_checkbox ? "true" : "false");
  fprintf(fp, "    }%s\n", is_last ? "" : ",");
}

bool l2m_cbt_save(const L2MCbtConfig *cfg) {
  if (!cfg)
    return false;

  const char *target_path = cfg->file_path;
  char resolved_path[MAX_PATH] = {0};
  if (strlen(target_path) == 0) {
    get_cbt_file_path(cfg->region, resolved_path, sizeof(resolved_path));
    target_path = resolved_path;
  }

  ensure_parent_dir_exists(target_path);

  FILE *fp = fopen(target_path, "wb");
  if (!fp) {
    /* 尝试获取规范绝对路径重试 */
    get_cbt_file_path(cfg->region, resolved_path, sizeof(resolved_path));
    ensure_parent_dir_exists(resolved_path);
    fp = fopen(resolved_path, "wb");
    if (!fp) {
      return false;
    }
  }

  fprintf(fp, "{\n");

  /* 1. 写入包含命名弹窗列表的 popup_scan_config 节点 */
  fprintf(fp, "  \"popup_scan_config\": {\n");
  if (cfg->popup_cfg.count > 0) {
    for (int i = 0; i < cfg->popup_cfg.count; i++) {
      write_json_popup_item(fp, &cfg->popup_cfg.items[i],
                            (i == cfg->popup_cfg.count - 1));
    }
  } else {
    write_json_popup_item(fp, &cfg->popup_cfg.top_left, false);
    write_json_popup_item(fp, &cfg->popup_cfg.center, false);
    write_json_popup_item(fp, &cfg->popup_cfg.fullscreen, true);
  }

  if (cfg->count > 0 || cfg->map_box_roi.width > 0) {
    fprintf(fp, "  },\n");
  } else {
    fprintf(fp, "  }\n");
  }

  /* 2. 写入 map_box_config 节点 */
  if (cfg->map_zone_cfg.width > 0 && cfg->map_zone_cfg.height > 0) {
    fprintf(fp, "  \"map_box_config\": {\n");
    fprintf(fp, "    \"enabled\": %s,\n",
            cfg->map_zone_cfg.enabled ? "true" : "false");
    if (strlen(cfg->map_zone_cfg.desc) > 0) {
      fprintf(fp, "    \"desc\": \"%s\",\n", cfg->map_zone_cfg.desc);
    }
    fprintf(fp, "    \"x\": %d,\n", cfg->map_zone_cfg.x);
    fprintf(fp, "    \"y\": %d,\n", cfg->map_zone_cfg.y);
    fprintf(fp, "    \"width\": %d,\n", cfg->map_zone_cfg.width);
    fprintf(fp, "    \"height\": %d,\n", cfg->map_zone_cfg.height);
    fprintf(fp, "    \"badge_offset_x\": %d,\n",
            cfg->map_zone_cfg.badge_offset_x);
    fprintf(fp, "    \"badge_offset_y\": %d,\n",
            cfg->map_zone_cfg.badge_offset_y);
    fprintf(fp, "    \"badge_width\": %d,\n", cfg->map_zone_cfg.badge_width);
    fprintf(fp, "    \"badge_height\": %d,\n", cfg->map_zone_cfg.badge_height);
    fprintf(fp, "    \"min_blue_ratio\": %.3f,\n",
            cfg->map_zone_cfg.min_blue_ratio);
    fprintf(fp, "    \"min_brown_ratio\": %.3f,\n",
            cfg->map_zone_cfg.min_brown_ratio);
    fprintf(fp, "    \"min_green_ratio\": %.3f,\n",
            cfg->map_zone_cfg.min_green_ratio);
    fprintf(fp, "    \"min_red_ratio\": %.3f,\n",
            cfg->map_zone_cfg.min_red_ratio);
    fprintf(fp, "    \"min_white_ratio\": %.3f,\n",
            cfg->map_zone_cfg.min_white_ratio);
    fprintf(fp, "    \"max_bg_brightness\": %.1f,\n",
            cfg->map_zone_cfg.max_bg_brightness);
    fprintf(fp, "    \"detect_player_indicator\": %s\n",
            cfg->map_zone_cfg.detect_player_indicator ? "true" : "false");
    if (cfg->count > 0 || cfg->hp_bar_cfg.width > 0) {
      fprintf(fp, "  },\n");
    } else {
      fprintf(fp, "  }\n");
    }
  }

  /* 3. 写入 hp_bar_config 血条参数节点 */
  if (cfg->hp_bar_cfg.width > 0 && cfg->hp_bar_cfg.height > 0) {
    fprintf(fp, "  \"hp_bar_config\": {\n");
    fprintf(fp, "    \"offset_x\": %d,\n", cfg->hp_bar_cfg.offset_x);
    fprintf(fp, "    \"offset_y\": %d,\n", cfg->hp_bar_cfg.offset_y);
    fprintf(fp, "    \"width\": %d,\n", cfg->hp_bar_cfg.width);
    fprintf(fp, "    \"height\": %d,\n", cfg->hp_bar_cfg.height);
    fprintf(fp, "    \"target_color_1\": [\n      %d,\n      %d,\n      %d\n    ],\n",
            cfg->hp_bar_cfg.target_color_1.r, cfg->hp_bar_cfg.target_color_1.g, cfg->hp_bar_cfg.target_color_1.b);
    fprintf(fp, "    \"tolerance_1\": [\n      %d,\n      %d,\n      %d\n    ],\n",
            cfg->hp_bar_cfg.tolerance_1.r, cfg->hp_bar_cfg.tolerance_1.g, cfg->hp_bar_cfg.tolerance_1.b);
    fprintf(fp, "    \"target_color_2\": [\n      %d,\n      %d,\n      %d\n    ],\n",
            cfg->hp_bar_cfg.target_color_2.r, cfg->hp_bar_cfg.target_color_2.g, cfg->hp_bar_cfg.target_color_2.b);
    fprintf(fp, "    \"tolerance_2\": [\n      %d,\n      %d,\n      %d\n    ]\n",
            cfg->hp_bar_cfg.tolerance_2.r, cfg->hp_bar_cfg.tolerance_2.g, cfg->hp_bar_cfg.tolerance_2.b);
    if (cfg->count > 0) {
      fprintf(fp, "  },\n");
    } else {
      fprintf(fp, "  }\n");
    }
  }

  /* 4. 写入常规 CBT 特征采样点 */
  for (int i = 0; i < cfg->count; i++) {
    const L2MCbtPoint *pt = &cfg->points[i];
    fprintf(fp, "  \"%s\": {\n", pt->key);
    fprintf(fp, "    \"pos\": [\n      %d,\n      %d\n    ],\n", pt->x, pt->y);

    if (pt->has_rgb) {
      fprintf(fp, "    \"rgb\": [\n      %d,\n      %d,\n      %d\n    ]",
              pt->r, pt->g, pt->b);
    } else {
      fprintf(fp, "    \"rgb\": null");
    }

    if (pt->tolerance != 12 && pt->tolerance > 0) {
      fprintf(fp, ",\n    \"tolerance\": %d\n", pt->tolerance);
    } else {
      fprintf(fp, "\n");
    }

    if (i < cfg->count - 1) {
      fprintf(fp, "  },\n");
    } else {
      fprintf(fp, "  }\n");
    }
  }
  fprintf(fp, "}\n");
  fclose(fp);
  return true;
}

bool l2m_cbt_get_hp_config(const L2MCbtConfig *cfg, L2MHpConfig *out_cfg) {
  if (!cfg || !out_cfg)
    return false;
  *out_cfg = cfg->hp_bar_cfg;
  return true;
}

bool l2m_cbt_set_hp_config(L2MCbtConfig *cfg, const L2MHpConfig *hp_cfg) {
  if (!cfg || !hp_cfg)
    return false;
  cfg->hp_bar_cfg = *hp_cfg;
  return true;
}

bool l2m_cbt_get_popup_item(const L2MCbtConfig *cfg, const char *name,
                            L2MPopupItem *out_item) {
  if (!cfg || !name || !out_item)
    return false;
  for (int i = 0; i < cfg->popup_cfg.count; i++) {
    if (strcmp(cfg->popup_cfg.items[i].name, name) == 0) {
      *out_item = cfg->popup_cfg.items[i];
      return true;
    }
  }
  return false;
}

bool l2m_cbt_set_popup_item(L2MCbtConfig *cfg, const L2MPopupItem *item) {
  if (!cfg || !item || strlen(item->name) == 0)
    return false;

  for (int i = 0; i < cfg->popup_cfg.count; i++) {
    if (strcmp(cfg->popup_cfg.items[i].name, item->name) == 0) {
      cfg->popup_cfg.items[i] = *item;
      if (strcmp(item->name, "top_left") == 0 ||
          strcmp(item->name, "top_left_tip") == 0)
        cfg->popup_cfg.top_left = *item;
      if (strcmp(item->name, "center") == 0 ||
          strcmp(item->name, "center_modal") == 0)
        cfg->popup_cfg.center = *item;
      if (strcmp(item->name, "fullscreen") == 0 ||
          strcmp(item->name, "fullscreen_event") == 0)
        cfg->popup_cfg.fullscreen = *item;
      return true;
    }
  }

  if (cfg->popup_cfg.count < MAX_POPUP_ITEMS) {
    cfg->popup_cfg.items[cfg->popup_cfg.count++] = *item;
    if (strcmp(item->name, "top_left") == 0 ||
        strcmp(item->name, "top_left_tip") == 0)
      cfg->popup_cfg.top_left = *item;
    if (strcmp(item->name, "center") == 0 ||
        strcmp(item->name, "center_modal") == 0)
      cfg->popup_cfg.center = *item;
    if (strcmp(item->name, "fullscreen") == 0 ||
        strcmp(item->name, "fullscreen_event") == 0)
      cfg->popup_cfg.fullscreen = *item;
    return true;
  }
  return false;
}

bool l2m_cbt_delete_popup_item(L2MCbtConfig *cfg, const char *name) {
  if (!cfg || !name)
    return false;
  for (int i = 0; i < cfg->popup_cfg.count; i++) {
    if (strcmp(cfg->popup_cfg.items[i].name, name) == 0) {
      for (int j = i; j < cfg->popup_cfg.count - 1; j++) {
        cfg->popup_cfg.items[j] = cfg->popup_cfg.items[j + 1];
      }
      cfg->popup_cfg.count--;
      return true;
    }
  }
  return false;
}

int32_t l2m_cbt_get_popup_count(const L2MCbtConfig *cfg) {
  return cfg ? cfg->popup_cfg.count : 0;
}

bool l2m_cbt_get_popup_by_index(const L2MCbtConfig *cfg, int32_t index,
                                L2MPopupItem *out_item) {
  if (!cfg || !out_item || index < 0 || index >= cfg->popup_cfg.count)
    return false;
  *out_item = cfg->popup_cfg.items[index];
  return true;
}

bool l2m_cbt_get_popup_roi(const L2MCbtConfig *cfg, L2MPopupType ptype,
                           L2MRect *out_roi) {
  if (!cfg || !out_roi)
    return false;
  const L2MPopupItem *p =
      (ptype == L2M_POPUP_TOP_LEFT)
          ? &cfg->popup_cfg.top_left
          : ((ptype == L2M_POPUP_CENTER) ? &cfg->popup_cfg.center
                                         : &cfg->popup_cfg.fullscreen);
  out_roi->x = p->x;
  out_roi->y = p->y;
  out_roi->width = p->width;
  out_roi->height = p->height;
  return true;
}

bool l2m_cbt_set_popup_roi(L2MCbtConfig *cfg, L2MPopupType ptype,
                           const L2MRect *roi) {
  if (!cfg || !roi)
    return false;
  L2MPopupItem *p =
      (ptype == L2M_POPUP_TOP_LEFT)
          ? &cfg->popup_cfg.top_left
          : ((ptype == L2M_POPUP_CENTER) ? &cfg->popup_cfg.center
                                         : &cfg->popup_cfg.fullscreen);
  p->x = roi->x;
  p->y = roi->y;
  p->width = roi->width;
  p->height = roi->height;
  return true;
}

bool l2m_cbt_get_popup_param(const L2MCbtConfig *cfg, L2MPopupType ptype,
                             L2MPopupTypeParam *out_param) {
  if (!cfg || !out_param)
    return false;
  if (ptype == L2M_POPUP_TOP_LEFT)
    *out_param = cfg->popup_cfg.top_left;
  else if (ptype == L2M_POPUP_CENTER)
    *out_param = cfg->popup_cfg.center;
  else
    *out_param = cfg->popup_cfg.fullscreen;
  return true;
}

bool l2m_cbt_set_popup_param(L2MCbtConfig *cfg, L2MPopupType ptype,
                             const L2MPopupTypeParam *param) {
  if (!cfg || !param)
    return false;
  if (ptype == L2M_POPUP_TOP_LEFT)
    cfg->popup_cfg.top_left = *param;
  else if (ptype == L2M_POPUP_CENTER)
    cfg->popup_cfg.center = *param;
  else
    cfg->popup_cfg.fullscreen = *param;
  return true;
}

bool l2m_cbt_get_point(const L2MCbtConfig *cfg, const char *key,
                       L2MCbtPoint *out_pt) {
  if (!cfg || !key || !out_pt)
    return false;
  for (int i = 0; i < cfg->count; i++) {
    if (strcmp(cfg->points[i].key, key) == 0) {
      *out_pt = cfg->points[i];
      return true;
    }
  }
  return false;
}

bool l2m_cbt_set_point(L2MCbtConfig *cfg, const L2MCbtPoint *pt) {
  if (!cfg || !pt || strlen(pt->key) == 0)
    return false;

  for (int i = 0; i < cfg->count; i++) {
    if (strcmp(cfg->points[i].key, pt->key) == 0) {
      cfg->points[i] = *pt;
      return true;
    }
  }

  if (cfg->count < MAX_CBT_POINTS) {
    cfg->points[cfg->count++] = *pt;
    return true;
  }
  return false;
}

bool l2m_cbt_delete_point(L2MCbtConfig *cfg, const char *key) {
  if (!cfg || !key)
    return false;
  for (int i = 0; i < cfg->count; i++) {
    if (strcmp(cfg->points[i].key, key) == 0) {
      for (int j = i; j < cfg->count - 1; j++) {
        cfg->points[j] = cfg->points[j + 1];
      }
      cfg->count--;
      return true;
    }
  }
  return false;
}

bool l2m_cbt_test_pixel_match(const L2MImageBuffer *img_rgb,
                              const L2MCbtPoint *pt, L2MRGB *out_actual_rgb,
                              int *out_color_diff, bool *out_is_match) {
  if (!img_rgb || !pt || !out_actual_rgb || !out_color_diff || !out_is_match)
    return false;

  int x = pt->x;
  int y = pt->y;

  if (x < 0 || x >= img_rgb->width || y < 0 || y >= img_rgb->height) {
    *out_is_match = false;
    *out_color_diff = 999;
    return false;
  }

  const uint8_t *row = img_rgb->data + y * img_rgb->stride;
  out_actual_rgb->r = row[x * 3 + 0];
  out_actual_rgb->g = row[x * 3 + 1];
  out_actual_rgb->b = row[x * 3 + 2];

  if (!pt->has_rgb) {
    *out_color_diff = 0;
    *out_is_match = true;
    return true;
  }

  int dr = abs((int)out_actual_rgb->r - (int)pt->r);
  int dg = abs((int)out_actual_rgb->g - (int)pt->g);
  int db = abs((int)out_actual_rgb->b - (int)pt->b);

  int max_diff = dr > dg ? dr : dg;
  if (db > max_diff)
    max_diff = db;

  *out_color_diff = max_diff;
  int tol = pt->tolerance > 0 ? pt->tolerance : 12;
  *out_is_match = (max_diff <= tol);

  return true;
}

bool l2m_cbt_get_map_zone_config(const L2MCbtConfig *cfg,
                                 L2MMapZoneConfig *out_cfg) {
  if (!cfg || !out_cfg)
    return false;
  *out_cfg = cfg->map_zone_cfg;
  return true;
}

bool l2m_cbt_set_map_zone_config(L2MCbtConfig *cfg,
                                 const L2MMapZoneConfig *map_cfg) {
  if (!cfg || !map_cfg)
    return false;
  cfg->map_zone_cfg = *map_cfg;
  cfg->map_box_roi =
      (L2MRect){map_cfg->x, map_cfg->y, map_cfg->width, map_cfg->height};
  return true;
}

bool l2m_cbt_get_map_box_roi(const L2MCbtConfig *cfg, L2MRect *out_roi) {
  if (!cfg || !out_roi)
    return false;
  if (cfg->map_zone_cfg.width > 0 && cfg->map_zone_cfg.height > 0) {
    *out_roi = (L2MRect){cfg->map_zone_cfg.x, cfg->map_zone_cfg.y,
                         cfg->map_zone_cfg.width, cfg->map_zone_cfg.height};
  } else if (cfg->map_box_roi.width > 0 && cfg->map_box_roi.height > 0) {
    *out_roi = cfg->map_box_roi;
  } else {
    *out_roi = (L2MRect){10, 10, 135, 95};
  }
  return true;
}

bool l2m_cbt_set_map_box_roi(L2MCbtConfig *cfg, const L2MRect *roi) {
  if (!cfg || !roi)
    return false;
  cfg->map_box_roi = *roi;
  cfg->map_zone_cfg.x = roi->x;
  cfg->map_zone_cfg.y = roi->y;
  cfg->map_zone_cfg.width = roi->width;
  cfg->map_zone_cfg.height = roi->height;
  return true;
}
