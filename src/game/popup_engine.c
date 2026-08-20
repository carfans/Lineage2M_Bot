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
#include "../../include/l2m_cbt.h"

bool l2m_check_popup_background(
    const L2MImageBuffer* crop_rgb,
    L2MPopupType popup_type,
    L2MPopupBgInfo* out_bg_info
) {
    if (!crop_rgb || !crop_rgb->data || !out_bg_info) return false;
    memset(out_bg_info, 0, sizeof(L2MPopupBgInfo));

    int32_t w = crop_rgb->width;
    int32_t h = crop_rgb->height;
    if (w < 10 || h < 10) {
        out_bg_info->is_valid = false;
        snprintf(out_bg_info->reason, sizeof(out_bg_info->reason), "图像切片过小 (%dx%d)", w, h);
        return false;
    }

    int64_t total_r = 0, total_g = 0, total_b = 0;
    int32_t dark_pixel_count = 0;
    int32_t high_chroma_count = 0;
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

            int32_t c_max = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
            int32_t c_min = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
            int32_t chroma = c_max - c_min;

            /* 1. 弹窗暗底/半透明蒙版特征像素 (暗调、低彩度) */
            if (r <= 95 && g <= 100 && b <= 110 && chroma <= 45) {
                dark_pixel_count++;
            }

            /* 2. 自然场景高饱和/高彩度像素 (如野外绿草、蓝天、高亮特效) */
            if (chroma >= 50 && (r + g + b) > 160) {
                high_chroma_count++;
            }
        }
    }

    float mean_r = (float)total_r / (float)total_pixels;
    float mean_g = (float)total_g / (float)total_pixels;
    float mean_b = (float)total_b / (float)total_pixels;
    float mean_brightness = (mean_r + mean_g + mean_b) / 3.0f;
    float dark_ratio = (float)dark_pixel_count / (float)total_pixels;
    float high_chroma_ratio = (float)high_chroma_count / (float)total_pixels;

    out_bg_info->mean_rgb.r = (uint8_t)mean_r;
    out_bg_info->mean_rgb.g = (uint8_t)mean_g;
    out_bg_info->mean_rgb.b = (uint8_t)mean_b;
    out_bg_info->mean_brightness = mean_brightness;
    out_bg_info->dark_ratio = dark_ratio;
    out_bg_info->high_chroma_ratio = high_chroma_ratio;
    out_bg_info->dark_pixels = dark_pixel_count;
    out_bg_info->total_pixels = total_pixels;

    float min_dark_ratio = (popup_type == L2M_POPUP_TOP_LEFT) ? 0.16f :
                           ((popup_type == L2M_POPUP_CENTER) ? 0.20f : 0.14f);
    float max_mean_brightness = (popup_type == L2M_POPUP_TOP_LEFT) ? 125.0f :
                                ((popup_type == L2M_POPUP_CENTER) ? 115.0f : 135.0f);
    float max_high_chroma_ratio = 0.35f;

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

    if (high_chroma_ratio > max_high_chroma_ratio) {
        out_bg_info->is_valid = false;
        snprintf(out_bg_info->reason, sizeof(out_bg_info->reason),
                 "高彩度自然色彩过多 (%.1f%% > %.1f%%)", high_chroma_ratio * 100.0f, max_high_chroma_ratio * 100.0f);
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
            L2MRGB min_c = {45, 45, 45};
            L2MRGB max_c = {225, 225, 225};
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

    /* 几何推导兜底点位 */
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
    int32_t n_orange = l2m_find_contours(bin_closed, contours, 16, 30, 25000);

    int32_t best_orange_idx = -1;
    float best_orange_total_score = -1.0f;
    L2MRGB best_orange_mean_rgb = {0, 0, 0};
    float best_orange_fill_ratio = 0.0f;
    float best_orange_color_score = 0.0f;
    float best_orange_size_score = 0.0f;

    for (int32_t i = 0; i < n_orange; i++) {
        float size_sc = 0.0f;
        if (!l2m_evaluate_button_size(&contours[i].bbox, L2M_POPUP_TOP_LEFT, &size_sc)) {
            continue;
        }

        L2MRGB mean_c = {0, 0, 0};
        float fill_r = 0.0f, color_sc = 0.0f;
        if (!l2m_verify_button_color(crop_rgb, &contours[i].bbox, true, &mean_c, &fill_r, &color_sc)) {
            continue;
        }

        /* 垂直位置偏向弹窗中下部 (y >= h * 0.3) */
        float pos_factor = (contours[i].center.y >= (int32_t)(h * 0.35f)) ? 1.2f : 0.85f;
        float total_candidate_score = ((size_sc * 0.4f) + (color_sc * 0.6f)) * pos_factor;

        if (total_candidate_score > best_orange_total_score) {
            best_orange_total_score = total_candidate_score;
            best_orange_idx = i;
            best_orange_mean_rgb = mean_c;
            best_orange_fill_ratio = fill_r;
            best_orange_color_score = color_sc;
            best_orange_size_score = size_sc;
        }
    }

    if (best_orange_idx >= 0 && contours[best_orange_idx].center.x > (int32_t)(w * 0.30f)) {
        /* 双按钮弹窗结构：右侧为橙色跳转按钮，推导并锁定左侧灰色确认按钮 */
        L2MContour* oc = &contours[best_orange_idx];
        int32_t left_bw = oc->bbox.width;
        int32_t left_bh = oc->bbox.height;
        int32_t gap = (int32_t)(left_bw * 0.15f); if (gap < 4) gap = 4;
        int32_t left_bx = oc->bbox.x - left_bw - gap; if (left_bx < 0) left_bx = 0;
        int32_t left_by = oc->bbox.y;

        int32_t left_cx = left_bx + left_bw / 2;
        int32_t left_cy = left_by + left_bh / 2;

        L2MRect left_roi = {left_bx, left_by, left_bw, left_bh};
        L2MRGB left_mean_rgb = {0, 0, 0};
        float left_fill_r = 0.0f, left_color_sc = 0.0f;
        bool left_verified = l2m_verify_button_color(crop_rgb, &left_roi, false, &left_mean_rgb, &left_fill_r, &left_color_sc);

        out_result->detected = true;
        out_result->popup_type = L2M_POPUP_TOP_LEFT;
        out_result->button_pos.x = base_x + left_cx;
        out_result->button_pos.y = base_y + left_cy;
        out_result->button_bbox.x = base_x + left_bx;
        out_result->button_bbox.y = base_y + left_by;
        out_result->button_bbox.width = left_bw;
        out_result->button_bbox.height = left_bh;
        out_result->button_aspect_ratio = (left_bh > 0) ? ((float)left_bw / (float)left_bh) : 1.0f;
        out_result->button_mean_rgb = left_verified ? left_mean_rgb : best_orange_mean_rgb;
        out_result->button_fill_ratio = left_verified ? left_fill_r : best_orange_fill_ratio;
        out_result->size_score = best_orange_size_score;
        out_result->color_score = left_verified ? left_color_sc : best_orange_color_score;
        out_result->score = best_orange_total_score + (left_verified ? 25.0f : 10.0f);

        /* 定位“不再显示该提示”勾选框 */
        out_result->has_checkbox = l2m_find_checkbox(crop_rgb, base_x, base_y, left_by, left_cx, &out_result->checkbox_pos);
        snprintf(out_result->desc, sizeof(out_result->desc), "左上角提示弹窗: 锁定左侧灰色确认(避开右侧跳转)");

        l2m_image_free(bin_orange);
        l2m_image_free(bin_closed);
        return true;
    }

    /* 2. 若未检测到右侧橙色按钮，直接提取左上角灰色确认/关闭按钮 */
    L2MImageBuffer* bin_gray = bin_orange;
    l2m_mask_gray_button(crop_rgb, bin_gray);
    l2m_morphology_close(bin_gray, bin_closed, 5, 3);

    int32_t n_gray = l2m_find_contours(bin_closed, contours, 16, 30, 20000);
    int32_t best_gray_idx = -1;
    float best_gray_total_score = -1.0f;
    L2MRGB best_gray_mean_rgb = {0, 0, 0};
    float best_gray_fill_ratio = 0.0f;
    float best_gray_color_score = 0.0f;
    float best_gray_size_score = 0.0f;

    for (int32_t i = 0; i < n_gray; i++) {
        float size_sc = 0.0f;
        if (!l2m_evaluate_button_size(&contours[i].bbox, L2M_POPUP_TOP_LEFT, &size_sc)) {
            continue;
        }

        L2MRGB mean_c = {0, 0, 0};
        float fill_r = 0.0f, color_sc = 0.0f;
        if (!l2m_verify_button_color(crop_rgb, &contours[i].bbox, false, &mean_c, &fill_r, &color_sc)) {
            continue;
        }

        float pos_factor = (contours[i].center.y >= (int32_t)(h * 0.35f)) ? 1.25f : 0.85f;
        float total_candidate_score = ((size_sc * 0.4f) + (color_sc * 0.6f)) * pos_factor;

        if (total_candidate_score > best_gray_total_score) {
            best_gray_total_score = total_candidate_score;
            best_gray_idx = i;
            best_gray_mean_rgb = mean_c;
            best_gray_fill_ratio = fill_r;
            best_gray_color_score = color_sc;
            best_gray_size_score = size_sc;
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
        out_result->button_aspect_ratio = gc->aspect_ratio;
        out_result->button_mean_rgb = best_gray_mean_rgb;
        out_result->button_fill_ratio = best_gray_fill_ratio;
        out_result->size_score = best_gray_size_score;
        out_result->color_score = best_gray_color_score;
        out_result->score = best_gray_total_score;

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
    int32_t n_cnt = l2m_find_contours(bin_closed, contours, 16, 40, 30000);

    int32_t best_idx = -1;
    float best_total_score = -1.0f;
    L2MRGB best_mean_rgb = {0, 0, 0};
    float best_fill_ratio = 0.0f;
    float best_color_score = 0.0f;
    float best_size_score = 0.0f;

    for (int32_t i = 0; i < n_cnt; i++) {
        float size_sc = 0.0f;
        if (!l2m_evaluate_button_size(&contours[i].bbox, popup_type, &size_sc)) {
            continue;
        }

        L2MRGB mean_c = {0, 0, 0};
        float fill_r = 0.0f, color_sc = 0.0f;
        if (!l2m_verify_button_color(crop_rgb, &contours[i].bbox, true, &mean_c, &fill_r, &color_sc)) {
            continue;
        }

        float pos_factor = (contours[i].center.y >= (int32_t)(h * 0.35f)) ? 1.25f : 0.85f;
        float total_candidate_score = ((size_sc * 0.35f) + (color_sc * 0.50f) + (contours[i].score * 0.15f)) * pos_factor;

        if (total_candidate_score > best_total_score) {
            best_total_score = total_candidate_score;
            best_idx = i;
            best_mean_rgb = mean_c;
            best_fill_ratio = fill_r;
            best_color_score = color_sc;
            best_size_score = size_sc;
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
        out_result->button_aspect_ratio = bc->aspect_ratio;
        out_result->button_mean_rgb = best_mean_rgb;
        out_result->button_fill_ratio = best_fill_ratio;
        out_result->size_score = best_size_score;
        out_result->color_score = best_color_score;
        out_result->has_checkbox = false;
        out_result->score = best_total_score;

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

bool l2m_detect_popup_features(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MPopupType popup_type,
    L2MPopupFeatureInfo* out_features
) {
    if (!crop_rgb || !crop_rgb->data || !out_features) return false;
    memset(out_features, 0, sizeof(L2MPopupFeatureInfo));

    int32_t w = crop_rgb->width;
    int32_t h = crop_rgb->height;
    if (w < 20 || h < 20) return false;

    float total_feature_score = 0.0f;
    int32_t feature_items_matched = 0;

    /* 1. 面板轮廓检测 (Panel Rectangle) */
    float panel_score = 0.0f;
    L2MRect local_panel = {0, 0, w, h};
    bool panel_ok = l2m_find_dialog_panel(crop_rgb, popup_type, &local_panel, &panel_score);
    if (panel_ok) {
        out_features->has_panel = true;
        out_features->panel_rect.x = base_x + local_panel.x;
        out_features->panel_rect.y = base_y + local_panel.y;
        out_features->panel_rect.width = local_panel.width;
        out_features->panel_rect.height = local_panel.height;
        total_feature_score += panel_score * 0.35f;
        feature_items_matched++;
    }

    /* 2. 顶部标题栏/金色装饰纹理检测 (Title Bar / Gold Decoration) */
    int32_t title_h = (int32_t)(local_panel.height * 0.30f);
    if (title_h > 8) {
        L2MRect title_roi = {local_panel.x, local_panel.y, local_panel.width, title_h};
        L2MRGB title_mean;
        float title_bright = 0.0f, title_chroma = 0.0f;
        if (l2m_analyze_region_color(crop_rgb, &title_roi, &title_mean, &title_bright, &title_chroma)) {
            if (title_bright >= 25.0f && (title_bright >= 45.0f || title_mean.r >= title_mean.b + 10)) {
                out_features->has_title_bar = true;
                out_features->title_contrast = (title_bright > 100.0f) ? 100.0f : (title_bright * 1.0f);
                total_feature_score += 25.0f;
                feature_items_matched++;
            }
        }
    }

    /* 3. 中间文本行水平投影纹理分析 (Message Text Lines) */
    int32_t text_y = local_panel.y + (int32_t)(local_panel.height * 0.20f);
    int32_t text_h = (int32_t)(local_panel.height * 0.50f);
    if (text_h > 12) {
        L2MRect text_roi = {local_panel.x + 5, text_y, local_panel.width - 10, text_h};
        int32_t line_cnt = 0;
        float text_contrast = 0.0f;
        if (l2m_analyze_text_line_projection(crop_rgb, &text_roi, &line_cnt, &text_contrast) && line_cnt >= 1) {
            out_features->has_content_text = true;
            out_features->text_line_count = line_cnt;
            total_feature_score += 25.0f + (line_cnt > 1 ? 10.0f : 0.0f);
            feature_items_matched++;
        }
    }

    /* 4. 右上角关闭叉号 (X) 特征检测 (Close Cross Icon) */
    int32_t cross_search_w = (int32_t)(local_panel.width * 0.25f);
    int32_t cross_search_h = (int32_t)(local_panel.height * 0.25f);
    if (cross_search_w >= 10 && cross_search_h >= 10) {
        L2MRect cross_roi = {
            local_panel.x + local_panel.width - cross_search_w,
            local_panel.y,
            cross_search_w,
            cross_search_h
        };
        L2MPoint cross_pt;
        float cross_score = 0.0f;
        if (l2m_find_close_cross_icon(crop_rgb, &cross_roi, &cross_pt, &cross_score)) {
            out_features->has_close_cross = true;
            out_features->close_cross_pos.x = base_x + cross_pt.x;
            out_features->close_cross_pos.y = base_y + cross_pt.y;
            total_feature_score += 15.0f;
            feature_items_matched++;
        }
    }

    if (total_feature_score > 100.0f) total_feature_score = 100.0f;
    out_features->feature_score = total_feature_score;

    snprintf(out_features->feature_desc, sizeof(out_features->feature_desc),
             "面板:[%s] 标题:[%s] 文本:[%d行] 叉号:[%s]",
             out_features->has_panel ? "是" : "否",
             out_features->has_title_bar ? "是" : "否",
             out_features->text_line_count,
             out_features->has_close_cross ? "是" : "否");

    return (feature_items_matched >= 1 || out_features->has_panel);
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
    out_result->popup_type = popup_type;

    if (popup_type == L2M_POPUP_TOP_LEFT) {
        strncpy(out_result->popup_name, "top_left_tip", sizeof(out_result->popup_name) - 1);
    } else if (popup_type == L2M_POPUP_CENTER) {
        strncpy(out_result->popup_name, "center_modal", sizeof(out_result->popup_name) - 1);
    } else {
        strncpy(out_result->popup_name, "fullscreen_event", sizeof(out_result->popup_name) - 1);
    }

    /* 1. 背景色先验校验 (区分暗色蒙版与高亮自然野外场景) */
    if (validate_bg) {
        bool bg_ok = l2m_check_popup_background(crop_rgb, popup_type, &out_result->bg_info);
        if (!bg_ok) {
            out_result->detected = false;
            return false;
        }
    }

    /* 2. 弹窗本体多维结构特征检测 (面板轮廓、标题栏、文本行投影与关闭叉号) */
    l2m_detect_popup_features(crop_rgb, base_x, base_y, popup_type, &out_result->feature_info);

    /* 3. 针对性识别确认/关闭按钮与勾选框 */
    bool detected = false;
    if (popup_type == L2M_POPUP_TOP_LEFT) {
        detected = l2m_detect_top_left_popup(crop_rgb, base_x, base_y, out_result);
    } else {
        detected = l2m_detect_standard_popup(crop_rgb, base_x, base_y, popup_type, out_result);
    }

    /* 4. 融合特征置信度打分 */
    if (detected && out_result->detected) {
        if (out_result->feature_info.feature_score > 0.0f) {
            out_result->score = (out_result->score * 0.70f) + (out_result->feature_info.feature_score * 0.30f);
        }
    }

    return detected;
}

bool l2m_detect_popup_by_item(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    const void* item_ptr,
    bool validate_bg,
    L2MPopupResult* out_result
) {
    if (!crop_rgb || !crop_rgb->data || !out_result) return false;
    memset(out_result, 0, sizeof(L2MPopupResult));

    const L2MPopupItem* item = (const L2MPopupItem*)item_ptr;
    if (item && !item->enabled) return false;

    out_result->scan_rect.x = base_x;
    out_result->scan_rect.y = base_y;
    out_result->scan_rect.width = crop_rgb->width;
    out_result->scan_rect.height = crop_rgb->height;

    if (item) {
        snprintf(out_result->popup_name, sizeof(out_result->popup_name), "%s", item->name);
    } else {
        snprintf(out_result->popup_name, sizeof(out_result->popup_name), "unknown_popup");
    }

    L2MPopupType mapped_type = L2M_POPUP_CENTER;
    if (item) {
        if (strstr(item->name, "top_left") || item->has_checkbox) {
            mapped_type = L2M_POPUP_TOP_LEFT;
        } else if (strstr(item->name, "fullscreen")) {
            mapped_type = L2M_POPUP_FULLSCREEN;
        }
    }
    out_result->popup_type = mapped_type;

    /* 1. 背景色先验校验 */
    if (validate_bg) {
        bool bg_ok = l2m_check_popup_background(crop_rgb, mapped_type, &out_result->bg_info);
        if (!bg_ok) {
            out_result->detected = false;
            return false;
        }
    }

    /* 2. 弹窗本体结构特征检测 */
    l2m_detect_popup_features(crop_rgb, base_x, base_y, mapped_type, &out_result->feature_info);

    /* 3. 针对性识别按钮与勾选框 */
    bool detected = false;
    if (mapped_type == L2M_POPUP_TOP_LEFT) {
        detected = l2m_detect_top_left_popup(crop_rgb, base_x, base_y, out_result);
    } else {
        detected = l2m_detect_standard_popup(crop_rgb, base_x, base_y, mapped_type, out_result);
    }

    /* 4. 融合特征置信度打分 */
    if (detected && out_result->detected) {
        snprintf(out_result->popup_name, sizeof(out_result->popup_name), "%s", item ? item->name : "popup");
        if (out_result->feature_info.feature_score > 0.0f) {
            out_result->score = (out_result->score * 0.70f) + (out_result->feature_info.feature_score * 0.30f);
        }
    }

    return detected;
}

bool l2m_detect_all_popups(
    const L2MImageBuffer* full_frame_rgb,
    const void* cbt_cfg_ptr,
    L2MPopupResult* out_result
) {
    if (!full_frame_rgb || !full_frame_rgb->data || !cbt_cfg_ptr || !out_result) return false;
    const L2MCbtConfig* cfg = (const L2MCbtConfig*)cbt_cfg_ptr;

    int total_popups = cfg->popup_cfg.count;
    if (total_popups <= 0) return false;

    for (int i = 0; i < total_popups; i++) {
        const L2MPopupItem* item = &cfg->popup_cfg.items[i];
        if (!item->enabled) continue;

        L2MRect roi = {item->x, item->y, item->width, item->height};
        if (roi.x < 0 || roi.y < 0 || roi.width <= 0 || roi.height <= 0 ||
            roi.x + roi.width > full_frame_rgb->width ||
            roi.y + roi.height > full_frame_rgb->height) {
            continue;
        }

        L2MImageBuffer* crop = l2m_image_create(roi.width, roi.height, L2M_FMT_RGB888);
        if (!crop || !l2m_image_crop(full_frame_rgb, &roi, crop)) {
            if (crop) l2m_image_free(crop);
            continue;
        }

        L2MPopupResult res;
        bool ok = l2m_detect_popup_by_item(crop, roi.x, roi.y, item, true, &res);
        if (ok && res.detected) {
            *out_result = res;
            l2m_image_free(crop);
            return true;
        }
        l2m_image_free(crop);
    }

    return false;
}
