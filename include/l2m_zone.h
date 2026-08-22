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

/* 地图框与区域检测配置参数 (对应 JSON map_box_config) */
typedef struct {
    bool enabled;               /* 是否启用地图区域识别 */
    char desc[128];             /* 配置描述 */
    int32_t x;                  /* 地图框扫描 ROI X 坐标 (960x540 标准参考系) */
    int32_t y;                  /* 地图框扫描 ROI Y 坐标 */
    int32_t width;              /* 地图框扫描 ROI 宽度 */
    int32_t height;             /* 地图框扫描 ROI 高度 */

    /* 区域状态徽章/文字子区域相对偏移 (默认位于小地图左上角) */
    int32_t badge_offset_x;     /* 相对地图框的 X 偏移 (默认 2) */
    int32_t badge_offset_y;     /* 相对地图框的 Y 偏移 (默认 2) */
    int32_t badge_width;        /* 区域状态检测子区域宽度 (默认 65) */
    int32_t badge_height;       /* 区域状态检测子区域高度 (默认 30) */

    /* 判据阈值参数 */
    float min_blue_ratio;       /* 判定为安全区域 (Safe) 的蓝色像素最小占比 (默认 0.015) */
    float min_brown_ratio;      /* 判定为普通区域 (Common) 的浅咖色像素最小占比 (默认 0.015) */
    float min_green_ratio;      /* 备用绿色像素最小占比 (兼容历史) */
    float min_red_ratio;        /* 判定为不可记忆/战斗区域的红色网格像素最小占比 (默认 0.020) */
    float min_white_ratio;      /* 备用白色像素最小占比 (兼容历史) */
    float max_bg_brightness;    /* 地图暗底最大平均亮度阈值 (默认 140.0) */

    /* 中心玩家指示器与朝向检测 */
    bool detect_player_indicator; /* 是否检测正中间橙色圆圈与白色朝向箭头 (默认 true) */
} L2MMapZoneConfig;

/* 地图框与区域检测完整结果 */
typedef struct {
    bool detected;              /* 是否成功检测到地图框 */
    L2MRect map_rect;           /* 地图框在全局画面中的绝对矩形区域 */
    L2MZoneType zone_type;      /* 判定出的区域类型 (SAFETY / NORMAL / COMBAT) */
    float confidence;           /* 综合置信度得分 (0.0 ~ 100.0) */

    /* 特征色彩统计指标 */
    float blue_safe_ratio;      /* 左上角蓝色 Safe 安全像素占比 (0.0 ~ 1.0) */
    float brown_common_ratio;   /* 左上角浅咖色 Common 普通像素占比 (0.0 ~ 1.0) */
    float green_ratio;          /* 备用绿色安全像素占比 (兼容历史) */
    float white_gray_ratio;     /* 备用白/灰色普通野外像素占比 (兼容历史) */
    float red_ratio;            /* 红色像素总占比 (0.0 ~ 1.0) */
    float red_grid_ratio;       /* 红色不可记忆网格像素占比 (0.0 ~ 1.0) */
    bool has_red_grid;          /* 是否存在不可记忆红色网格 */
    float mean_brightness;      /* 地图区域整体平均亮度 */
    L2MRGB badge_mean_rgb;      /* 区域标识/文字局部的实测平均 RGB */

    /* 中心玩家定位与视角朝向 */
    bool has_player_indicator;  /* 是否成功检测到正中间玩家指示器 */
    L2MPoint player_center_pos; /* 玩家在全局 960x540 坐标系下的绝对中心坐标 */
    float player_heading_angle; /* 玩家当前朝向角度 (0.0° ~ 360.0°，正东 0° / 正北 90° / 正西 180° / 正南 270°) */
    bool has_view_cone;         /* 是否检测到扇形橙色渐变视角范围 */

    char zone_name[64];         /* 区域中文名称 ("安全区域(Safe)", "普通区域(Common)", "不可记忆区域(红网格)") */
    char desc[128];             /* 详细诊断说明 */
} L2MMapBoxResult;

/**
 * @brief 在局部切片图像中基于指定配置识别地图框与判别区域类型
 * @param crop_rgb 左上角地图区域图像切片 (RGB888)
 * @param base_x 切片在全局 960x540 画布中的 X 偏移
 * @param base_y 切片在全局 960x540 画布中的 Y 偏移
 * @param cfg 地图区域配置参数 (为 NULL 时采用默认参数)
 * @param out_result 输出地图框与区域分析结果
 * @return 是否检测到有效地图框
 */
bool l2m_detect_map_box_with_config(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    const L2MMapZoneConfig* cfg,
    L2MMapBoxResult* out_result
);

/**
 * @brief 在局部切片图像中识别地图框与判别区域类型 (兼容接口)
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

