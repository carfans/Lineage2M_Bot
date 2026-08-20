/**
 * @file win_main_gui.c
 * @brief Lineage2MBot 纯 C Win32 原生多开监控主工作台 (支持多窗口并发挂机、独立血量监控与弹窗防御)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#include "../../include/l2m_gui.h"

#ifdef _WIN32

#define ID_BTN_REFRESH_ALL     2001
#define ID_BTN_SELECT_ALL      2002
#define ID_BTN_DESELECT_ALL    2003
#define ID_BTN_START_SELECTED  2004
#define ID_BTN_STOP_SELECTED   2005
#define ID_BTN_START_ALL       2006
#define ID_BTN_STOP_ALL        2007
#define ID_BTN_DEBUG_SELECTED  2008
#define ID_BTN_ESCAPE_SELECTED 2009
#define ID_LV_CLIENTS          2010
#define ID_TXT_MULTI_LOG       2011

#define ID_HOTKEY_EMERGENCY_STOP 3001
#define WM_USER_REFRESH_LV       (WM_USER + 105)

/* 多开客户端最大支持数 */
#define MAX_MULTI_CLIENTS 64

typedef struct {
    HWND hwnd;
    wchar_t title[128];
    wchar_t region[8];
    int width;
    int height;
    bool is_running;
    int current_hp;
    int popup_blocked_count;
    wchar_t status_text[64];
    wchar_t last_action[64];
} L2MClientMonitorInfo;

static HWND g_hMainWnd = NULL;
static HWND g_hListView = NULL;
static HWND g_hLogEdit = NULL;
static HFONT g_hFontUI = NULL;
static HFONT g_hFontBold = NULL;

static L2MClientMonitorInfo g_clients[MAX_MULTI_CLIENTS];
static int g_client_count = 0;

static volatile bool g_global_worker_running = false;
static HANDLE g_hGlobalWorkerThread = NULL;

static void update_listview_item(int index);
static void append_multi_log_w(const wchar_t* text);

/* 全局紧急制动: 立即强制停止所有多开窗口挂机与物理鼠标操作 */
static void emergency_stop_all(void) {
    int stopped_count = 0;
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].is_running) {
            g_clients[i].is_running = false;
            wcsncpy_s(g_clients[i].status_text, sizeof(g_clients[i].status_text)/sizeof(wchar_t), L"🔴 紧急停止", _TRUNCATE);
            wcsncpy_s(g_clients[i].last_action, sizeof(g_clients[i].last_action)/sizeof(wchar_t), L"Ctrl+Q 紧急制动", _TRUNCATE);
            update_listview_item(i);
            stopped_count++;
        }
    }

    /* 立即抬起物理鼠标所有按键，恢复鼠标控制权 */
    mouse_event(MOUSEEVENTF_LEFTUP | MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);

    /* 播放警告音 */
    MessageBeep(MB_ICONWARNING);

    wchar_t log[256];
    swprintf(log, sizeof(log)/sizeof(wchar_t),
             L"🛑 【全局紧急制动】捕获快捷键 [Ctrl + Q]！已立即强制停止所有多开窗口巡检并释放鼠标控制权 (已停止 %d 个窗口)。",
             stopped_count);
    append_multi_log_w(log);
}

/* 递归设置字体 */
static void apply_ui_font(HWND hWnd, HFONT hFont) {
    if (!hWnd || !hFont) return;
    SendMessageW(hWnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    HWND hChild = GetWindow(hWnd, GW_CHILD);
    while (hChild) {
        SendMessageW(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

/* 追加日志 */
static void append_multi_log_w(const wchar_t* text) {
    if (!g_hLogEdit || !IsWindow(g_hLogEdit)) return;

    time_t now = time(NULL);
    struct tm t;
    localtime_s(&t, &now);

    wchar_t time_str[32];
    wcsftime(time_str, sizeof(time_str)/sizeof(wchar_t), L"%H:%M:%S", &t);

    wchar_t formatted[512];
    swprintf(formatted, sizeof(formatted)/sizeof(wchar_t), L"[%ls] %ls\r\n", time_str, text);

    int len = GetWindowTextLengthW(g_hLogEdit);
    SendMessageW(g_hLogEdit, EM_SETSEL, len, len);
    SendMessageW(g_hLogEdit, EM_REPLACESEL, FALSE, (LPARAM)formatted);
    SendMessageW(g_hLogEdit, EM_SCROLLCARET, 0, 0);
}

/* 更新 ListView 单行显示 */
static void update_listview_item(int index) {
    if (!g_hListView || index < 0 || index >= g_client_count) return;

    L2MClientMonitorInfo* c = &g_clients[index];

    /* 列 0: 标题与句柄 */
    wchar_t col0[160];
    swprintf(col0, sizeof(col0)/sizeof(wchar_t), L"[%08X] %ls", (unsigned int)(uintptr_t)c->hwnd, c->title);
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.iSubItem = 0;
    item.pszText = col0;
    SendMessageW(g_hListView, LVM_SETITEMW, 0, (LPARAM)&item);

    /* 列 1: 分辨率 */
    wchar_t col1[32];
    swprintf(col1, sizeof(col1)/sizeof(wchar_t), L"%dx%d", c->width, c->height);
    ListView_SetItemText(g_hListView, index, 1, col1);

    /* 列 2: 地区 */
    ListView_SetItemText(g_hListView, index, 2, c->region);

    /* 列 3: 当前血量 */
    wchar_t col3[32];
    if (c->current_hp >= 0) {
        swprintf(col3, sizeof(col3)/sizeof(wchar_t), L"%d%%", c->current_hp);
    } else {
        swprintf(col3, sizeof(col3)/sizeof(wchar_t), L"--");
    }
    ListView_SetItemText(g_hListView, index, 3, col3);

    /* 列 4: 弹窗拦截数 */
    wchar_t col4[32];
    swprintf(col4, sizeof(col4)/sizeof(wchar_t), L"%d 次", c->popup_blocked_count);
    ListView_SetItemText(g_hListView, index, 4, col4);

    /* 列 5: 运行状态 */
    ListView_SetItemText(g_hListView, index, 5, c->status_text);

    /* 列 6: 最近操作 */
    ListView_SetItemText(g_hListView, index, 6, c->last_action);
}

/* 重新扫描所有游戏客户端窗口 */
static BOOL CALLBACK EnumMultiWindowsProcW(HWND hwnd, LPARAM lParam) {
    (void)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t title[256];
    GetWindowTextW(hwnd, title, sizeof(title)/sizeof(wchar_t));
    if (wcslen(title) == 0) return TRUE;

    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    /* 过滤多开游戏窗口 (Lineage2M, PURPLE, 模拟器或 960x540 分辨率窗口) */
    if (wcsstr(title, L"Lineage2M") || wcsstr(title, L"PURPLE") || wcsstr(title, L"L2M") ||
        (w >= 900 && w <= 1000 && h >= 500 && h <= 600)) {
        if (g_client_count < MAX_MULTI_CLIENTS) {
            /* 检查是否已存在 */
            for (int i = 0; i < g_client_count; i++) {
                if (g_clients[i].hwnd == hwnd) return TRUE;
            }

            L2MClientMonitorInfo* c = &g_clients[g_client_count];
            c->hwnd = hwnd;
            wcsncpy_s(c->title, sizeof(c->title)/sizeof(wchar_t), title, _TRUNCATE);
            wcsncpy_s(c->region, sizeof(c->region)/sizeof(wchar_t), L"CN", _TRUNCATE);
            c->width = w;
            c->height = h;
            c->is_running = false;
            c->current_hp = 100;
            c->popup_blocked_count = 0;
            wcsncpy_s(c->status_text, sizeof(c->status_text)/sizeof(wchar_t), L"🔴 已就绪", _TRUNCATE);
            wcsncpy_s(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"待命中", _TRUNCATE);

            g_client_count++;
        }
    }
    return TRUE;
}

static void refresh_multi_clients(void) {
    ListView_DeleteAllItems(g_hListView);
    g_client_count = 0;

    EnumWindows(EnumMultiWindowsProcW, 0);

    for (int i = 0; i < g_client_count; i++) {
        LVITEMW item;
        memset(&item, 0, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = g_clients[i].title;
        SendMessageW(g_hListView, LVM_INSERTITEMW, 0, (LPARAM)&item);

        /* 默认勾选 */
        ListView_SetCheckState(g_hListView, i, TRUE);
        update_listview_item(i);
    }

    wchar_t log[128];
    swprintf(log, sizeof(log)/sizeof(wchar_t), L"多开窗口扫描完成: 共发现 %d 个在线游戏客户端。", g_client_count);
    append_multi_log_w(log);
}

/* 多开并发挂机与状态监控守护线程 */
static DWORD WINAPI MultiClientWorkerThread(LPVOID lpParam) {
    (void)lpParam;
    append_multi_log_w(L"▶️ 多开高频并发监控守护引擎已启动 (C 核心 0.3 微秒级分析)...");

    L2MImageBuffer* frame_bgr = l2m_image_create(960, 540, L2M_FMT_BGR888);
    L2MImageBuffer* frame_rgb = l2m_image_create(960, 540, L2M_FMT_RGB888);

    L2MHpConfig hp_cfg;
    memset(&hp_cfg, 0, sizeof(hp_cfg));
    hp_cfg.offset_x = 64;
    hp_cfg.offset_y = 21;
    hp_cfg.width = 103;
    hp_cfg.height = 2;
    hp_cfg.target_color_1 = (L2MRGB){168, 69, 2};
    hp_cfg.tolerance_1 = (L2MRGB){30, 30, 20};
    hp_cfg.target_color_2 = (L2MRGB){255, 157, 57};
    hp_cfg.tolerance_2 = (L2MRGB){10, 10, 10};

    int cycle = 0;
    while (g_global_worker_running) {
        cycle++;

        for (int i = 0; i < g_client_count; i++) {
            L2MClientMonitorInfo* c = &g_clients[i];
            if (!c->is_running || !c->hwnd || !IsWindow(c->hwnd)) continue;

            /* 1. 后台多级截屏 */
            if (l2m_capture_window(c->hwnd, true, frame_bgr)) {
                l2m_image_bgr_to_rgb(frame_bgr, frame_rgb);

                /* 2. 弹窗先验巡检与自动关闭 (每 2 次循环检查一次) */
                if (cycle % 2 == 0) {
                    L2MRect tl_roi = {10, 10, 260, 150};
                    L2MImageBuffer* tl_crop = l2m_image_create(tl_roi.width, tl_roi.height, L2M_FMT_RGB888);
                    if (tl_crop && l2m_image_crop(frame_rgb, &tl_roi, tl_crop)) {
                        L2MPopupResult pop_res;
                        if (l2m_detect_popup(tl_crop, tl_roi.x, tl_roi.y, L2M_POPUP_TOP_LEFT, true, &pop_res) && pop_res.detected) {
                            c->popup_blocked_count++;
                            wcsncpy_s(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"🛡️ 拦截并关闭弹窗", _TRUNCATE);

                            l2m_post_popup_dismiss_flow(
                                c->hwnd,
                                pop_res.has_checkbox ? pop_res.checkbox_pos.x : 0,
                                pop_res.has_checkbox ? pop_res.checkbox_pos.y : 0,
                                pop_res.button_pos.x,
                                pop_res.button_pos.y,
                                250
                            );

                            wchar_t pop_log[256];
                            swprintf(pop_log, sizeof(pop_log)/sizeof(wchar_t),
                                     L"🛡️ 窗口 [%08X] 成功拦截左上角提示弹窗并安全关闭 (累计 %d 次)",
                                     (unsigned int)(uintptr_t)c->hwnd, c->popup_blocked_count);
                            append_multi_log_w(pop_log);
                        }
                        l2m_image_free(tl_crop);
                    }
                }

                /* 3. 血条计算与逃跑保护 */
                L2MHpResult hp_res;
                if (l2m_calculate_hp_from_fullscreen(frame_rgb, &hp_cfg, &hp_res) && hp_res.is_valid) {
                    c->current_hp = hp_res.hp_percent;

                    if (c->current_hp < 30) {
                        wcsncpy_s(c->status_text, sizeof(c->status_text)/sizeof(wchar_t), L"⚠️ 低血量瞬移中", _TRUNCATE);
                        wcsncpy_s(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"瞬移逃跑保护", _TRUNCATE);

                        l2m_execute_teleport_home(c->hwnd);

                        wchar_t esc_log[256];
                        swprintf(esc_log, sizeof(esc_log)/sizeof(wchar_t),
                                 L"⚠️ 窗口 [%08X] 血量仅剩 %d%%，已自动触发安全瞬移保护！",
                                 (unsigned int)(uintptr_t)c->hwnd, c->current_hp);
                        append_multi_log_w(esc_log);
                    } else {
                        wcsncpy_s(c->status_text, sizeof(c->status_text)/sizeof(wchar_t), L"🟢 挂机运行中", _TRUNCATE);
                        wcsncpy_s(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"常规巡检", _TRUNCATE);
                    }
                }
            }

            /* 通知 UI 刷新当前行 */
            PostMessageW(g_hMainWnd, WM_USER_REFRESH_LV, (WPARAM)i, 0);
        }

        Sleep(200);
    }

    if (frame_bgr) l2m_image_free(frame_bgr);
    if (frame_rgb) l2m_image_free(frame_rgb);

    append_multi_log_w(L"⏹ 多开并发监控守护引擎已停止。");
    return 0;
}

/* 确保全局监控线程处于运行状态 */
static void ensure_global_worker_running(void) {
    if (!g_global_worker_running) {
        g_global_worker_running = true;
        g_hGlobalWorkerThread = CreateThread(NULL, 0, MultiClientWorkerThread, NULL, 0, NULL);
    }
}

/* 启动勾选的所有窗口 */
static void start_selected_clients(void) {
    int started_count = 0;
    for (int i = 0; i < g_client_count; i++) {
        if (ListView_GetCheckState(g_hListView, i)) {
            g_clients[i].is_running = true;
            wcsncpy_s(g_clients[i].status_text, sizeof(g_clients[i].status_text)/sizeof(wchar_t), L"🟢 挂机运行中", _TRUNCATE);
            update_listview_item(i);
            started_count++;
        }
    }

    if (started_count > 0) {
        ensure_global_worker_running();
        wchar_t log[128];
        swprintf(log, sizeof(log)/sizeof(wchar_t), L"已成功批量启动 %d 个多开客户端挂机监控！", started_count);
        append_multi_log_w(log);
    } else {
        MessageBoxW(g_hMainWnd, L"请先在列表中勾选要启动的游戏客户端！", L"提示", MB_OK | MB_ICONINFORMATION);
    }
}

/* 停止勾选的所有窗口 */
static void stop_selected_clients(void) {
    int stopped_count = 0;
    for (int i = 0; i < g_client_count; i++) {
        if (ListView_GetCheckState(g_hListView, i)) {
            g_clients[i].is_running = false;
            wcsncpy_s(g_clients[i].status_text, sizeof(g_clients[i].status_text)/sizeof(wchar_t), L"🔴 已停止", _TRUNCATE);
            update_listview_item(i);
            stopped_count++;
        }
    }

    wchar_t log[128];
    swprintf(log, sizeof(log)/sizeof(wchar_t), L"已批量停止 %d 个多开客户端。", stopped_count);
    append_multi_log_w(log);
}

/* 获取当前 ListView 中选中的窗口序号 */
static int get_selected_client_index(void) {
    if (!g_hListView) return -1;
    return (int)SendMessageW(g_hListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
}

/* 打开选中窗口的调试界面 */
static void open_selected_client_debug(void) {
    int idx = get_selected_client_index();
    if (idx >= 0 && idx < g_client_count) {
        l2m_open_debug_dialog(g_hMainWnd, g_clients[idx].hwnd);
    } else {
        /* 若未选中单行，打开第一个或者打开模拟测试 */
        if (g_client_count > 0) {
            l2m_open_debug_dialog(g_hMainWnd, g_clients[0].hwnd);
        } else {
            l2m_open_debug_dialog(g_hMainWnd, NULL);
        }
    }
}

/* 初始化 ListView 列结构 */
static void init_listview_columns(HWND hLV) {
    ListView_SetExtendedListViewStyle(hLV, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW col;
    memset(&col, 0, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = L"🎮 游戏客户端 / 窗口句柄";
    col.cx = 260;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);

    col.pszText = L"📐 分辨率";
    col.cx = 90;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);

    col.pszText = L"🌐 地区";
    col.cx = 65;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 2, (LPARAM)&col);

    col.pszText = L"🩸 血量 (HP)";
    col.cx = 95;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 3, (LPARAM)&col);

    col.pszText = L"🛡️ 弹窗拦截";
    col.cx = 95;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 4, (LPARAM)&col);

    col.pszText = L"📊 运行状态";
    col.cx = 120;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 5, (LPARAM)&col);

    col.pszText = L"⚡ 最近操作";
    col.cx = 160;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 6, (LPARAM)&col);
}

/* 主窗口过程 */
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hFontUI = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            g_hFontBold = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");

            /* 1. 顶部操作工具栏 */
            CreateWindowW(L"BUTTON", L"🔄 扫描多开客户端", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, 12, 140, 32, hWnd, (HMENU)ID_BTN_REFRESH_ALL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"☑️ 全选", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 165, 14, 65, 28, hWnd, (HMENU)ID_BTN_SELECT_ALL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"⬜ 全不选", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 235, 14, 75, 28, hWnd, (HMENU)ID_BTN_DESELECT_ALL, NULL, NULL);

            CreateWindowW(L"BUTTON", L"▶️ 启动所选客户端", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 330, 12, 145, 32, hWnd, (HMENU)ID_BTN_START_SELECTED, NULL, NULL);
            CreateWindowW(L"BUTTON", L"⏹ 停止所选客户端", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 485, 12, 145, 32, hWnd, (HMENU)ID_BTN_STOP_SELECTED, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🔍 选中窗口调试器", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 640, 12, 145, 32, hWnd, (HMENU)ID_BTN_DEBUG_SELECTED, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🏠 一键瞬移/回城", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 795, 12, 130, 32, hWnd, (HMENU)ID_BTN_ESCAPE_SELECTED, NULL, NULL);

            /* 2. 多开表格控件 ListView */
            g_hListView = CreateWindowExW(
                0, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                15, 52, 910, 290,
                hWnd, (HMENU)ID_LV_CLIENTS, GetModuleHandle(NULL), NULL
            );
            init_listview_columns(g_hListView);

            /* 3. 底部实时日志区 */
            CreateWindowW(L"STATIC", L"📋 多开全局高频监控与防御实时日志:", WS_CHILD | WS_VISIBLE, 15, 348, 300, 20, hWnd, NULL, NULL, NULL);
            g_hLogEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                       15, 370, 910, 185, hWnd, (HMENU)ID_TXT_MULTI_LOG, NULL, NULL);

            /* 统一设置字体 */
            apply_ui_font(hWnd, g_hFontUI);

            /* 注册全局紧急停止快捷键 Ctrl + Q */
            RegisterHotKey(hWnd, ID_HOTKEY_EMERGENCY_STOP, MOD_CONTROL, 'Q');

            /* 自动扫描一次 */
            refresh_multi_clients();
            if (l2m_is_run_as_admin()) {
                append_multi_log_w(L"🛡️ 运行环境: Administrator 管理员权限已就绪 (DirectInput / UIPI 无阻碍)。");
            } else {
                append_multi_log_w(L"⚠️ 提示: 当前未以管理员运行。若游戏为管理员模式，建议右键【以管理员身份运行】以确保键鼠穿透。");
            }
            append_multi_log_w(L"🛑 【全局安全防护】已启用紧急停止快捷键: [Ctrl + Q]，任何异常情况下按下即可瞬间切断挂机并释放鼠标。");
            append_multi_log_w(L"Lineage2MBot 纯 C 原生多开监控平台就绪 (默认回家快捷键为数字 0)。");
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_REFRESH_ALL) {
                refresh_multi_clients();
            } else if (id == ID_BTN_SELECT_ALL) {
                for (int i = 0; i < g_client_count; i++) ListView_SetCheckState(g_hListView, i, TRUE);
            } else if (id == ID_BTN_DESELECT_ALL) {
                for (int i = 0; i < g_client_count; i++) ListView_SetCheckState(g_hListView, i, FALSE);
            } else if (id == ID_BTN_START_SELECTED) {
                start_selected_clients();
            } else if (id == ID_BTN_STOP_SELECTED) {
                stop_selected_clients();
            } else if (id == ID_BTN_DEBUG_SELECTED) {
                open_selected_client_debug();
            } else if (id == ID_BTN_ESCAPE_SELECTED) {
                int idx = get_selected_client_index();
                if (idx >= 0 && idx < g_client_count) {
                    l2m_execute_teleport_home(g_clients[idx].hwnd);
                    wcsncpy_s(g_clients[idx].last_action, sizeof(g_clients[idx].last_action)/sizeof(wchar_t), L"🏠 按下 0 回家", _TRUNCATE);
                    update_listview_item(idx);

                    wchar_t esc_log[256];
                    swprintf(esc_log, sizeof(esc_log)/sizeof(wchar_t), L"已切换至目标游戏窗口 [%08X] 并执行模拟按下快捷键 '0' 回家！", (unsigned int)(uintptr_t)g_clients[idx].hwnd);
                    append_multi_log_w(esc_log);
                } else {
                    MessageBoxW(g_hMainWnd, L"请先在列表中点击选择要执行回城的游戏窗口！", L"提示", MB_OK | MB_ICONINFORMATION);
                }
            }
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == ID_HOTKEY_EMERGENCY_STOP) {
                emergency_stop_all();
            }
            return 0;
        }

        case WM_NOTIFY: {
            LPNMHDR pnm = (LPNMHDR)lParam;
            /* 双击表格某一行，直接打开该特定客户端的调试器 */
            if (pnm->idFrom == ID_LV_CLIENTS && pnm->code == NM_DBLCLK) {
                open_selected_client_debug();
            }
            return 0;
        }

        case WM_USER_REFRESH_LV: {
            int idx = (int)wParam;
            update_listview_item(idx);
            return 0;
        }

        case WM_DESTROY:
            UnregisterHotKey(hWnd, ID_HOTKEY_EMERGENCY_STOP);
            g_global_worker_running = false;
            if (g_hGlobalWorkerThread) {
                WaitForSingleObject(g_hGlobalWorkerThread, 1500);
                CloseHandle(g_hGlobalWorkerThread);
                g_hGlobalWorkerThread = NULL;
            }
            if (g_hFontUI) { DeleteObject(g_hFontUI); g_hFontUI = NULL; }
            if (g_hFontBold) { DeleteObject(g_hFontBold); g_hFontBold = NULL; }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool l2m_gui_init_common_controls(void) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES;
    return InitCommonControlsEx(&icex);
}

HWND l2m_create_main_window(HINSTANCE hInstance, int nCmdShow) {
    l2m_gui_init_common_controls();

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"L2M_Main_Multi_Window";
    RegisterClassW(&wc);

    int win_w = 960;
    int win_h = 610;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_w - win_w) / 2;
    int y = (screen_h - win_h) / 2;

    g_hMainWnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
        L"L2M_Main_Multi_Window",
        L"Lineage2MBot - 纯 C 原生多开高频监控工作台 (Multi-Client Dashboard)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, win_w, win_h,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    return g_hMainWnd;
}

#endif
