/**
 * @file win_capture.c
 * @brief Windows 平台多级鲁棒截屏引擎实现 (支持屏幕绝对坐标 BitBlt、DirectX 硬件加速捕获与后台回退)
 */

#include <stdlib.h>
#include <string.h>
#include "../../include/l2m_platform.h"
#include "../../include/l2m_vision.h"

#ifdef _WIN32

/* 检查内存图像是否全黑/无效 */
static bool is_image_all_black(const uint8_t* p_bits, int32_t w, int32_t h, int32_t stride) {
    if (!p_bits || w <= 0 || h <= 0) return true;

    /* 采样前中后各区域检测是否存在非零像素 */
    int64_t sum = 0;
    int32_t step_y = h > 20 ? (h / 20) : 1;
    int32_t step_x = w > 20 ? (w / 20) : 1;

    for (int32_t y = 0; y < h; y += step_y) {
        const uint8_t* row = p_bits + y * stride;
        for (int32_t x = 0; x < w; x += step_x) {
            sum += row[x * 3 + 0] + row[x * 3 + 1] + row[x * 3 + 2];
            if (sum > 200) {
                return false; /* 存在有效图像像素，非纯黑 */
            }
        }
    }
    return true;
}

bool l2m_capture_window(HWND hwnd, bool client_only, L2MImageBuffer* out_img) {
    if (!hwnd || !out_img) return false;
    if (!IsWindow(hwnd)) return false;

    /* 若窗口被最小化，还原显示 */
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
        Sleep(50);
    }

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int32_t w = rcClient.right - rcClient.left;
    int32_t h = rcClient.bottom - rcClient.top;

    if (w <= 0 || h <= 0) {
        RECT rcWin;
        GetWindowRect(hwnd, &rcWin);
        w = rcWin.right - rcWin.left;
        h = rcWin.bottom - rcWin.top;
    }
    if (w <= 0 || h <= 0) return false;

    /* 转换为屏幕绝对坐标 */
    POINT ptTopLeft = {0, 0};
    if (client_only) {
        ClientToScreen(hwnd, &ptTopLeft);
    } else {
        RECT rcWin;
        GetWindowRect(hwnd, &rcWin);
        ptTopLeft.x = rcWin.left;
        ptTopLeft.y = rcWin.top;
    }

    /* 准备目标内存图像缓冲区 */
    if (out_img->width != w || out_img->height != h || out_img->channels != 3 || !out_img->data) {
        if (out_img->is_owner && out_img->data) free(out_img->data);
        out_img->width = w;
        out_img->height = h;
        out_img->channels = 3;
        out_img->format = L2M_FMT_BGR888;
        out_img->stride = (w * 3 + 3) & ~3;
        out_img->data = (uint8_t*)malloc((size_t)out_img->stride * h);
        out_img->is_owner = true;
        if (!out_img->data) return false;
    }

    HDC hdc_screen = GetDC(NULL);
    if (!hdc_screen) return false;

    HDC hdc_mem = CreateCompatibleDC(hdc_screen);
    if (!hdc_mem) {
        ReleaseDC(NULL, hdc_screen);
        return false;
    }

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; /* Top-down DIB */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* p_bits = NULL;
    HBITMAP hbm = CreateDIBSection(hdc_mem, &bmi, DIB_RGB_COLORS, &p_bits, NULL, 0);
    if (!hbm || !p_bits) {
        DeleteDC(hdc_mem);
        ReleaseDC(NULL, hdc_screen);
        return false;
    }

    HBITMAP old_bm = (HBITMAP)SelectObject(hdc_mem, hbm);
    bool capture_success = false;

    /* 策略 1 (最可靠)：直接从桌面屏幕 DC 中抓取对应客户区坐标 (支持 DirectX/GPU/Vulkan 硬件加速) */
    BOOL blt_ok = BitBlt(hdc_mem, 0, 0, w, h, hdc_screen, ptTopLeft.x, ptTopLeft.y, SRCCOPY | 0x40000000 /* CAPTUREBLT */);
    int32_t stride = (w * 3 + 3) & ~3;

    if (blt_ok && !is_image_all_black((const uint8_t*)p_bits, w, h, stride)) {
        capture_success = true;
    } else {
        /* 策略 2 (回退)：使用 Win10/11 PW_RENDERFULLCONTENT 硬件加速截屏 (支持部分后台/非最小化遮挡) */
        BOOL pw_ok = PrintWindow(hwnd, hdc_mem, 2 /* PW_RENDERFULLCONTENT */);
        if (pw_ok && !is_image_all_black((const uint8_t*)p_bits, w, h, stride)) {
            capture_success = true;
        } else {
            /* 策略 3 (回退)：标准 PrintWindow */
            PrintWindow(hwnd, hdc_mem, 0);
            if (!is_image_all_black((const uint8_t*)p_bits, w, h, stride)) {
                capture_success = true;
            } else {
                /* 策略 4 (回退)：从窗口专属 DC 抓取 */
                HDC hdc_win = GetDC(hwnd);
                if (hdc_win) {
                    BitBlt(hdc_mem, 0, 0, w, h, hdc_win, 0, 0, SRCCOPY);
                    ReleaseDC(hwnd, hdc_win);
                    if (!is_image_all_black((const uint8_t*)p_bits, w, h, stride)) {
                        capture_success = true;
                    }
                }
            }
        }
    }

    /* 拷贝捕获像素至输出缓冲区 */
    for (int32_t y = 0; y < h; y++) {
        const uint8_t* src_row = (const uint8_t*)p_bits + y * stride;
        uint8_t* dst_row = out_img->data + y * out_img->stride;
        memcpy(dst_row, src_row, w * 3);
    }

    SelectObject(hdc_mem, old_bm);
    DeleteObject(hbm);
    DeleteDC(hdc_mem);
    ReleaseDC(NULL, hdc_screen);

    return capture_success;
}

bool l2m_capture_window_roi(HWND hwnd, const L2MRect* roi, L2MImageBuffer* out_img) {
    if (!hwnd || !roi || !out_img || roi->width <= 0 || roi->height <= 0) return false;
    if (!IsWindow(hwnd)) return false;

    POINT ptScreen = {roi->x, roi->y};
    ClientToScreen(hwnd, &ptScreen);

    int32_t w = roi->width;
    int32_t h = roi->height;

    /* 准备输出缓冲区 */
    if (out_img->width != w || out_img->height != h || out_img->channels != 3 || !out_img->data) {
        if (out_img->is_owner && out_img->data) free(out_img->data);
        out_img->width = w;
        out_img->height = h;
        out_img->channels = 3;
        out_img->format = L2M_FMT_BGR888;
        out_img->stride = (w * 3 + 3) & ~3;
        out_img->data = (uint8_t*)malloc((size_t)out_img->stride * h);
        out_img->is_owner = true;
        if (!out_img->data) return false;
    }

    HDC hdc_screen = GetDC(NULL);
    if (!hdc_screen) return false;

    HDC hdc_mem = CreateCompatibleDC(hdc_screen);
    if (!hdc_mem) {
        ReleaseDC(NULL, hdc_screen);
        return false;
    }

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; /* Top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* p_bits = NULL;
    HBITMAP hbm = CreateDIBSection(hdc_mem, &bmi, DIB_RGB_COLORS, &p_bits, NULL, 0);
    if (!hbm || !p_bits) {
        DeleteDC(hdc_mem);
        ReleaseDC(NULL, hdc_screen);
        return false;
    }

    HBITMAP old_bm = (HBITMAP)SelectObject(hdc_mem, hbm);
    BOOL blt_ok = BitBlt(hdc_mem, 0, 0, w, h, hdc_screen, ptScreen.x, ptScreen.y, SRCCOPY | 0x40000000);

    int32_t stride = (w * 3 + 3) & ~3;
    if (blt_ok) {
        for (int32_t y = 0; y < h; y++) {
            const uint8_t* src_row = (const uint8_t*)p_bits + y * stride;
            uint8_t* dst_row = out_img->data + y * out_img->stride;
            memcpy(dst_row, src_row, w * 3);
        }
    }

    SelectObject(hdc_mem, old_bm);
    DeleteObject(hbm);
    DeleteDC(hdc_mem);
    ReleaseDC(NULL, hdc_screen);

    return (blt_ok != FALSE);
}

#endif
