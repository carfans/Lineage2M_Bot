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
