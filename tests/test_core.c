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

    /* 验证 popup_scan_config 解析精度 */
    TEST_ASSERT(cfg.popup_cfg.top_left.x == 25 && cfg.popup_cfg.top_left.y == 54,
                "popup_scan_config.top_left parsed correctly (x=25, y=54)");
    TEST_ASSERT(cfg.popup_cfg.top_left.width == 235 && cfg.popup_cfg.top_left.height == 390,
                "popup_scan_config.top_left parsed correctly (w=235, h=390)");
    TEST_ASSERT(cfg.popup_cfg.center.x == 280 && cfg.popup_cfg.center.width == 400,
                "popup_scan_config.center parsed correctly");
    TEST_ASSERT(cfg.popup_cfg.fullscreen.width == 960 && cfg.popup_cfg.fullscreen.height == 540,
                "popup_scan_config.fullscreen parsed correctly");

    /* 验证没有将 center 和 fullscreen 误解析为普通点位 */
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

    l2m_shutdown_engine();

    printf("\n===================================================================\n");
    printf("  Results: %d / %d Tests Passed (%.1f%%)\n",
           g_tests_passed, g_tests_total, (float)g_tests_passed * 100.0f / (float)g_tests_total);
    printf("===================================================================\n");

    return (g_tests_passed == g_tests_total) ? 0 : 1;
}
