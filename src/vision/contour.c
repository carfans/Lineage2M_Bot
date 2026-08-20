/**
 * @file contour.c
 * @brief 纯 C 二值图像连通域标记与轮廓提取实现 (Two-Pass Connected Component Labeling)
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../include/l2m_vision.h"

/* 并查集查找根节点 */
static int32_t find_root(int32_t* parent, int32_t i) {
    int32_t root = i;
    while (parent[root] > 0) {
        root = parent[root];
    }
    /* 路径压缩 */
    int32_t curr = i;
    while (curr != root) {
        int32_t next = parent[curr];
        if (next > 0) parent[curr] = root;
        curr = next;
    }
    return root;
}

/* 并查集合并 */
static void union_labels(int32_t* parent, int32_t i, int32_t j) {
    int32_t root_i = find_root(parent, i);
    int32_t root_j = find_root(parent, j);
    if (root_i != root_j) {
        if (root_i < root_j) {
            parent[root_j] = root_i;
        } else {
            parent[root_i] = root_j;
        }
    }
}

int32_t l2m_find_contours(
    const L2MImageBuffer* src_bin,
    L2MContour* out_contours,
    int32_t max_contours,
    int32_t min_area,
    int32_t max_area
) {
    if (!src_bin || !src_bin->data || !out_contours || max_contours <= 0) return 0;

    int32_t w = src_bin->width;
    int32_t h = src_bin->height;
    if (w <= 0 || h <= 0) return 0;

    int32_t* labels = (int32_t*)calloc((size_t)w * h, sizeof(int32_t));
    if (!labels) return 0;

    int32_t max_label_alloc = (w * h) / 2 + 100;
    int32_t* parent = (int32_t*)calloc((size_t)max_label_alloc, sizeof(int32_t));
    if (!parent) {
        free(labels);
        return 0;
    }

    int32_t next_label = 1;

    /* First Pass: 扫描并记录等价连通标签 */
    for (int32_t y = 0; y < h; y++) {
        const uint8_t* row = src_bin->data + y * src_bin->stride;
        int32_t* l_row = labels + y * w;
        int32_t* l_prev_row = (y > 0) ? (labels + (y - 1) * w) : NULL;

        for (int32_t x = 0; x < w; x++) {
            if (row[x] == 0) continue;

            int32_t left_label = (x > 0) ? l_row[x - 1] : 0;
            int32_t up_label = (l_prev_row) ? l_prev_row[x] : 0;

            if (left_label == 0 && up_label == 0) {
                if (next_label < max_label_alloc) {
                    l_row[x] = next_label;
                    parent[next_label] = 0;
                    next_label++;
                }
            } else if (left_label != 0 && up_label == 0) {
                l_row[x] = left_label;
            } else if (left_label == 0 && up_label != 0) {
                l_row[x] = up_label;
            } else {
                l_row[x] = left_label;
                if (left_label != up_label) {
                    union_labels(parent, left_label, up_label);
                }
            }
        }
    }

    /* 统计每个根连通域的几何属性 */
    typedef struct {
        int32_t min_x, max_x;
        int32_t min_y, max_y;
        int32_t area;
        int64_t sum_x, sum_y;
    } LabelStats;

    LabelStats* stats = (LabelStats*)calloc((size_t)next_label, sizeof(LabelStats));
    if (!stats) {
        free(labels);
        free(parent);
        return 0;
    }

    for (int32_t i = 1; i < next_label; i++) {
        stats[i].min_x = w;
        stats[i].min_y = h;
        stats[i].max_x = -1;
        stats[i].max_y = -1;
    }

    /* Second Pass: 归一化并累加属性 */
    for (int32_t y = 0; y < h; y++) {
        int32_t* l_row = labels + y * w;
        for (int32_t x = 0; x < w; x++) {
            int32_t lbl = l_row[x];
            if (lbl == 0) continue;

            int32_t root = find_root(parent, lbl);
            LabelStats* st = &stats[root];
            st->area++;
            st->sum_x += x;
            st->sum_y += y;
            if (x < st->min_x) st->min_x = x;
            if (x > st->max_x) st->max_x = x;
            if (y < st->min_y) st->min_y = y;
            if (y > st->max_y) st->max_y = y;
        }
    }

    /* 过滤并填充输出结果 */
    int32_t count = 0;
    for (int32_t i = 1; i < next_label && count < max_contours; i++) {
        LabelStats* st = &stats[i];
        if (st->area <= 0 || st->area < min_area || st->area > max_area) continue;

        int32_t bw = st->max_x - st->min_x + 1;
        int32_t bh = st->max_y - st->min_y + 1;
        if (bw <= 0 || bh <= 0) continue;

        L2MContour* c = &out_contours[count];
        c->bbox.x = st->min_x;
        c->bbox.y = st->min_y;
        c->bbox.width = bw;
        c->bbox.height = bh;
        c->area = st->area;
        c->aspect_ratio = (float)bw / (float)bh;
        c->center.x = (int32_t)(st->sum_x / st->area);
        c->center.y = (int32_t)(st->sum_y / st->area);
        c->score = (float)st->area;

        count++;
    }

    free(labels);
    free(parent);
    free(stats);

    return count;
}

bool l2m_find_dialog_panel(
    const L2MImageBuffer* src_rgb,
    L2MPopupType ptype,
    L2MRect* out_panel_rect,
    float* out_panel_score
) {
    if (!src_rgb || !src_rgb->data || !out_panel_rect) return false;
    int32_t w = src_rgb->width;
    int32_t h = src_rgb->height;
    if (w < 30 || h < 30) return false;

    /* 提取暗色面板二值图 */
    L2MImageBuffer* bin_dark = l2m_image_create(w, h, L2M_FMT_BIN8);
    L2MImageBuffer* bin_closed = l2m_image_create(w, h, L2M_FMT_BIN8);
    if (!bin_dark || !bin_closed) {
        if (bin_dark) l2m_image_free(bin_dark);
        if (bin_closed) l2m_image_free(bin_closed);
        return false;
    }

    for (int32_t y = 0; y < h; y++) {
        const uint8_t* s_row = src_rgb->data + y * src_rgb->stride;
        uint8_t* d_row = bin_dark->data + y * bin_dark->stride;
        for (int32_t x = 0; x < w; x++) {
            int32_t r = s_row[x * src_rgb->channels + 0];
            int32_t g = s_row[x * src_rgb->channels + 1];
            int32_t b = s_row[x * src_rgb->channels + 2];
            int32_t max_c = (r > g) ? (r > b ? r : b) : (g > b ? g : b);
            int32_t min_c = (r < g) ? (r < b ? r : b) : (g < b ? g : b);
            int32_t chroma = max_c - min_c;
            /* 弹窗面板本体特征: 暗调低彩度 (亮度 <= 115, 彩度 <= 38) */
            if (r <= 115 && g <= 115 && b <= 120 && chroma <= 38) {
                d_row[x] = 255;
            } else {
                d_row[x] = 0;
            }
        }
    }

    l2m_morphology_close(bin_dark, bin_closed, 7, 7);

    L2MContour contours[16];
    int32_t n_cnt = l2m_find_contours(bin_closed, contours, 16, (w * h) / 10, w * h);

    int32_t best_idx = -1;
    float best_score = -1.0f;

    for (int32_t i = 0; i < n_cnt; i++) {
        float aspect = contours[i].aspect_ratio;
        int32_t area = contours[i].area;

        /* 面积占比至少 15% */
        float area_ratio = (float)area / (float)(w * h);
        if (area_ratio < 0.15f) continue;

        /* 弹窗面板宽高比通常在 0.5 ~ 4.0 之间 */
        if (aspect < 0.5f || aspect > 4.0f) continue;

        float score = area_ratio * 60.0f;
        if (aspect >= 1.1f && aspect <= 3.2f) score += 25.0f;
        if (contours[i].center.x >= (int32_t)(w * 0.25f) && contours[i].center.x <= (int32_t)(w * 0.75f)) score += 15.0f;

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    bool found = false;
    if (best_idx >= 0) {
        *out_panel_rect = contours[best_idx].bbox;
        if (out_panel_score) *out_panel_score = best_score;
        found = true;
    } else {
        /* 若整个切片就是面板本身，使用切片尺寸作为面板 */
        out_panel_rect->x = 0;
        out_panel_rect->y = 0;
        out_panel_rect->width = w;
        out_panel_rect->height = h;
        if (out_panel_score) *out_panel_score = 50.0f;
        found = (ptype != L2M_POPUP_FULLSCREEN);
    }

    l2m_image_free(bin_dark);
    l2m_image_free(bin_closed);
    return found;
}

bool l2m_analyze_text_line_projection(
    const L2MImageBuffer* src_rgb,
    const L2MRect* roi,
    int32_t* out_line_count,
    float* out_contrast
) {
    if (!src_rgb || !src_rgb->data || !roi) return false;
    int32_t rx = (roi->x < 0) ? 0 : roi->x;
    int32_t ry = (roi->y < 0) ? 0 : roi->y;
    int32_t rw = roi->width;
    int32_t rh = roi->height;
    if (rx + rw > src_rgb->width) rw = src_rgb->width - rx;
    if (ry + rh > src_rgb->height) rh = src_rgb->height - ry;
    if (rw < 20 || rh < 10) return false;

    float* row_brightness = (float*)calloc((size_t)rh, sizeof(float));
    if (!row_brightness) return false;

    float global_sum = 0.0f;
    for (int32_t y = 0; y < rh; y++) {
        const uint8_t* row = src_rgb->data + (ry + y) * src_rgb->stride;
        int64_t line_sum = 0;
        for (int32_t x = rx; x < rx + rw; x++) {
            int32_t r = row[x * src_rgb->channels + 0];
            int32_t g = row[x * src_rgb->channels + 1];
            int32_t b = row[x * src_rgb->channels + 2];
            line_sum += (r + g + b) / 3;
        }
        row_brightness[y] = (float)line_sum / (float)rw;
        global_sum += row_brightness[y];
    }

    float mean_b = global_sum / (float)rh;
    float variance = 0.0f;
    for (int32_t y = 0; y < rh; y++) {
        float diff = row_brightness[y] - mean_b;
        variance += diff * diff;
    }
    float std_dev = sqrtf(variance / (float)rh);

    /* 寻找峰值 (文字行) */
    int32_t line_peaks = 0;
    float peak_threshold = mean_b + std_dev * 0.35f;
    bool in_peak = false;

    for (int32_t y = 1; y < rh - 1; y++) {
        if (!in_peak && row_brightness[y] > peak_threshold &&
            row_brightness[y] >= row_brightness[y - 1] && row_brightness[y] >= row_brightness[y + 1]) {
            line_peaks++;
            in_peak = true;
        } else if (row_brightness[y] < mean_b) {
            in_peak = false;
        }
    }

    free(row_brightness);

    if (out_line_count) *out_line_count = line_peaks;
    if (out_contrast) *out_contrast = std_dev;

    return (line_peaks >= 1 && std_dev >= 1.8f);
}

bool l2m_find_close_cross_icon(
    const L2MImageBuffer* src_rgb,
    const L2MRect* search_roi,
    L2MPoint* out_cross_pos,
    float* out_cross_score
) {
    if (!src_rgb || !src_rgb->data || !search_roi || !out_cross_pos) return false;
    int32_t rx = (search_roi->x < 0) ? 0 : search_roi->x;
    int32_t ry = (search_roi->y < 0) ? 0 : search_roi->y;
    int32_t rw = search_roi->width;
    int32_t rh = search_roi->height;
    if (rx + rw > src_rgb->width) rw = src_rgb->width - rx;
    if (ry + rh > src_rgb->height) rh = src_rgb->height - ry;
    if (rw < 8 || rh < 8) return false;

    L2MImageBuffer* sub_crop = l2m_image_create(rw, rh, src_rgb->format);
    L2MImageBuffer* sub_bin = l2m_image_create(rw, rh, L2M_FMT_BIN8);
    L2MRect roi = {rx, ry, rw, rh};

    if (!sub_crop || !sub_bin || !l2m_image_crop(src_rgb, &roi, sub_crop)) {
        if (sub_crop) l2m_image_free(sub_crop);
        if (sub_bin) l2m_image_free(sub_bin);
        return false;
    }

    /* 提取关闭叉号 (高对比浅白/浅灰色像素) */
    L2MRGB min_c = {130, 130, 130};
    L2MRGB max_c = {255, 255, 255};
    l2m_color_mask_range(sub_crop, min_c, max_c, sub_bin);

    L2MContour contours[8];
    int32_t n_cnt = l2m_find_contours(sub_bin, contours, 8, 12, 400);

    for (int32_t i = 0; i < n_cnt; i++) {
        int32_t bw = contours[i].bbox.width;
        int32_t bh = contours[i].bbox.height;
        float aspect = contours[i].aspect_ratio;
        if (bw >= 6 && bw <= 28 && bh >= 6 && bh <= 28 && aspect >= 0.65f && aspect <= 1.55f) {
            out_cross_pos->x = rx + contours[i].center.x;
            out_cross_pos->y = ry + contours[i].center.y;
            if (out_cross_score) *out_cross_score = 85.0f;
            l2m_image_free(sub_crop);
            l2m_image_free(sub_bin);
            return true;
        }
    }

    l2m_image_free(sub_crop);
    l2m_image_free(sub_bin);
    return false;
}
