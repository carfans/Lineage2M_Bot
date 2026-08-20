/**
 * @file morphology.c
 * @brief 纯 C 二值图像形态学算子实现 (膨胀、腐蚀、闭运算)
 */

#include <stdlib.h>
#include <string.h>
#include "../../include/l2m_vision.h"

bool l2m_morphology_dilate(
    const L2MImageBuffer* src_bin,
    L2MImageBuffer* dst_bin,
    int32_t k_width,
    int32_t k_height
) {
    if (!src_bin || !src_bin->data || !dst_bin || !dst_bin->data) return false;

    int32_t w = src_bin->width;
    int32_t h = src_bin->height;
    int32_t rx = k_width / 2;
    int32_t ry = k_height / 2;

    for (int32_t y = 0; y < h; y++) {
        uint8_t* d_row = dst_bin->data + y * dst_bin->stride;
        for (int32_t x = 0; x < w; x++) {
            uint8_t hit = 0;
            for (int32_t ky = -ry; ky <= ry && !hit; ky++) {
                int32_t sy = y + ky;
                if (sy < 0 || sy >= h) continue;
                const uint8_t* s_row = src_bin->data + sy * src_bin->stride;

                for (int32_t kx = -rx; kx <= rx; kx++) {
                    int32_t sx = x + kx;
                    if (sx < 0 || sx >= w) continue;
                    if (s_row[sx] > 0) {
                        hit = 255;
                        break;
                    }
                }
            }
            d_row[x] = hit;
        }
    }
    return true;
}

bool l2m_morphology_erode(
    const L2MImageBuffer* src_bin,
    L2MImageBuffer* dst_bin,
    int32_t k_width,
    int32_t k_height
) {
    if (!src_bin || !src_bin->data || !dst_bin || !dst_bin->data) return false;

    int32_t w = src_bin->width;
    int32_t h = src_bin->height;
    int32_t rx = k_width / 2;
    int32_t ry = k_height / 2;

    for (int32_t y = 0; y < h; y++) {
        uint8_t* d_row = dst_bin->data + y * dst_bin->stride;
        for (int32_t x = 0; x < w; x++) {
            uint8_t all_hit = 255;
            for (int32_t ky = -ry; ky <= ry && all_hit; ky++) {
                int32_t sy = y + ky;
                if (sy < 0 || sy >= h) { all_hit = 0; break; }
                const uint8_t* s_row = src_bin->data + sy * src_bin->stride;

                for (int32_t kx = -rx; kx <= rx; kx++) {
                    int32_t sx = x + kx;
                    if (sx < 0 || sx >= w || s_row[sx] == 0) {
                        all_hit = 0;
                        break;
                    }
                }
            }
            d_row[x] = all_hit;
        }
    }
    return true;
}

bool l2m_morphology_close(
    const L2MImageBuffer* src_bin,
    L2MImageBuffer* dst_bin,
    int32_t k_width,
    int32_t k_height
) {
    if (!src_bin || !src_bin->data || !dst_bin || !dst_bin->data) return false;

    /* 闭运算 = 先膨胀后腐蚀 (Dilation then Erosion) */
    L2MImageBuffer* temp = l2m_image_create(src_bin->width, src_bin->height, L2M_FMT_BIN8);
    if (!temp) return false;

    bool ok1 = l2m_morphology_dilate(src_bin, temp, k_width, k_height);
    bool ok2 = l2m_morphology_erode(temp, dst_bin, k_width, k_height);

    l2m_image_free(temp);
    return ok1 && ok2;
}
