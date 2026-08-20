/**
 * @file l2m_cbt.h
 * @brief Lineage2MBot 多语言 CBT 采样特征点与弹窗扫描配置管理头文件
 */

#ifndef L2M_CBT_H
#define L2M_CBT_H

#include "l2m_types.h"
#include "l2m_vision.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CBT_POINTS 1024

typedef struct {
    char key[64];
    int x;
    int y;
    bool has_rgb;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int tolerance;
} L2MCbtPoint;

typedef struct {
    L2MRect top_left;
    L2MRect center;
    L2MRect fullscreen;
} L2MPopupScanConfig;

typedef struct {
    char region[8];          /* "CN", "EN", "JP", "RU" */
    char file_path[260];     /* JSON 文件完整路径 */
    L2MCbtPoint points[MAX_CBT_POINTS];
    int count;
    L2MPopupScanConfig popup_cfg;
} L2MCbtConfig;

/* 从 data/cbt/<REGION>.json 或 bot/data/cbt/<REGION>.json 加载配置 */
bool l2m_cbt_load(const char* region, L2MCbtConfig* cfg);

/* 保存配置回 JSON 文件 (同时保存特征点与弹窗配置) */
bool l2m_cbt_save(const L2MCbtConfig* cfg);

/* 提取指定弹窗类型的扫描区域 ROI */
bool l2m_cbt_get_popup_roi(const L2MCbtConfig* cfg, L2MPopupType ptype, L2MRect* out_roi);

/* 更新指定弹窗类型的扫描区域 ROI */
bool l2m_cbt_set_popup_roi(L2MCbtConfig* cfg, L2MPopupType ptype, const L2MRect* roi);

/* 获取指定 Key 的点位 */
bool l2m_cbt_get_point(const L2MCbtConfig* cfg, const char* key, L2MCbtPoint* out_pt);

/* 新增或修改点位 */
bool l2m_cbt_set_point(L2MCbtConfig* cfg, const L2MCbtPoint* pt);

/* 删除指定 Key 的点位 */
bool l2m_cbt_delete_point(L2MCbtConfig* cfg, const char* key);

/* 测试当前画面指定点位是否匹配 (计算色差) */
bool l2m_cbt_test_pixel_match(
    const L2MImageBuffer* img_rgb,
    const L2MCbtPoint* pt,
    L2MRGB* out_actual_rgb,
    int* out_color_diff,
    bool* out_is_match
);

#ifdef __cplusplus
}
#endif

#endif /* L2M_CBT_H */
