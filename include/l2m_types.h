/**
 * @file l2m_types.h
 * @brief Lineage2MBot C 核心通用数据类型与结构体定义
 */

#ifndef L2M_TYPES_H
#define L2M_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RGB 颜色结构体 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} L2MRGB;

/* BGR 颜色结构体 (用于 Windows GDI / DIB 内存格式) */
typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
} L2MBGR;

/* 二维整型坐标点 */
typedef struct {
    int32_t x;
    int32_t y;
} L2MPoint;

/* 二维矩形区域 (X, Y, Width, Height) */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} L2MRect;

/* 连通域轮廓外接矩形与属性 */
typedef struct {
    L2MRect bbox;           /* 外接矩形 */
    L2MPoint center;        /* 几何质心或中心坐标 */
    int32_t area;           /* 轮廓像素面积 */
    float aspect_ratio;     /* 宽高比 (width / height) */
    float score;            /* 匹配评分 */
} L2MContour;

/* 图像内存缓冲区结构体 (支持 RGB/BGR/Grayscale/Binary) */
typedef enum {
    L2M_FMT_RGB888 = 0,
    L2M_FMT_BGR888 = 1,
    L2M_FMT_BGRA8888 = 2,
    L2M_FMT_GRAY8 = 3,
    L2M_FMT_BIN8 = 4
} L2MImageFormat;

typedef struct {
    uint8_t* data;          /* 像素数据内存指针 */
    int32_t width;          /* 图像宽度 (像素) */
    int32_t height;         /* 图像高度 (像素) */
    int32_t channels;       /* 通道数 (1, 3, 4) */
    int32_t stride;         /* 每行扫描线的字节跨度 (Row pitch in bytes) */
    L2MImageFormat format;  /* 图像格式 */
    bool is_owner;          /* 是否由当前结构体拥有内存所有权 (用于释放) */
} L2MImageBuffer;

/* 弹窗类型枚举 */
typedef enum {
    L2M_POPUP_TOP_LEFT = 0,     /* 左上角提示弹窗 (双按钮/单按钮，带不再显示勾选) */
    L2M_POPUP_CENTER = 1,       /* 中间标准模态确认弹窗 */
    L2M_POPUP_FULLSCREEN = 2,   /* 全屏活动/广告弹窗 */
    L2M_POPUP_ALL = 3           /* 自动优先级扫描全类型 */
} L2MPopupType;

/* 弹窗背景色先验校验结果 */
typedef struct {
    bool is_valid;              /* 背景色是否符合弹窗暗底特征 */
    float dark_ratio;           /* 暗色像素占比 (0.0 ~ 1.0) */
    float mean_brightness;      /* 区域整体平均亮度 (0.0 ~ 255.0) */
    L2MRGB mean_rgb;            /* 区域平均 RGB 颜色 */
    int32_t dark_pixels;        /* 暗色像素数量 */
    int32_t total_pixels;       /* 总像素数量 */
    char reason[64];            /* 判定说明或失败原因 */
} L2MPopupBgInfo;

/* 弹窗检测完整结果 */
typedef struct {
    bool detected;              /* 是否成功检测到弹窗 */
    L2MPopupType popup_type;    /* 检测到的弹窗类型 */
    L2MPoint button_pos;        /* 确认/关闭按钮点击中心绝对坐标 */
    L2MRect button_bbox;        /* 按钮外接矩形 */
    bool has_checkbox;          /* 是否存在“不再显示该提示”勾选框 */
    L2MPoint checkbox_pos;      /* 勾选框点击坐标 */
    L2MRect scan_rect;          /* 扫描 ROI 区域 */
    float score;                /* 识别置信度得分 */
    L2MPopupBgInfo bg_info;     /* 背景色先验校验信息 */
    char desc[128];             /* 详细描述文本 */
} L2MPopupResult;

/* 血条参数配置结构体 (对应 JSON hp_bar_config) */
typedef struct {
    int32_t offset_x;
    int32_t offset_y;
    int32_t width;
    int32_t height;
    L2MRGB target_color_1;
    L2MRGB tolerance_1;
    L2MRGB target_color_2;
    L2MRGB tolerance_2;
    float mean_threshold;
} L2MHpConfig;

/* 血条计算结果 */
typedef struct {
    int32_t hp_percent;         /* 血量百分比 (0 ~ 100) */
    int32_t sample_hp_end;      /* 有效匹配采样的最右像素点 */
    int32_t sample_red_end;     /* 红色基准采样点 */
    L2MRGB mean_rgb;            /* 采样区域实测平均 RGB */
    bool is_valid;              /* 结果是否有效 (若为 0 且颜色全偏则可能被遮挡) */
} L2MHpResult;

/* 窗口边框裁剪配置 (对应 JSON window_frame_config) */
typedef struct {
    int32_t crop_top;
    int32_t crop_bottom;
    int32_t crop_left;
    int32_t crop_right;
} L2MWindowFrameConfig;

#ifdef __cplusplus
}
#endif

#endif /* L2M_TYPES_H */
