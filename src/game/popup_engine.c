/**
 * @file popup_engine.c
 * @brief Lineage2MBot 弹窗检测、背景色先验确认与关闭按钮提取引擎 (纯 C 原生实现)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "../../include/l2m_popup.h"
#include "../../include/l2m_vision.h"

bool l2m_check_popup_background(
    const L2MImageBuffer* crop_rgb,
    L2MPopupType popup_type,
    L2MPopupBgInfo* out_bg_info
) {
    if (!crop_rgb || !crop_rgb->data || !out_bg_info) return false;

    int32_t w = crop_rgb->width;
    int32_t h = crop_rgb->height;
    if (w < 10 || h < 10) {
        out_bg_info->is_valid = false;
        snprintf(out_bg_info->reason, sizeof(out_bg_info->reason), "图像切片过小");
        return false;
    }

    int64_t total_r = 0, total_g = 0, total_b = 0;
    int32_t dark_pixel_count = 0;
    int32_t total_pixels = w * h;

    for (int32_t y = 0; y < h; y++) {
        const uint8_t* row = crop_rgb->data + y * crop_rgb->stride;
        for (int32_t x = 0; x < w; x++) {
            int32_t r = row[x * crop_rgb->channels + 0];
            int32_t g = row[x * crop_rgb->channels + 1];
            int32_t b = row[x * crop_rgb->channels + 2];

            total_r += r;
            total_g += g;
            total_b += b;

            /* 弹窗暗底/半透明蒙版特征像素:
               R <= 95, G <= 100, B <= 110, |R-G| <= 45, |G-B| <= 45 */
            if (r <= 95 && g <= 100 && b <= 110 &&
                abs(r - g) <= 45 && abs(g - b) <= 45) {
                dark_pixel_count++;
            }
        }
    }

    float mean_r = (float)total_r / (float)total_pixels;
    float mean_g = (float)total_g / (float)total_pixels;
    float mean_b = (float)total_b / (float)total_pixels;
    float mean_brightness = (mean_r + mean_g + mean_b) / 3.0f;
    float dark_ratio = (float)dark_pixel_count / (float)total_pixels;

    out_bg_info->mean_rgb.r = (uint8_t)mean_r;
    out_bg_info->mean_rgb.g = (uint8_t)mean_g;
    out_bg_info->mean_rgb.b = (uint8_t)mean_b;
    out_bg_info->mean_brightness = mean_brightness;
    out_bg_info->dark_ratio = dark_ratio;
    out_bg_info->dark_pixels = dark_pixel_count;
    out_bg_info->total_pixels = total_pixels;

    float min_dark_ratio = (popup_type == L2M_POPUP_TOP_LEFT) ? 0.18f :
                           ((popup_type == L2M_POPUP_CENTER) ? 0.22f : 0.15f);
    float max_mean_brightness = (popup_type == L2M_POPUP_TOP_LEFT) ? 120.0f :
                                ((popup_type == L2M_POPUP_CENTER) ? 110.0f : 130.0f);

    if (dark_ratio < min_dark_ratio) {
        out_bg_info->is_valid = false;
        snprintf(out_bg_info->reason, sizeof(out_bg_info->reason),
                 "暗色底占比过低 (%.1f%% < %.1f%%)", dark_ratio * 100.0f, min_dark_ratio * 100.0f);
        return false;
    }

    if (mean_brightness > max_mean_brightness) {
        out_bg_info->is_valid = false;
        snprintf(out_bg_info->reason, sizeof(out_bg_info->reason),
                 "整体画面过亮 (%.1f > %.1f)", mean_brightness, max_mean_brightness);
        return false;
    }

    out_bg_info->is_valid = true;
    snprintf(out_bg_info->reason, sizeof(out_bg_info->reason), "背景色符合弹窗暗底特征");
    return true;
}

bool l2m_find_checkbox(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    int32_t ref_btn_y,
    int32_t ref_btn_x,
    L2MPoint* out_cb_pos
) {
    if (!crop_rgb || !crop_rgb->data || !out_cb_pos) return false;

    int32_t w = crop_rgb->width;

    int32_t top_y = ref_btn_y - 45; if (top_y < 0) top_y = 0;
    int32_t bottom_y = ref_btn_y - 6; if (bottom_y < 5) bottom_y = 5;
    int32_t right_x = (int32_t)(w * 0.75f); if (right_x > w) right_x = w;

    int32_t roi_w = right_x;
    int32_t roi_h = bottom_y - top_y;

    if (roi_w > 10 && roi_h > 5) {
        L2MRect roi = {0, top_y, roi_w, roi_h};
        L2MImageBuffer* sub_crop = l2m_image_create(roi_w, roi_h, crop_rgb->format);
        L2MImageBuffer* sub_bin = l2m_image_create(roi_w, roi_h, L2M_FMT_BIN8);

        if (sub_crop && sub_bin && l2m_image_crop(crop_rgb, &roi, sub_crop)) {
            /* 提取方形边框掩码 */
            L2MRGB min_c = {50, 50, 50};
            L2MRGB max_c = {220, 220, 220};
            l2m_color_mask_range(sub_crop, min_c, max_c, sub_bin);

            L2MContour contours[16];
            int32_t cnt_count = l2m_find_contours(sub_bin, contours, 16, 25, 600);
            for (int32_t i = 0; i < cnt_count; i++) {
                int32_t bw = contours[i].bbox.width;
                int32_t bh = contours[i].bbox.height;
                float aspect = contours[i].aspect_ratio;
                if (bw >= 8 && bw <= 28 && bh >= 8 && bh <= 28 && aspect >= 0.7f && aspect <= 1.4f) {
                    out_cb_pos->x = base_x + contours[i].center.x;
                    out_cb_pos->y = base_y + top_y + contours[i].center.y;
                    l2m_image_free(sub_crop);
                    l2m_image_free(sub_bin);
                    return true;
                }
            }
        }
        if (sub_crop) l2m_image_free(sub_crop);
        if (sub_bin) l2m_image_free(sub_bin);
    }

    /* 几何推导兜底点位 (灰色按钮上方偏左) */
    int32_t def_x = base_x + ref_btn_x - 15; if (def_x < base_x + 16) def_x = base_x + 16;
    int32_t def_y = base_y + ref_btn_y - 25; if (def_y < base_y + 10) def_y = base_y + 10;
    out_cb_pos->x = def_x;
    out_cb_pos->y = def_y;
    return true;
}

bool l2m_detect_top_left_popup(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MPopupResult* out_result
) {
    if (!crop_rgb || !crop_rgb->data || !out_result) return false;

    int32_t w = crop_rgb->width;
    int32_t h = crop_rgb->height;

    /* 1. 先探测是否存在右侧橙色跳转按钮 */
    L2MImageBuffer* bin_orange = l2m_image_create(w, h, L2M_FMT_BIN8);
    L2MImageBuffer* bin_closed = l2m_image_create(w, h, L2M_FMT_BIN8);
    if (!bin_orange || !bin_closed) {
        if (bin_orange) l2m_image_free(bin_orange);
        if (bin_closed) l2m_image_free(bin_closed);
        return false;
    }

    l2m_mask_orange_popup_button(crop_rgb, bin_orange);
    l2m_morphology_close(bin_orange, bin_closed, 5, 3);

    L2MContour contours[16];
    int32_t n_orange = l2m_find_contours(bin_closed, contours, 16, 25, 30000);

    int32_t best_orange_idx = -1;
    float best_orange_score = -1.0f;
    for (int32_t i = 0; i < n_orange; i++) {
        if (contours[i].bbox.width >= 12 && contours[i].bbox.height >= 6 &&
            contours[i].aspect_ratio >= 0.8f && contours[i].aspect_ratio <= 9.0f) {
            if (contours[i].score > best_orange_score) {
                best_orange_score = contours[i].score;
                best_orange_idx = i;
            }
        }
    }

    if (best_orange_idx >= 0 && contours[best_orange_idx].center.x > (int32_t)(w * 0.35f)) {
        /* 双按钮弹窗结构：右侧为橙色跳转按钮，推导并锁定左侧灰色确认按钮 */
        L2MContour* oc = &contours[best_orange_idx];
        int32_t left_bw = oc->bbox.width;
        int32_t left_bh = oc->bbox.height;
        int32_t gap = (int32_t)(left_bw * 0.15f); if (gap < 4) gap = 4;
        int32_t left_bx = oc->bbox.x - left_bw - gap; if (left_bx < 0) left_bx = 0;
        int32_t left_by = oc->bbox.y;

        int32_t left_cx = left_bx + left_bw / 2;
        int32_t left_cy = left_by + left_bh / 2;

        out_result->detected = true;
        out_result->popup_type = L2M_POPUP_TOP_LEFT;
        out_result->button_pos.x = base_x + left_cx;
        out_result->button_pos.y = base_y + left_cy;
        out_result->button_bbox.x = base_x + left_bx;
        out_result->button_bbox.y = base_y + left_by;
        out_result->button_bbox.width = left_bw;
        out_result->button_bbox.height = left_bh;
        out_result->score = best_orange_score + 20.0f;

        /* 定位“不再显示该提示”勾选框 */
        out_result->has_checkbox = l2m_find_checkbox(crop_rgb, base_x, base_y, left_by, left_cx, &out_result->checkbox_pos);
        snprintf(out_result->desc, sizeof(out_result->desc), "左上角提示弹窗: 锁定左侧灰色确认(避开右侧跳转)");

        l2m_image_free(bin_orange);
        l2m_image_free(bin_closed);
        return true;
    }

    /* 2. 若未检测到右侧橙色按钮，直接提取左上角灰色关闭按钮 */
    L2MImageBuffer* bin_gray = bin_orange;
    l2m_mask_gray_button(crop_rgb, bin_gray);
    l2m_morphology_close(bin_gray, bin_closed, 5, 3);

    int32_t n_gray = l2m_find_contours(bin_closed, contours, 16, 25, 20000);
    int32_t best_gray_idx = -1;
    float best_gray_score = -1.0f;

    for (int32_t i = 0; i < n_gray; i++) {
        if (contours[i].bbox.width >= 12 && contours[i].bbox.height >= 6 &&
            contours[i].aspect_ratio >= 0.8f && contours[i].aspect_ratio <= 9.0f) {
            float sc = contours[i].score;
            if (contours[i].aspect_ratio >= 1.5f && contours[i].aspect_ratio <= 5.0f) sc *= 1.4f;
            if (contours[i].center.y > (int32_t)(h * 0.3f)) sc *= 1.2f;

            if (sc > best_gray_score) {
                best_gray_score = sc;
                best_gray_idx = i;
            }
        }
    }

    if (best_gray_idx >= 0) {
        L2MContour* gc = &contours[best_gray_idx];
        out_result->detected = true;
        out_result->popup_type = L2M_POPUP_TOP_LEFT;
        out_result->button_pos.x = base_x + gc->center.x;
        out_result->button_pos.y = base_y + gc->center.y;
        out_result->button_bbox.x = base_x + gc->bbox.x;
        out_result->button_bbox.y = base_y + gc->bbox.y;
        out_result->button_bbox.width = gc->bbox.width;
        out_result->button_bbox.height = gc->bbox.height;
        out_result->score = best_gray_score;

        out_result->has_checkbox = l2m_find_checkbox(crop_rgb, base_x, base_y, gc->bbox.y, gc->center.x, &out_result->checkbox_pos);
        snprintf(out_result->desc, sizeof(out_result->desc), "左上角提示弹窗: 发现灰色确认按钮");

        l2m_image_free(bin_orange);
        l2m_image_free(bin_closed);
        return true;
    }

    l2m_image_free(bin_orange);
    l2m_image_free(bin_closed);
    return false;
}

bool l2m_detect_standard_popup(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MPopupType popup_type,
    L2MPopupResult* out_result
) {
    if (!crop_rgb || !crop_rgb->data || !out_result) return false;

    int32_t w = crop_rgb->width;
    int32_t h = crop_rgb->height;

    L2MImageBuffer* bin = l2m_image_create(w, h, L2M_FMT_BIN8);
    L2MImageBuffer* bin_closed = l2m_image_create(w, h, L2M_FMT_BIN8);
    if (!bin || !bin_closed) {
        if (bin) l2m_image_free(bin);
        if (bin_closed) l2m_image_free(bin_closed);
        return false;
    }

    l2m_mask_orange_popup_button(crop_rgb, bin);
    l2m_morphology_close(bin, bin_closed, 5, 3);

    L2MContour contours[16];
    int32_t n_cnt = l2m_find_contours(bin_closed, contours, 16, 25, 30000);

    int32_t best_idx = -1;
    float best_score = -1.0f;

    for (int32_t i = 0; i < n_cnt; i++) {
        if (contours[i].bbox.width >= 12 && contours[i].bbox.height >= 6 &&
            contours[i].aspect_ratio >= 0.8f && contours[i].aspect_ratio <= 9.0f) {
            float sc = contours[i].score;
            if (contours[i].aspect_ratio >= 1.8f && contours[i].aspect_ratio <= 5.0f) sc *= 1.5f;
            if (contours[i].center.y > (int32_t)(h * 0.35f)) sc *= 1.2f;

            if (sc > best_score) {
                best_score = sc;
                best_idx = i;
            }
        }
    }

    if (best_idx >= 0) {
        L2MContour* bc = &contours[best_idx];
        out_result->detected = true;
        out_result->popup_type = popup_type;
        out_result->button_pos.x = base_x + bc->center.x;
        out_result->button_pos.y = base_y + bc->center.y;
        out_result->button_bbox.x = base_x + bc->bbox.x;
        out_result->button_bbox.y = base_y + bc->bbox.y;
        out_result->button_bbox.width = bc->bbox.width;
        out_result->button_bbox.height = bc->bbox.height;
        out_result->has_checkbox = false;
        out_result->score = best_score;

        const char* type_name = (popup_type == L2M_POPUP_FULLSCREEN) ? "全屏活动弹窗" : "中间标准弹窗";
        snprintf(out_result->desc, sizeof(out_result->desc), "%s: 发现橙色确认按钮", type_name);

        l2m_image_free(bin);
        l2m_image_free(bin_closed);
        return true;
    }

    l2m_image_free(bin);
    l2m_image_free(bin_closed);
    return false;
}

bool l2m_detect_popup(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MPopupType popup_type,
    bool validate_bg,
    L2MPopupResult* out_result
) {
    if (!crop_rgb || !crop_rgb->data || !out_result) return false;
    memset(out_result, 0, sizeof(L2MPopupResult));

    out_result->scan_rect.x = base_x;
    out_result->scan_rect.y = base_y;
    out_result->scan_rect.width = crop_rgb->width;
    out_result->scan_rect.height = crop_rgb->height;

    /* 1. 背景色先验校验 */
    if (validate_bg) {
        bool bg_ok = l2m_check_popup_background(crop_rgb, popup_type, &out_result->bg_info);
        if (!bg_ok) {
            out_result->detected = false;
            return false;
        }
    }

    /* 2. 针对性识别 */
    if (popup_type == L2M_POPUP_TOP_LEFT) {
        return l2m_detect_top_left_popup(crop_rgb, base_x, base_y, out_result);
    } else {
        return l2m_detect_standard_popup(crop_rgb, base_x, base_y, popup_type, out_result);
    }
}
