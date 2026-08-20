/**
 * @file win_debug_dialog.c
 * @brief Lineage2MBot 纯 C Win32 原生交互调试窗口实现 (支持采样点 11x11 像素放大镜、多语言 CBT 特征管理、弹窗配置保存、截图保存与本地图片离线调试)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#include <windows.h>
#include <commdlg.h>
#include "../../include/l2m_gui.h"
#include "../../include/l2m_cbt.h"
#include "../../include/l2m_zone.h"

#ifdef _WIN32

#define ID_BTN_CAPTURE       1001
#define ID_BTN_DETECT_POPUP  1002
#define ID_BTN_TEST_CLICK    1003
#define ID_BTN_TEST_HP       1004
#define ID_BTN_DETECT_MAP    1005
#define ID_CB_POPUP_TYPE     1005
#define ID_TXT_POPUP_RECT    1006
#define ID_TXT_STATUS        1007
#define ID_CANVAS_VIEW       1008
#define ID_BTN_SAVE_POPUP    1009

/* 命名弹窗管理扩展控件 ID */
#define ID_TXT_POPUP_NAME    1030
#define ID_TXT_POPUP_DESC    1031
#define ID_CB_POPUP_LINK_CBT 1032
#define ID_BTN_NEW_POPUP     1033
#define ID_BTN_DEL_POPUP     1034

/* CBT 采样点管理控件 ID */
#define ID_CB_CBT_REGION     1010
#define ID_CB_CBT_POINTS     1011
#define ID_TXT_CBT_KEY       1012
#define ID_TXT_CBT_POS       1013
#define ID_TXT_CBT_RGB       1014
#define ID_TXT_CBT_TOL       1015
#define ID_BTN_CBT_APPLY_PT  1016
#define ID_BTN_CBT_SAVE_JSON 1017
#define ID_BTN_CBT_TEST_PT   1018
#define ID_BTN_CBT_DEL_PT    1019

/* 截图保存与载入控件 ID */
#define ID_BTN_SAVE_IMAGE    1020
#define ID_BTN_LOAD_IMAGE    1021
#define ID_ZOOM_VIEW         1022

static HWND g_hDebugWnd = NULL;
static HWND g_hTargetGameWnd = NULL;
static HWND g_hCanvas = NULL;
static HWND g_hZoomCanvas = NULL;
static HWND g_hZoomInfoLbl = NULL;
static HWND g_hStatusText = NULL;
static HWND g_hPopupRectTxt = NULL;
static HWND g_hPopupTypeCb = NULL;
static HWND g_hPopupNameTxt = NULL;
static HWND g_hPopupDescTxt = NULL;
static HWND g_hPopupLinkedCbtCb = NULL;
static HWND g_hColorInfoLbl = NULL;
static HFONT g_hFontDebugUI = NULL;
static HFONT g_hFontBoldUI = NULL;

/* CBT 管理控件句柄 */
static HWND g_hCbtRegionCb = NULL;
static HWND g_hCbtPointsCb = NULL;
static HWND g_hCbtKeyTxt = NULL;
static HWND g_hCbtPosTxt = NULL;
static HWND g_hCbtRgbTxt = NULL;
static HWND g_hCbtTolTxt = NULL;

static L2MCbtConfig g_current_cbt_cfg;

static L2MImageBuffer* g_current_frame_bgr = NULL;
static L2MImageBuffer* g_current_frame_rgb = NULL;
static HBITMAP g_hCurrentBmp = NULL;

/* 弹窗高亮叠加 */
static bool g_has_popup_overlay = false;
static L2MRect g_overlay_scan_rect = {0};
static L2MPoint g_overlay_btn_pt = {0};
static L2MRect g_overlay_btn_bbox = {0};
static bool g_overlay_has_cb = false;
static L2MPoint g_overlay_cb_pt = {0};

/* 地图框与区域高亮叠加 */
static bool g_has_map_overlay = false;
static L2MMapBoxResult g_last_map_result = {0};

/* 鼠标交互取点取色 */
static L2MPoint g_last_pick_pt = {-1, -1};
static L2MRGB g_last_pick_rgb = {0, 0, 0};

/* 当前高亮选中的 CBT 采样点与放大镜中心点 */
static bool g_has_selected_cbt_pt = false;
static L2MPoint g_selected_cbt_pt = {-1, -1};
static L2MPoint g_zoom_center_pt = {-1, -1};

static void update_debug_window_title(void);

/* 递归设置字体 */
static void apply_debug_ui_font(HWND hWnd, HFONT hFont) {
    if (!hWnd || !hFont) return;
    SendMessageW(hWnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    HWND hChild = GetWindow(hWnd, GW_CHILD);
    while (hChild) {
        SendMessageW(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

/* 释放当前帧位图 */
static void free_current_frame(void) {
    if (g_hCurrentBmp) {
        DeleteObject(g_hCurrentBmp);
        g_hCurrentBmp = NULL;
    }
    if (g_current_frame_bgr) {
        l2m_image_free(g_current_frame_bgr);
        g_current_frame_bgr = NULL;
    }
    if (g_current_frame_rgb) {
        l2m_image_free(g_current_frame_rgb);
        g_current_frame_rgb = NULL;
    }
}

/* 更新画板显示 GDI 位图 */
static void update_canvas_bitmap_from_rgb(void) {
    if (!g_current_frame_rgb || !g_hCanvas) return;

    if (g_hCurrentBmp) {
        DeleteObject(g_hCurrentBmp);
        g_hCurrentBmp = NULL;
    }

    HDC hdc = GetDC(g_hCanvas);
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_current_frame_rgb->width;
    bmi.bmiHeader.biHeight = -g_current_frame_rgb->height; /* Top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    g_hCurrentBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (g_hCurrentBmp && bits) {
        int32_t w = g_current_frame_rgb->width;
        int32_t h = g_current_frame_rgb->height;
        int32_t stride = (w * 3 + 3) & ~3;
        uint8_t* d_ptr = (uint8_t*)bits;

        for (int32_t y = 0; y < h; y++) {
            const uint8_t* s_row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
            uint8_t* d_row = d_ptr + y * stride;
            for (int32_t x = 0; x < w; x++) {
                d_row[x * 3 + 0] = s_row[x * 3 + 2]; /* B */
                d_row[x * 3 + 1] = s_row[x * 3 + 1]; /* G */
                d_row[x * 3 + 2] = s_row[x * 3 + 0]; /* R */
            }
        }
    }
    ReleaseDC(g_hCanvas, hdc);

    InvalidateRect(g_hCanvas, NULL, FALSE);
    if (g_hZoomCanvas) InvalidateRect(g_hZoomCanvas, NULL, FALSE);
}

/* 刷新采样点放大镜小画板 (11x11 像素放大 10 倍) */
static void on_paint_zoom_canvas(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);
    int zw = rc.right - rc.left;
    int zh = rc.bottom - rc.top;

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, zw, zh);
    HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

    HBRUSH hBrBg = CreateSolidBrush(RGB(32, 34, 38));
    FillRect(hdcMem, &rc, hBrBg);
    DeleteObject(hBrBg);

    /* 获取当前要放大的目标中心点 */
    int cx = g_zoom_center_pt.x;
    int cy = g_zoom_center_pt.y;

    const int GRID_SIZE = 11; /* 11x11 邻域 */
    const int BLOCK_SIZE = 10; /* 每个像素放大为 10x10 px */

    if (g_current_frame_rgb && cx >= 0 && cy >= 0) {
        int half = GRID_SIZE / 2;
        int img_w = g_current_frame_rgb->width;
        int img_h = g_current_frame_rgb->height;

        HPEN hPenGrid = CreatePen(PS_SOLID, 1, RGB(55, 58, 65));
        HGDIOBJ hOldPen = SelectObject(hdcMem, hPenGrid);

        for (int gy = 0; gy < GRID_SIZE; gy++) {
            for (int gx = 0; gx < GRID_SIZE; gx++) {
                int src_x = cx - half + gx;
                int src_y = cy - half + gy;

                COLORREF col = RGB(20, 20, 20);
                if (src_x >= 0 && src_x < img_w && src_y >= 0 && src_y < img_h) {
                    const uint8_t* row = g_current_frame_rgb->data + src_y * g_current_frame_rgb->stride;
                    col = RGB(row[src_x * 3 + 0], row[src_x * 3 + 1], row[src_x * 3 + 2]);
                }

                HBRUSH hBr = CreateSolidBrush(col);
                RECT block_rc = { gx * BLOCK_SIZE, gy * BLOCK_SIZE, (gx + 1) * BLOCK_SIZE, (gy + 1) * BLOCK_SIZE };
                FillRect(hdcMem, &block_rc, hBr);
                DeleteObject(hBr);

                /* 绘制网格边框 */
                MoveToEx(hdcMem, block_rc.left, block_rc.top, NULL);
                LineTo(hdcMem, block_rc.right, block_rc.top);
                LineTo(hdcMem, block_rc.right, block_rc.bottom);
                LineTo(hdcMem, block_rc.left, block_rc.bottom);
                LineTo(hdcMem, block_rc.left, block_rc.top);
            }
        }

        /* 在正中心 (half, half) 绘制明亮高亮准星与黄色方框 */
        int center_left = half * BLOCK_SIZE;
        int center_top = half * BLOCK_SIZE;
        HPEN hPenCenter = CreatePen(PS_SOLID, 2, RGB(255, 220, 0));
        SelectObject(hdcMem, hPenCenter);
        SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
        Rectangle(hdcMem, center_left, center_top, center_left + BLOCK_SIZE, center_top + BLOCK_SIZE);

        /* 准星十字 */
        HPEN hPenCross = CreatePen(PS_SOLID, 1, RGB(255, 60, 60));
        SelectObject(hdcMem, hPenCross);
        int mid_x = center_left + BLOCK_SIZE / 2;
        int mid_y = center_top + BLOCK_SIZE / 2;
        MoveToEx(hdcMem, mid_x - 12, mid_y, NULL); LineTo(hdcMem, mid_x + 13, mid_y);
        MoveToEx(hdcMem, mid_x, mid_y - 12, NULL); LineTo(hdcMem, mid_x, mid_y + 13);

        SelectObject(hdcMem, hOldPen);
        DeleteObject(hPenGrid);
        DeleteObject(hPenCenter);
        DeleteObject(hPenCross);
    } else {
        SetTextColor(hdcMem, RGB(140, 140, 140));
        SetBkMode(hdcMem, TRANSPARENT);
        SelectObject(hdcMem, g_hFontDebugUI);
        TextOutW(hdcMem, 10, 45, L"请选择点位", 5);
    }

    BitBlt(hdc, 0, 0, zw, zh, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOldBm);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);

    EndPaint(hWnd, &ps);
}

/* 放大镜点击点选像素 */
static void on_zoom_canvas_click(int mouse_x, int mouse_y) {
    if (!g_current_frame_rgb || !g_hZoomCanvas) return;
    if (g_zoom_center_pt.x < 0 || g_zoom_center_pt.y < 0) return;

    const int GRID_SIZE = 11;
    const int BLOCK_SIZE = 10;
    int half = GRID_SIZE / 2;

    int gx = mouse_x / BLOCK_SIZE;
    int gy = mouse_y / BLOCK_SIZE;

    if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) return;

    int target_x = g_zoom_center_pt.x - half + gx;
    int target_y = g_zoom_center_pt.y - half + gy;

    int img_w = g_current_frame_rgb->width;
    int img_h = g_current_frame_rgb->height;

    if (target_x < 0 || target_x >= img_w || target_y < 0 || target_y >= img_h) return;

    g_last_pick_pt.x = target_x;
    g_last_pick_pt.y = target_y;
    g_zoom_center_pt.x = target_x;
    g_zoom_center_pt.y = target_y;
    g_has_selected_cbt_pt = false;

    const uint8_t* row = g_current_frame_rgb->data + target_y * g_current_frame_rgb->stride;
    g_last_pick_rgb.r = row[target_x * 3 + 0];
    g_last_pick_rgb.g = row[target_x * 3 + 1];
    g_last_pick_rgb.b = row[target_x * 3 + 2];

    wchar_t buf[128];
    swprintf(buf, sizeof(buf)/sizeof(wchar_t), L"📍 放大图点选: (%d, %d) | RGB: (%d, %d, %d) #%02X%02X%02X",
             target_x, target_y, g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b,
             g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b);
    SetWindowTextW(g_hColorInfoLbl, buf);

    /* 同步坐标 X,Y 数据框与 RGB 目标框 */
    wchar_t pos_str[64], rgb_str[64];
    swprintf(pos_str, sizeof(pos_str)/sizeof(wchar_t), L"%d, %d", target_x, target_y);
    swprintf(rgb_str, sizeof(rgb_str)/sizeof(wchar_t), L"%d, %d, %d", g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b);
    if (g_hCbtPosTxt) SetWindowTextW(g_hCbtPosTxt, pos_str);
    if (g_hCbtRgbTxt) SetWindowTextW(g_hCbtRgbTxt, rgb_str);

    wchar_t zinfo[128];
    swprintf(zinfo, sizeof(zinfo)/sizeof(wchar_t), L"🔍 放大镜 (10x 放大) - 当前拾取中心: (%d, %d) | RGB(%d, %d, %d)",
             target_x, target_y, g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b);
    SetWindowTextW(g_hZoomInfoLbl, zinfo);

    if (g_hCanvas) {
        InvalidateRect(g_hCanvas, NULL, FALSE);
        UpdateWindow(g_hCanvas);
    }
    if (g_hZoomCanvas) {
        InvalidateRect(g_hZoomCanvas, NULL, FALSE);
        UpdateWindow(g_hZoomCanvas);
    }
}

/* 放大镜控件窗口过程 */
static LRESULT CALLBACK ZoomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            on_paint_zoom_canvas(hWnd);
            return 0;
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            on_zoom_canvas_click(x, y);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (wParam & MK_LBUTTON) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                on_zoom_canvas_click(x, y);
            }
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

/* 刷新弹窗关联 CBT 采样点下拉框 */
static void refresh_popup_linked_cbt_ui(const char* select_key) {
    if (!g_hPopupLinkedCbtCb) return;
    SendMessageW(g_hPopupLinkedCbtCb, CB_RESETCONTENT, 0, 0);

    /* 第 0 项: 无关联 */
    SendMessageW(g_hPopupLinkedCbtCb, CB_ADDSTRING, 0, (LPARAM)L"(无关联 CBT 采样点)");

    int select_idx = 0;
    for (int i = 0; i < g_current_cbt_cfg.count; i++) {
        wchar_t wkey[64];
        MultiByteToWideChar(CP_UTF8, 0, g_current_cbt_cfg.points[i].key, -1, wkey, 64);
        SendMessageW(g_hPopupLinkedCbtCb, CB_ADDSTRING, 0, (LPARAM)wkey);

        if (select_key && strcmp(g_current_cbt_cfg.points[i].key, select_key) == 0) {
            select_idx = i + 1;
        }
    }

    SendMessageW(g_hPopupLinkedCbtCb, CB_SETCURSEL, select_idx, 0);
}

/* 联动更新选中的弹窗配置到界面编辑框 */
static void sync_popup_item_selection(int idx) {
    if (idx < 0 || idx >= g_current_cbt_cfg.popup_cfg.count) return;
    const L2MPopupItem* item = &g_current_cbt_cfg.popup_cfg.items[idx];

    wchar_t wbuf[256];
    MultiByteToWideChar(CP_UTF8, 0, item->name, -1, wbuf, 64);
    SetWindowTextW(g_hPopupNameTxt, wbuf);

    MultiByteToWideChar(CP_UTF8, 0, item->desc, -1, wbuf, 128);
    SetWindowTextW(g_hPopupDescTxt, wbuf);

    swprintf(wbuf, sizeof(wbuf)/sizeof(wchar_t), L"%d, %d, %d, %d", item->x, item->y, item->width, item->height);
    SetWindowTextW(g_hPopupRectTxt, wbuf);

    refresh_popup_linked_cbt_ui(item->linked_cbt_key);
}

/* 刷新命名弹窗列表下拉框 */
static void refresh_popup_list_ui(const char* select_name) {
    if (!g_hPopupTypeCb) return;
    SendMessageW(g_hPopupTypeCb, CB_RESETCONTENT, 0, 0);

    int select_idx = -1;
    for (int i = 0; i < g_current_cbt_cfg.popup_cfg.count; i++) {
        const L2MPopupItem* item = &g_current_cbt_cfg.popup_cfg.items[i];
        wchar_t wname[64] = {0};
        wchar_t wdesc[128] = {0};
        MultiByteToWideChar(CP_UTF8, 0, item->name, -1, wname, 64);
        MultiByteToWideChar(CP_UTF8, 0, item->desc, -1, wdesc, 128);

        wchar_t item_text[256];
        if (wdesc[0]) {
            swprintf(item_text, sizeof(item_text)/sizeof(wchar_t), L"%ls (%ls)", wname, wdesc);
        } else {
            swprintf(item_text, sizeof(item_text)/sizeof(wchar_t), L"%ls", wname);
        }
        SendMessageW(g_hPopupTypeCb, CB_ADDSTRING, 0, (LPARAM)item_text);

        if (select_name && strcmp(item->name, select_name) == 0) {
            select_idx = i;
        }
    }

    if (select_idx >= 0) {
        SendMessageW(g_hPopupTypeCb, CB_SETCURSEL, select_idx, 0);
        sync_popup_item_selection(select_idx);
    } else if (g_current_cbt_cfg.popup_cfg.count > 0) {
        SendMessageW(g_hPopupTypeCb, CB_SETCURSEL, 0, 0);
        sync_popup_item_selection(0);
    }
}

/* 新建弹窗输入准备 */
static void create_new_popup_item_ui(void) {
    SetWindowTextW(g_hPopupNameTxt, L"");
    SetWindowTextW(g_hPopupDescTxt, L"自定义弹窗");
    SetWindowTextW(g_hPopupRectTxt, L"280, 150, 400, 240");
    if (g_hPopupLinkedCbtCb) SendMessageW(g_hPopupLinkedCbtCb, CB_SETCURSEL, 0, 0);
    SetFocus(g_hPopupNameTxt);
    SetWindowTextW(g_hStatusText, L"✨ 已重置输入框。请输入新弹窗名称(Key)与描述，设置ROI区域后点击【💾 保存/更新】即可新建弹窗。");
}

/* 刷新 CBT 采样点列表下拉框 */
static void refresh_cbt_points_ui(const char* select_key) {
    if (!g_hCbtPointsCb) return;
    SendMessageW(g_hCbtPointsCb, CB_RESETCONTENT, 0, 0);

    int select_idx = -1;
    for (int i = 0; i < g_current_cbt_cfg.count; i++) {
        wchar_t wkey[64];
        MultiByteToWideChar(CP_UTF8, 0, g_current_cbt_cfg.points[i].key, -1, wkey, 64);
        SendMessageW(g_hCbtPointsCb, CB_ADDSTRING, 0, (LPARAM)wkey);

        if (select_key && strcmp(g_current_cbt_cfg.points[i].key, select_key) == 0) {
            select_idx = i;
        }
    }

    if (select_idx >= 0) {
        SendMessageW(g_hCbtPointsCb, CB_SETCURSEL, select_idx, 0);
    } else if (g_current_cbt_cfg.count > 0) {
        SendMessageW(g_hCbtPointsCb, CB_SETCURSEL, 0, 0);
    }
}

/* 联动更新采样点信息与放大镜 */
static void sync_cbt_point_selection(int idx) {
    if (idx < 0 || idx >= g_current_cbt_cfg.count) return;
    const L2MCbtPoint* pt = &g_current_cbt_cfg.points[idx];
    wchar_t wbuf[64];

    MultiByteToWideChar(CP_UTF8, 0, pt->key, -1, wbuf, 64);
    SetWindowTextW(g_hCbtKeyTxt, wbuf);

    swprintf(wbuf, sizeof(wbuf)/sizeof(wchar_t), L"%d, %d", pt->x, pt->y);
    SetWindowTextW(g_hCbtPosTxt, wbuf);

    if (pt->has_rgb) {
        swprintf(wbuf, sizeof(wbuf)/sizeof(wchar_t), L"%d, %d, %d", pt->r, pt->g, pt->b);
    } else {
        wcscpy_s(wbuf, sizeof(wbuf)/sizeof(wchar_t), L"null");
    }
    SetWindowTextW(g_hCbtRgbTxt, wbuf);

    swprintf(wbuf, sizeof(wbuf)/sizeof(wchar_t), L"%d", pt->tolerance > 0 ? pt->tolerance : 12);
    SetWindowTextW(g_hCbtTolTxt, wbuf);

    g_has_selected_cbt_pt = true;
    g_selected_cbt_pt.x = pt->x;
    g_selected_cbt_pt.y = pt->y;
    g_zoom_center_pt.x = pt->x;
    g_zoom_center_pt.y = pt->y;
    g_last_pick_pt = (L2MPoint){-1, -1}; /* 清空鼠标点，让放大镜与画板严格聚焦当前 CBT 点位 */

    /* 更新放大镜提示文字 */
    wchar_t zinfo[128];
    if (pt->has_rgb) {
        swprintf(zinfo, sizeof(zinfo)/sizeof(wchar_t), L"🔍 放大镜 (10x 放大) - 采样中心: (%d, %d) | 目标: RGB(%d, %d, %d)",
                 pt->x, pt->y, pt->r, pt->g, pt->b);
    } else {
        swprintf(zinfo, sizeof(zinfo)/sizeof(wchar_t), L"🔍 放大镜 (10x 放大) - 采样中心: (%d, %d) [仅坐标]", pt->x, pt->y);
    }
    SetWindowTextW(g_hZoomInfoLbl, zinfo);

    if (g_hCanvas) InvalidateRect(g_hCanvas, NULL, FALSE);
    if (g_hZoomCanvas) InvalidateRect(g_hZoomCanvas, NULL, FALSE);
}

/* 加载并切换当前语言 CBT 配置 */
static void switch_cbt_region(const char* region) {
    l2m_cbt_load(region, &g_current_cbt_cfg);
    refresh_cbt_points_ui(NULL);
    refresh_popup_list_ui(NULL);

    if (g_current_cbt_cfg.count > 0) {
        sync_cbt_point_selection(0);
    }
}

/* 捕获最新画面 */
static bool capture_game_screen(void) {
    free_current_frame();

    if (!g_hTargetGameWnd || !IsWindow(g_hTargetGameWnd)) {
        g_current_frame_rgb = l2m_image_create(960, 540, L2M_FMT_RGB888);
        if (!g_current_frame_rgb) return false;

        /* 背景填暗黑灰 */
        for (int y = 0; y < 540; y++) {
            uint8_t* row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
            for (int x = 0; x < 960; x++) {
                row[x * 3 + 0] = 30; row[x * 3 + 1] = 32; row[x * 3 + 2] = 36;
            }
        }
        /* 绘制模拟左上角提示弹窗 (10, 10, 260, 150) */
        for (int y = 10; y < 160; y++) {
            uint8_t* row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
            for (int x = 10; x < 270; x++) {
                row[x * 3 + 0] = 42; row[x * 3 + 1] = 46; row[x * 3 + 2] = 52;
            }
        }
        /* 橙色按钮 */
        for (int y = 110; y < 140; y++) {
            uint8_t* row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
            for (int x = 150; x < 230; x++) {
                row[x * 3 + 0] = 220; row[x * 3 + 1] = 115; row[x * 3 + 2] = 10;
            }
        }
        /* 灰色确认按钮 */
        for (int y = 110; y < 140; y++) {
            uint8_t* row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
            for (int x = 50; x < 130; x++) {
                row[x * 3 + 0] = 80; row[x * 3 + 1] = 85; row[x * 3 + 2] = 90;
            }
        }
        /* 勾选框 */
        for (int y = 75; y < 90; y++) {
            uint8_t* row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
            for (int x = 60; x < 75; x++) {
                row[x * 3 + 0] = 180; row[x * 3 + 1] = 180; row[x * 3 + 2] = 180;
            }
        }
        /* 血条 (64, 21, 103, 2) */
        for (int y = 21; y < 23; y++) {
            uint8_t* row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
            for (int x = 64; x < 140; x++) {
                row[x * 3 + 0] = 170; row[x * 3 + 1] = 40; row[x * 3 + 2] = 10;
            }
        }
        /* 回城卷轴 (217, 487) */
        for (int y = 475; y < 500; y++) {
            uint8_t* row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
            for (int x = 205; x < 230; x++) {
                row[x * 3 + 0] = 174; row[x * 3 + 1] = 149; row[x * 3 + 2] = 130;
            }
        }
    } else {
        g_current_frame_bgr = l2m_image_create(960, 540, L2M_FMT_BGR888);
        if (!l2m_capture_window(g_hTargetGameWnd, true, g_current_frame_bgr)) {
            return false;
        }
        g_current_frame_rgb = l2m_image_create(g_current_frame_bgr->width, g_current_frame_bgr->height, L2M_FMT_RGB888);
        l2m_image_bgr_to_rgb(g_current_frame_bgr, g_current_frame_rgb);
    }

    update_canvas_bitmap_from_rgb();
    return true;
}

/* 保存当前画面截图至本地磁盘 */
static void save_current_screenshot(void) {
    if (!g_current_frame_rgb) {
        MessageBoxW(g_hDebugWnd, L"当前没有可保存的画面截图！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    wchar_t default_name[MAX_PATH];
    swprintf(default_name, sizeof(default_name)/sizeof(wchar_t),
             L"screenshot_%04d%02d%02d_%02d%02d%02d.bmp",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"";
    wcscpy_s(szFile, MAX_PATH, default_name);

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hDebugWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"BMP 位图文件 (*.bmp)\0*.bmp\0所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"bmp";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn)) {
        if (l2m_image_save_bmp(g_current_frame_rgb, ofn.lpstrFile)) {
            wchar_t msg[512];
            swprintf(msg, sizeof(msg)/sizeof(wchar_t), L"✅ 截图已成功保存至:\r\n%ls", ofn.lpstrFile);
            MessageBoxW(g_hDebugWnd, msg, L"截图保存成功", MB_OK | MB_ICONINFORMATION);
            SetWindowTextW(g_hStatusText, msg);
        } else {
            MessageBoxW(g_hDebugWnd, L"❌ 保存截图文件失败！", L"错误", MB_OK | MB_ICONERROR);
        }
    }
}

/* 手动载入本地图片文件进行离线调试 */
static void load_local_image_file(void) {
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"";

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hDebugWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"BMP 位图文件 (*.bmp)\0*.bmp\0所有文件 (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        L2MImageBuffer* loaded = l2m_image_load_bmp(ofn.lpstrFile);
        if (!loaded) {
            MessageBoxW(g_hDebugWnd, L"❌ 无法解析该图片文件，请确保文件为标准的 24 位 BMP 图像格式！", L"加载失败", MB_OK | MB_ICONERROR);
            return;
        }

        free_current_frame();
        g_current_frame_rgb = loaded;
        update_canvas_bitmap_from_rgb();

        wchar_t msg[512];
        swprintf(msg, sizeof(msg)/sizeof(wchar_t), L"📂 已成功载入本地调试图片:\r\n%ls\r\n尺寸: %dx%d px",
                 ofn.lpstrFile, loaded->width, loaded->height);
        SetWindowTextW(g_hStatusText, msg);
    }
}

/* 绘制右侧主画面 (GDI 双缓冲) */
static void on_paint_canvas(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, cw, ch);
    HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

    HBRUSH hBrBg = CreateSolidBrush(RGB(24, 24, 28));
    FillRect(hdcMem, &rc, hBrBg);
    DeleteObject(hBrBg);

    if (g_hCurrentBmp && g_current_frame_rgb) {
        HDC hdcSrc = CreateCompatibleDC(hdc);
        HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcSrc, g_hCurrentBmp);

        int img_w = g_current_frame_rgb->width;
        int img_h = g_current_frame_rgb->height;

        SetStretchBltMode(hdcMem, HALFTONE);
        StretchBlt(hdcMem, 0, 0, cw, ch, hdcSrc, 0, 0, img_w, img_h, SRCCOPY);

        float scale_x = (float)cw / (float)img_w;
        float scale_y = (float)ch / (float)img_h;

        /* 绘制弹窗高亮叠加层 */
        if (g_has_popup_overlay) {
            HPEN hPenScan = CreatePen(PS_DOT, 1, RGB(255, 204, 0));
            HGDIOBJ hOldPen = SelectObject(hdcMem, hPenScan);
            SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
            Rectangle(hdcMem,
                      (int)(g_overlay_scan_rect.x * scale_x),
                      (int)(g_overlay_scan_rect.y * scale_y),
                      (int)((g_overlay_scan_rect.x + g_overlay_scan_rect.width) * scale_x),
                      (int)((g_overlay_scan_rect.y + g_overlay_scan_rect.height) * scale_y));

            HPEN hPenBtn = CreatePen(PS_SOLID, 2, RGB(0, 255, 128));
            SelectObject(hdcMem, hPenBtn);
            Rectangle(hdcMem,
                      (int)(g_overlay_btn_bbox.x * scale_x),
                      (int)(g_overlay_btn_bbox.y * scale_y),
                      (int)((g_overlay_btn_bbox.x + g_overlay_btn_bbox.width) * scale_x),
                      (int)((g_overlay_btn_bbox.y + g_overlay_btn_bbox.height) * scale_y));

            HPEN hPenCross = CreatePen(PS_SOLID, 2, RGB(255, 48, 48));
            SelectObject(hdcMem, hPenCross);
            int cx = (int)(g_overlay_btn_pt.x * scale_x);
            int cy = (int)(g_overlay_btn_pt.y * scale_y);
            MoveToEx(hdcMem, cx - 8, cy, NULL); LineTo(hdcMem, cx + 9, cy);
            MoveToEx(hdcMem, cx, cy - 8, NULL); LineTo(hdcMem, cx, cy + 9);

            if (g_overlay_has_cb) {
                HPEN hPenCb = CreatePen(PS_SOLID, 2, RGB(0, 229, 255));
                SelectObject(hdcMem, hPenCb);
                int kx = (int)(g_overlay_cb_pt.x * scale_x);
                int ky = (int)(g_overlay_cb_pt.y * scale_y);
                Rectangle(hdcMem, kx - 7, ky - 7, kx + 8, ky + 8);
                DeleteObject(hPenCb);
            }

            SelectObject(hdcMem, hOldPen);
            DeleteObject(hPenScan);
            DeleteObject(hPenBtn);
            DeleteObject(hPenCross);
        }

        /* 绘制地图框与区域高亮叠加层 */
        if (g_has_map_overlay && g_last_map_result.detected) {
            COLORREF zone_col = RGB(255, 204, 0);
            if (g_last_map_result.zone_type == L2M_ZONE_SAFETY) {
                zone_col = RGB(0, 255, 128); /* 安全区域：高亮绿 */
            } else if (g_last_map_result.zone_type == L2M_ZONE_COMBAT) {
                zone_col = RGB(255, 48, 48);  /* 战斗区域：高亮红 */
            }

            HPEN hPenMap = CreatePen(PS_SOLID, 2, zone_col);
            HGDIOBJ hOldPen = SelectObject(hdcMem, hPenMap);
            SelectObject(hdcMem, GetStockObject(NULL_BRUSH));

            int mx = (int)(g_last_map_result.map_rect.x * scale_x);
            int my = (int)(g_last_map_result.map_rect.y * scale_y);
            int mw = (int)(g_last_map_result.map_rect.width * scale_x);
            int mh = (int)(g_last_map_result.map_rect.height * scale_y);

            Rectangle(hdcMem, mx, my, mx + mw, my + mh);

            SetTextColor(hdcMem, zone_col);
            SetBkMode(hdcMem, TRANSPARENT);
            SelectObject(hdcMem, g_hFontBoldUI);
            wchar_t w_tag[64];
            swprintf(w_tag, sizeof(w_tag)/sizeof(wchar_t), L"[%hs] %.1f%%",
                     g_last_map_result.zone_name, g_last_map_result.confidence);
            TextOutW(hdcMem, mx + 4, my + mh + 2, w_tag, (int)wcslen(w_tag));

            SelectObject(hdcMem, hOldPen);
            DeleteObject(hPenMap);
        }

        /* 绘制当前选中的 CBT 采样点 */
        if (g_has_selected_cbt_pt && g_selected_cbt_pt.x >= 0 && g_selected_cbt_pt.y >= 0) {
            HPEN hPenCbt = CreatePen(PS_SOLID, 2, RGB(255, 0, 200));
            HGDIOBJ hOldPen = SelectObject(hdcMem, hPenCbt);
            SelectObject(hdcMem, GetStockObject(NULL_BRUSH));

            int kx = (int)(g_selected_cbt_pt.x * scale_x);
            int ky = (int)(g_selected_cbt_pt.y * scale_y);

            Rectangle(hdcMem, kx - 6, ky - 6, kx + 7, ky + 7);
            MoveToEx(hdcMem, kx - 10, ky, NULL); LineTo(hdcMem, kx + 11, ky);
            MoveToEx(hdcMem, kx, ky - 10, NULL); LineTo(hdcMem, kx, ky + 11);

            SelectObject(hdcMem, hOldPen);
            DeleteObject(hPenCbt);
        }

        /* 绘制鼠标交互选点标记 */
        if (g_last_pick_pt.x >= 0 && g_last_pick_pt.y >= 0) {
            HPEN hPenPick = CreatePen(PS_SOLID, 1, RGB(255, 255, 0));
            HGDIOBJ hOldPen = SelectObject(hdcMem, hPenPick);
            int px = (int)(g_last_pick_pt.x * scale_x);
            int py = (int)(g_last_pick_pt.y * scale_y);
            MoveToEx(hdcMem, px - 6, py, NULL); LineTo(hdcMem, px + 7, py);
            MoveToEx(hdcMem, px, py - 6, NULL); LineTo(hdcMem, px, py + 7);
            SelectObject(hdcMem, hOldPen);
            DeleteObject(hPenPick);
        }

        SelectObject(hdcSrc, hOldSrc);
        DeleteDC(hdcSrc);
    } else {
        SetTextColor(hdcMem, RGB(160, 160, 160));
        SetBkMode(hdcMem, TRANSPARENT);
        SelectObject(hdcMem, g_hFontDebugUI);
        TextOutW(hdcMem, 350, 240, L"请点击左侧【📸 捕获实时画面】或【📂 载入本地图片】载入图像", 32);
    }

    BitBlt(hdc, 0, 0, cw, ch, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOldBm);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);

    EndPaint(hWnd, &ps);
}

/* 画板鼠标点击取点取色 */
static void on_canvas_click(int mouse_x, int mouse_y) {
    if (!g_current_frame_rgb || !g_hCanvas) return;

    RECT rc;
    GetClientRect(g_hCanvas, &rc);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) return;

    int img_w = g_current_frame_rgb->width;
    int img_h = g_current_frame_rgb->height;

    int x = (int)((float)mouse_x * (float)img_w / (float)cw);
    int y = (int)((float)mouse_y * (float)img_h / (float)ch);

    if (x < 0 || x >= img_w || y < 0 || y >= img_h) return;

    g_last_pick_pt.x = x;
    g_last_pick_pt.y = y;
    g_zoom_center_pt.x = x;
    g_zoom_center_pt.y = y;
    g_has_selected_cbt_pt = false; /* 鼠标拾取时优先展示鼠标点准星 */

    const uint8_t* row = g_current_frame_rgb->data + y * g_current_frame_rgb->stride;
    g_last_pick_rgb.r = row[x * 3 + 0];
    g_last_pick_rgb.g = row[x * 3 + 1];
    g_last_pick_rgb.b = row[x * 3 + 2];

    wchar_t buf[128];
    swprintf(buf, sizeof(buf)/sizeof(wchar_t), L"📍 鼠标取点: (%d, %d) | RGB: (%d, %d, %d) #%02X%02X%02X",
             x, y, g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b,
             g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b);
    SetWindowTextW(g_hColorInfoLbl, buf);

    /* 同步坐标 X,Y 数据框与 RGB 目标框 */
    wchar_t pos_str[64], rgb_str[64];
    swprintf(pos_str, sizeof(pos_str)/sizeof(wchar_t), L"%d, %d", x, y);
    swprintf(rgb_str, sizeof(rgb_str)/sizeof(wchar_t), L"%d, %d, %d", g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b);
    if (g_hCbtPosTxt) SetWindowTextW(g_hCbtPosTxt, pos_str);
    if (g_hCbtRgbTxt) SetWindowTextW(g_hCbtRgbTxt, rgb_str);

    wchar_t zinfo[128];
    swprintf(zinfo, sizeof(zinfo)/sizeof(wchar_t), L"🔍 放大镜 (10x 放大) - 当前拾取中心: (%d, %d) | RGB(%d, %d, %d)",
             x, y, g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b);
    SetWindowTextW(g_hZoomInfoLbl, zinfo);

    if (g_hCanvas) {
        InvalidateRect(g_hCanvas, NULL, FALSE);
        UpdateWindow(g_hCanvas);
    }
    if (g_hZoomCanvas) {
        InvalidateRect(g_hZoomCanvas, NULL, FALSE);
        UpdateWindow(g_hZoomCanvas);
    }
}

/* 画板控件窗口过程 */
static LRESULT CALLBACK CanvasWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            on_paint_canvas(hWnd);
            return 0;
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            on_canvas_click(x, y);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (wParam & MK_LBUTTON) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                on_canvas_click(x, y);
            }
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

/* UTF-8 转 Unicode 宽字符串辅助函数 (彻底消除界面中文乱码) */
static void utf8_to_wide(const char* utf8_str, wchar_t* out_wstr, int max_wlen) {
    if (!out_wstr || max_wlen <= 0) return;
    out_wstr[0] = L'\0';
    if (!utf8_str || utf8_str[0] == '\0') return;
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, out_wstr, max_wlen);
    out_wstr[max_wlen - 1] = L'\0';
}

/* 执行弹窗检测 */
static void execute_popup_detection(void) {
    if (!g_current_frame_rgb) {
        MessageBoxW(g_hDebugWnd, L"请先捕获画面截图或载入图片！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    int pt_idx = (int)SendMessageW(g_hPopupTypeCb, CB_GETCURSEL, 0, 0);
    const L2MPopupItem* target_item = NULL;
    if (pt_idx >= 0 && pt_idx < g_current_cbt_cfg.popup_cfg.count) {
        target_item = &g_current_cbt_cfg.popup_cfg.items[pt_idx];
    }

    wchar_t rect_str_w[64];
    GetWindowTextW(g_hPopupRectTxt, rect_str_w, sizeof(rect_str_w)/sizeof(wchar_t));
    int rx = 0, ry = 0, rw = 0, rh = 0;
    if (swscanf(rect_str_w, L"%d, %d, %d, %d", &rx, &ry, &rw, &rh) != 4) {
        if (target_item) {
            rx = target_item->x; ry = target_item->y; rw = target_item->width; rh = target_item->height;
        } else {
            rx = 280; ry = 150; rw = 400; rh = 240;
        }
    }

    L2MRect roi = {rx, ry, rw, rh};
    L2MImageBuffer* crop = l2m_image_create(rw, rh, L2M_FMT_RGB888);
    if (!crop || !l2m_image_crop(g_current_frame_rgb, &roi, crop)) {
        if (crop) l2m_image_free(crop);
        SetWindowTextW(g_hStatusText, L"❌ 弹窗区域截取失败！");
        return;
    }

    L2MPopupResult res;
    bool ok = false;
    if (target_item) {
        ok = l2m_detect_popup_by_item(crop, rx, ry, target_item, true, &res);
    } else {
        ok = l2m_detect_popup(crop, rx, ry, L2M_POPUP_CENTER, true, &res);
    }
    l2m_image_free(crop);

    wchar_t log_buf[1024];
    if (ok && res.detected) {
        g_has_popup_overlay = true;
        g_overlay_scan_rect = roi;
        g_overlay_btn_pt = res.button_pos;
        g_overlay_btn_bbox = res.button_bbox;
        g_overlay_has_cb = res.has_checkbox;
        g_overlay_cb_pt = res.checkbox_pos;

        wchar_t cb_tip[128] = L"";
        if (res.has_checkbox) {
            swprintf(cb_tip, sizeof(cb_tip)/sizeof(wchar_t), L"\r\n☑ 发现【不再显示】勾选框: (%d, %d)", res.checkbox_pos.x, res.checkbox_pos.y);
        }

        wchar_t w_popname[128] = {0};
        wchar_t w_featdesc[256] = {0};
        utf8_to_wide(res.popup_name[0] ? res.popup_name : res.desc, w_popname, 128);
        utf8_to_wide(res.feature_info.feature_desc[0] ? res.feature_info.feature_desc : "已提取", w_featdesc, 256);

        swprintf(log_buf, sizeof(log_buf)/sizeof(wchar_t),
                 L"✅ 成功锁定弹窗【%ls】(置信度: %.1f分)\r\n"
                 L"🎯 按钮坐标: (%d, %d) | 尺寸: %dx%d (比例: %.2f:1, 尺寸得分: %.1f)\r\n"
                 L"🎨 按钮颜色: RGB(%d, %d, %d) | 填充纯度: %.1f%% | 色彩得分: %.1f\r\n"
                 L"🏛️ 弹窗特征: %ls (特征得分: %.1f分)\r\n"
                 L"📊 背景校验: ✅ 暗底占比 %.1f%% | 彩度干扰 %.1f%% | 均值亮度 %.1f%ls",
                 w_popname, res.score,
                 res.button_pos.x, res.button_pos.y,
                 res.button_bbox.width, res.button_bbox.height, res.button_aspect_ratio, res.size_score,
                 res.button_mean_rgb.r, res.button_mean_rgb.g, res.button_mean_rgb.b,
                 res.button_fill_ratio * 100.0f, res.color_score,
                 w_featdesc, res.feature_info.feature_score,
                 res.bg_info.dark_ratio * 100.0f, res.bg_info.high_chroma_ratio * 100.0f, res.bg_info.mean_brightness,
                 cb_tip);
    } else {
        g_has_popup_overlay = false;

        wchar_t w_popname[128] = {0};
        wchar_t w_reason[256] = {0};
        wchar_t w_featdesc[256] = {0};
        utf8_to_wide(target_item ? target_item->name : "弹窗", w_popname, 128);
        utf8_to_wide(res.bg_info.reason[0] ? res.bg_info.reason : "未找到符合尺寸与颜色的按钮", w_reason, 256);
        utf8_to_wide(res.feature_info.feature_desc[0] ? res.feature_info.feature_desc : "未匹配", w_featdesc, 256);

        swprintf(log_buf, sizeof(log_buf)/sizeof(wchar_t),
                 L"❌ 未检测到弹窗【%ls】\r\n"
                 L"原因: %ls\r\n"
                 L"🏛️ 特征指标: %ls (特征得分: %.1f分)\r\n"
                 L"📊 背景指标: 暗底占比 %.1f%% | 彩度干扰 %.1f%% | 均值亮度 %.1f",
                 w_popname,
                 w_reason,
                 w_featdesc, res.feature_info.feature_score,
                 res.bg_info.dark_ratio * 100.0f, res.bg_info.high_chroma_ratio * 100.0f, res.bg_info.mean_brightness);
    }

    SetWindowTextW(g_hStatusText, log_buf);
    if (g_hCanvas) InvalidateRect(g_hCanvas, NULL, FALSE);
}

/* 执行左上角地图框与安全/普通区域检测 */
static void execute_map_detection(void) {
    if (!g_current_frame_rgb) {
        MessageBoxW(g_hDebugWnd, L"请先捕获画面截图或载入图片！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    L2MMapBoxResult res;
    bool ok = l2m_detect_map_zone(g_current_frame_rgb, &g_current_cbt_cfg, &res);

    wchar_t log_buf[1024];
    if (ok && res.detected) {
        g_has_map_overlay = true;
        g_last_map_result = res;

        wchar_t w_desc[256] = {0};
        utf8_to_wide(res.desc, w_desc, 256);

        wchar_t zone_type_name[64] = L"普通区域(野外)";
        if (res.zone_type == L2M_ZONE_SAFETY) {
            wcscpy_s(zone_type_name, 64, L"🛡️ 安全区域(村庄/城镇/和平区)");
        } else if (res.zone_type == L2M_ZONE_COMBAT) {
            wcscpy_s(zone_type_name, 64, L"⚔️ 自由战斗区域(PVP/攻城)");
        } else {
            wcscpy_s(zone_type_name, 64, L"🌾 普通区域(野外刷怪/常规战斗)");
        }

        swprintf(log_buf, sizeof(log_buf)/sizeof(wchar_t),
                 L"🗺️ 成功定位左上角地图框: (%d, %d, %d, %d)\r\n"
                 L"🛡️ 区域类型判定: 【%ls】(置信度: %.1f分)\r\n"
                 L"📊 色彩空间统计: 绿色安全占比 %.1f%% | 普通野外占比 %.1f%% | 红色战斗占比 %.1f%%\r\n"
                 L"🎨 区域状态标识RGB: (%d, %d, %d) | 区域平均亮度: %.1f\r\n"
                 L"📝 诊断说明: %ls",
                 res.map_rect.x, res.map_rect.y, res.map_rect.width, res.map_rect.height,
                 zone_type_name, res.confidence,
                 res.green_ratio * 100.0f, res.white_gray_ratio * 100.0f, res.red_ratio * 100.0f,
                 res.badge_mean_rgb.r, res.badge_mean_rgb.g, res.badge_mean_rgb.b, res.mean_brightness,
                 w_desc);
    } else {
        g_has_map_overlay = false;
        swprintf(log_buf, sizeof(log_buf)/sizeof(wchar_t),
                 L"❌ 未识别到左上角地图框\r\n"
                 L"可能原因: 当前不在主界面、地图被全屏弹窗遮挡、或画面截取异常。");
    }

    SetWindowTextW(g_hStatusText, log_buf);
    if (g_hCanvas) InvalidateRect(g_hCanvas, NULL, FALSE);
}

/* 自动查找或绑定游戏窗口句柄 */
static BOOL CALLBACK EnumFindGameWndProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[256];
    GetWindowTextW(hwnd, title, sizeof(title)/sizeof(wchar_t));
    if (wcslen(title) == 0) return TRUE;

    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    if (wcsstr(title, L"Lineage2M") || wcsstr(title, L"PURPLE") || wcsstr(title, L"L2M") ||
        (w >= 900 && w <= 1000 && h >= 500 && h <= 600)) {
        HWND* pFound = (HWND*)lParam;
        *pFound = hwnd;
        return FALSE; /* 停止枚举 */
    }
    return TRUE;
}

static HWND get_active_or_first_game_window(void) {
    if (g_hTargetGameWnd && IsWindow(g_hTargetGameWnd)) return g_hTargetGameWnd;
    HWND found = NULL;
    EnumWindows(EnumFindGameWndProc, (LPARAM)&found);
    if (found) {
        g_hTargetGameWnd = found;
        update_debug_window_title();
    }
    return g_hTargetGameWnd;
}

/* 保存或更新弹窗配置到当前 JSON 配置文件 (支持新名称新建或修改已有名称) */
static void save_popup_config_to_json(void) {
    wchar_t wname[64] = {0};
    wchar_t wdesc[128] = {0};
    wchar_t wrect[64] = {0};

    GetWindowTextW(g_hPopupNameTxt, wname, 64);
    GetWindowTextW(g_hPopupDescTxt, wdesc, 128);
    GetWindowTextW(g_hPopupRectTxt, wrect, 64);

    if (wcslen(wname) == 0) {
        MessageBoxW(g_hDebugWnd, L"弹窗名称标识(Key)不能为空！", L"错误", MB_OK | MB_ICONWARNING);
        SetFocus(g_hPopupNameTxt);
        return;
    }

    int rx = 0, ry = 0, rw = 0, rh = 0;
    if (swscanf(wrect, L"%d, %d, %d, %d", &rx, &ry, &rw, &rh) != 4 || rw <= 0 || rh <= 0) {
        MessageBoxW(g_hDebugWnd, L"扫描区域格式错误！格式应为: X, Y, Width, Height 且宽高大于0", L"错误", MB_OK | MB_ICONWARNING);
        return;
    }

    char name_utf8[64] = {0};
    char desc_utf8[128] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name_utf8, sizeof(name_utf8), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, wdesc, -1, desc_utf8, sizeof(desc_utf8), NULL, NULL);

    /* 获取选中的关联 CBT 采样点 */
    char linked_cbt_utf8[64] = {0};
    if (g_hPopupLinkedCbtCb) {
        int cb_idx = (int)SendMessageW(g_hPopupLinkedCbtCb, CB_GETCURSEL, 0, 0);
        if (cb_idx > 0 && cb_idx <= g_current_cbt_cfg.count) {
            snprintf(linked_cbt_utf8, sizeof(linked_cbt_utf8), "%s", g_current_cbt_cfg.points[cb_idx - 1].key);
        }
    }

    /* 查找是否已有该名称的弹窗项 */
    L2MPopupItem item;
    memset(&item, 0, sizeof(item));
    if (l2m_cbt_get_popup_item(&g_current_cbt_cfg, name_utf8, &item)) {
        /* 已存在：更新关键属性与关联 CBT */
        snprintf(item.desc, sizeof(item.desc), "%s", desc_utf8);
        item.x = rx; item.y = ry; item.width = rw; item.height = rh;
        snprintf(item.linked_cbt_key, sizeof(item.linked_cbt_key), "%s", linked_cbt_utf8);
    } else {
        /* 全新创建：初始化默认特征开关与几何参数 */
        snprintf(item.name, sizeof(item.name), "%s", name_utf8);
        snprintf(item.desc, sizeof(item.desc), "%s", desc_utf8[0] ? desc_utf8 : "自定义弹窗");
        item.enabled = true;
        item.x = rx; item.y = ry; item.width = rw; item.height = rh;
        snprintf(item.linked_cbt_key, sizeof(item.linked_cbt_key), "%s", linked_cbt_utf8);

        item.min_dark_ratio = 0.18f;
        item.max_brightness = 120.0f;
        item.max_high_chroma = 0.35f;
        item.btn_min_w = 30; item.btn_max_w = 240;
        item.btn_min_h = 16; item.btn_max_h = 65;
        item.btn_ideal_aspect = 2.8f;
        item.has_btn_rgb = true;
        item.btn_target_rgb = (L2MRGB){215, 105, 12};
        item.btn_min_fill_ratio = 0.30f;
        item.check_panel = true;
        item.check_title = true;
        item.check_text_lines = true;
        item.check_close_cross = true;
    }

    l2m_cbt_set_popup_item(&g_current_cbt_cfg, &item);

    if (l2m_cbt_save(&g_current_cbt_cfg)) {
        refresh_popup_list_ui(name_utf8);
        wchar_t tip[512];
        wchar_t w_itemname[128] = {0};
        wchar_t w_cbtkey[128] = {0};
        utf8_to_wide(item.name, w_itemname, 128);
        utf8_to_wide(item.linked_cbt_key[0] ? item.linked_cbt_key : "无", w_cbtkey, 128);

        swprintf(tip, sizeof(tip)/sizeof(wchar_t),
                 L"✅ 弹窗【%ls】(关联CBT: %ls) 区域 (%d, %d, %d, %d) 已成功保存至配置文件！\r\n路径: %hs",
                 w_itemname, w_cbtkey,
                 rx, ry, rw, rh, g_current_cbt_cfg.file_path);
        SetWindowTextW(g_hStatusText, tip);
        MessageBoxW(g_hDebugWnd, tip, L"弹窗配置保存成功", MB_OK | MB_ICONINFORMATION);
    } else {
        wchar_t err_msg[512];
        swprintf(err_msg, sizeof(err_msg)/sizeof(wchar_t),
                 L"❌ 保存弹窗配置失败！\r\n目标文件: %hs\r\n请检查文件是否被占用或是否存在写入权限。",
                 g_current_cbt_cfg.file_path);
        MessageBoxW(g_hDebugWnd, err_msg, L"保存错误", MB_OK | MB_ICONERROR);
    }
}

/* 删除当前选中的弹窗配置 */
static void delete_popup_item_from_json(void) {
    wchar_t wname[64] = {0};
    GetWindowTextW(g_hPopupNameTxt, wname, 64);
    if (wcslen(wname) == 0) return;

    wchar_t confirm_msg[256];
    swprintf(confirm_msg, sizeof(confirm_msg)/sizeof(wchar_t),
             L"确定要从 %hs.json 配置文件中永久删除弹窗【%ls】吗？",
             g_current_cbt_cfg.region, wname);
    if (MessageBoxW(g_hDebugWnd, confirm_msg, L"确认删除", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    char name_utf8[64] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, name_utf8, sizeof(name_utf8), NULL, NULL);

    if (l2m_cbt_delete_popup_item(&g_current_cbt_cfg, name_utf8)) {
        l2m_cbt_save(&g_current_cbt_cfg);
        refresh_popup_list_ui(NULL);
        SetWindowTextW(g_hStatusText, L"弹窗配置已从 JSON 文件中成功删除。");
    }
}

/* 模拟点击测试 */
static void execute_test_click(void) {
    if (!g_has_popup_overlay) {
        MessageBoxW(g_hDebugWnd, L"请先执行【🔍 识别弹窗】以定位关闭按钮与勾选框！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    HWND hGame = get_active_or_first_game_window();
    if (!hGame) {
        int r = MessageBoxW(g_hDebugWnd,
                            L"未检测到在线游戏窗口(Lineage2M/PURPLE)。\r\n"
                            L"是否使用真实物理鼠标直接向屏幕当前物理位置发送点击测试？",
                            L"未绑定游戏窗口", MB_YESNO | MB_ICONQUESTION);
        if (r != IDYES) return;
        hGame = GetDesktopWindow();
    }

    bool is_admin = l2m_is_run_as_admin();

    bool click_ok = l2m_post_popup_dismiss_flow(
        hGame,
        g_overlay_has_cb ? g_overlay_cb_pt.x : 0,
        g_overlay_has_cb ? g_overlay_cb_pt.y : 0,
        g_overlay_btn_pt.x, g_overlay_btn_pt.y,
        380
    );

    wchar_t tip[512];
    swprintf(tip, sizeof(tip)/sizeof(wchar_t),
             L"🖱️ 已发送模拟点击: 按钮目标(%d, %d)%ls (状态: %ls)%ls",
             g_overlay_btn_pt.x, g_overlay_btn_pt.y,
             g_overlay_has_cb ? L" | 已勾选不再显示" : L"",
             click_ok ? L"✅ 物理注入完成" : L"❌ 失败",
             is_admin ? L"" : L"\r\n⚠️ 注意: 当前未以管理员身份运行，若游戏是管理员权限，Windows UIPI 会拦截物理点击！");
    SetWindowTextW(g_hStatusText, tip);
}

/* 执行血条计算测试 */
static void execute_hp_test(void) {
    if (!g_current_frame_rgb) {
        MessageBoxW(g_hDebugWnd, L"请先捕获画面截图或载入图片！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    L2MHpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.offset_x = 64;
    cfg.offset_y = 21;
    cfg.width = 103;
    cfg.height = 2;
    cfg.target_color_1 = (L2MRGB){168, 69, 2};
    cfg.tolerance_1 = (L2MRGB){30, 30, 20};
    cfg.target_color_2 = (L2MRGB){255, 157, 57};
    cfg.tolerance_2 = (L2MRGB){10, 10, 10};

    L2MHpResult hp_res;
    bool ok = l2m_calculate_hp_from_fullscreen(g_current_frame_rgb, &cfg, &hp_res);

    wchar_t buf[256];
    if (ok && hp_res.is_valid) {
        swprintf(buf, sizeof(buf)/sizeof(wchar_t), L"🩸 血条计算成功:\r\n  • 当前血量: %d%%\r\n  • 采样有效端点: %d px\r\n  • 实测采样均值: RGB(%d, %d, %d)",
                 hp_res.hp_percent, hp_res.sample_hp_end,
                 hp_res.mean_rgb.r, hp_res.mean_rgb.g, hp_res.mean_rgb.b);
    } else {
        swprintf(buf, sizeof(buf)/sizeof(wchar_t), L"⚠️ 血条计算异常: 未匹配到有效血条像素 (可能血条被弹窗遮挡或角色死亡)");
    }
    SetWindowTextW(g_hStatusText, buf);
}

/* 比对当前 CBT 采样点在画面的实测颜色 */
static void execute_cbt_point_test(void) {
    if (!g_current_frame_rgb) {
        MessageBoxW(g_hDebugWnd, L"请先捕获画面截图或载入图片！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    wchar_t wkey[64], wpos[64], wrgb[64], wtol[32];
    GetWindowTextW(g_hCbtKeyTxt, wkey, 64);
    GetWindowTextW(g_hCbtPosTxt, wpos, 64);
    GetWindowTextW(g_hCbtRgbTxt, wrgb, 64);
    GetWindowTextW(g_hCbtTolTxt, wtol, 32);

    L2MCbtPoint pt;
    memset(&pt, 0, sizeof(L2MCbtPoint));
    WideCharToMultiByte(CP_UTF8, 0, wkey, -1, pt.key, sizeof(pt.key), NULL, NULL);

    int px = 0, py = 0;
    if (swscanf(wpos, L"%d, %d", &px, &py) == 2 || swscanf(wpos, L"%d , %d", &px, &py) == 2) {
        pt.x = px;
        pt.y = py;
    }

    int pr = 0, pg = 0, pb = 0;
    if (swscanf(wrgb, L"%d, %d, %d", &pr, &pg, &pb) == 3) {
        pt.has_rgb = true;
        pt.r = (uint8_t)pr; pt.g = (uint8_t)pg; pt.b = (uint8_t)pb;
    } else {
        pt.has_rgb = false;
    }

    int tol = 12;
    if (swscanf(wtol, L"%d", &tol) == 1 && tol > 0) pt.tolerance = tol;

    L2MRGB actual_rgb;
    int diff = 0;
    bool is_match = false;
    l2m_cbt_test_pixel_match(g_current_frame_rgb, &pt, &actual_rgb, &diff, &is_match);

    g_has_selected_cbt_pt = true;
    g_selected_cbt_pt.x = pt.x;
    g_selected_cbt_pt.y = pt.y;
    g_zoom_center_pt.x = pt.x;
    g_zoom_center_pt.y = pt.y;
    g_last_pick_pt = (L2MPoint){-1, -1};

    wchar_t buf[512];
    if (pt.has_rgb) {
        swprintf(buf, sizeof(buf)/sizeof(wchar_t),
                 L"🔬 CBT 采样点比对结果:\r\n  • 特征点: %ls\r\n  • 坐标: (%d, %d)\r\n  • 目标颜色: RGB(%d, %d, %d)\r\n  • 实测颜色: RGB(%d, %d, %d)\r\n  • 色差偏离: %d (容差: %d)\r\n  • 匹配状态: %ls",
                 wkey, pt.x, pt.y, pt.r, pt.g, pt.b, actual_rgb.r, actual_rgb.g, actual_rgb.b,
                 diff, pt.tolerance, is_match ? L"✅ 匹配通过" : L"❌ 不匹配 (色差超标)");
    } else {
        swprintf(buf, sizeof(buf)/sizeof(wchar_t),
                 L"🔬 CBT 采样点比对结果:\r\n  • 特征点: %ls\r\n  • 坐标: (%d, %d) [无颜色要求]\r\n  • 实测颜色: RGB(%d, %d, %d)\r\n  • 匹配状态: ✅ 坐标有效",
                 wkey, pt.x, pt.y, actual_rgb.r, actual_rgb.g, actual_rgb.b);
    }
    SetWindowTextW(g_hStatusText, buf);

    /* 联动更新放大镜 */
    wchar_t zinfo[128];
    swprintf(zinfo, sizeof(zinfo)/sizeof(wchar_t), L"🔍 放大镜 (10x 放大) - 采样中心: (%d, %d) | 实测: RGB(%d, %d, %d)",
             pt.x, pt.y, actual_rgb.r, actual_rgb.g, actual_rgb.b);
    SetWindowTextW(g_hZoomInfoLbl, zinfo);

    if (g_hCanvas) { InvalidateRect(g_hCanvas, NULL, FALSE); UpdateWindow(g_hCanvas); }
    if (g_hZoomCanvas) { InvalidateRect(g_hZoomCanvas, NULL, FALSE); UpdateWindow(g_hZoomCanvas); }
}

/* 将鼠标拾取点填入 CBT 编辑框 */
static void apply_picked_point_to_cbt(void) {
    if (g_last_pick_pt.x < 0 || g_last_pick_pt.y < 0) {
        MessageBoxW(g_hDebugWnd, L"请先在右侧画板点击选择一个像素点！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t pos_str[64], rgb_str[64];
    swprintf(pos_str, sizeof(pos_str)/sizeof(wchar_t), L"%d, %d", g_last_pick_pt.x, g_last_pick_pt.y);
    swprintf(rgb_str, sizeof(rgb_str)/sizeof(wchar_t), L"%d, %d, %d", g_last_pick_rgb.r, g_last_pick_rgb.g, g_last_pick_rgb.b);

    SetWindowTextW(g_hCbtPosTxt, pos_str);
    SetWindowTextW(g_hCbtRgbTxt, rgb_str);

    g_has_selected_cbt_pt = true;
    g_selected_cbt_pt = g_last_pick_pt;
    g_zoom_center_pt = g_last_pick_pt;

    if (g_hCanvas) { InvalidateRect(g_hCanvas, NULL, FALSE); UpdateWindow(g_hCanvas); }
    if (g_hZoomCanvas) { InvalidateRect(g_hZoomCanvas, NULL, FALSE); UpdateWindow(g_hZoomCanvas); }
}

/* 保存当前 CBT 点位至对应语言 JSON */
static void save_cbt_point_to_json(void) {
    wchar_t wkey[64], wpos[64], wrgb[64], wtol[32];
    GetWindowTextW(g_hCbtKeyTxt, wkey, 64);
    GetWindowTextW(g_hCbtPosTxt, wpos, 64);
    GetWindowTextW(g_hCbtRgbTxt, wrgb, 64);
    GetWindowTextW(g_hCbtTolTxt, wtol, 32);

    if (wcslen(wkey) == 0) {
        MessageBoxW(g_hDebugWnd, L"点位名称 (Key) 不能为空！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    L2MCbtPoint pt;
    memset(&pt, 0, sizeof(L2MCbtPoint));
    WideCharToMultiByte(CP_UTF8, 0, wkey, -1, pt.key, sizeof(pt.key), NULL, NULL);

    int px = 0, py = 0;
    if (swscanf(wpos, L"%d, %d", &px, &py) == 2 || swscanf(wpos, L"%d , %d", &px, &py) == 2) {
        pt.x = px;
        pt.y = py;
    }

    int pr = 0, pg = 0, pb = 0;
    if (swscanf(wrgb, L"%d, %d, %d", &pr, &pg, &pb) == 3) {
        pt.has_rgb = true;
        pt.r = (uint8_t)pr; pt.g = (uint8_t)pg; pt.b = (uint8_t)pb;
    } else {
        pt.has_rgb = false;
    }

    int tol = 12;
    if (swscanf(wtol, L"%d", &tol) == 1 && tol > 0) pt.tolerance = tol;

    l2m_cbt_set_point(&g_current_cbt_cfg, &pt);
    if (l2m_cbt_save(&g_current_cbt_cfg)) {
        refresh_cbt_points_ui(pt.key);
        wchar_t tip[512];
        swprintf(tip, sizeof(tip)/sizeof(wchar_t), L"✅ 点位【%ls】已成功保存至配置文件！\r\n路径: %hs", wkey, g_current_cbt_cfg.file_path);
        SetWindowTextW(g_hStatusText, tip);
        MessageBoxW(g_hDebugWnd, tip, L"保存成功", MB_OK | MB_ICONINFORMATION);
    } else {
        wchar_t err_msg[512];
        swprintf(err_msg, sizeof(err_msg)/sizeof(wchar_t),
                 L"❌ 保存特征点失败！\r\n目标文件: %hs\r\n请检查文件是否被占用或是否存在写入权限。",
                 g_current_cbt_cfg.file_path);
        MessageBoxW(g_hDebugWnd, err_msg, L"保存错误", MB_OK | MB_ICONERROR);
    }
}

/* 删除当前 CBT 点位 */
static void delete_cbt_point_from_json(void) {
    wchar_t wkey[64];
    GetWindowTextW(g_hCbtKeyTxt, wkey, 64);
    if (wcslen(wkey) == 0) return;

    wchar_t confirm_msg[256];
    swprintf(confirm_msg, sizeof(confirm_msg)/sizeof(wchar_t), L"确定要从 %hs.json 中删除点位【%ls】吗？", g_current_cbt_cfg.region, wkey);
    if (MessageBoxW(g_hDebugWnd, confirm_msg, L"确认删除", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    char key_utf8[64];
    WideCharToMultiByte(CP_UTF8, 0, wkey, -1, key_utf8, sizeof(key_utf8), NULL, NULL);

    if (l2m_cbt_delete_point(&g_current_cbt_cfg, key_utf8)) {
        l2m_cbt_save(&g_current_cbt_cfg);
        refresh_cbt_points_ui(NULL);
        SetWindowTextW(g_hStatusText, L"点位已从配置文件中删除。");
    }
}

/* 调试窗口主过程 */
static LRESULT CALLBACK DebugWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hFontDebugUI = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            g_hFontBoldUI = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");

            /* 注册主画板与放大镜窗口类 */
            WNDCLASSW wc;
            memset(&wc, 0, sizeof(wc));
            wc.lpfnWndProc = CanvasWndProc;
            wc.hInstance = GetModuleHandle(NULL);
            wc.hCursor = LoadCursorW(NULL, IDC_CROSS);
            wc.lpszClassName = L"L2M_Canvas_View";
            RegisterClassW(&wc);

            WNDCLASSW wcZ;
            memset(&wcZ, 0, sizeof(wcZ));
            wcZ.lpfnWndProc = ZoomWndProc;
            wcZ.hInstance = GetModuleHandle(NULL);
            wcZ.hCursor = LoadCursorW(NULL, IDC_CROSS);
            wcZ.lpszClassName = L"L2M_Zoom_View";
            RegisterClassW(&wcZ);

            /* ===== 区域 1: 命名弹窗管理与截图控制 ===== */
            CreateWindowW(L"STATIC", L"弹窗选择:", WS_CHILD | WS_VISIBLE, 15, 8, 60, 20, hWnd, NULL, NULL, NULL);
            g_hPopupTypeCb = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 75, 5, 210, 250, hWnd, (HMENU)ID_CB_POPUP_TYPE, NULL, NULL);
            CreateWindowW(L"BUTTON", L"➕ 新建", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 290, 4, 70, 25, hWnd, (HMENU)ID_BTN_NEW_POPUP, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🗑️ 删除", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 365, 4, 75, 25, hWnd, (HMENU)ID_BTN_DEL_POPUP, NULL, NULL);

            CreateWindowW(L"STATIC", L"弹窗标识:", WS_CHILD | WS_VISIBLE, 15, 34, 60, 20, hWnd, NULL, NULL, NULL);
            g_hPopupNameTxt = CreateWindowW(L"EDIT", L"center_modal", WS_CHILD | WS_VISIBLE | WS_BORDER, 75, 32, 150, 22, hWnd, (HMENU)ID_TXT_POPUP_NAME, NULL, NULL);
            CreateWindowW(L"STATIC", L"描述:", WS_CHILD | WS_VISIBLE, 232, 34, 35, 20, hWnd, NULL, NULL, NULL);
            g_hPopupDescTxt = CreateWindowW(L"EDIT", L"中间标准模态确认弹窗", WS_CHILD | WS_VISIBLE | WS_BORDER, 270, 32, 170, 22, hWnd, (HMENU)ID_TXT_POPUP_DESC, NULL, NULL);

            CreateWindowW(L"STATIC", L"扫描 ROI:", WS_CHILD | WS_VISIBLE, 15, 59, 60, 20, hWnd, NULL, NULL, NULL);
            g_hPopupRectTxt = CreateWindowW(L"EDIT", L"280, 150, 400, 240", WS_CHILD | WS_VISIBLE | WS_BORDER, 75, 57, 130, 22, hWnd, (HMENU)ID_TXT_POPUP_RECT, NULL, NULL);
            CreateWindowW(L"STATIC", L"关联CBT:", WS_CHILD | WS_VISIBLE, 212, 59, 55, 20, hWnd, NULL, NULL, NULL);
            g_hPopupLinkedCbtCb = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 270, 56, 170, 250, hWnd, (HMENU)ID_CB_POPUP_LINK_CBT, NULL, NULL);

            CreateWindowW(L"BUTTON", L"🔍 识别弹窗", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, 84, 100, 25, hWnd, (HMENU)ID_BTN_DETECT_POPUP, NULL, NULL);
            CreateWindowW(L"BUTTON", L"💾 保存/更新", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 120, 84, 105, 25, hWnd, (HMENU)ID_BTN_SAVE_POPUP, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🖱️ 模拟关闭", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 230, 84, 100, 25, hWnd, (HMENU)ID_BTN_TEST_CLICK, NULL, NULL);
            CreateWindowW(L"BUTTON", L"📸 捕获画面", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 335, 84, 105, 25, hWnd, (HMENU)ID_BTN_CAPTURE, NULL, NULL);

            CreateWindowW(L"BUTTON", L"💾 保存当前截图", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, 112, 210, 25, hWnd, (HMENU)ID_BTN_SAVE_IMAGE, NULL, NULL);
            CreateWindowW(L"BUTTON", L"📂 载入本地图片", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 230, 112, 210, 25, hWnd, (HMENU)ID_BTN_LOAD_IMAGE, NULL, NULL);

            /* ===== 区域 2: 多语言 CBT 采样点管理 ===== */
            CreateWindowW(L"STATIC", L"🌐 语言:", WS_CHILD | WS_VISIBLE, 15, 144, 45, 20, hWnd, NULL, NULL, NULL);
            g_hCbtRegionCb = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 60, 141, 60, 200, hWnd, (HMENU)ID_CB_CBT_REGION, NULL, NULL);
            SendMessageW(g_hCbtRegionCb, CB_ADDSTRING, 0, (LPARAM)L"CN");
            SendMessageW(g_hCbtRegionCb, CB_ADDSTRING, 0, (LPARAM)L"EN");
            SendMessageW(g_hCbtRegionCb, CB_ADDSTRING, 0, (LPARAM)L"JP");
            SendMessageW(g_hCbtRegionCb, CB_ADDSTRING, 0, (LPARAM)L"RU");
            SendMessageW(g_hCbtRegionCb, CB_SETCURSEL, 0, 0);

            CreateWindowW(L"STATIC", L"🎯 CBT点位:", WS_CHILD | WS_VISIBLE, 125, 144, 70, 20, hWnd, NULL, NULL, NULL);
            g_hCbtPointsCb = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 195, 141, 245, 450, hWnd, (HMENU)ID_CB_CBT_POINTS, NULL, NULL);

            CreateWindowW(L"STATIC", L"点位Key:", WS_CHILD | WS_VISIBLE, 15, 169, 58, 20, hWnd, NULL, NULL, NULL);
            g_hCbtKeyTxt = CreateWindowW(L"EDIT", L"home_scroll_button_no_energomode", WS_CHILD | WS_VISIBLE | WS_BORDER, 75, 167, 365, 22, hWnd, (HMENU)ID_TXT_CBT_KEY, NULL, NULL);

            CreateWindowW(L"STATIC", L"坐标:", WS_CHILD | WS_VISIBLE, 15, 194, 35, 20, hWnd, NULL, NULL, NULL);
            g_hCbtPosTxt = CreateWindowW(L"EDIT", L"217, 487", WS_CHILD | WS_VISIBLE | WS_BORDER, 50, 192, 75, 22, hWnd, (HMENU)ID_TXT_CBT_POS, NULL, NULL);

            CreateWindowW(L"STATIC", L"RGB:", WS_CHILD | WS_VISIBLE, 130, 194, 30, 20, hWnd, NULL, NULL, NULL);
            g_hCbtRgbTxt = CreateWindowW(L"EDIT", L"174, 149, 130", WS_CHILD | WS_VISIBLE | WS_BORDER, 162, 192, 98, 22, hWnd, (HMENU)ID_TXT_CBT_RGB, NULL, NULL);

            CreateWindowW(L"STATIC", L"容差:", WS_CHILD | WS_VISIBLE, 265, 194, 35, 20, hWnd, NULL, NULL, NULL);
            g_hCbtTolTxt = CreateWindowW(L"EDIT", L"12", WS_CHILD | WS_VISIBLE | WS_BORDER, 300, 192, 38, 22, hWnd, (HMENU)ID_TXT_CBT_TOL, NULL, NULL);

            CreateWindowW(L"BUTTON", L"🩸血条", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 342, 190, 48, 25, hWnd, (HMENU)ID_BTN_TEST_HP, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🗺️地图", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 393, 190, 48, 25, hWnd, (HMENU)ID_BTN_DETECT_MAP, NULL, NULL);

            CreateWindowW(L"BUTTON", L"🎯 填入拾取", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, 218, 100, 25, hWnd, (HMENU)ID_BTN_CBT_APPLY_PT, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🔬 比对测试", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 120, 218, 100, 25, hWnd, (HMENU)ID_BTN_CBT_TEST_PT, NULL, NULL);
            CreateWindowW(L"BUTTON", L"💾 保存特征", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 225, 218, 105, 25, hWnd, (HMENU)ID_BTN_CBT_SAVE_JSON, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🗑️ 删除点位", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 335, 218, 105, 25, hWnd, (HMENU)ID_BTN_CBT_DEL_PT, NULL, NULL);

            /* ===== 区域 3: 采样点 11x11 像素放大镜 (110x110 像素网格完整展示) ===== */
            g_hZoomInfoLbl = CreateWindowW(L"STATIC", L"🔍 放大镜 (10x 放大) - 采样中心: 未选择", WS_CHILD | WS_VISIBLE, 15, 248, 425, 18, hWnd, NULL, NULL, NULL);
            g_hZoomCanvas = CreateWindowW(L"L2M_Zoom_View", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 15, 268, 112, 112, hWnd, (HMENU)ID_ZOOM_VIEW, NULL, NULL);

            g_hColorInfoLbl = CreateWindowW(L"STATIC", L"📍 鼠标取点: 点击右侧画面任意位置拾取坐标与 RGB", WS_CHILD | WS_VISIBLE, 135, 268, 305, 36, hWnd, NULL, NULL, NULL);

            CreateWindowW(L"STATIC", L"【检测与比对诊断报告】:", WS_CHILD | WS_VISIBLE, 135, 310, 200, 18, hWnd, NULL, NULL, NULL);

            /* ===== 区域 4: 诊断与状态信息框 (下移至 y=388，绝不遮挡放大镜) ===== */
            g_hStatusText = CreateWindowW(L"EDIT", L"就绪。可进行实时画面捕获、载入本地图片、放大镜观察与特征点微调。",
                                          WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                          15, 388, 425, 162, hWnd, (HMENU)ID_TXT_STATUS, NULL, NULL);

            /* 右侧 960x540 大画板 (向右平移至 x=455) */
            g_hCanvas = CreateWindowW(L"L2M_Canvas_View", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 455, 10, 960, 540, hWnd, (HMENU)ID_CANVAS_VIEW, NULL, NULL);

            apply_debug_ui_font(hWnd, g_hFontDebugUI);

            /* 初始载入 CN 语言配置 */
            switch_cbt_region("CN");

            capture_game_screen();
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_CAPTURE) {
                capture_game_screen();
            } else if (id == ID_BTN_SAVE_IMAGE) {
                save_current_screenshot();
            } else if (id == ID_BTN_LOAD_IMAGE) {
                load_local_image_file();
            } else if (id == ID_BTN_DETECT_POPUP) {
                execute_popup_detection();
            } else if (id == ID_BTN_SAVE_POPUP) {
                save_popup_config_to_json();
            } else if (id == ID_BTN_NEW_POPUP) {
                create_new_popup_item_ui();
            } else if (id == ID_BTN_DEL_POPUP) {
                delete_popup_item_from_json();
            } else if (id == ID_BTN_TEST_CLICK) {
                execute_test_click();
            } else if (id == ID_BTN_TEST_HP) {
                execute_hp_test();
            } else if (id == ID_BTN_DETECT_MAP) {
                execute_map_detection();
            } else if (id == ID_CB_POPUP_TYPE && HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = (int)SendMessageW(g_hPopupTypeCb, CB_GETCURSEL, 0, 0);
                sync_popup_item_selection(idx);
            } else if (id == ID_CB_CBT_REGION && HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = (int)SendMessageW(g_hCbtRegionCb, CB_GETCURSEL, 0, 0);
                const char* rlist[] = {"CN", "EN", "JP", "RU"};
                if (idx >= 0 && idx < 4) switch_cbt_region(rlist[idx]);
            } else if (id == ID_CB_CBT_POINTS && HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = (int)SendMessageW(g_hCbtPointsCb, CB_GETCURSEL, 0, 0);
                sync_cbt_point_selection(idx);
            } else if (id == ID_BTN_CBT_APPLY_PT) {
                apply_picked_point_to_cbt();
            } else if (id == ID_BTN_CBT_TEST_PT) {
                execute_cbt_point_test();
            } else if (id == ID_BTN_CBT_SAVE_JSON) {
                save_cbt_point_to_json();
            } else if (id == ID_BTN_CBT_DEL_PT) {
                delete_cbt_point_from_json();
            }
            return 0;
        }

        case WM_DESTROY:
            free_current_frame();
            if (g_hFontDebugUI) {
                DeleteObject(g_hFontDebugUI);
                g_hFontDebugUI = NULL;
            }
            if (g_hFontBoldUI) {
                DeleteObject(g_hFontBoldUI);
                g_hFontBoldUI = NULL;
            }
            g_hDebugWnd = NULL;
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void update_debug_window_title(void) {
    if (!g_hDebugWnd || !IsWindow(g_hDebugWnd)) return;

    wchar_t dlg_title[512];
    if (g_hTargetGameWnd && IsWindow(g_hTargetGameWnd)) {
        wchar_t game_title[256];
        GetWindowTextW(g_hTargetGameWnd, game_title, sizeof(game_title)/sizeof(wchar_t));
        RECT rc;
        GetClientRect(g_hTargetGameWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        swprintf(dlg_title, sizeof(dlg_title)/sizeof(wchar_t),
                 L"Lineage2MBot 调试器 - [目标窗口: %ls | 句柄: %08X | 尺寸: %dx%d]",
                 game_title, (unsigned int)(uintptr_t)g_hTargetGameWnd, w, h);
    } else {
        swprintf(dlg_title, sizeof(dlg_title)/sizeof(wchar_t),
                 L"Lineage2MBot 调试器 - [模拟测试 / 离线图片分析模式]");
    }
    SetWindowTextW(g_hDebugWnd, dlg_title);
}

void l2m_open_debug_dialog(HWND hParentWnd, HWND hTargetGameWnd) {
    g_hTargetGameWnd = hTargetGameWnd;

    if (g_hDebugWnd && IsWindow(g_hDebugWnd)) {
        update_debug_window_title();
        capture_game_screen();
        SetForegroundWindow(g_hDebugWnd);
        return;
    }

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = DebugWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"L2M_Debug_Dialog";
    RegisterClassW(&wc);

    int dlg_w = 1450;
    int dlg_h = 600;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_w - dlg_w) / 2;
    int y = (screen_h - dlg_h) / 2;

    g_hDebugWnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
        L"L2M_Debug_Dialog",
        L"Lineage2MBot 调试器",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, dlg_w, dlg_h,
        hParentWnd, NULL, GetModuleHandle(NULL), NULL
    );

    update_debug_window_title();
    ShowWindow(g_hDebugWnd, SW_SHOW);
    UpdateWindow(g_hDebugWnd);
}

#endif
