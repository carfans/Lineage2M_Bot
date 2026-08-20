/**
 * @file image_buffer.c
 * @brief 图像像素内存缓冲区管理实现 (纯 C)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/l2m_vision.h"

L2MImageBuffer* l2m_image_create(int32_t width, int32_t height, L2MImageFormat format) {
    if (width <= 0 || height <= 0) {
        return NULL;
    }

    L2MImageBuffer* img = (L2MImageBuffer*)calloc(1, sizeof(L2MImageBuffer));
    if (!img) return NULL;

    img->width = width;
    img->height = height;
    img->format = format;

    switch (format) {
        case L2M_FMT_RGB888:
        case L2M_FMT_BGR888:
            img->channels = 3;
            break;
        case L2M_FMT_BGRA8888:
            img->channels = 4;
            break;
        case L2M_FMT_GRAY8:
        case L2M_FMT_BIN8:
        default:
            img->channels = 1;
            break;
    }

    /* 4 字节对齐 stride */
    img->stride = (width * img->channels + 3) & ~3;
    size_t total_bytes = (size_t)img->stride * height;

    img->data = (uint8_t*)malloc(total_bytes);
    if (!img->data) {
        free(img);
        return NULL;
    }
    memset(img->data, 0, total_bytes);
    img->is_owner = true;

    return img;
}

void l2m_image_free(L2MImageBuffer* img) {
    if (!img) return;
    if (img->is_owner && img->data) {
        free(img->data);
        img->data = NULL;
    }
    free(img);
}

L2MImageBuffer* l2m_image_clone(const L2MImageBuffer* src) {
    if (!src || !src->data) return NULL;

    L2MImageBuffer* dst = l2m_image_create(src->width, src->height, src->format);
    if (!dst) return NULL;

    for (int32_t y = 0; y < src->height; y++) {
        const uint8_t* s_row = src->data + y * src->stride;
        uint8_t* d_row = dst->data + y * dst->stride;
        memcpy(d_row, s_row, src->width * src->channels);
    }
    return dst;
}

bool l2m_image_crop(const L2MImageBuffer* src, const L2MRect* roi, L2MImageBuffer* dst) {
    if (!src || !src->data || !roi || !dst) return false;

    int32_t x = roi->x < 0 ? 0 : roi->x;
    int32_t y = roi->y < 0 ? 0 : roi->y;
    int32_t w = roi->width;
    int32_t h = roi->height;

    if (x >= src->width || y >= src->height) return false;
    if (x + w > src->width) w = src->width - x;
    if (y + h > src->height) h = src->height - y;
    if (w <= 0 || h <= 0) return false;

    /* 若 dst 尺寸与通道不匹配，先重新分配 */
    if (dst->width != w || dst->height != h || dst->channels != src->channels) {
        if (dst->is_owner && dst->data) free(dst->data);
        dst->width = w;
        dst->height = h;
        dst->channels = src->channels;
        dst->format = src->format;
        dst->stride = (w * src->channels + 3) & ~3;
        dst->data = (uint8_t*)malloc((size_t)dst->stride * h);
        dst->is_owner = true;
        if (!dst->data) return false;
    }

    for (int32_t r = 0; r < h; r++) {
        const uint8_t* s_row = src->data + (y + r) * src->stride + x * src->channels;
        uint8_t* d_row = dst->data + r * dst->stride;
        memcpy(d_row, s_row, w * src->channels);
    }
    return true;
}

bool l2m_image_bgr_to_rgb(const L2MImageBuffer* src, L2MImageBuffer* dst) {
    if (!src || !src->data || !dst || src->channels < 3) return false;

    int32_t w = src->width;
    int32_t h = src->height;
    if (w <= 0 || h <= 0) return false;

    /* 若 dst 尺寸与通道不匹配，先重新分配 */
    if (dst->width != w || dst->height != h || dst->channels < 3 || !dst->data) {
        if (dst->is_owner && dst->data) free(dst->data);
        dst->width = w;
        dst->height = h;
        dst->channels = 3;
        dst->format = L2M_FMT_RGB888;
        dst->stride = (w * 3 + 3) & ~3;
        dst->data = (uint8_t*)malloc((size_t)dst->stride * h);
        dst->is_owner = true;
        if (!dst->data) return false;
    }

    for (int32_t y = 0; y < h; y++) {
        const uint8_t* s = src->data + y * src->stride;
        uint8_t* d = dst->data + y * dst->stride;
        for (int32_t x = 0; x < w; x++) {
            uint8_t b = s[x * src->channels + 0];
            uint8_t g = s[x * src->channels + 1];
            uint8_t r = s[x * src->channels + 2];
            d[x * dst->channels + 0] = r;
            d[x * dst->channels + 1] = g;
            d[x * dst->channels + 2] = b;
        }
    }
    dst->format = L2M_FMT_RGB888;
    return true;
}

bool l2m_image_rgb_to_gray(const L2MImageBuffer* src, L2MImageBuffer* dst) {
    if (!src || !src->data || !dst || src->channels < 3) return false;

    int32_t w = src->width;
    int32_t h = src->height;
    if (w <= 0 || h <= 0) return false;

    /* 若 dst 尺寸与通道不匹配，先重新分配 */
    if (dst->width != w || dst->height != h || dst->channels != 1 || !dst->data) {
        if (dst->is_owner && dst->data) free(dst->data);
        dst->width = w;
        dst->height = h;
        dst->channels = 1;
        dst->format = L2M_FMT_GRAY8;
        dst->stride = (w * 1 + 3) & ~3;
        dst->data = (uint8_t*)malloc((size_t)dst->stride * h);
        dst->is_owner = true;
        if (!dst->data) return false;
    }

    for (int32_t y = 0; y < h; y++) {
        const uint8_t* s = src->data + y * src->stride;
        uint8_t* d = dst->data + y * dst->stride;
        for (int32_t x = 0; x < w; x++) {
            uint8_t r = s[x * src->channels + 0];
            uint8_t g = s[x * src->channels + 1];
            uint8_t b = s[x * src->channels + 2];
            /* 快速整数加权灰度转换: (r*77 + g*150 + b*29) >> 8 */
            uint8_t gray = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
            d[x] = gray;
        }
    }
    dst->format = L2M_FMT_GRAY8;
    return true;
}

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} L2MBmpHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} L2MBmpInfoHeader;
#pragma pack(pop)

bool l2m_image_save_bmp(const L2MImageBuffer* img_rgb, const wchar_t* file_path) {
    if (!img_rgb || !img_rgb->data || !file_path) return false;

    FILE* fp = _wfopen(file_path, L"wb");
    if (!fp) return false;

    int32_t w = img_rgb->width;
    int32_t h = img_rgb->height;
    int32_t row_stride = (w * 3 + 3) & ~3;
    uint32_t image_size = (uint32_t)row_stride * h;

    L2MBmpHeader bh;
    bh.bfType = 0x4D42; /* "BM" */
    bh.bfSize = sizeof(L2MBmpHeader) + sizeof(L2MBmpInfoHeader) + image_size;
    bh.bfReserved1 = 0;
    bh.bfReserved2 = 0;
    bh.bfOffBits = sizeof(L2MBmpHeader) + sizeof(L2MBmpInfoHeader);

    L2MBmpInfoHeader bih;
    memset(&bih, 0, sizeof(bih));
    bih.biSize = sizeof(L2MBmpInfoHeader);
    bih.biWidth = w;
    bih.biHeight = h; /* Bottom-up */
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = 0;
    bih.biSizeImage = image_size;

    fwrite(&bh, sizeof(bh), 1, fp);
    fwrite(&bih, sizeof(bih), 1, fp);

    uint8_t* row_buf = (uint8_t*)malloc(row_stride);
    if (!row_buf) {
        fclose(fp);
        return false;
    }

    for (int32_t y = h - 1; y >= 0; y--) {
        const uint8_t* s_row = img_rgb->data + y * img_rgb->stride;
        memset(row_buf, 0, row_stride);
        for (int32_t x = 0; x < w; x++) {
            row_buf[x * 3 + 0] = s_row[x * img_rgb->channels + 2]; /* B */
            row_buf[x * 3 + 1] = s_row[x * img_rgb->channels + 1]; /* G */
            row_buf[x * 3 + 2] = s_row[x * img_rgb->channels + 0]; /* R */
        }
        fwrite(row_buf, 1, row_stride, fp);
    }

    free(row_buf);
    fclose(fp);
    return true;
}

L2MImageBuffer* l2m_image_load_bmp(const wchar_t* file_path) {
    if (!file_path) return NULL;

    FILE* fp = _wfopen(file_path, L"rb");
    if (!fp) return NULL;

    L2MBmpHeader bh;
    if (fread(&bh, sizeof(bh), 1, fp) != 1 || bh.bfType != 0x4D42) {
        fclose(fp);
        return NULL;
    }

    L2MBmpInfoHeader bih;
    if (fread(&bih, sizeof(bih), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    int32_t w = bih.biWidth;
    int32_t h = bih.biHeight > 0 ? bih.biHeight : -bih.biHeight;
    bool is_top_down = (bih.biHeight < 0);

    if (w <= 0 || h <= 0 || bih.biBitCount != 24) {
        fclose(fp);
        return NULL;
    }

    L2MImageBuffer* img = l2m_image_create(w, h, L2M_FMT_RGB888);
    if (!img) {
        fclose(fp);
        return NULL;
    }

    fseek(fp, bh.bfOffBits, SEEK_SET);

    int32_t row_stride = (w * 3 + 3) & ~3;
    uint8_t* row_buf = (uint8_t*)malloc(row_stride);
    if (!row_buf) {
        l2m_image_free(img);
        fclose(fp);
        return NULL;
    }

    for (int32_t i = 0; i < h; i++) {
        int32_t y = is_top_down ? i : (h - 1 - i);
        if (fread(row_buf, 1, row_stride, fp) != (size_t)row_stride) break;

        uint8_t* d_row = img->data + y * img->stride;
        for (int32_t x = 0; x < w; x++) {
            d_row[x * 3 + 0] = row_buf[x * 3 + 2]; /* R */
            d_row[x * 3 + 1] = row_buf[x * 3 + 1]; /* G */
            d_row[x * 3 + 2] = row_buf[x * 3 + 0]; /* B */
        }
    }

    free(row_buf);
    fclose(fp);
    return img;
}
