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

#define MAX_POPUP_ITEMS 64

/* 命名弹窗特征配置项 (支持任意自定义名称与独立特征参数) */
typedef struct {
    char name[64];                  /* 弹窗唯一名称标识 (如 "top_left_tip", "center_modal", "resurrect_confirm") */
    char desc[128];                 /* 弹窗中文描述 (如 "左上角提示弹窗(带不再显示)") */
    bool enabled;                   /* 是否启用该弹窗识别 */

    /* 1. 扫描 ROI 矩形区域 (完全兼容原 .x / .y / .width / .height 访问) */
    int x;
    int y;
    int width;
    int height;

    /* 2. 背景色特征参数 */
    float min_dark_ratio;           /* 背景色最小暗底占比 (如 0.16) */
    float max_brightness;           /* 背景色最大平均亮度 (如 125.0) */
    float max_high_chroma;          /* 最大高彩度自然干扰占比 (如 0.35) */

    /* 3. 按钮几何特征参数 */
    int btn_min_w;                  /* 按钮最小宽度 (像素) */
    int btn_max_w;                  /* 按钮最大宽度 (像素) */
    int btn_min_h;                  /* 按钮最小高度 (像素) */
    int btn_max_h;                  /* 按钮最大高度 (像素) */
    float btn_ideal_aspect;         /* 按钮黄金宽高比 */

    /* 4. 按钮颜色特征参数 */
    bool has_btn_rgb;               /* 是否指定了按钮参考色彩 */
    L2MRGB btn_target_rgb;          /* 按钮目标参考颜色 */
    float btn_min_fill_ratio;       /* 按钮特征色彩最小填充纯度 */

    /* 5. 弹窗多维结构特征开关 */
    bool check_panel;               /* 是否检测弹窗暗底面板 */
    bool check_title;               /* 是否检测顶部标题栏 */
    bool check_text_lines;          /* 是否检测中间文本行 */
    bool check_close_cross;         /* 是否检测右上角叉号 */
    bool has_checkbox;              /* 是否尝试检测“不再显示”复选框 */

    /* 6. 关联链接的 CBT 特征采样点 */
    char linked_cbt_key[64];        /* 关联的主 CBT 采样点 Key (如 "home_scroll_button_no_energomode" 或自定义点位) */
} L2MPopupItem;

/* 向后兼容别名 */
typedef L2MPopupItem L2MPopupTypeParam;

/* 命名弹窗配置集合管理器 */
typedef struct {
    L2MPopupItem items[MAX_POPUP_ITEMS];
    int count;                      /* 动态命名弹窗总数 */

    /* 便捷访问历史固定项 (完全兼容历史代码的直接访问) */
    L2MPopupItem top_left;
    L2MPopupItem center;
    L2MPopupItem fullscreen;
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

/* 保存配置回 JSON 文件 (同时保存特征点与命名弹窗配置) */
bool l2m_cbt_save(const L2MCbtConfig* cfg);

/* 命名弹窗管理接口 (增删改查) */
bool l2m_cbt_get_popup_item(const L2MCbtConfig* cfg, const char* name, L2MPopupItem* out_item);
bool l2m_cbt_set_popup_item(L2MCbtConfig* cfg, const L2MPopupItem* item);
bool l2m_cbt_delete_popup_item(L2MCbtConfig* cfg, const char* name);
int32_t l2m_cbt_get_popup_count(const L2MCbtConfig* cfg);
bool l2m_cbt_get_popup_by_index(const L2MCbtConfig* cfg, int32_t index, L2MPopupItem* out_item);

/* 提取指定弹窗类型的扫描区域 ROI (兼容接口) */
bool l2m_cbt_get_popup_roi(const L2MCbtConfig* cfg, L2MPopupType ptype, L2MRect* out_roi);

/* 更新指定弹窗类型的扫描区域 ROI (兼容接口) */
bool l2m_cbt_set_popup_roi(L2MCbtConfig* cfg, L2MPopupType ptype, const L2MRect* roi);

/* 提取指定弹窗类型的完整特征参数 (兼容接口) */
bool l2m_cbt_get_popup_param(const L2MCbtConfig* cfg, L2MPopupType ptype, L2MPopupTypeParam* out_param);

/* 更新指定弹窗类型的完整特征参数 (兼容接口) */
bool l2m_cbt_set_popup_param(L2MCbtConfig* cfg, L2MPopupType ptype, const L2MPopupTypeParam* param);

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
