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
#include <windows.h>
#endif

/* 寻找项目中的 data/cbt/<REGION>.json 或 bot/data/cbt/<REGION>.json 路径 */
static bool get_cbt_file_path(const char *region, char *out_path,
                              size_t max_len) {
  const char *candidates[] = {
      "data/cbt/%s.json",
      "../data/cbt/%s.json",
      "bot/data/cbt/%s.json",
      "../bot/data/cbt/%s.json"
  };
  int num_candidates = (int)(sizeof(candidates) / sizeof(candidates[0]));

  for (int i = 0; i < num_candidates; i++) {
    char temp[260];
    snprintf(temp, sizeof(temp), candidates[i], region);
    FILE *fp = fopen(temp, "r");
    if (fp) {
      fclose(fp);
      strncpy(out_path, temp, max_len);
      return true;
    }
  }

  /* 默认路径 */
  snprintf(out_path, max_len, "data/cbt/%s.json", region);
  return true;
}

/* 提取 JSON 对象中的矩形字段 x, y, width, height */
static bool parse_json_rect(const char *json_obj_str, L2MRect *out_rect) {
  if (!json_obj_str || !out_rect) return false;

  char *px = strstr(json_obj_str, "\"x\"");
  char *py = strstr(json_obj_str, "\"y\"");
  char *pw = strstr(json_obj_str, "\"width\"");
  char *ph = strstr(json_obj_str, "\"height\"");

  if (px && py && pw && ph) {
    char *cx = strchr(px, ':');
    char *cy = strchr(py, ':');
    char *cw = strchr(pw, ':');
    char *ch = strchr(ph, ':');
    if (cx && cy && cw && ch) {
      out_rect->x = atoi(cx + 1);
      out_rect->y = atoi(cy + 1);
      out_rect->width = atoi(cw + 1);
      out_rect->height = atoi(ch + 1);
      return true;
    }
  }
  return false;
}

/* 查找与开括号匹配的闭大括号，支持任意层级嵌套和字符串转义 */
static char* find_matching_brace(char *start) {
  if (!start || *start != '{') return NULL;
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
        if (depth == 0) return p;
      }
    }
    p++;
  }
  return NULL;
}

bool l2m_cbt_load(const char *region, L2MCbtConfig *cfg) {
  if (!region || !cfg)
    return false;

  memset(cfg, 0, sizeof(L2MCbtConfig));
  strncpy(cfg->region, region, sizeof(cfg->region) - 1);
  get_cbt_file_path(region, cfg->file_path, sizeof(cfg->file_path));

  /* 设置弹窗默认参数 */
  cfg->popup_cfg.top_left = (L2MRect){10, 10, 260, 150};
  cfg->popup_cfg.center = (L2MRect){280, 150, 400, 240};
  cfg->popup_cfg.fullscreen = (L2MRect){0, 0, 960, 540};

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

  /* 检查并解析 "popup_scan_config" */
  char *pop_tag = strstr(content, "\"popup_scan_config\"");
  if (pop_tag) {
    char *tl_tag = strstr(pop_tag, "\"top_left\"");
    if (tl_tag) parse_json_rect(tl_tag, &cfg->popup_cfg.top_left);

    char *ct_tag = strstr(pop_tag, "\"center\"");
    if (ct_tag) parse_json_rect(ct_tag, &cfg->popup_cfg.center);

    char *fs_tag = strstr(pop_tag, "\"fullscreen\"");
    if (fs_tag) parse_json_rect(fs_tag, &cfg->popup_cfg.fullscreen);
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
        strcmp(key_name, "hp_bar_config") == 0 ||
        strcmp(key_name, "window_frame_config") == 0 ||
        strcmp(key_name, "top_left") == 0 ||
        strcmp(key_name, "center") == 0 ||
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

bool l2m_cbt_save(const L2MCbtConfig *cfg) {
  if (!cfg)
    return false;

  FILE *fp = fopen(cfg->file_path, "wb");
  if (!fp)
    return false;

  fprintf(fp, "{\n");

  /* 1. 写入 popup_scan_config 节点 */
  fprintf(fp, "  \"popup_scan_config\": {\n");
  fprintf(fp, "    \"top_left\": {\n      \"x\": %d,\n      \"y\": %d,\n      \"width\": %d,\n      \"height\": %d\n    },\n",
          cfg->popup_cfg.top_left.x, cfg->popup_cfg.top_left.y, cfg->popup_cfg.top_left.width, cfg->popup_cfg.top_left.height);
  fprintf(fp, "    \"center\": {\n      \"x\": %d,\n      \"y\": %d,\n      \"width\": %d,\n      \"height\": %d\n    },\n",
          cfg->popup_cfg.center.x, cfg->popup_cfg.center.y, cfg->popup_cfg.center.width, cfg->popup_cfg.center.height);
  fprintf(fp, "    \"fullscreen\": {\n      \"x\": %d,\n      \"y\": %d,\n      \"width\": %d,\n      \"height\": %d\n    }\n",
          cfg->popup_cfg.fullscreen.x, cfg->popup_cfg.fullscreen.y, cfg->popup_cfg.fullscreen.width, cfg->popup_cfg.fullscreen.height);

  if (cfg->count > 0) {
    fprintf(fp, "  },\n");
  } else {
    fprintf(fp, "  }\n");
  }

  /* 2. 写入常规 CBT 特征采样点 */
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

bool l2m_cbt_get_popup_roi(const L2MCbtConfig* cfg, L2MPopupType ptype, L2MRect* out_roi) {
  if (!cfg || !out_roi) return false;
  if (ptype == L2M_POPUP_TOP_LEFT) *out_roi = cfg->popup_cfg.top_left;
  else if (ptype == L2M_POPUP_CENTER) *out_roi = cfg->popup_cfg.center;
  else *out_roi = cfg->popup_cfg.fullscreen;
  return true;
}

bool l2m_cbt_set_popup_roi(L2MCbtConfig* cfg, L2MPopupType ptype, const L2MRect* roi) {
  if (!cfg || !roi) return false;
  if (ptype == L2M_POPUP_TOP_LEFT) cfg->popup_cfg.top_left = *roi;
  else if (ptype == L2M_POPUP_CENTER) cfg->popup_cfg.center = *roi;
  else cfg->popup_cfg.fullscreen = *roi;
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
