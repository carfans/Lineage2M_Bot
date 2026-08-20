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

    /* 验证没有将 center 和 fullscreen 误解析为普通 CBT 点位 */
    L2MCbtPoint pt_center, pt_fullscreen;
    bool has_bad_center = l2m_cbt_get_point(&cfg, "center", &pt_center);
    bool has_bad_fullscreen = l2m_cbt_get_point(&cfg, "fullscreen", &pt_fullscreen);
    TEST_ASSERT(!has_bad_center, "Key 'center' from popup config must NOT be parsed as CBT point");
    TEST_ASSERT(!has_bad_fullscreen, "Key 'fullscreen' from popup config must NOT be parsed as CBT point");

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

    l2m_shutdown_engine();

    printf("\n===================================================================\n");
    printf("  Results: %d / %d Tests Passed (%.1f%%)\n",
           g_tests_passed, g_tests_total, (float)g_tests_passed * 100.0f / (float)g_tests_total);
    printf("===================================================================\n");

    return (g_tests_passed == g_tests_total) ? 0 : 1;
}
