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
#include "../../include/l2m_cbt.h"
#include "../../include/l2m_window_profile.h"

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
#define ID_CHK_AUTO_POPUP      2012
#define ID_BTN_SAVE_CONFIG     2013
#define ID_EDIT_LOW_HP         2014
#define ID_EDIT_RECOVER_HP     2015
#define ID_CMB_REGION          2016
#define ID_CMB_MONITOR         2017
#define ID_BTN_ALIGN_4WINS     2018

#define ID_HOTKEY_EMERGENCY_STOP 3001
#define WM_USER_REFRESH_LV       (WM_USER + 105)

/* 多开客户端最大支持数 */
#define MAX_MULTI_CLIENTS 64

typedef struct {
    HWND hwnd;
    wchar_t title[128];
    wchar_t region[8];
    char character_name[64];
    int width;
    int height;
    bool is_running;
    bool auto_dismiss_popup;
    int low_hp_threshold;
    int recover_hp_threshold;
    bool is_resting_heal;
    int current_hp;
    int popup_blocked_count;
    wchar_t status_text[64];
    wchar_t last_action[64];
} L2MClientMonitorInfo;

static HWND g_hMainWnd = NULL;
static HWND g_hListView = NULL;
static HWND g_hLogEdit = NULL;
static HWND g_hChkAutoPopup = NULL;
static HWND g_hCmbRegion = NULL;
static HWND g_hCmbMonitor = NULL;
static HWND g_hEditLowHp = NULL;
static HWND g_hEditRecoverHp = NULL;
static HWND g_hSelectedClientLbl = NULL;
static HFONT g_hFontUI = NULL;
static HFONT g_hFontBold = NULL;

static L2MMonitorList g_monitor_list;
static L2MClientMonitorInfo g_clients[MAX_MULTI_CLIENTS];
static int g_client_count = 0;

static volatile bool g_global_worker_running = false;
static HANDLE g_hGlobalWorkerThread = NULL;

static void update_listview_item(int index);
static void append_multi_log_w(const wchar_t* text);
static void sync_selected_client_to_ui(int index);
static void save_current_client_popup_config(void);
static int get_selected_client_index(void);

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

    /* 列 3: 当前血量与阈值 */
    wchar_t col3[64];
    int low_th = (c->low_hp_threshold > 0) ? c->low_hp_threshold : 30;
    int rec_th = (c->recover_hp_threshold > 0) ? c->recover_hp_threshold : 80;
    if (c->current_hp >= 0) {
        swprintf(col3, sizeof(col3)/sizeof(wchar_t), L"%d%% (回:%d%% 出:%d%%)", c->current_hp, low_th, rec_th);
    } else {
        swprintf(col3, sizeof(col3)/sizeof(wchar_t), L"-- (回:%d%% 出:%d%%)", low_th, rec_th);
    }
    ListView_SetItemText(g_hListView, index, 3, col3);

    /* 列 4: 弹窗拦截数与开关状态 */
    wchar_t col4[48];
    if (c->auto_dismiss_popup) {
        swprintf(col4, sizeof(col4)/sizeof(wchar_t), L"%d 次 [开启]", c->popup_blocked_count);
    } else {
        swprintf(col4, sizeof(col4)/sizeof(wchar_t), L"%d 次 [关闭]", c->popup_blocked_count);
    }
    ListView_SetItemText(g_hListView, index, 4, col4);

    /* 列 5: 运行状态 */
    ListView_SetItemText(g_hListView, index, 5, c->status_text);

    /* 列 6: 最近操作 */
    ListView_SetItemText(g_hListView, index, 6, c->last_action);
}

/* 同步当前选中客户端的状态至顶部策略配置栏 */
static void sync_selected_client_to_ui(int index) {
    if (index < 0 || index >= g_client_count) {
        if (g_hSelectedClientLbl) SetWindowTextW(g_hSelectedClientLbl, L"🎮 目标窗口: 未选择 (请在下方列表点击选中)");
        if (g_hChkAutoPopup) SendMessageW(g_hChkAutoPopup, BM_SETCHECK, BST_UNCHECKED, 0);
        if (g_hCmbRegion) SendMessageW(g_hCmbRegion, CB_SETCURSEL, 0, 0);
        if (g_hEditLowHp) SetWindowTextW(g_hEditLowHp, L"30");
        if (g_hEditRecoverHp) SetWindowTextW(g_hEditRecoverHp, L"80");
        return;
    }

    L2MClientMonitorInfo* c = &g_clients[index];
    wchar_t info[256];
    wchar_t char_name_w[64] = {0};
    MultiByteToWideChar(CP_UTF8, 0, c->character_name, -1, char_name_w, sizeof(char_name_w)/sizeof(wchar_t) - 1);

    swprintf(info, sizeof(info)/sizeof(wchar_t), L"🎮 目标: [%08X] %ls (%ls)",
             (unsigned int)(uintptr_t)c->hwnd, char_name_w, c->region);
    if (g_hSelectedClientLbl) SetWindowTextW(g_hSelectedClientLbl, info);

    if (g_hChkAutoPopup) {
        SendMessageW(g_hChkAutoPopup, BM_SETCHECK, c->auto_dismiss_popup ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    /* 同步地区下拉框 */
    if (g_hCmbRegion) {
        int sel_idx = (int)SendMessageW(g_hCmbRegion, CB_FINDSTRINGEXACT, -1, (LPARAM)c->region);
        if (sel_idx != CB_ERR) {
            SendMessageW(g_hCmbRegion, CB_SETCURSEL, (WPARAM)sel_idx, 0);
        } else {
            SendMessageW(g_hCmbRegion, CB_SETCURSEL, 0, 0);
        }
    }

    int low_th = (c->low_hp_threshold > 0) ? c->low_hp_threshold : 30;
    int rec_th = (c->recover_hp_threshold > 0) ? c->recover_hp_threshold : 80;
    wchar_t num_buf[16];
    if (g_hEditLowHp) {
        swprintf(num_buf, sizeof(num_buf)/sizeof(wchar_t), L"%d", low_th);
        SetWindowTextW(g_hEditLowHp, num_buf);
    }
    if (g_hEditRecoverHp) {
        swprintf(num_buf, sizeof(num_buf)/sizeof(wchar_t), L"%d", rec_th);
        SetWindowTextW(g_hEditRecoverHp, num_buf);
    }
}

/* 保存当前选中客户端的自动弹窗配置与血量阈值策略 */
static void save_current_client_popup_config(void) {
    int idx = get_selected_client_index();
    if (idx < 0 || idx >= g_client_count) {
        MessageBoxW(g_hMainWnd, L"请先在下方列表中点击选择要配置的游戏窗口客户端！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    L2MClientMonitorInfo* c = &g_clients[idx];
    bool is_checked = (SendMessageW(g_hChkAutoPopup, BM_GETCHECK, 0, 0) == BST_CHECKED);
    c->auto_dismiss_popup = is_checked;

    /* 读取选中的地区 */
    wchar_t reg_w[16] = L"CN";
    if (g_hCmbRegion) {
        int sel = (int)SendMessageW(g_hCmbRegion, CB_GETCURSEL, 0, 0);
        if (sel != CB_ERR) {
            SendMessageW(g_hCmbRegion, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)reg_w);
        }
    }
    wcsncpy_s(c->region, sizeof(c->region)/sizeof(wchar_t), reg_w, _TRUNCATE);
    char reg_utf8[16] = "CN";
    WideCharToMultiByte(CP_UTF8, 0, reg_w, -1, reg_utf8, sizeof(reg_utf8) - 1, NULL, NULL);

    /* 读取低血量回城与恢复出战阈值 */
    wchar_t low_buf[16] = {0}, rec_buf[16] = {0};
    if (g_hEditLowHp) GetWindowTextW(g_hEditLowHp, low_buf, sizeof(low_buf)/sizeof(wchar_t));
    if (g_hEditRecoverHp) GetWindowTextW(g_hEditRecoverHp, rec_buf, sizeof(rec_buf)/sizeof(wchar_t));

    int low_hp = _wtoi(low_buf);
    int rec_hp = _wtoi(rec_buf);

    if (low_hp < 5 || low_hp > 90) low_hp = 30;
    if (rec_hp < 20 || rec_hp > 100) rec_hp = 80;
    if (rec_hp <= low_hp) rec_hp = low_hp + 10;

    c->low_hp_threshold = low_hp;
    c->recover_hp_threshold = rec_hp;

    /* 若未命名角色，自动分配角色名称 */
    if (c->character_name[0] == '\0' || strcmp(c->character_name, "Unknown") == 0) {
        if (c->title[0] != L'\0') {
            WideCharToMultiByte(CP_UTF8, 0, c->title, -1, c->character_name, sizeof(c->character_name) - 1, NULL, NULL);
        } else {
            snprintf(c->character_name, sizeof(c->character_name), "Client_%d", idx + 1);
        }
    }

    /* 1. 保存至 data/id/<name>.json (更新地区、弹窗开关与血量阈值) */
    l2m_id_profile_set_region(c->character_name, reg_utf8);
    l2m_id_profile_set_auto_dismiss_popup(c->character_name, is_checked);
    bool id_saved = l2m_id_profile_set_hp_thresholds(c->character_name, low_hp, rec_hp);

    /* 2. 保存至 data/window_profiles.json */
    L2MWindowProfileList list;
    bool win_prof_saved = false;
    if (l2m_window_profiles_load(NULL, &list)) {
        bool found = false;
        for (int i = 0; i < list.count; i++) {
            if (strcmp(list.profiles[i].character_name, c->character_name) == 0 ||
                (list.profiles[i].match_rule == L2M_WIN_MATCH_INDEX && list.profiles[i].match_window_index == idx)) {
                list.profiles[i].auto_dismiss_popup = is_checked;
                list.profiles[i].low_hp_threshold = low_hp;
                list.profiles[i].recover_hp_threshold = rec_hp;
                snprintf(list.profiles[i].region, sizeof(list.profiles[i].region), "%s", reg_utf8);
                snprintf(list.profiles[i].character_name, sizeof(list.profiles[i].character_name), "%s", c->character_name);
                found = true;
                break;
            }
        }
        if (!found && list.count < MAX_WINDOW_PROFILES) {
            L2MWindowProfile* p = &list.profiles[list.count++];
            snprintf(p->profile_id, sizeof(p->profile_id), "profile_%02d", list.count);
            snprintf(p->profile_name, sizeof(p->profile_name), "%s", c->character_name);
            snprintf(p->character_name, sizeof(p->character_name), "%s", c->character_name);
            snprintf(p->region, sizeof(p->region), "%s", reg_utf8);
            p->match_rule = L2M_WIN_MATCH_INDEX;
            p->match_window_index = idx;
            p->auto_dismiss_popup = is_checked;
            p->low_hp_threshold = low_hp;
            p->recover_hp_threshold = rec_hp;
            p->enabled = true;
        }
        win_prof_saved = l2m_window_profiles_save(NULL, &list);
    }

    update_listview_item(idx);
    sync_selected_client_to_ui(idx);

    wchar_t char_name_w[64] = {0};
    MultiByteToWideChar(CP_UTF8, 0, c->character_name, -1, char_name_w, sizeof(char_name_w)/sizeof(wchar_t) - 1);
    wchar_t log[256];
    swprintf(log, sizeof(log)/sizeof(wchar_t),
             L"💾 窗口 [%08X] (角色: %ls) 策略已保存: 地区[%ls], 弹窗[%ls], 低血回城[%d%%], 恢复出战[%d%%] (ID:%ls, WinProf:%ls)",
             (unsigned int)(uintptr_t)c->hwnd, char_name_w, reg_w, is_checked ? L"开启" : L"关闭",
             low_hp, rec_hp, id_saved ? L"已更新" : L"创建失败", win_prof_saved ? L"已更新" : L"创建失败");
    append_multi_log_w(log);
}

static void refresh_multi_clients(void) {
    ListView_DeleteAllItems(g_hListView);
    g_client_count = 0;

    L2MWindowProfileList prof_list;
    l2m_window_profiles_load(NULL, &prof_list);

    L2MWindowInstance detected_wins[MAX_MULTI_CLIENTS];
    int32_t detected_count = 0;
    l2m_enum_game_windows(detected_wins, MAX_MULTI_CLIENTS, &detected_count);

    for (int i = 0; i < detected_count; i++) {
        const L2MWindowInstance* win_inst = &detected_wins[i];

        L2MClientMonitorInfo* c = &g_clients[g_client_count];
        c->hwnd = win_inst->hwnd;
        MultiByteToWideChar(CP_UTF8, 0, win_inst->window_title, -1, c->title, sizeof(c->title)/sizeof(wchar_t) - 1);
        c->width = win_inst->client_width;
        c->height = win_inst->client_height;
        c->is_running = false;
        c->current_hp = 100;
        c->popup_blocked_count = 0;
        c->low_hp_threshold = 30;
        c->recover_hp_threshold = 80;
        c->is_resting_heal = false;
        wcsncpy_s(c->status_text, sizeof(c->status_text)/sizeof(wchar_t), L"🔴 已就绪", _TRUNCATE);
        wcsncpy_s(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"待命中", _TRUNCATE);

        /* 匹配角色与语言配置 */
        L2MWindowProfile matched_prof;
        if (l2m_window_profile_match(&prof_list, win_inst, i, &matched_prof)) {
            snprintf(c->character_name, sizeof(c->character_name), "%s", matched_prof.character_name);
            if (matched_prof.region[0] != '\0') {
                MultiByteToWideChar(CP_UTF8, 0, matched_prof.region, -1, c->region, sizeof(c->region)/sizeof(wchar_t) - 1);
            } else {
                wcsncpy_s(c->region, sizeof(c->region)/sizeof(wchar_t), L"CN", _TRUNCATE);
            }
            c->auto_dismiss_popup = matched_prof.auto_dismiss_popup;
            if (matched_prof.low_hp_threshold > 0) c->low_hp_threshold = matched_prof.low_hp_threshold;
            if (matched_prof.recover_hp_threshold > 0) c->recover_hp_threshold = matched_prof.recover_hp_threshold;

            /* 尝试读取独立 data/id/<character_name>.json */
            L2MIdConfig id_cfg;
            if (l2m_id_profile_load(matched_prof.character_name, &id_cfg)) {
                c->auto_dismiss_popup = id_cfg.auto_dismiss_popup;
                if (id_cfg.region[0] != '\0') {
                    MultiByteToWideChar(CP_UTF8, 0, id_cfg.region, -1, c->region, sizeof(c->region)/sizeof(wchar_t) - 1);
                }
                if (id_cfg.low_hp_threshold > 0) c->low_hp_threshold = id_cfg.low_hp_threshold;
                if (id_cfg.recover_hp_threshold > 0) c->recover_hp_threshold = id_cfg.recover_hp_threshold;
            }
        } else {
            snprintf(c->character_name, sizeof(c->character_name), "Unknown");
            wcsncpy_s(c->region, sizeof(c->region)/sizeof(wchar_t), L"CN", _TRUNCATE);
            c->auto_dismiss_popup = true;
            c->low_hp_threshold = 30;
            c->recover_hp_threshold = 80;
        }

        LVITEMW item;
        memset(&item, 0, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = g_client_count;
        item.iSubItem = 0;
        item.pszText = c->title;
        SendMessageW(g_hListView, LVM_INSERTITEMW, 0, (LPARAM)&item);

        /* 默认勾选 */
        ListView_SetCheckState(g_hListView, g_client_count, TRUE);
        update_listview_item(g_client_count);
        g_client_count++;
    }

    if (g_client_count > 0) {
        ListView_SetItemState(g_hListView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        sync_selected_client_to_ui(0);
    } else {
        sync_selected_client_to_ui(-1);
    }

    wchar_t log[128];
    swprintf(log, sizeof(log)/sizeof(wchar_t), L"多开窗口扫描完成: 共发现 %d 个在线游戏客户端。", g_client_count);
    append_multi_log_w(log);
}

/* 预加载 CBT 配置缓存 (完全由配置文件驱动) */
static L2MHpConfig g_default_hp_cfg;

/* 多开并发挂机与状态监控守护线程 (超高频血量监测 + 低频弹窗巡检 + 节流刷新) */
static DWORD WINAPI MultiClientWorkerThread(LPVOID lpParam) {
    (void)lpParam;
    append_multi_log_w(L"▶️ 多开超高频并发监控守护引擎已启动 (血量 30ms 极速巡检, 弹窗智能分频)...");

    /* 初始化默认血条配置 */
    L2MCbtConfig cbt_cn;
    bool cbt_cn_loaded = l2m_cbt_load("CN", &cbt_cn);
    memset(&g_default_hp_cfg, 0, sizeof(g_default_hp_cfg));
    if (cbt_cn_loaded && cbt_cn.hp_bar_cfg.width > 0) {
        g_default_hp_cfg = cbt_cn.hp_bar_cfg;
    } else {
        g_default_hp_cfg.width = 103;
        g_default_hp_cfg.height = 2;
        g_default_hp_cfg.target_color_1 = (L2MRGB){230, 48, 48};
        g_default_hp_cfg.tolerance_1 = (L2MRGB){25, 25, 25};
        g_default_hp_cfg.target_color_2 = (L2MRGB){255, 157, 57};
        g_default_hp_cfg.tolerance_2 = (L2MRGB){10, 10, 10};
    }

    /* 准备血条极速 ROI 缓冲区 (动态按配置宽度分配，微秒级抓取) */
    int roi_w = (g_default_hp_cfg.width > 0) ? g_default_hp_cfg.width : 150;
    int roi_h = (g_default_hp_cfg.height > 0) ? g_default_hp_cfg.height : 4;
    L2MImageBuffer* hp_bgr = l2m_image_create(roi_w, roi_h, L2M_FMT_BGR888);

    /* 准备弹窗低频全屏缓冲区 */
    L2MImageBuffer* frame_bgr = l2m_image_create(960, 540, L2M_FMT_BGR888);
    L2MImageBuffer* frame_rgb = l2m_image_create(960, 540, L2M_FMT_RGB888);

    int cycle = 0;
    DWORD last_ui_tick = GetTickCount();

    while (g_global_worker_running) {
        cycle++;

        for (int i = 0; i < g_client_count; i++) {
            L2MClientMonitorInfo* c = &g_clients[i];
            if (!c->is_running || !c->hwnd || !IsWindow(c->hwnd)) continue;

            /* 1. 超高频血条 ROI 极速截屏与计算 (每 30ms 触发一次，0 内存分配，0 格式转换开销) */
            char reg_utf8[16] = "CN";
            WideCharToMultiByte(CP_UTF8, 0, c->region, -1, reg_utf8, sizeof(reg_utf8), NULL, NULL);

            L2MCbtConfig* p_cbt = NULL;
            L2MCbtConfig cbt_local;
            if (strcmp(reg_utf8, "CN") == 0 && cbt_cn_loaded) {
                p_cbt = &cbt_cn;
            } else if (l2m_cbt_load(reg_utf8, &cbt_local)) {
                p_cbt = &cbt_local;
            }

            L2MRect dyn_hp_roi = {g_default_hp_cfg.offset_x, g_default_hp_cfg.offset_y, g_default_hp_cfg.width, g_default_hp_cfg.height};
            L2MHpConfig dyn_hp_cfg = g_default_hp_cfg;
            dyn_hp_cfg.offset_x = 0;
            dyn_hp_cfg.offset_y = 0;

            if (p_cbt && p_cbt->hp_bar_cfg.width > 0 && p_cbt->hp_bar_cfg.height > 0) {
                dyn_hp_roi.x = p_cbt->hp_bar_cfg.offset_x;
                dyn_hp_roi.y = p_cbt->hp_bar_cfg.offset_y;
                dyn_hp_roi.width = p_cbt->hp_bar_cfg.width;
                dyn_hp_roi.height = p_cbt->hp_bar_cfg.height;

                dyn_hp_cfg = p_cbt->hp_bar_cfg;
                dyn_hp_cfg.offset_x = 0;
                dyn_hp_cfg.offset_y = 0;
            }

            if (l2m_capture_window_roi(c->hwnd, &dyn_hp_roi, hp_bgr)) {
                L2MHpResult hp_res;
                if (l2m_calculate_hp_bgr(hp_bgr, &dyn_hp_cfg, &hp_res) && hp_res.is_valid) {
                    c->current_hp = hp_res.hp_percent;
                    int low_th = (c->low_hp_threshold > 0) ? c->low_hp_threshold : 30;
                    int rec_th = (c->recover_hp_threshold > 0) ? c->recover_hp_threshold : 80;

                    if (c->current_hp < low_th) {
                        /* 毫秒级极速逃生回城保护 */
                        if (!c->is_resting_heal) {
                            c->is_resting_heal = true;
                            l2m_execute_teleport_home(c->hwnd);

                            wchar_t esc_log[256];
                            swprintf(esc_log, sizeof(esc_log)/sizeof(wchar_t),
                                     L"⚠️ 窗口 [%08X] 实测血量 %d%% (低于阈值 %d%%)，毫秒级瞬发回城并进入休整状态！",
                                     (unsigned int)(uintptr_t)c->hwnd, c->current_hp, low_th);
                            append_multi_log_w(esc_log);
                        }
                        wcsncpy_s(c->status_text, sizeof(c->status_text)/sizeof(wchar_t), L"⚠️ 低血回城休整", _TRUNCATE);
                        wcsncpy_s(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"安全区回血中...", _TRUNCATE);
                    } else if (c->is_resting_heal) {
                        /* 处于休整状态，检查是否回满达标 */
                        if (c->current_hp >= rec_th) {
                            c->is_resting_heal = false;
                            wcsncpy_s(c->status_text, sizeof(c->status_text)/sizeof(wchar_t), L"🟢 恢复出战挂机", _TRUNCATE);
                            wcsncpy_s(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"血量达标重新出战", _TRUNCATE);

                            wchar_t rec_log[256];
                            swprintf(rec_log, sizeof(rec_log)/sizeof(wchar_t),
                                     L"✨ 窗口 [%08X] 血量已恢复至 %d%% (达标 >= %d%%)，重新投入战斗挂机！",
                                     (unsigned int)(uintptr_t)c->hwnd, c->current_hp, rec_th);
                            append_multi_log_w(rec_log);
                        } else {
                            wcsncpy_s(c->status_text, sizeof(c->status_text)/sizeof(wchar_t), L"🟡 休整回血中", _TRUNCATE);
                            swprintf(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"回血进度: %d%% / %d%%", c->current_hp, rec_th);
                        }
                    } else {
                        wcsncpy_s(c->status_text, sizeof(c->status_text)/sizeof(wchar_t), L"🟢 挂机运行中", _TRUNCATE);
                        wcsncpy_s(c->last_action, sizeof(c->last_action)/sizeof(wchar_t), L"高频血量监控中", _TRUNCATE);
                    }
                }
            }

            /* 2. 弹窗先验巡检与自动关闭 (智能降频分流: 每 30 个周期 / 约 1 秒巡检一次) */
            if (c->auto_dismiss_popup && (cycle % 30 == 0)) {
                if (l2m_capture_window(c->hwnd, true, frame_bgr)) {
                    l2m_image_bgr_to_rgb(frame_bgr, frame_rgb);

                    char reg_utf8[16] = "CN";
                    WideCharToMultiByte(CP_UTF8, 0, c->region, -1, reg_utf8, sizeof(reg_utf8), NULL, NULL);

                    L2MCbtConfig* p_cbt = NULL;
                    L2MCbtConfig cbt_local;
                    if (strcmp(reg_utf8, "CN") == 0 && cbt_cn_loaded) {
                        p_cbt = &cbt_cn;
                    } else if (l2m_cbt_load(reg_utf8, &cbt_local)) {
                        p_cbt = &cbt_local;
                    }

                    if (p_cbt) {
                        L2MPopupResult pop_res;
                        if (l2m_detect_all_popups(frame_rgb, p_cbt, &pop_res) && pop_res.detected) {
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
                                     L"🛡️ 窗口 [%08X] 成功拦截弹窗 [%hs] 并安全关闭 (累计 %d 次)",
                                     (unsigned int)(uintptr_t)c->hwnd, pop_res.popup_name, c->popup_blocked_count);
                            append_multi_log_w(pop_log);
                        }
                    }
                }
            }
        }

        /* 3. UI 列表平滑节流刷新 (每 150ms 刷新一次，避免主线程 Windows 消息队列风暴) */
        DWORD now = GetTickCount();
        if (now - last_ui_tick >= 150) {
            last_ui_tick = now;
            for (int i = 0; i < g_client_count; i++) {
                if (g_clients[i].is_running) {
                    PostMessageW(g_hMainWnd, WM_USER_REFRESH_LV, (WPARAM)i, 0);
                }
            }
        }

        /* 超高频 30ms 循环间隔 (~33 FPS 实时血量高敏捕捉) */
        Sleep(30);
    }

    if (hp_bgr) l2m_image_free(hp_bgr);
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

/* 刷新物理显示器列表至下拉框 */
static void refresh_monitors_ui(void) {
    if (!g_hCmbMonitor) return;
    SendMessageW(g_hCmbMonitor, CB_RESETCONTENT, 0, 0);

    l2m_enum_monitors(&g_monitor_list);

    for (int i = 0; i < g_monitor_list.count; i++) {
        const L2MMonitorInfo* mon = &g_monitor_list.monitors[i];
        wchar_t wdesc[128];
        MultiByteToWideChar(CP_UTF8, 0, mon->desc, -1, wdesc, sizeof(wdesc)/sizeof(wchar_t));
        SendMessageW(g_hCmbMonitor, CB_ADDSTRING, 0, (LPARAM)wdesc);
    }

    int sel = (g_monitor_list.primary_index >= 0 && g_monitor_list.primary_index < g_monitor_list.count)
              ? g_monitor_list.primary_index : 0;
    SendMessageW(g_hCmbMonitor, CB_SETCURSEL, sel, 0);
}

/* 执行多开游戏窗口对齐排列至选中的物理显示器 */
static void execute_main_align_windows(void) {
    int mon_idx = (int)SendMessageW(g_hCmbMonitor, CB_GETCURSEL, 0, 0);
    if (mon_idx < 0) mon_idx = 0;

    int32_t aligned_cnt = 0;
    if (l2m_align_game_windows_ex(L2M_ALIGN_GRID_2X2, 960, 540, mon_idx, &aligned_cnt)) {
        refresh_multi_clients();

        wchar_t mon_name[128] = L"主屏幕";
        if (mon_idx < g_monitor_list.count) {
            MultiByteToWideChar(CP_UTF8, 0, g_monitor_list.monitors[mon_idx].desc, -1, mon_name, 128);
        }

        wchar_t log[256];
        swprintf(log, sizeof(log)/sizeof(wchar_t),
                 L"🪟 已将 %d 个游戏窗口自动对齐排列至【%ls】工作区 (960x540 标准四宫格)！",
                 aligned_cnt, mon_name);
        append_multi_log_w(log);
        MessageBoxW(g_hMainWnd, log, L"四开对齐完成", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(g_hMainWnd, L"未检测到运行中的游戏窗口，无法执行对齐！", L"提示", MB_OK | MB_ICONWARNING);
    }
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
    col.cx = 250;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);

    col.pszText = L"📐 分辨率";
    col.cx = 85;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);

    col.pszText = L"🌐 地区";
    col.cx = 50;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 2, (LPARAM)&col);

    col.pszText = L"🩸 血量 (回城/出战)";
    col.cx = 150;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 3, (LPARAM)&col);

    col.pszText = L"🛡️ 自动弹窗防御";
    col.cx = 110;
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 4, (LPARAM)&col);

    col.pszText = L"📊 运行状态";
    col.cx = 115;
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

            /* 1. 顶部操作工具栏 (第 1 行) */
            CreateWindowW(L"BUTTON", L"🔄 扫描多开", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 15, 10, 110, 30, hWnd, (HMENU)ID_BTN_REFRESH_ALL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"☑️ 全选", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130, 11, 58, 28, hWnd, (HMENU)ID_BTN_SELECT_ALL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"▶️ 启动", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 193, 10, 80, 30, hWnd, (HMENU)ID_BTN_START_SELECTED, NULL, NULL);
            CreateWindowW(L"BUTTON", L"⏹ 停止", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 278, 10, 80, 30, hWnd, (HMENU)ID_BTN_STOP_SELECTED, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🔍 调试器", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 363, 10, 85, 30, hWnd, (HMENU)ID_BTN_DEBUG_SELECTED, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🏠 瞬移回城", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 453, 10, 95, 30, hWnd, (HMENU)ID_BTN_ESCAPE_SELECTED, NULL, NULL);

            /* 显示器选择与对齐 */
            CreateWindowW(L"STATIC", L"🖥️ 显示器:", WS_CHILD | WS_VISIBLE, 555, 15, 65, 20, hWnd, NULL, NULL, NULL);
            g_hCmbMonitor = CreateWindowW(L"COMBOBOX", L"",
                                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                          620, 12, 220, 200, hWnd, (HMENU)ID_CMB_MONITOR, NULL, NULL);
            CreateWindowW(L"BUTTON", L"🪟 四开对齐", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 845, 10, 90, 30, hWnd, (HMENU)ID_BTN_ALIGN_4WINS, NULL, NULL);

            /* 2. 选中窗口快速策略设置栏 (第 2 行) */
            g_hSelectedClientLbl = CreateWindowW(L"STATIC", L"🎮 目标: 未选择 (请在下方列表点击选中)",
                                                 WS_CHILD | WS_VISIBLE, 15, 49, 210, 20, hWnd, NULL, NULL, NULL);

            CreateWindowW(L"STATIC", L"地区:", WS_CHILD | WS_VISIBLE, 230, 49, 35, 20, hWnd, NULL, NULL, NULL);
            g_hCmbRegion = CreateWindowW(L"COMBOBOX", L"",
                                         WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                         268, 46, 70, 160, hWnd, (HMENU)ID_CMB_REGION, NULL, NULL);
            SendMessageW(g_hCmbRegion, CB_ADDSTRING, 0, (LPARAM)L"CN");
            SendMessageW(g_hCmbRegion, CB_ADDSTRING, 0, (LPARAM)L"TW");
            SendMessageW(g_hCmbRegion, CB_ADDSTRING, 0, (LPARAM)L"EN");
            SendMessageW(g_hCmbRegion, CB_ADDSTRING, 0, (LPARAM)L"JP");
            SendMessageW(g_hCmbRegion, CB_ADDSTRING, 0, (LPARAM)L"KR");
            SendMessageW(g_hCmbRegion, CB_ADDSTRING, 0, (LPARAM)L"RU");
            SendMessageW(g_hCmbRegion, CB_SETCURSEL, 0, 0);

            g_hChkAutoPopup = CreateWindowW(L"BUTTON", L"🛡️ 自动关弹窗",
                                            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 345, 47, 115, 24, hWnd, (HMENU)ID_CHK_AUTO_POPUP, NULL, NULL);

            CreateWindowW(L"STATIC", L"低血回城:", WS_CHILD | WS_VISIBLE, 465, 49, 55, 20, hWnd, NULL, NULL, NULL);
            g_hEditLowHp = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"30",
                                           WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER, 522, 47, 32, 22, hWnd, (HMENU)ID_EDIT_LOW_HP, NULL, NULL);
            CreateWindowW(L"STATIC", L"%", WS_CHILD | WS_VISIBLE, 556, 49, 14, 20, hWnd, NULL, NULL, NULL);

            CreateWindowW(L"STATIC", L"回满出战:", WS_CHILD | WS_VISIBLE, 573, 49, 55, 20, hWnd, NULL, NULL, NULL);
            g_hEditRecoverHp = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"80",
                                               WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER, 630, 47, 32, 22, hWnd, (HMENU)ID_EDIT_RECOVER_HP, NULL, NULL);
            CreateWindowW(L"STATIC", L"%", WS_CHILD | WS_VISIBLE, 664, 49, 14, 20, hWnd, NULL, NULL, NULL);

            CreateWindowW(L"BUTTON", L"💾 保存配置", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 685, 45, 90, 26, hWnd, (HMENU)ID_BTN_SAVE_CONFIG, NULL, NULL);

            /* 3. 多开表格控件 ListView */
            g_hListView = CreateWindowExW(
                0, WC_LISTVIEWW, L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                15, 76, 920, 255,
                hWnd, (HMENU)ID_LV_CLIENTS, GetModuleHandle(NULL), NULL
            );
            init_listview_columns(g_hListView);

            /* 4. 底部实时日志区 */
            CreateWindowW(L"STATIC", L"📋 多开全局高频监控与防御实时日志:", WS_CHILD | WS_VISIBLE, 15, 338, 320, 20, hWnd, NULL, NULL, NULL);
            g_hLogEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                       15, 358, 920, 210, hWnd, (HMENU)ID_TXT_MULTI_LOG, NULL, NULL);

            /* 统一设置字体 */
            apply_ui_font(hWnd, g_hFontUI);

            /* 注册全局紧急停止快捷键 Ctrl + Q */
            RegisterHotKey(hWnd, ID_HOTKEY_EMERGENCY_STOP, MOD_CONTROL, 'Q');

            /* 枚举系统物理显示器并填入下拉框 */
            refresh_monitors_ui();

            /* 自动扫描一次 */
            refresh_multi_clients();
            if (l2m_is_run_as_admin()) {
                append_multi_log_w(L"🛡️ 运行环境: Administrator 管理员权限已就绪 (DirectInput / UIPI 无阻碍)。");
            } else {
                append_multi_log_w(L"⚠️ 提示: 当前未以管理员运行。若游戏为管理员模式，建议右键【以管理员身份运行】以确保键鼠穿透。");
            }
            append_multi_log_w(L"🛑 【全局安全防护】已启用紧急停止快捷键: [Ctrl + Q]，任何异常情况下按下即可瞬间切断挂机并释放鼠标。");
            append_multi_log_w(L"Lineage2MBot 纯 C 原生多开监控平台就绪 (支持多显示器选择、四开对齐与独立角色弹窗防御)。");
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_REFRESH_ALL) {
                refresh_multi_clients();
                refresh_monitors_ui();
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
            } else if (id == ID_BTN_ALIGN_4WINS) {
                execute_main_align_windows();
            } else if (id == ID_CHK_AUTO_POPUP || id == ID_BTN_SAVE_CONFIG) {
                save_current_client_popup_config();
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
            if (pnm->idFrom == ID_LV_CLIENTS) {
                if (pnm->code == NM_DBLCLK) {
                    open_selected_client_debug();
                } else if (pnm->code == LVN_ITEMCHANGED) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if (pnmv->uNewState & LVIS_SELECTED) {
                        sync_selected_client_to_ui(pnmv->iItem);
                    }
                }
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

    int win_w = 970;
    int win_h = 635;
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
