/**
 * @file l2m_vision.h
 * @brief Lineage2MBot 计算机视觉算法与图像算子头文件 (纯 C 实现)
 */

#ifndef L2M_VISION_H
#define L2M_VISION_H

#include "l2m_types.h"
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 图像内存分配与释放 */
L2MImageBuffer* l2m_image_create(int32_t width, int32_t height, L2MImageFormat format);
void l2m_image_free(L2MImageBuffer* img);
L2MImageBuffer* l2m_image_clone(const L2MImageBuffer* src);
bool l2m_image_crop(const L2MImageBuffer* src, const L2MRect* roi, L2MImageBuffer* dst);

/* 格式转换 (BGR 转 RGB, RGB 转灰度等) */
bool l2m_image_bgr_to_rgb(const L2MImageBuffer* src, L2MImageBuffer* dst);
bool l2m_image_rgb_to_gray(const L2MImageBuffer* src, L2MImageBuffer* dst);

/* BMP 图片文件保存与载入 */
bool l2m_image_save_bmp(const L2MImageBuffer* img_rgb, const wchar_t* file_path);
L2MImageBuffer* l2m_image_load_bmp(const wchar_t* file_path);

/* 纯 C 色彩掩码提取算子 (支持多核/向量化对齐) */
bool l2m_color_mask_range(
    const L2MImageBuffer* src_rgb,
    L2MRGB min_rgb,
    L2MRGB max_rgb,
    L2MImageBuffer* dst_bin
);

bool l2m_color_mask_tolerance(
    const L2MImageBuffer* src_rgb,
    L2MRGB target_rgb,
    L2MRGB tolerance,
    L2MImageBuffer* dst_bin
);

/* 橙色弹窗按钮专属高灵敏掩码提取 */
bool l2m_mask_orange_popup_button(
    const L2MImageBuffer* src_rgb,
    L2MImageBuffer* dst_bin
);

/* 灰色关闭按钮专属掩码提取 */
bool l2m_mask_gray_button(
    const L2MImageBuffer* src_rgb,
    L2MImageBuffer* dst_bin
);

/* 形态学算子 (二值图像膨胀、腐蚀、闭运算) */
bool l2m_morphology_dilate(
    const L2MImageBuffer* src_bin,
    L2MImageBuffer* dst_bin,
    int32_t k_width,
    int32_t k_height
);

bool l2m_morphology_erode(
    const L2MImageBuffer* src_bin,
    L2MImageBuffer* dst_bin,
    int32_t k_width,
    int32_t k_height
);

bool l2m_morphology_close(
    const L2MImageBuffer* src_bin,
    L2MImageBuffer* dst_bin,
    int32_t k_width,
    int32_t k_height
);

/* 轮廓查找与几何特征提取 (等价 OpenCV findContours / minAreaRect) */
int32_t l2m_find_contours(
    const L2MImageBuffer* src_bin,
    L2MContour* out_contours,
    int32_t max_contours,
    int32_t min_area,
    int32_t max_area
);

/* 区域内部颜色均值与色彩特征统计 */
bool l2m_analyze_region_color(
    const L2MImageBuffer* src_rgb,
    const L2MRect* roi,
    L2MRGB* out_mean_rgb,
    float* out_mean_brightness,
    float* out_max_chroma
);

/* 按钮内部像素色彩精确核验与相似度打分 */
bool l2m_verify_button_color(
    const L2MImageBuffer* src_rgb,
    const L2MRect* roi,
    bool is_orange,
    L2MRGB* out_mean_rgb,
    float* out_fill_ratio,
    float* out_color_score
);

/* 按钮尺寸与宽高比几何形态打分 */
bool l2m_evaluate_button_size(
    const L2MRect* bbox,
    L2MPopupType ptype,
    float* out_size_score
);

/* 弹窗面板边界/轮廓检测算子 */
bool l2m_find_dialog_panel(
    const L2MImageBuffer* src_rgb,
    L2MPopupType ptype,
    L2MRect* out_panel_rect,
    float* out_panel_score
);

/* 水平文本行纹理投影分析算子 (计算文字行数与波峰对比度) */
bool l2m_analyze_text_line_projection(
    const L2MImageBuffer* src_rgb,
    const L2MRect* roi,
    int32_t* out_line_count,
    float* out_contrast
);

/* 右上角关闭叉号 (X) 特征检测算子 */
bool l2m_find_close_cross_icon(
    const L2MImageBuffer* src_rgb,
    const L2MRect* search_roi,
    L2MPoint* out_cross_pos,
    float* out_cross_score
);

#ifdef __cplusplus
}
#endif

#endif /* L2M_VISION_H */
