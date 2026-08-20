/**
 * @file l2m_zone.h
 * @brief Lineage2MBot 左上角地图框识别与安全/普通区域判别引擎头文件
 */

#ifndef L2M_ZONE_H
#define L2M_ZONE_H

#include "l2m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 游戏地图区域安全类型枚举 */
typedef enum {
    L2M_ZONE_UNKNOWN = 0,       /* 未知 / 未检测到地图 / 界面遮挡 */
    L2M_ZONE_SAFETY = 1,        /* 安全区域 (Safety / Peace Zone / 村庄/城镇 / 无法PVP) */
    L2M_ZONE_NORMAL = 2,        /* 普通区域 (Normal Zone / 野外常规战斗/刷怪区) */
    L2M_ZONE_COMBAT = 3         /* 自由战斗/危险区域 (Combat / Danger Zone / 攻城战区) */
} L2MZoneType;

/* 地图框与区域检测完整结果 */
typedef struct {
    bool detected;              /* 是否成功检测到地图框 */
    L2MRect map_rect;           /* 地图框在全局画面中的绝对矩形区域 */
    L2MZoneType zone_type;      /* 判定出的区域类型 */
    float confidence;           /* 综合置信度得分 (0.0 ~ 100.0) */

    /* 特征色彩统计指标 */
    float green_ratio;          /* 区域标签内绿色安全像素占比 (0.0 ~ 1.0) */
    float white_gray_ratio;     /* 区域标签内白/灰色普通野外像素占比 (0.0 ~ 1.0) */
    float red_ratio;            /* 区域标签内红色危险战斗像素占比 (0.0 ~ 1.0) */
    float mean_brightness;      /* 地图区域整体平均亮度 */
    L2MRGB badge_mean_rgb;      /* 区域标识/文字局部的实测平均 RGB */

    char zone_name[32];         /* 区域中文名称 ("安全区域(村庄)", "普通区域(野外)", "危险区域(PVP)") */
    char desc[128];             /* 详细诊断说明 */
} L2MMapBoxResult;

/**
 * @brief 在局部切片图像中识别地图框与判别区域类型
 * @param crop_rgb 左上角地图区域图像切片 (RGB888)
 * @param base_x 切片在全局 960x540 画布中的 X 偏移
 * @param base_y 切片在全局 960x540 画布中的 Y 偏移
 * @param out_result 输出地图框与区域分析结果
 * @return 是否检测到有效地图框
 */
bool l2m_detect_map_box(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MMapBoxResult* out_result
);

/**
 * @brief 在全屏画面中根据 CBT 配置自动检测左上角地图与安全/普通区域
 * @param full_frame_rgb 完整游戏画面 (如 960x540 RGB)
 * @param cbt_cfg CBT 配置 (const L2MCbtConfig*)
 * @param out_result 输出检测结果
 * @return 是否成功检测到地图与区域类型
 */
bool l2m_detect_map_zone(
    const L2MImageBuffer* full_frame_rgb,
    const void* cbt_cfg,
    L2MMapBoxResult* out_result
);

#ifdef __cplusplus
}
#endif

#endif /* L2M_ZONE_H */
