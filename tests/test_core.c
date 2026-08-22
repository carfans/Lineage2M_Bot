/**
 * @file test_core.c
 * @brief Lineage2MBot C 核心算法与数据解析单元测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/l2m_types.h"
#include "../include/l2m_vision.h"
#include "../include/l2m_hp.h"
#include "../include/l2m_popup.h"
#include "../include/l2m_cbt.h"
#include "../include/l2m_api.h"
#include "../include/l2m_window_profile.h"

static int g_tests_passed = 0;
static int g_tests_total = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_total++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (Line %d: %s)\n", msg, __LINE__, #expr); \
    } \
} while(0)

/* 1. 测试 CBT 配置文件加载与解析 */
static void test_cbt_manager(void) {
    printf("[*] Testing CBT Manager JSON Parsing...\n");

    L2MCbtConfig cfg;
    bool ok = l2m_cbt_load("CN", &cfg);
    TEST_ASSERT(ok, "l2m_cbt_load('CN') should succeed");
    TEST_ASSERT(cfg.count > 0, "CBT points count should be > 0");

    /* 验证动态命名弹窗配置 (Named Popup Manager) 解析精度 */
    TEST_ASSERT(cfg.popup_cfg.count >= 3, "Named popup count should be >= 3");
    TEST_ASSERT(cfg.popup_cfg.top_left.x == 25 && cfg.popup_cfg.top_left.y == 54,
                "popup_scan_config.top_left parsed correctly (x=25, y=54)");
    TEST_ASSERT(cfg.popup_cfg.center.x == 280 && cfg.popup_cfg.center.width == 400,
                "popup_scan_config.center parsed correctly");
    TEST_ASSERT(cfg.popup_cfg.fullscreen.width == 960 && cfg.popup_cfg.fullscreen.height == 540,
                "popup_scan_config.fullscreen parsed correctly");

    /* 验证命名弹窗查找与特征参数 (包含 linked_cbt_key) */
    L2MPopupItem tl_item, ct_item;
    bool has_tl = l2m_cbt_get_popup_item(&cfg, "top_left_tip", &tl_item);
    bool has_ct = l2m_cbt_get_popup_item(&cfg, "center_modal", &ct_item);
    TEST_ASSERT(has_tl && tl_item.has_checkbox, "Named popup 'top_left_tip' retrieved with checkbox enabled");
    TEST_ASSERT(has_tl && strcmp(tl_item.linked_cbt_key, "home_scroll_button_no_energomode") == 0,
                "Named popup 'top_left_tip' links to 'home_scroll_button_no_energomode'");
    TEST_ASSERT(has_ct && ct_item.btn_target_rgb.r == 215, "Named popup 'center_modal' retrieved with orange rgb");

    /* 验证命名弹窗动态添加、修改名称与删除 (CRUD) */
    L2MPopupItem custom_item;
    memset(&custom_item, 0, sizeof(custom_item));
    snprintf(custom_item.name, sizeof(custom_item.name), "resurrect_confirm");
    snprintf(custom_item.desc, sizeof(custom_item.desc), "死亡复活确认弹窗");
    snprintf(custom_item.linked_cbt_key, sizeof(custom_item.linked_cbt_key), "inventory_slot1_empty_1");
    custom_item.enabled = true;
    custom_item.x = 300; custom_item.y = 200; custom_item.width = 360; custom_item.height = 180;
    custom_item.btn_ideal_aspect = 3.2f;

    int prev_popup_cnt = l2m_cbt_get_popup_count(&cfg);
    bool add_pop_ok = l2m_cbt_set_popup_item(&cfg, &custom_item);
    TEST_ASSERT(add_pop_ok && l2m_cbt_get_popup_count(&cfg) == prev_popup_cnt + 1,
                "l2m_cbt_set_popup_item successfully added new named popup profile");

    L2MPopupItem query_item;
    bool query_pop_ok = l2m_cbt_get_popup_item(&cfg, "resurrect_confirm", &query_item);
    TEST_ASSERT(query_pop_ok && query_item.width == 360 && strcmp(query_item.linked_cbt_key, "inventory_slot1_empty_1") == 0,
                "l2m_cbt_get_popup_item retrieved newly added named popup with linked cbt");

    /* 验证修改弹窗配置与重命名支持 */
    query_item.x = 320;
    snprintf(query_item.desc, sizeof(query_item.desc), "死亡复活确认弹窗(已修改描述)");
    bool update_pop_ok = l2m_cbt_set_popup_item(&cfg, &query_item);
    TEST_ASSERT(update_pop_ok, "l2m_cbt_set_popup_item updated existing popup item");

    L2MPopupItem updated_item;
    l2m_cbt_get_popup_item(&cfg, "resurrect_confirm", &updated_item);
    TEST_ASSERT(updated_item.x == 320 && strcmp(updated_item.desc, "死亡复活确认弹窗(已修改描述)") == 0,
                "Updated popup properties verified");

    bool del_pop_ok = l2m_cbt_delete_popup_item(&cfg, "resurrect_confirm");
    TEST_ASSERT(del_pop_ok && l2m_cbt_get_popup_count(&cfg) == prev_popup_cnt,
                "l2m_cbt_delete_popup_item successfully deleted named popup profile");

    /* 验证地图区域 map_box_config 完整参数解析与存取 */
    L2MMapZoneConfig map_cfg;
    bool has_map_cfg = l2m_cbt_get_map_zone_config(&cfg, &map_cfg);
    TEST_ASSERT(has_map_cfg, "map_box_config parsed successfully");
    TEST_ASSERT(map_cfg.x == 25 && map_cfg.y == 59 && map_cfg.width == 144 && map_cfg.height == 94,
                "map_box_config ROI (25, 59, 144, 94) parsed correctly");
    TEST_ASSERT(map_cfg.badge_offset_x == 2 && map_cfg.badge_width == 50,
                "map_box_config badge sub-ROI (2, 2, 50, 20) parsed correctly");
    TEST_ASSERT(map_cfg.min_blue_ratio >= 0.010f && map_cfg.max_bg_brightness >= 120.0f,
                "map_box_config threshold parameters parsed correctly");

    /* 验证修改与设置地图区域配置 */
    map_cfg.x = 12; map_cfg.y = 14;
    map_cfg.min_green_ratio = 0.018f;
    bool set_map_ok = l2m_cbt_set_map_zone_config(&cfg, &map_cfg);
    TEST_ASSERT(set_map_ok && cfg.map_zone_cfg.x == 12 && cfg.map_box_roi.x == 12,
                "l2m_cbt_set_map_zone_config synchronized map_zone_cfg and map_box_roi");

    /* 验证血条 hp_bar_config 完整参数解析与存取 */
    L2MHpConfig hp_bar_cfg;
    bool has_hp_bar = l2m_cbt_get_hp_config(&cfg, &hp_bar_cfg);
    TEST_ASSERT(has_hp_bar && hp_bar_cfg.width == 103 && hp_bar_cfg.height == 2,
                "hp_bar_config parsed successfully (w=103, h=2)");
    TEST_ASSERT(hp_bar_cfg.offset_x == 64 && hp_bar_cfg.offset_y == 21,
                "hp_bar_config offset is (64, 21)");
    TEST_ASSERT(hp_bar_cfg.target_color_1.r == 230 && hp_bar_cfg.target_color_1.g == 48,
                "hp_bar_config target_color_1 is RGB(230, 48, 48)");

    /* 验证修改与设置血条配置 */
    hp_bar_cfg.offset_x = 65;
    hp_bar_cfg.target_color_1 = (L2MRGB){220, 50, 50};
    bool set_hp_ok = l2m_cbt_set_hp_config(&cfg, &hp_bar_cfg);
    TEST_ASSERT(set_hp_ok && cfg.hp_bar_cfg.offset_x == 65 && cfg.hp_bar_cfg.target_color_1.r == 220,
                "l2m_cbt_set_hp_config updated hp_bar_cfg correctly");

    /* 验证没有将 center, fullscreen, map_box_config 和 hp_bar_config 误解析为普通 CBT 点位 */
    L2MCbtPoint pt_center, pt_fullscreen, pt_map, pt_hp;
    bool has_bad_center = l2m_cbt_get_point(&cfg, "center", &pt_center);
    bool has_bad_fullscreen = l2m_cbt_get_point(&cfg, "fullscreen", &pt_fullscreen);
    bool has_bad_map = l2m_cbt_get_point(&cfg, "map_box_config", &pt_map);
    bool has_bad_hp = l2m_cbt_get_point(&cfg, "hp_bar_config", &pt_hp);
    TEST_ASSERT(!has_bad_center, "Key 'center' from popup config must NOT be parsed as CBT point");
    TEST_ASSERT(!has_bad_fullscreen, "Key 'fullscreen' from popup config must NOT be parsed as CBT point");
    TEST_ASSERT(!has_bad_map, "Key 'map_box_config' must NOT be parsed as CBT point");
    TEST_ASSERT(!has_bad_hp, "Key 'hp_bar_config' must NOT be parsed as CBT point");

    /* 验证具体点位解析 */
    L2MCbtPoint pt_slot1;
    bool has_slot1 = l2m_cbt_get_point(&cfg, "inventory_slot1_empty_1", &pt_slot1);
    TEST_ASSERT(has_slot1, "Point 'inventory_slot1_empty_1' should exist");
    if (has_slot1) {
        TEST_ASSERT(pt_slot1.x == 305 && pt_slot1.y == 62, "inventory_slot1 pos is (305, 62)");
        TEST_ASSERT(pt_slot1.has_rgb && pt_slot1.r == 42 && pt_slot1.g == 44 && pt_slot1.b == 47,
                    "inventory_slot1 rgb is (42, 44, 47)");
    }

    /* 验证增删改查 */
    L2MCbtPoint test_pt;
    memset(&test_pt, 0, sizeof(test_pt));
    strncpy(test_pt.key, "unit_test_dummy_key", sizeof(test_pt.key) - 1);
    test_pt.x = 123;
    test_pt.y = 456;
    test_pt.has_rgb = true;
    test_pt.r = 10; test_pt.g = 20; test_pt.b = 30;
    test_pt.tolerance = 15;

    int prev_count = cfg.count;
    bool set_ok = l2m_cbt_set_point(&cfg, &test_pt);
    TEST_ASSERT(set_ok && cfg.count == prev_count + 1, "l2m_cbt_set_point should add new point");

    L2MCbtPoint get_pt;
    bool get_ok = l2m_cbt_get_point(&cfg, "unit_test_dummy_key", &get_pt);
    TEST_ASSERT(get_ok && get_pt.x == 123 && get_pt.y == 456 && get_pt.r == 10,
                "l2m_cbt_get_point retrieved newly added point");

    bool del_ok = l2m_cbt_delete_point(&cfg, "unit_test_dummy_key");
    TEST_ASSERT(del_ok && cfg.count == prev_count, "l2m_cbt_delete_point should remove point");
}

/* 2. 测试图像创建、裁剪与格式转换安全性 */
static void test_image_buffer(void) {
    printf("[*] Testing Image Buffer & Color Formats...\n");

    L2MImageBuffer* img = l2m_image_create(100, 100, L2M_FMT_BGR888);
    TEST_ASSERT(img != NULL, "l2m_image_create(100, 100, BGR) succeeded");
    TEST_ASSERT(img->stride >= 300, "Stride is 4-byte aligned");

    /* 填充特定像素 */
    uint8_t* row = img->data + 50 * img->stride;
    row[50 * 3 + 0] = 11; /* B */
    row[50 * 3 + 1] = 22; /* G */
    row[50 * 3 + 2] = 33; /* R */

    /* 测试格式转换且自动扩容校验 (目标初始尺寸较小) */
    L2MImageBuffer* dst_rgb = l2m_image_create(10, 10, L2M_FMT_RGB888);
    bool conv_ok = l2m_image_bgr_to_rgb(img, dst_rgb);
    TEST_ASSERT(conv_ok, "l2m_image_bgr_to_rgb with size reallocation succeeded");
    TEST_ASSERT(dst_rgb->width == 100 && dst_rgb->height == 100, "dst_rgb resized automatically");

    uint8_t* d_row = dst_rgb->data + 50 * dst_rgb->stride;
    TEST_ASSERT(d_row[50 * 3 + 0] == 33 && d_row[50 * 3 + 1] == 22 && d_row[50 * 3 + 2] == 11,
                "BGR to RGB swapped correctly");

    /* 测试裁剪 */
    L2MRect roi = {40, 40, 20, 20};
    L2MImageBuffer* crop = l2m_image_create(1, 1, L2M_FMT_RGB888);
    bool crop_ok = l2m_image_crop(dst_rgb, &roi, crop);
    TEST_ASSERT(crop_ok && crop->width == 20 && crop->height == 20, "l2m_image_crop succeeded");

    uint8_t* c_row = crop->data + 10 * crop->stride;
    TEST_ASSERT(c_row[10 * 3 + 0] == 33 && c_row[10 * 3 + 1] == 22 && c_row[10 * 3 + 2] == 11,
                "Cropped pixel value matches origin");

    l2m_image_free(crop);
    l2m_image_free(dst_rgb);
    l2m_image_free(img);
}

/* 3. 测试连通域分析与除零保护 */
static void test_contour_division_guard(void) {
    printf("[*] Testing Contour Extraction & Zero Division Guard...\n");

    L2MImageBuffer* bin = l2m_image_create(50, 50, L2M_FMT_BIN8);
    /* 绘制一个 10x10 的白色方块 (20, 20) 到 (29, 29) */
    for (int y = 20; y < 30; y++) {
        uint8_t* r = bin->data + y * bin->stride;
        for (int x = 20; x < 30; x++) {
            r[x] = 255;
        }
    }

    L2MContour contours[8];
    /* 传入 min_area = 0 测试除零安全防护 */
    int32_t cnt = l2m_find_contours(bin, contours, 8, 0, 10000);
    TEST_ASSERT(cnt == 1, "Found exactly 1 contour");
    if (cnt > 0) {
        TEST_ASSERT(contours[0].area == 100, "Contour area is 100");
        TEST_ASSERT(contours[0].bbox.x == 20 && contours[0].bbox.y == 20, "BBox pos is (20, 20)");
        TEST_ASSERT(contours[0].bbox.width == 10 && contours[0].bbox.height == 10, "BBox size is 10x10");
        TEST_ASSERT(contours[0].center.x == 24 && contours[0].center.y == 24, "Centroid is (24, 24)");
    }

    l2m_image_free(bin);
}

/* 4. 测试血条计算 */
static void test_hp_calculation(void) {
    printf("[*] Testing HP Calculation Engine...\n");

    L2MImageBuffer* hp_bar = l2m_image_create(100, 2, L2M_FMT_RGB888);
    /* 填充前 60 像素为红色目标色 (168, 69, 2)，后 40 像素为黑色 */
    for (int y = 0; y < 2; y++) {
        uint8_t* r = hp_bar->data + y * hp_bar->stride;
        for (int x = 0; x < 60; x++) {
            r[x * 3 + 0] = 168; r[x * 3 + 1] = 69; r[x * 3 + 2] = 2;
        }
        for (int x = 60; x < 100; x++) {
            r[x * 3 + 0] = 10; r[x * 3 + 1] = 10; r[x * 3 + 2] = 10;
        }
    }

    L2MHpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.target_color_1 = (L2MRGB){168, 69, 2};
    cfg.tolerance_1 = (L2MRGB){20, 20, 20};
    cfg.target_color_2 = (L2MRGB){255, 157, 57};
    cfg.tolerance_2 = (L2MRGB){10, 10, 10};

    L2MHpResult res;
    bool ok = l2m_calculate_hp(hp_bar, &cfg, &res);
    TEST_ASSERT(ok && res.is_valid, "l2m_calculate_hp succeeded");
    TEST_ASSERT(res.hp_percent == 60, "HP percentage is 60%");
    TEST_ASSERT(res.sample_hp_end == 59, "sample_hp_end is 59");

    /* 4.2 测试极速零拷贝 BGR 血量计算 (l2m_calculate_hp_bgr) */
    L2MImageBuffer* hp_bar_bgr = l2m_image_create(100, 2, L2M_FMT_BGR888);
    for (int y = 0; y < 2; y++) {
        uint8_t* r = hp_bar_bgr->data + y * hp_bar_bgr->stride;
        for (int x = 0; x < 75; x++) {
            /* BGR: B=2, G=69, R=168 */
            r[x * 3 + 0] = 2; r[x * 3 + 1] = 69; r[x * 3 + 2] = 168;
        }
        for (int x = 75; x < 100; x++) {
            r[x * 3 + 0] = 10; r[x * 3 + 1] = 10; r[x * 3 + 2] = 10;
        }
    }

    L2MHpResult res_bgr;
    bool ok_bgr = l2m_calculate_hp_bgr(hp_bar_bgr, &cfg, &res_bgr);
    TEST_ASSERT(ok_bgr && res_bgr.is_valid, "l2m_calculate_hp_bgr succeeded (zero-copy direct BGR)");
    TEST_ASSERT(res_bgr.hp_percent == 75, "HP percentage via BGR is 75%");
    TEST_ASSERT(res_bgr.sample_hp_end == 74, "sample_hp_end via BGR is 74");

    /* 4.3 测试配置宽度为 97px 时的纯配置驱动计算 (满血 97/97 px 对应 100%) */
    L2MImageBuffer* custom_hp_bar = l2m_image_create(97, 2, L2M_FMT_RGB888);
    for (int y = 0; y < 2; y++) {
        uint8_t* r = custom_hp_bar->data + y * custom_hp_bar->stride;
        for (int x = 0; x < 97; x++) {
            r[x * 3 + 0] = 230; r[x * 3 + 1] = 48; r[x * 3 + 2] = 48;
        }
    }
    L2MHpConfig cfg_cn;
    memset(&cfg_cn, 0, sizeof(cfg_cn));
    cfg_cn.width = 97;
    cfg_cn.height = 2;
    cfg_cn.target_color_1 = (L2MRGB){230, 48, 48};
    cfg_cn.tolerance_1 = (L2MRGB){25, 25, 25};

    L2MHpResult res_custom;
    bool ok_custom = l2m_calculate_hp(custom_hp_bar, &cfg_cn, &res_custom);
    TEST_ASSERT(ok_custom && res_custom.is_valid, "Pure config-driven HP calculation succeeded");
    TEST_ASSERT(res_custom.hp_percent == 100, "Config width 97px with 97 filled pixels correctly outputs 100%");

    /* 4.4 测试配置宽度为 100px 时填充 90px 严格输出 90% */
    L2MImageBuffer* bar_100 = l2m_image_create(100, 2, L2M_FMT_RGB888);
    for (int y = 0; y < 2; y++) {
        uint8_t* r = bar_100->data + y * bar_100->stride;
        for (int x = 0; x < 90; x++) {
            r[x * 3 + 0] = 230; r[x * 3 + 1] = 48; r[x * 3 + 2] = 48;
        }
        for (int x = 90; x < 100; x++) {
            r[x * 3 + 0] = 10; r[x * 3 + 1] = 10; r[x * 3 + 2] = 10;
        }
    }
    cfg_cn.width = 100;
    L2MHpResult res_drop;
    bool ok_drop = l2m_calculate_hp(bar_100, &cfg_cn, &res_drop);
    TEST_ASSERT(ok_drop && res_drop.is_valid, "HP drop calculation succeeded");
    TEST_ASSERT(res_drop.hp_percent == 90, "Config width 100px with 90px filled correctly outputs 90%");

    l2m_image_free(bar_100);
    l2m_image_free(custom_hp_bar);
    l2m_image_free(hp_bar_bgr);
    l2m_image_free(hp_bar);
}

/* 5. 测试强化后的弹窗背景色比对、按钮尺寸与颜色精确识别引擎 */
static void test_popup_recognition_engine(void) {
    printf("[*] Testing Enhanced Popup Recognition (Background Check + Button Size & Color)...\n");

    /* 5.1 测试背景色比对：暗底 vs 高亮 vs 高彩度草地 */
    L2MImageBuffer* dark_bg = l2m_image_create(100, 100, L2M_FMT_RGB888);
    for (int y = 0; y < 100; y++) {
        uint8_t* r = dark_bg->data + y * dark_bg->stride;
        for (int x = 0; x < 100; x++) {
            r[x * 3 + 0] = 30; r[x * 3 + 1] = 32; r[x * 3 + 2] = 35;
        }
    }
    L2MPopupBgInfo bg_info;
    bool bg_ok = l2m_check_popup_background(dark_bg, L2M_POPUP_CENTER, &bg_info);
    TEST_ASSERT(bg_ok && bg_info.is_valid, "Dark popup background passes validation");
    TEST_ASSERT(bg_info.dark_ratio >= 0.90f, "Dark ratio is > 90%");

    /* 高亮场景拦截 */
    L2MImageBuffer* bright_scene = l2m_image_create(100, 100, L2M_FMT_RGB888);
    for (int y = 0; y < 100; y++) {
        uint8_t* r = bright_scene->data + y * bright_scene->stride;
        for (int x = 0; x < 100; x++) {
            r[x * 3 + 0] = 200; r[x * 3 + 1] = 200; r[x * 3 + 2] = 200;
        }
    }
    bool bright_ok = l2m_check_popup_background(bright_scene, L2M_POPUP_CENTER, &bg_info);
    TEST_ASSERT(!bright_ok && !bg_info.is_valid, "Bright scene is blocked by background check");

    /* 高彩度草地场景拦截 (绿草 R=40, G=180, B=30) */
    L2MImageBuffer* grass_scene = l2m_image_create(100, 100, L2M_FMT_RGB888);
    for (int y = 0; y < 100; y++) {
        uint8_t* r = grass_scene->data + y * grass_scene->stride;
        for (int x = 0; x < 100; x++) {
            r[x * 3 + 0] = 40; r[x * 3 + 1] = 180; r[x * 3 + 2] = 30;
        }
    }
    bool grass_ok = l2m_check_popup_background(grass_scene, L2M_POPUP_CENTER, &bg_info);
    TEST_ASSERT(!grass_ok && !bg_info.is_valid, "High-chroma grass scene is blocked by chroma check");

    /* 5.2 测试按钮尺寸几何打分 */
    L2MRect valid_btn_bbox = {50, 60, 60, 24}; /* 60x24 按钮 (宽高比 2.5:1) */
    float size_score = 0.0f;
    bool size_ok = l2m_evaluate_button_size(&valid_btn_bbox, L2M_POPUP_TOP_LEFT, &size_score);
    TEST_ASSERT(size_ok && size_score >= 80.0f, "Standard button size (60x24) evaluated as valid with high score");

    L2MRect tiny_noise_bbox = {10, 10, 8, 5}; /* 8x5 极小噪点 */
    bool tiny_ok = l2m_evaluate_button_size(&tiny_noise_bbox, L2M_POPUP_TOP_LEFT, &size_score);
    TEST_ASSERT(!tiny_ok, "Tiny noise (8x5) is filtered by size check");

    L2MRect skinny_line_bbox = {10, 10, 120, 3}; /* 120x3 细长分割线 */
    bool skinny_ok = l2m_evaluate_button_size(&skinny_line_bbox, L2M_POPUP_TOP_LEFT, &size_score);
    TEST_ASSERT(!skinny_ok, "Skinny line (120x3) is filtered by aspect ratio check");

    /* 5.3 测试按钮内部颜色核验 */
    L2MImageBuffer* test_img = l2m_image_create(120, 120, L2M_FMT_RGB888);
    /* 填充暗底 */
    for (int y = 0; y < 120; y++) {
        uint8_t* r = test_img->data + y * test_img->stride;
        for (int x = 0; x < 120; x++) {
            r[x * 3 + 0] = 30; r[x * 3 + 1] = 30; r[x * 3 + 2] = 32;
        }
    }
    /* 在 (60, 70) 处绘制 50x22 的橙色确认按钮 (215, 105, 12) */
    for (int y = 70; y < 92; y++) {
        uint8_t* r = test_img->data + y * test_img->stride;
        for (int x = 60; x < 110; x++) {
            r[x * 3 + 0] = 215; r[x * 3 + 1] = 105; r[x * 3 + 2] = 12;
        }
    }

    L2MRect orange_roi = {60, 70, 50, 22};
    L2MRGB mean_c;
    float fill_r = 0.0f, color_sc = 0.0f;
    bool orange_ver_ok = l2m_verify_button_color(test_img, &orange_roi, true, &mean_c, &fill_r, &color_sc);
    TEST_ASSERT(orange_ver_ok && color_sc >= 80.0f, "Orange button color verification passed with high score");
    TEST_ASSERT(mean_c.r >= 200 && mean_c.g >= 95 && mean_c.b <= 20, "Orange mean RGB matches (215, 105, 12)");
    TEST_ASSERT(fill_r >= 0.90f, "Orange fill ratio is > 90%");

    /* 5.4 测试弹窗多维结构特征检测 (面板轮廓 + 标题栏 + 文本行投影 + 右上角叉号) */
    L2MImageBuffer* dialog_img = l2m_image_create(200, 150, L2M_FMT_RGB888);
    /* 填充全屏半透明暗幕底色 */
    for (int y = 0; y < 150; y++) {
        uint8_t* r = dialog_img->data + y * dialog_img->stride;
        for (int x = 0; x < 200; x++) {
            r[x * 3 + 0] = 20; r[x * 3 + 1] = 22; r[x * 3 + 2] = 25;
        }
    }
    /* 绘制弹窗暗底面板 (20, 15, 160, 120) */
    for (int y = 15; y < 135; y++) {
        uint8_t* r = dialog_img->data + y * dialog_img->stride;
        for (int x = 20; x < 180; x++) {
            r[x * 3 + 0] = 35; r[x * 3 + 1] = 38; r[x * 3 + 2] = 42;
        }
    }
    /* 绘制顶部标题文字栏 (高亮淡黄色, y: 25~32) */
    for (int y = 25; y < 33; y++) {
        uint8_t* r = dialog_img->data + y * dialog_img->stride;
        for (int x = 60; x < 140; x++) {
            r[x * 3 + 0] = 210; r[x * 3 + 1] = 185; r[x * 3 + 2] = 130;
        }
    }
    /* 绘制两行说明文本行 (y: 50~54, y: 65~69) */
    for (int y = 50; y < 55; y++) {
        uint8_t* r = dialog_img->data + y * dialog_img->stride;
        for (int x = 40; x < 160; x++) {
            r[x * 3 + 0] = 190; r[x * 3 + 1] = 190; r[x * 3 + 2] = 195;
        }
    }
    for (int y = 65; y < 70; y++) {
        uint8_t* r = dialog_img->data + y * dialog_img->stride;
        for (int x = 40; x < 150; x++) {
            r[x * 3 + 0] = 185; r[x * 3 + 1] = 185; r[x * 3 + 2] = 190;
        }
    }
    /* 绘制右上角关闭叉号 (x: 162~174, y: 20~32) */
    for (int y = 20; y < 32; y++) {
        uint8_t* r = dialog_img->data + y * dialog_img->stride;
        for (int x = 162; x < 174; x++) {
            r[x * 3 + 0] = 200; r[x * 3 + 1] = 200; r[x * 3 + 2] = 200;
        }
    }
    /* 绘制底部橙色确认按钮 (70, 95, 60, 26) */
    for (int y = 95; y < 121; y++) {
        uint8_t* r = dialog_img->data + y * dialog_img->stride;
        for (int x = 70; x < 130; x++) {
            r[x * 3 + 0] = 215; r[x * 3 + 1] = 105; r[x * 3 + 2] = 12;
        }
    }

    L2MPopupFeatureInfo feat_info;
    bool feat_ok = l2m_detect_popup_features(dialog_img, 0, 0, L2M_POPUP_CENTER, &feat_info);
    TEST_ASSERT(feat_ok, "l2m_detect_popup_features returned true");
    TEST_ASSERT(feat_info.has_panel, "Popup panel rectangle detected");
    TEST_ASSERT(feat_info.has_title_bar, "Popup title bar detected");
    TEST_ASSERT(feat_info.has_content_text && feat_info.text_line_count >= 1, "Popup text line projection detected lines");
    TEST_ASSERT(feat_info.has_close_cross, "Popup close cross (X) detected");
    TEST_ASSERT(feat_info.feature_score >= 60.0f, "Popup feature score is >= 60");

    /* 5.5 综合弹窗识别测试 (背景色先验 + 弹窗结构特征 + 按钮尺寸与颜色精确识别) */
    L2MPopupResult popup_res;
    bool detect_ok = l2m_detect_popup(dialog_img, 0, 0, L2M_POPUP_CENTER, true, &popup_res);
    TEST_ASSERT(detect_ok && popup_res.detected, "l2m_detect_popup successfully recognized modal popup");
    TEST_ASSERT(strcmp(popup_res.popup_name, "center_modal") == 0, "Recognized popup_name is 'center_modal'");
    TEST_ASSERT(popup_res.button_pos.x >= 90 && popup_res.button_pos.x <= 110, "Button center X around 100");
    TEST_ASSERT(popup_res.button_pos.y >= 100 && popup_res.button_pos.y <= 115, "Button center Y around 108");
    TEST_ASSERT(popup_res.button_bbox.width >= 55 && popup_res.button_bbox.height >= 22, "Button width/height match");
    TEST_ASSERT(popup_res.feature_info.has_panel && popup_res.feature_info.feature_score > 0, "Feature info integrated into result");
    TEST_ASSERT(popup_res.score >= 80.0f, "Integrated confidence score is >= 80");

    /* 5.6 基于命名项的直接检测测试 (l2m_detect_popup_by_item) */
    L2MPopupItem modal_item;
    memset(&modal_item, 0, sizeof(modal_item));
    strncpy(modal_item.name, "dungeon_enter_dialog", sizeof(modal_item.name) - 1);
    modal_item.enabled = true;
    modal_item.btn_min_w = 40; modal_item.btn_max_w = 220;
    modal_item.btn_min_h = 18; modal_item.btn_max_h = 65;
    modal_item.btn_ideal_aspect = 3.0f;
    modal_item.check_panel = true;
    modal_item.check_title = true;

    L2MPopupResult by_item_res;
    bool by_item_ok = l2m_detect_popup_by_item(dialog_img, 0, 0, &modal_item, true, &by_item_res);
    TEST_ASSERT(by_item_ok && by_item_res.detected, "l2m_detect_popup_by_item successfully detected popup");
    TEST_ASSERT(strcmp(by_item_res.popup_name, "dungeon_enter_dialog") == 0, "by_item_res popup_name is 'dungeon_enter_dialog'");

    l2m_image_free(dark_bg);
    l2m_image_free(bright_scene);
    l2m_image_free(grass_scene);
    l2m_image_free(test_img);
    l2m_image_free(dialog_img);
}

/* 6. 测试左上角地图框识别与蓝色Safe/浅咖色Common/红网格/中心朝向判别引擎 */
static void test_map_zone_recognition(void) {
    printf("[*] Testing Minimap Frame with Blue Safe, Brown Common, Red Grid & Player Heading Angle...\n");

    /* 6.1 安全区域 (Safe Zone / 蓝色标识) 模拟画面 */
    L2MImageBuffer* safe_map = l2m_image_create(135, 95, L2M_FMT_RGB888);
    for (int y = 0; y < 95; y++) {
        uint8_t* r = safe_map->data + y * safe_map->stride;
        for (int x = 0; x < 135; x++) {
            /* 暗色雷达底板 */
            r[x * 3 + 0] = 30; r[x * 3 + 1] = 35; r[x * 3 + 2] = 40;
        }
    }
    /* 在左上角区域 (x: 4~40, y: 4~20) 绘制高亮蓝色 Safe 徽标 (40, 140, 240) */
    for (int y = 4; y < 20; y++) {
        uint8_t* r = safe_map->data + y * safe_map->stride;
        for (int x = 4; x < 40; x++) {
            r[x * 3 + 0] = 40; r[x * 3 + 1] = 140; r[x * 3 + 2] = 240;
        }
    }
    /* 在中心位置 (67, 47) 绘制白色玩家箭头与正上方扇形橙色视角 (230, 120, 20) */
    for (int y = 45; y <= 49; y++) {
        uint8_t* r = safe_map->data + y * safe_map->stride;
        for (int x = 65; x <= 69; x++) {
            r[x * 3 + 0] = 255; r[x * 3 + 1] = 255; r[x * 3 + 2] = 255;
        }
    }
    /* 视角向上方延伸 (y: 35~44, x: 62~72) */
    for (int y = 35; y < 45; y++) {
        uint8_t* r = safe_map->data + y * safe_map->stride;
        for (int x = 62; x <= 72; x++) {
            r[x * 3 + 0] = 230; r[x * 3 + 1] = 120; r[x * 3 + 2] = 20;
        }
    }

    L2MMapBoxResult safe_res;
    bool safe_ok = l2m_detect_map_box(safe_map, 10, 10, &safe_res);
    TEST_ASSERT(safe_ok && safe_res.detected, "l2m_detect_map_box successfully detected map frame");
    TEST_ASSERT(safe_res.zone_type == L2M_ZONE_SAFETY, "Map zone recognized as L2M_ZONE_SAFETY (Safe/Blue)");
    TEST_ASSERT(safe_res.blue_safe_ratio > 0.05f, "Blue safe ratio > 5%");
    TEST_ASSERT(safe_res.has_player_indicator, "Center player indicator detected");
    TEST_ASSERT(safe_res.player_center_pos.x == 77 && safe_res.player_center_pos.y == 57, "Player center pos matches (77, 57)");
    /* 朝向正上方 (正北 0° / 360° 附近，误差小于 20°) */
    TEST_ASSERT(safe_res.player_heading_angle <= 25.0f || safe_res.player_heading_angle >= 335.0f,
                "Player heading angle is facing North (~0 deg)");

    /* 6.2 普通区域 (Common Zone / 浅咖色标识) 模拟画面 */
    L2MImageBuffer* normal_map = l2m_image_create(135, 95, L2M_FMT_RGB888);
    for (int y = 0; y < 95; y++) {
        uint8_t* r = normal_map->data + y * normal_map->stride;
        for (int x = 0; x < 135; x++) {
            r[x * 3 + 0] = 32; r[x * 3 + 1] = 36; r[x * 3 + 2] = 42;
        }
    }
    /* 在左上角区域绘制浅咖色 Common 徽标 (160, 110, 60) */
    for (int y = 4; y < 20; y++) {
        uint8_t* r = normal_map->data + y * normal_map->stride;
        for (int x = 4; x < 40; x++) {
            r[x * 3 + 0] = 160; r[x * 3 + 1] = 110; r[x * 3 + 2] = 60;
        }
    }

    L2MMapBoxResult normal_res;
    bool normal_ok = l2m_detect_map_box(normal_map, 10, 10, &normal_res);
    TEST_ASSERT(normal_ok && normal_res.detected, "Normal map detected successfully");
    TEST_ASSERT(normal_res.zone_type == L2M_ZONE_NORMAL, "Map zone recognized as L2M_ZONE_NORMAL (Common/Brown)");
    TEST_ASSERT(normal_res.brown_common_ratio > 0.05f, "Brown common ratio > 5%");

    /* 6.3 不可记忆红色网格区域 (Combat / Red Grid) 模拟画面 */
    L2MImageBuffer* combat_map = l2m_image_create(135, 95, L2M_FMT_RGB888);
    for (int y = 0; y < 95; y++) {
        uint8_t* r = combat_map->data + y * combat_map->stride;
        for (int x = 0; x < 135; x++) {
            r[x * 3 + 0] = 30; r[x * 3 + 1] = 35; r[x * 3 + 2] = 40;
        }
    }
    /* 绘制红色战斗区角标 (210, 45, 45) */
    for (int y = 4; y < 20; y++) {
        uint8_t* r = combat_map->data + y * combat_map->stride;
        for (int x = 4; x < 40; x++) {
            r[x * 3 + 0] = 210; r[x * 3 + 1] = 45; r[x * 3 + 2] = 45;
        }
    }
    /* 绘制红色不可记忆网格线条 (230, 35, 35) */
    for (int y = 0; y < 95; y += 4) {
        uint8_t* r = combat_map->data + y * combat_map->stride;
        for (int x = 0; x < 135; x++) {
            r[x * 3 + 0] = 230; r[x * 3 + 1] = 35; r[x * 3 + 2] = 35;
        }
    }

    L2MMapBoxResult combat_res;
    bool combat_ok = l2m_detect_map_box(combat_map, 10, 10, &combat_res);
    TEST_ASSERT(combat_ok && combat_res.detected, "Combat map detected successfully");
    TEST_ASSERT(combat_res.zone_type == L2M_ZONE_COMBAT, "Map zone recognized as L2M_ZONE_COMBAT");
    TEST_ASSERT(combat_res.has_red_grid, "Red grid detected (No-Memory zone)");

    /* 6.4 全屏画面 l2m_detect_map_zone 联合测试 */
    L2MImageBuffer* full_screen = l2m_image_create(960, 540, L2M_FMT_RGB888);
    memset(full_screen->data, 20, (size_t)full_screen->stride * full_screen->height);
    /* 将 safe_map 贴入全屏画面的 (25, 59) */
    for (int y = 0; y < 94; y++) {
        uint8_t* src_r = safe_map->data + y * safe_map->stride;
        uint8_t* dst_r = full_screen->data + (y + 59) * full_screen->stride + (25 * 3);
        memcpy(dst_r, src_r, 135 * 3);
    }

    L2MCbtConfig cbt_cfg;
    memset(&cbt_cfg, 0, sizeof(cbt_cfg));
    cbt_cfg.map_zone_cfg.x = 25;
    cbt_cfg.map_zone_cfg.y = 59;
    cbt_cfg.map_zone_cfg.width = 144;
    cbt_cfg.map_zone_cfg.height = 94;
    cbt_cfg.map_zone_cfg.enabled = true;
    cbt_cfg.map_box_roi = (L2MRect){25, 59, 144, 94};

    L2MMapBoxResult full_map_res;
    bool full_map_ok = l2m_detect_map_zone(full_screen, &cbt_cfg, &full_map_res);
    TEST_ASSERT(full_map_ok && full_map_res.detected, "l2m_detect_map_zone on full 960x540 screen succeeded");
    TEST_ASSERT(full_map_res.zone_type == L2M_ZONE_SAFETY, "Full screen recognized safety zone correctly");

    l2m_image_free(safe_map);
    l2m_image_free(normal_map);
    l2m_image_free(combat_map);
    l2m_image_free(full_screen);
}

/* 7. 测试游戏窗口对应配置、多开实例管理与多语言角色绑定引擎 */
static void test_window_profile_manager(void) {
    printf("[*] Testing Window Profile Manager, Multi-Instance Binding & Character Recognition...\n");

    /* 7.1 测试加载 data/window_profiles.json 配置文件 */
    L2MWindowProfileList list;
    bool load_ok = l2m_window_profiles_load("data/window_profiles.json", &list);
    TEST_ASSERT(load_ok, "l2m_window_profiles_load successfully loaded window_profiles.json");
    TEST_ASSERT(list.count >= 4, "Loaded at least 4 window profiles");

    /* 验证第 1 个配置 (CN / Andyusa) */
    TEST_ASSERT(strcmp(list.profiles[0].character_name, "Andyusa") == 0, "Profile 1 character_name is 'Andyusa'");
    TEST_ASSERT(strcmp(list.profiles[0].region, "CN") == 0, "Profile 1 region is 'CN'");
    TEST_ASSERT(list.profiles[0].match_rule == L2M_WIN_MATCH_INDEX && list.profiles[0].match_window_index == 0, "Profile 1 matches Window Index 0");

    /* 验证第 2 个配置 (CN / 中文角色名: 狂风舞者) */
    TEST_ASSERT(strcmp(list.profiles[1].character_name, "狂风舞者") == 0, "Profile 2 character_name is Chinese '狂风舞者'");
    TEST_ASSERT(strcmp(list.profiles[1].region, "CN") == 0, "Profile 2 region is 'CN'");
    TEST_ASSERT(list.profiles[1].match_rule == L2M_WIN_MATCH_INDEX && list.profiles[1].match_window_index == 1, "Profile 2 matches Window Index 1");

    /* 7.2 测试窗口智能匹配引擎 (解耦窗口标题与角色名称) */
    /* 模拟中文客户端窗口实例：窗口标题是通用模拟器标题 "Lineage2M(2)"，非角色名 */
    L2MWindowInstance win_cn;
    memset(&win_cn, 0, sizeof(win_cn));
    win_cn.pid = 8848;
    strncpy(win_cn.process_name, "Lineage2M.exe", sizeof(win_cn.process_name) - 1);
    strncpy(win_cn.window_title, "Lineage2M(2)", sizeof(win_cn.window_title) - 1);
    win_cn.client_width = 960;
    win_cn.client_height = 540;

    L2MWindowProfile matched_cn;
    memset(&matched_cn, 0, sizeof(matched_cn));
    bool match_ok = l2m_window_profile_match(&list, &win_cn, 1, &matched_cn);
    TEST_ASSERT(match_ok, "l2m_window_profile_match successfully matched 2nd window instance");
    TEST_ASSERT(strcmp(matched_cn.character_name, "狂风舞者") == 0, "Matched character name is '狂风舞者'");
    TEST_ASSERT(strcmp(matched_cn.region, "CN") == 0, "Matched region is 'CN'");

    /* 模拟第 1 个窗口实例 */
    L2MWindowInstance win_en;
    memset(&win_en, 0, sizeof(win_en));
    win_en.pid = 8847;
    strncpy(win_en.process_name, "Lineage2M.exe", sizeof(win_en.process_name) - 1);
    strncpy(win_en.window_title, "PURPLE - Lineage2M", sizeof(win_en.window_title) - 1);
    win_en.client_width = 960;
    win_en.client_height = 540;

    L2MWindowProfile matched_en;
    memset(&matched_en, 0, sizeof(matched_en));
    bool match_en_ok = l2m_window_profile_match(&list, &win_en, 0, &matched_en);
    TEST_ASSERT(match_en_ok, "l2m_window_profile_match successfully matched 1st window instance");
    TEST_ASSERT(strcmp(matched_en.character_name, "Andyusa") == 0, "Matched character name is 'Andyusa'");
    TEST_ASSERT(strcmp(matched_en.region, "CN") == 0, "Matched region is 'CN'");

    /* 7.3 测试配置保存与回读 */
    bool save_ok = l2m_window_profiles_save("build/test_profiles.json", &list);
    TEST_ASSERT(save_ok, "l2m_window_profiles_save successfully saved test_profiles.json");

    L2MWindowProfileList list_reload;
    bool reload_ok = l2m_window_profiles_load("build/test_profiles.json", &list_reload);
    TEST_ASSERT(reload_ok && list_reload.count == list.count, "Reloaded saved profiles successfully");
    TEST_ASSERT(strcmp(list_reload.profiles[1].character_name, "狂风舞者") == 0, "Reloaded Chinese character name matches");

    /* 7.4 测试按窗口名称/标题手动保存绑定 (l2m_save_window_profile_by_title) */
    bool save_by_title_ok = l2m_save_window_profile_by_title("Lineage2M_CN_Main", "天下无双", "CN", "手动绑定中文测试");
    TEST_ASSERT(save_by_title_ok, "l2m_save_window_profile_by_title succeeded");

    L2MWindowProfile loaded_by_title;
    memset(&loaded_by_title, 0, sizeof(loaded_by_title));
    bool load_by_title_ok = l2m_load_window_profile_by_title("Lineage2M_CN_Main", &loaded_by_title);
    TEST_ASSERT(load_by_title_ok, "l2m_load_window_profile_by_title loaded profile");
    TEST_ASSERT(strcmp(loaded_by_title.character_name, "天下无双") == 0, "Saved character name '天下无双' verified");
    TEST_ASSERT(strcmp(loaded_by_title.region, "CN") == 0, "Saved region 'CN' verified");

    /* 7.5 测试多物理显示器枚举与指定显示器网格排版 */
    L2MMonitorList mon_list;
    bool enum_mon_ok = l2m_enum_monitors(&mon_list);
    TEST_ASSERT(enum_mon_ok && mon_list.count >= 1, "l2m_enum_monitors detected at least 1 physical monitor");
    TEST_ASSERT(mon_list.primary_index >= 0 && mon_list.primary_index < mon_list.count, "Primary monitor index is valid");
    TEST_ASSERT(mon_list.monitors[mon_list.primary_index].width > 0 && mon_list.monitors[mon_list.primary_index].height > 0,
                "Primary monitor resolution is positive");

    L2MMonitorInfo mon_info;
    bool get_mon_ok = l2m_get_monitor_by_index(0, &mon_info);
    TEST_ASSERT(get_mon_ok && mon_info.width > 0, "l2m_get_monitor_by_index(0) succeeded");

    int32_t aligned_count = 0;
    /* 无真实游戏窗口时安全返回 false */
    l2m_align_game_windows(L2M_ALIGN_GRID_2X2, 960, 540, &aligned_count);
    TEST_ASSERT(aligned_count >= 0, "l2m_align_game_windows executed safely");

    int32_t aligned_count_ex = 0;
    l2m_align_game_windows_ex(L2M_ALIGN_GRID_2X2, 960, 540, 0, &aligned_count_ex);
    TEST_ASSERT(aligned_count_ex >= 0, "l2m_align_game_windows_ex executed safely on monitor 0");

    /* 7.6 测试 data/id/<name>.json 独立角色配置文件读取与语言绑定功能 */
    L2MIdConfig andy_cfg;
    bool id_load_ok = l2m_id_profile_load("Andyusa", &andy_cfg);
    TEST_ASSERT(id_load_ok, "l2m_id_profile_load('Andyusa') loaded data/id/Andyusa.json successfully");
    TEST_ASSERT(strcmp(andy_cfg.region, "CN") == 0, "Andyusa.json REGION is 'CN'");
    TEST_ASSERT(andy_cfg.low_hp_dodge == true, "Andyusa.json LOW_HP_DODGE is true");
    TEST_ASSERT(andy_cfg.peace_mode == false, "Andyusa.json PEACE_MODE is false (temporarily disabled)");
    TEST_ASSERT(andy_cfg.pvp_evade == false, "Andyusa.json PVP_EVADE is false (temporarily disabled)");
    TEST_ASSERT(andy_cfg.overweight_afk == 80, "Andyusa.json OVERWEIGHT_AFK is 80");

    /* 测试读取语言地区 */
    char reg_buf[16] = {0};
    bool get_reg_ok = l2m_id_profile_get_region("Andyusa", reg_buf, sizeof(reg_buf));
    TEST_ASSERT(get_reg_ok && strcmp(reg_buf, "CN") == 0, "l2m_id_profile_get_region returned 'CN'");

    /* 测试新建/保存中文角色独立配置文件 */
    bool id_set_reg_ok = l2m_id_profile_set_region("ChineseHero", "CN");
    TEST_ASSERT(id_set_reg_ok, "l2m_id_profile_set_region('ChineseHero', 'CN') created/saved data/id/ChineseHero.json");

    L2MIdConfig hero_cfg;
    bool hero_load_ok = l2m_id_profile_load("ChineseHero", &hero_cfg);
    TEST_ASSERT(hero_load_ok && strcmp(hero_cfg.region, "CN") == 0, "Reloaded ChineseHero.json REGION is 'CN'");

    /* 测试扫描 data/id/ 目录下的所有配置文件 */
    char id_list[16][64];
    int32_t id_count = 0;
    bool enum_id_ok = l2m_enum_id_profiles(id_list, 16, &id_count);
    TEST_ASSERT(enum_id_ok && id_count >= 1, "l2m_enum_id_profiles found at least 1 profile in data/id/");

    /* 7.7 测试自动关闭弹窗 (auto_dismiss_popup) 配置读写与持久化 */
    bool get_pop_init = false;
    bool get_pop_ok = l2m_id_profile_get_auto_dismiss_popup("Andyusa", &get_pop_init);
    TEST_ASSERT(get_pop_ok, "l2m_id_profile_get_auto_dismiss_popup('Andyusa') succeeded");

    /* 切换为 false 并持久化 */
    bool set_pop_false_ok = l2m_id_profile_set_auto_dismiss_popup("Andyusa", false);
    TEST_ASSERT(set_pop_false_ok, "l2m_id_profile_set_auto_dismiss_popup('Andyusa', false) succeeded");

    bool get_pop_val = true;
    l2m_id_profile_get_auto_dismiss_popup("Andyusa", &get_pop_val);
    TEST_ASSERT(get_pop_val == false, "Andyusa auto_dismiss_popup verified as false");

    L2MIdConfig reload_andy_pop;
    l2m_id_profile_load("Andyusa", &reload_andy_pop);
    TEST_ASSERT(reload_andy_pop.auto_dismiss_popup == false, "Reloaded Andyusa.json AUTO_DISMISS_POPUP is false");

    /* 7.8 测试中文角色独立配置文件 (UTF-8 路径) 的读写与弹窗开关持久化 */
    bool set_cn_pop_ok = l2m_id_profile_set_auto_dismiss_popup("狂风舞者", false);
    TEST_ASSERT(set_cn_pop_ok, "l2m_id_profile_set_auto_dismiss_popup('狂风舞者', false) succeeded");

    bool get_cn_val = true;
    l2m_id_profile_get_auto_dismiss_popup("狂风舞者", &get_cn_val);
    TEST_ASSERT(get_cn_val == false, "狂风舞者 auto_dismiss_popup is false");

    L2MIdConfig reload_cn_pop;
    l2m_id_profile_load("狂风舞者", &reload_cn_pop);
    TEST_ASSERT(reload_cn_pop.auto_dismiss_popup == false, "Reloaded 狂风舞者.json AUTO_DISMISS_POPUP is false");

    /* 7.9 测试低血量阈值与恢复出战阈值读写与持久化 */
    int32_t low_hp_val = 0, rec_hp_val = 0;
    bool get_hp_ok = l2m_id_profile_get_hp_thresholds("Andyusa", &low_hp_val, &rec_hp_val);
    TEST_ASSERT(get_hp_ok && low_hp_val > 0 && rec_hp_val > 0, "l2m_id_profile_get_hp_thresholds('Andyusa') succeeded");

    /* 设置自定义阈值 (如 25% 回城, 85% 恢复出战) */
    bool set_hp_ok = l2m_id_profile_set_hp_thresholds("Andyusa", 25, 85);
    TEST_ASSERT(set_hp_ok, "l2m_id_profile_set_hp_thresholds('Andyusa', 25, 85) succeeded");

    int32_t read_low = 0, read_rec = 0;
    l2m_id_profile_get_hp_thresholds("Andyusa", &read_low, &read_rec);
    TEST_ASSERT(read_low == 25, "Andyusa low_hp_threshold updated to 25%");
    TEST_ASSERT(read_rec == 85, "Andyusa recover_hp_threshold updated to 85%");

    /* 测试中文角色配置文件的阈值保存与边界修剪 (low_hp=2, recover_hp=2 非法边界测试) */
    bool set_cn_hp_ok = l2m_id_profile_set_hp_thresholds("狂风舞者", 2, 2);
    TEST_ASSERT(set_cn_hp_ok, "l2m_id_profile_set_hp_thresholds('狂风舞者', 2, 2) handled safely");

    int32_t cn_low = 0, cn_rec = 0;
    l2m_id_profile_get_hp_thresholds("狂风舞者", &cn_low, &cn_rec);
    TEST_ASSERT(cn_low == 5, "狂风舞者 low_hp clamped to minimum 5%");
    TEST_ASSERT(cn_rec == 15, "狂风舞者 recover_hp auto adjusted > low_hp (15%)");

    /* 恢复正常阈值 */
    l2m_id_profile_set_hp_thresholds("Andyusa", 30, 80);
    l2m_id_profile_set_hp_thresholds("狂风舞者", 30, 80);
}

int main(void) {
    printf("===================================================================\n");
    printf("     Lineage2MBot Automated Core Unit & Regression Tests\n");
    printf("===================================================================\n\n");

    l2m_init_engine();

    test_cbt_manager();
    printf("\n");
    test_image_buffer();
    printf("\n");
    test_contour_division_guard();
    printf("\n");
    test_hp_calculation();
    printf("\n");
    test_popup_recognition_engine();
    printf("\n");
    test_map_zone_recognition();
    printf("\n");
    test_window_profile_manager();

    l2m_shutdown_engine();

    printf("\n===================================================================\n");
    printf("  Results: %d / %d Tests Passed (%.1f%%)\n",
           g_tests_passed, g_tests_total, (float)g_tests_passed * 100.0f / (float)g_tests_total);
    printf("===================================================================\n");

    return (g_tests_passed == g_tests_total) ? 0 : 1;
}

