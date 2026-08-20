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

#ifdef __cplusplus
}
#endif

#endif /* L2M_VISION_H */
