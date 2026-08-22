/**
 * @file window_profile_manager.c
 * @brief Lineage2MBot 游戏窗口枚举、多开配置与多语言角色绑定管理器 (100% 纯 C 原生实现)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../../include/l2m_window_profile.h"
#include "../../include/l2m_cbt.h"
#include "../../include/l2m_zone.h"
#include "../../include/l2m_vision.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <direct.h>
#endif

/* 辅助：确保父目录存在 */
static void ensure_parent_dir_exists_local(const char* filepath) {
    if (!filepath) return;
    char temp[MAX_PATH];
    strncpy(temp, filepath, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    for (char* p = temp; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char old = *p;
            *p = '\0';
            if (strlen(temp) > 0) {
#ifdef _WIN32
                _mkdir(temp);
#endif
            }
            *p = old;
        }
    }
}

/* 探测 window_profiles.json 文件的物理绝对路径 */
static bool get_window_profile_path(const char* custom_path, char* out_path, size_t max_len) {
    if (!out_path || max_len == 0) return false;
    if (custom_path && strlen(custom_path) > 0) {
        strncpy(out_path, custom_path, max_len - 1);
        out_path[max_len - 1] = '\0';
        return true;
    }

#ifdef _WIN32
    char exe_dir[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir))) {
        char* last_slash = strrchr(exe_dir, '\\');
        if (!last_slash) last_slash = strrchr(exe_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            char candidate[MAX_PATH];

            snprintf(candidate, sizeof(candidate), "%s/data/window_profiles.json", exe_dir);
            FILE* fp = fopen(candidate, "rb");
            if (fp) { fclose(fp); strncpy(out_path, candidate, max_len); return true; }

            snprintf(candidate, sizeof(candidate), "%s/../data/window_profiles.json", exe_dir);
            fp = fopen(candidate, "rb");
            if (fp) { fclose(fp); strncpy(out_path, candidate, max_len); return true; }
        }
    }
#endif

    const char* candidates[] = {
        "data/window_profiles.json",
        "../data/window_profiles.json",
        "bot/data/window_profiles.json"
    };
    int num = (int)(sizeof(candidates) / sizeof(candidates[0]));
    for (int i = 0; i < num; i++) {
        FILE* fp = fopen(candidates[i], "rb");
        if (fp) {
            fclose(fp);
            strncpy(out_path, candidates[i], max_len - 1);
            out_path[max_len - 1] = '\0';
            return true;
        }
    }

#ifdef _WIN32
    if (exe_dir[0]) {
        snprintf(out_path, max_len, "%s/../data/window_profiles.json", exe_dir);
        return true;
    }
#endif
    strncpy(out_path, "data/window_profiles.json", max_len - 1);
    out_path[max_len - 1] = '\0';
    return true;
}

#ifdef _WIN32
typedef struct {
    L2MWindowInstance* list;
    int32_t max_count;
    int32_t count;
} EnumWinContext;

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    EnumWinContext* ctx = (EnumWinContext*)lParam;
    if (ctx->count >= ctx->max_count) return FALSE;

    if (!IsWindowVisible(hwnd)) return TRUE;

    /* 过滤子窗口与无尺寸窗口 */
    if (GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int cw = rcClient.right - rcClient.left;
    int ch = rcClient.bottom - rcClient.top;

    if (cw < 100 || ch < 100) return TRUE;

    wchar_t wTitle[256] = {0};
    GetWindowTextW(hwnd, wTitle, sizeof(wTitle)/sizeof(wchar_t));

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    char procName[64] = "Unknown";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        char fullPath[MAX_PATH] = {0};
        if (GetModuleFileNameExA(hProcess, NULL, fullPath, sizeof(fullPath))) {
            char* slash = strrchr(fullPath, '\\');
            if (slash) snprintf(procName, sizeof(procName), "%s", slash + 1);
            else snprintf(procName, sizeof(procName), "%s", fullPath);
        }
        CloseHandle(hProcess);
    }

    /* 判定是否为 Lineage2M、PURPLE 或标准 960x540 游戏客户区 */
    bool is_game = false;
    if (wcsstr(wTitle, L"Lineage2M") || wcsstr(wTitle, L"PURPLE") || wcsstr(wTitle, L"L2M") ||
        wcsstr(wTitle, L"리니지2M") || wcsstr(wTitle, L"天堂2M")) {
        is_game = true;
    } else if (strstr(procName, "Lineage2M") || strstr(procName, "Purple") || strstr(procName, "L2M")) {
        is_game = true;
    } else if (cw >= 850 && cw <= 1050 && ch >= 480 && ch <= 620) {
        /* 符合 960x540 标准分辨率范围 */
        is_game = true;
    }

    if (is_game) {
        L2MWindowInstance* inst = &ctx->list[ctx->count++];
        inst->hwnd = hwnd;
        inst->pid = (uint32_t)pid;
        snprintf(inst->process_name, sizeof(inst->process_name), "%s", procName);
        WideCharToMultiByte(CP_UTF8, 0, wTitle, -1, inst->window_title, sizeof(inst->window_title), NULL, NULL);
        inst->client_width = cw;
        inst->client_height = ch;
        inst->is_minimized = IsIconic(hwnd);
        inst->is_active = (GetForegroundWindow() == hwnd);
    }

    return TRUE;
}
#endif

bool l2m_enum_game_windows(
    L2MWindowInstance* out_windows,
    int32_t max_count,
    int32_t* out_count
) {
    if (!out_windows || max_count <= 0 || !out_count) return false;
    *out_count = 0;

#ifdef _WIN32
    EnumWinContext ctx = { out_windows, max_count, 0 };
    EnumWindows(EnumWindowsCallback, (LPARAM)&ctx);
    *out_count = ctx.count;
    return true;
#else
    return false;
#endif
}

void l2m_window_profiles_init_default(L2MWindowProfileList* out_list) {
    if (!out_list) return;
    memset(out_list, 0, sizeof(L2MWindowProfileList));

    /* 1. 主号默认配置 (EN / 美欧服) */
    L2MWindowProfile* p1 = &out_list->profiles[out_list->count++];
    strncpy(p1->profile_id, "profile_01", sizeof(p1->profile_id) - 1);
    strncpy(p1->profile_name, "主号 (EN / Andyusa)", sizeof(p1->profile_name) - 1);
    strncpy(p1->character_name, "Andyusa", sizeof(p1->character_name) - 1);
    strncpy(p1->region, "EN", sizeof(p1->region) - 1);
    p1->match_rule = L2M_WIN_MATCH_INDEX;
    p1->match_window_index = 0;
    p1->auto_detect_region = true;
    p1->enabled = true;
    strncpy(p1->notes, "第1个游戏窗口，加载美欧服英文配置", sizeof(p1->notes) - 1);

    /* 2. 二号默认配置 (CN / 国服简中) */
    L2MWindowProfile* p2 = &out_list->profiles[out_list->count++];
    strncpy(p2->profile_id, "profile_02", sizeof(p2->profile_id) - 1);
    strncpy(p2->profile_name, "二号 (CN / 中文角色)", sizeof(p2->profile_name) - 1);
    strncpy(p2->character_name, "狂风舞者", sizeof(p2->character_name) - 1);
    strncpy(p2->region, "CN", sizeof(p2->region) - 1);
    p2->match_rule = L2M_WIN_MATCH_INDEX;
    p2->match_window_index = 1;
    p2->auto_detect_region = false;
    p2->enabled = true;
    strncpy(p2->notes, "第2个游戏窗口，加载国服简中配置", sizeof(p2->notes) - 1);

    /* 3. 三号默认配置 (JP / 日服小号) */
    L2MWindowProfile* p3 = &out_list->profiles[out_list->count++];
    strncpy(p3->profile_id, "profile_03", sizeof(p3->profile_id) - 1);
    strncpy(p3->profile_name, "三号 (JP / 日服小号)", sizeof(p3->profile_name) - 1);
    strncpy(p3->character_name, "MyAndy", sizeof(p3->character_name) - 1);
    strncpy(p3->region, "JP", sizeof(p3->region) - 1);
    p3->match_rule = L2M_WIN_MATCH_INDEX;
    p3->match_window_index = 2;
    p3->auto_detect_region = false;
    p3->enabled = true;
    strncpy(p3->notes, "第3个游戏窗口，加载日服配置", sizeof(p3->notes) - 1);
}

/* 简单的 JSON 字段提取工具 */
static bool json_extract_str(const char* src, const char* key, char* dst, size_t max_len) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char* pos = strstr(src, pattern);
    if (!pos) return false;
    char* colon = strchr(pos, ':');
    if (!colon) return false;
    char* q1 = strchr(colon, '"');
    if (!q1) return false;
    char* q2 = strchr(q1 + 1, '"');
    if (!q2) return false;
    size_t len = q2 - (q1 + 1);
    if (len >= max_len) len = max_len - 1;
    strncpy(dst, q1 + 1, len);
    dst[len] = '\0';
    return true;
}

static bool json_extract_int(const char* src, const char* key, int32_t* out_val) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char* pos = strstr(src, pattern);
    if (!pos) return false;
    char* colon = strchr(pos, ':');
    if (!colon) return false;
    *out_val = atoi(colon + 1);
    return true;
}

static bool json_extract_bool(const char* src, const char* key, bool* out_val) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char* pos = strstr(src, pattern);
    if (!pos) return false;
    char* colon = strchr(pos, ':');
    if (!colon) return false;
    while (*colon && (*colon == ':' || *colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n')) colon++;
    if (strncmp(colon, "true", 4) == 0) { *out_val = true; return true; }
    if (strncmp(colon, "false", 5) == 0) { *out_val = false; return true; }
    return false;
}

bool l2m_window_profiles_load(const char* file_path, L2MWindowProfileList* out_list) {
    if (!out_list) return false;
    l2m_window_profiles_init_default(out_list);

    char full_path[MAX_PATH] = {0};
    get_window_profile_path(file_path, full_path, sizeof(full_path));

    FILE* fp = fopen(full_path, "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 10 || size > 512 * 1024) {
        fclose(fp);
        return false;
    }

    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        return false;
    }

    size_t read_bytes = fread(buffer, 1, size, fp);
    buffer[read_bytes] = '\0';
    fclose(fp);

    char* arr_start = strstr(buffer, "\"window_profiles\"");
    if (arr_start) {
        char* lbracket = strchr(arr_start, '[');
        if (lbracket) {
            out_list->count = 0;
            char* p = lbracket + 1;
            while (*p && out_list->count < MAX_WINDOW_PROFILES) {
                char* obj_start = strchr(p, '{');
                if (!obj_start) break;
                char* obj_end = strchr(obj_start, '}');
                if (!obj_end) break;

                char obj_content[1024] = {0};
                size_t obj_len = obj_end - obj_start + 1;
                if (obj_len >= sizeof(obj_content)) obj_len = sizeof(obj_content) - 1;
                strncpy(obj_content, obj_start, obj_len);
                obj_content[obj_len] = '\0';

                L2MWindowProfile prof;
                memset(&prof, 0, sizeof(prof));
                prof.enabled = true;
                prof.auto_detect_region = true;

                json_extract_str(obj_content, "profile_id", prof.profile_id, sizeof(prof.profile_id));
                json_extract_str(obj_content, "profile_name", prof.profile_name, sizeof(prof.profile_name));
                json_extract_str(obj_content, "character_name", prof.character_name, sizeof(prof.character_name));
                json_extract_str(obj_content, "region", prof.region, sizeof(prof.region));
                json_extract_str(obj_content, "custom_cbt_path", prof.custom_cbt_path, sizeof(prof.custom_cbt_path));
                json_extract_str(obj_content, "match_title_keyword", prof.match_title_keyword, sizeof(prof.match_title_keyword));
                json_extract_int(obj_content, "match_window_index", &prof.match_window_index);
                json_extract_bool(obj_content, "auto_detect_region", &prof.auto_detect_region);
                json_extract_bool(obj_content, "enabled", &prof.enabled);
                json_extract_str(obj_content, "notes", prof.notes, sizeof(prof.notes));

                char rule_str[32] = {0};
                if (json_extract_str(obj_content, "match_rule", rule_str, sizeof(rule_str))) {
                    if (strcmp(rule_str, "TITLE_KEYWORD") == 0) prof.match_rule = L2M_WIN_MATCH_TITLE_KEYWORD;
                    else if (strcmp(rule_str, "HWND") == 0) prof.match_rule = L2M_WIN_MATCH_HWND;
                    else if (strcmp(rule_str, "AUTO") == 0) prof.match_rule = L2M_WIN_MATCH_AUTO;
                    else prof.match_rule = L2M_WIN_MATCH_INDEX;
                }

                if (prof.profile_id[0] != '\0') {
                    out_list->profiles[out_list->count++] = prof;
                }

                p = obj_end + 1;
            }
        }
    }

    free(buffer);
    return (out_list->count > 0);
}

bool l2m_window_profiles_save(const char* file_path, const L2MWindowProfileList* list) {
    if (!list) return false;
    char full_path[MAX_PATH] = {0};
    get_window_profile_path(file_path, full_path, sizeof(full_path));
    ensure_parent_dir_exists_local(full_path);

    FILE* fp = fopen(full_path, "wb");
    if (!fp) return false;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"window_profiles\": [\n");

    for (int i = 0; i < list->count; i++) {
        const L2MWindowProfile* p = &list->profiles[i];
        const char* rule_str = (p->match_rule == L2M_WIN_MATCH_TITLE_KEYWORD) ? "TITLE_KEYWORD" :
                               ((p->match_rule == L2M_WIN_MATCH_HWND) ? "HWND" :
                               ((p->match_rule == L2M_WIN_MATCH_AUTO) ? "AUTO" : "INDEX"));

        fprintf(fp, "    {\n");
        fprintf(fp, "      \"profile_id\": \"%s\",\n", p->profile_id);
        fprintf(fp, "      \"profile_name\": \"%s\",\n", p->profile_name);
        fprintf(fp, "      \"character_name\": \"%s\",\n", p->character_name);
        fprintf(fp, "      \"region\": \"%s\",\n", p->region);
        fprintf(fp, "      \"match_rule\": \"%s\",\n", rule_str);
        fprintf(fp, "      \"match_window_index\": %d,\n", p->match_window_index);
        fprintf(fp, "      \"match_title_keyword\": \"%s\",\n", p->match_title_keyword);
        fprintf(fp, "      \"auto_detect_region\": %s,\n", p->auto_detect_region ? "true" : "false");
        fprintf(fp, "      \"enabled\": %s,\n", p->enabled ? "true" : "false");
        fprintf(fp, "      \"notes\": \"%s\"\n", p->notes);
        fprintf(fp, "    }%s\n", (i == list->count - 1) ? "" : ",");
    }

    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return true;
}

bool l2m_window_profile_match(
    const L2MWindowProfileList* list,
    const L2MWindowInstance* win_inst,
    int32_t window_index,
    L2MWindowProfile* out_profile
) {
    if (!list || !out_profile) return false;

    /* 1. 优先尝试按窗口标题关键字匹配 */
    if (win_inst && win_inst->window_title[0]) {
        for (int i = 0; i < list->count; i++) {
            const L2MWindowProfile* p = &list->profiles[i];
            if (p->enabled && p->match_rule == L2M_WIN_MATCH_TITLE_KEYWORD && p->match_title_keyword[0]) {
                if (strstr(win_inst->window_title, p->match_title_keyword)) {
                    *out_profile = *p;
                    return true;
                }
            }
        }
    }

    /* 2. 尝试按多开窗口序号匹配 (第1个窗口对应 index 0, 第2个对应 index 1) */
    for (int i = 0; i < list->count; i++) {
        const L2MWindowProfile* p = &list->profiles[i];
        if (p->enabled && p->match_rule == L2M_WIN_MATCH_INDEX && p->match_window_index == window_index) {
            *out_profile = *p;
            return true;
        }
    }

    /* 3. 兜底回退：取第1个启用的配置 */
    for (int i = 0; i < list->count; i++) {
        if (list->profiles[i].enabled) {
            *out_profile = list->profiles[i];
            return true;
        }
    }

    return false;
}

bool l2m_save_window_profile_by_title(
    const char* window_title,
    const char* character_name,
    const char* region,
    const char* notes
) {
    if (!window_title || window_title[0] == '\0') return false;

    L2MWindowProfileList list;
    l2m_window_profiles_load(NULL, &list);

    /* 查找是否已有匹配项 (按 title_keyword 或 profile_name) */
    int found_idx = -1;
    for (int i = 0; i < list.count; i++) {
        if ((list.profiles[i].match_title_keyword[0] != '\0' && strcmp(list.profiles[i].match_title_keyword, window_title) == 0) ||
            strcmp(list.profiles[i].profile_name, window_title) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx >= 0) {
        /* 更新已有配置 */
        L2MWindowProfile* p = &list.profiles[found_idx];
        if (character_name && character_name[0]) {
            snprintf(p->character_name, sizeof(p->character_name), "%s", character_name);
        }
        if (region && region[0]) {
            snprintf(p->region, sizeof(p->region), "%s", region);
        }
        if (notes) {
            snprintf(p->notes, sizeof(p->notes), "%s", notes);
        }
    } else {
        /* 新增配置项 */
        if (list.count < MAX_WINDOW_PROFILES) {
            L2MWindowProfile* p = &list.profiles[list.count++];
            memset(p, 0, sizeof(L2MWindowProfile));
            snprintf(p->profile_id, sizeof(p->profile_id), "prof_%d", list.count);
            snprintf(p->profile_name, sizeof(p->profile_name), "%s", window_title);
            snprintf(p->match_title_keyword, sizeof(p->match_title_keyword), "%s", window_title);
            p->match_rule = L2M_WIN_MATCH_TITLE_KEYWORD;
            p->enabled = true;
            p->auto_detect_region = false;

            if (character_name && character_name[0]) {
                snprintf(p->character_name, sizeof(p->character_name), "%s", character_name);
            } else {
                snprintf(p->character_name, sizeof(p->character_name), "自定义角色");
            }

            if (region && region[0]) {
                snprintf(p->region, sizeof(p->region), "%s", region);
            } else {
                snprintf(p->region, sizeof(p->region), "CN");
            }

            if (notes) {
                snprintf(p->notes, sizeof(p->notes), "%s", notes);
            }
        }
    }

    return l2m_window_profiles_save(NULL, &list);
}

bool l2m_load_window_profile_by_title(
    const char* window_title,
    L2MWindowProfile* out_prof
) {
    if (!window_title || !out_prof) return false;
    L2MWindowProfileList list;
    if (!l2m_window_profiles_load(NULL, &list)) return false;

    for (int i = 0; i < list.count; i++) {
        if ((list.profiles[i].match_title_keyword[0] != '\0' && strcmp(list.profiles[i].match_title_keyword, window_title) == 0) ||
            (list.profiles[i].match_title_keyword[0] != '\0' && strstr(window_title, list.profiles[i].match_title_keyword) != NULL) ||
            strcmp(list.profiles[i].profile_name, window_title) == 0) {
            *out_prof = list.profiles[i];
            return true;
        }
    }
    return false;
}

bool l2m_align_game_windows(
    L2MWindowAlignMode mode,
    int32_t target_client_w,
    int32_t target_client_h,
    int32_t* out_aligned_count
) {
    if (out_aligned_count) *out_aligned_count = 0;

#ifdef _WIN32
    L2MWindowInstance win_list[MAX_GAME_WINDOWS];
    int32_t win_count = 0;
    if (!l2m_enum_game_windows(win_list, MAX_GAME_WINDOWS, &win_count) || win_count <= 0) {
        return false;
    }

    int cl_w = (target_client_w > 100) ? target_client_w : 960;
    int cl_h = (target_client_h > 100) ? target_client_h : 540;

    /* 获取当前屏幕可用工作区 (避开 Windows 任务栏) */
    RECT rcWork;
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0)) {
        rcWork.left = 0;
        rcWork.top = 0;
        rcWork.right = GetSystemMetrics(SM_CXSCREEN);
        rcWork.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    int aligned_cnt = 0;

    for (int i = 0; i < win_count; i++) {
        HWND hwnd = win_list[i].hwnd;
        if (!hwnd || !IsWindow(hwnd)) continue;

        /* 若窗口最小化，先恢复为常规显示 */
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }

        /* 精确计算包含标题栏和边框的完整外框尺寸 */
        DWORD dwStyle = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
        DWORD dwExStyle = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        RECT rcCalc = { 0, 0, cl_w, cl_h };
        AdjustWindowRectEx(&rcCalc, dwStyle, FALSE, dwExStyle);
        int frame_w = rcCalc.right - rcCalc.left;
        int frame_h = rcCalc.bottom - rcCalc.top;

        int target_x = rcWork.left;
        int target_y = rcWork.top;

        if (mode == L2M_ALIGN_GRID_2X2) {
            /* 2x2 四宫格对齐排列 */
            int row = i / 2;
            int col = i % 2;
            target_x = rcWork.left + col * frame_w;
            target_y = rcWork.top + row * frame_h;
        } else if (mode == L2M_ALIGN_HORIZONTAL) {
            /* 水平横向排列 */
            target_x = rcWork.left + i * frame_w;
            target_y = rcWork.top;
        } else if (mode == L2M_ALIGN_VERTICAL) {
            /* 垂直纵向排列 */
            target_x = rcWork.left;
            target_y = rcWork.top + i * frame_h;
        }

        /* 移动并调整窗口尺寸 */
        SetWindowPos(hwnd, HWND_TOP, target_x, target_y, frame_w, frame_h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        aligned_cnt++;
    }

    if (out_aligned_count) *out_aligned_count = aligned_cnt;
    return (aligned_cnt > 0);
#else
    (void)mode; (void)target_client_w; (void)target_client_h;
    return false;
#endif
}

/* 探测 data/id/<id_name>.json 文件的物理绝对路径 */
static bool get_id_profile_path(const char* id_name, char* out_path, size_t max_len) {
    if (!id_name || id_name[0] == '\0' || !out_path || max_len == 0) return false;

#ifdef _WIN32
    char exe_dir[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir))) {
        char* last_slash = strrchr(exe_dir, '\\');
        if (!last_slash) last_slash = strrchr(exe_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            char candidate[MAX_PATH];

            snprintf(candidate, sizeof(candidate), "%s/data/id/%s.json", exe_dir, id_name);
            FILE* fp = fopen(candidate, "rb");
            if (fp) { fclose(fp); snprintf(out_path, max_len, "%s", candidate); return true; }

            snprintf(candidate, sizeof(candidate), "%s/../data/id/%s.json", exe_dir, id_name);
            fp = fopen(candidate, "rb");
            if (fp) { fclose(fp); snprintf(out_path, max_len, "%s", candidate); return true; }
        }
    }
#endif

    const char* candidates[] = {
        "data/id",
        "../data/id",
        "bot/data/id"
    };
    int num = (int)(sizeof(candidates) / sizeof(candidates[0]));
    for (int i = 0; i < num; i++) {
        char candidate[MAX_PATH];
        snprintf(candidate, sizeof(candidate), "%s/%s.json", candidates[i], id_name);
        FILE* fp = fopen(candidate, "rb");
        if (fp) {
            fclose(fp);
            snprintf(out_path, max_len, "%s", candidate);
            return true;
        }
    }

#ifdef _WIN32
    if (exe_dir[0]) {
        snprintf(out_path, max_len, "%s/../data/id/%s.json", exe_dir, id_name);
        return true;
    }
#endif
    snprintf(out_path, max_len, "data/id/%s.json", id_name);
    return true;
}

bool l2m_id_profile_load(const char* id_name, L2MIdConfig* out_cfg) {
    if (!id_name || id_name[0] == '\0' || !out_cfg) return false;
    memset(out_cfg, 0, sizeof(L2MIdConfig));
    snprintf(out_cfg->id_name, sizeof(out_cfg->id_name), "%s", id_name);
    snprintf(out_cfg->region, sizeof(out_cfg->region), "EN"); /* 默认 EN */

    char path[MAX_PATH] = {0};
    get_id_profile_path(id_name, path, sizeof(path));
    snprintf(out_cfg->file_path, sizeof(out_cfg->file_path), "%s", path);

    FILE* fp = fopen(path, "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 2 || size > 256 * 1024) {
        fclose(fp);
        return false;
    }

    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        return false;
    }

    size_t read_bytes = fread(buffer, 1, size, fp);
    buffer[read_bytes] = '\0';
    fclose(fp);

    /* 提取 REGION 字段 */
    char reg_buf[16] = {0};
    if (json_extract_str(buffer, "REGION", reg_buf, sizeof(reg_buf)) ||
        json_extract_str(buffer, "region", reg_buf, sizeof(reg_buf))) {
        snprintf(out_cfg->region, sizeof(out_cfg->region), "%s", reg_buf);
    }

    /* 提取常用配置 */
    json_extract_bool(buffer, "PEACE_MODE", &out_cfg->peace_mode);
    json_extract_bool(buffer, "PVP_EVADE", &out_cfg->pvp_evade);
    json_extract_bool(buffer, "PVP_ANSWER", &out_cfg->pvp_answer);
    json_extract_bool(buffer, "LOW_HP_DODGE", &out_cfg->low_hp_dodge);
    json_extract_bool(buffer, "BUY_LOOT_TOWN", &out_cfg->buy_loot_town);
    json_extract_bool(buffer, "BUY_LOOT_RIP", &out_cfg->buy_loot_rip);
    json_extract_bool(buffer, "HP_BANK_CHECKER", &out_cfg->hp_bank_checker);
    json_extract_bool(buffer, "DEATH_CHECKER", &out_cfg->death_checker);
    json_extract_bool(buffer, "OVERWEIGHT_CHECKER", &out_cfg->overweight_checker);
    json_extract_int(buffer, "OVERWEIGHT_AFK", &out_cfg->overweight_afk);
    json_extract_bool(buffer, "AUTOHUNT_BEFORE_TP", &out_cfg->autohunt_before_tp);

    free(buffer);
    return true;
}

bool l2m_id_profile_save(const char* id_name, const L2MIdConfig* in_cfg) {
    if (!id_name || id_name[0] == '\0' || !in_cfg) return false;

    char path[MAX_PATH] = {0};
    get_id_profile_path(id_name, path, sizeof(path));
    ensure_parent_dir_exists_local(path);

    /* 尝试读取原文件内容，若存在则精准替换/更新 REGION 字段，保持原 JSON 其他字段完整无损 */
    FILE* fp_in = fopen(path, "rb");
    if (fp_in) {
        fseek(fp_in, 0, SEEK_END);
        long size = ftell(fp_in);
        fseek(fp_in, 0, SEEK_SET);

        if (size > 0 && size < 512 * 1024) {
            char* buffer = (char*)malloc(size + 1);
            if (buffer) {
                size_t read_bytes = fread(buffer, 1, size, fp_in);
                buffer[read_bytes] = '\0';
                fclose(fp_in);

                /* 查找 "REGION": "XX" 并替换 */
                char* reg_pos = strstr(buffer, "\"REGION\"");
                if (!reg_pos) reg_pos = strstr(buffer, "\"region\"");

                if (reg_pos) {
                    char* colon = strchr(reg_pos, ':');
                    char* q1 = colon ? strchr(colon, '"') : NULL;
                    char* q2 = q1 ? strchr(q1 + 1, '"') : NULL;

                    if (q1 && q2) {
                        FILE* fp_out = fopen(path, "wb");
                        if (fp_out) {
                            fwrite(buffer, 1, q1 - buffer + 1, fp_out);
                            fprintf(fp_out, "%s", in_cfg->region);
                            fwrite(q2, 1, buffer + read_bytes - q2, fp_out);
                            fclose(fp_out);
                            free(buffer);
                            return true;
                        }
                    }
                }
                free(buffer);
            }
        } else {
            fclose(fp_in);
        }
    }

    /* 若原文件不存在，生成完整的标准 data/id/<id_name>.json 模板 */
    FILE* fp = fopen(path, "wb");
    if (!fp) return false;

    const char* reg_str = (in_cfg->region[0]) ? in_cfg->region : "EN";

    fprintf(fp, "{\n");
    fprintf(fp, "  \"REGION\": \"%s\",\n", reg_str);
    fprintf(fp, "  \"PEACE_MODE\": %s,\n", in_cfg->peace_mode ? "true" : "false");
    fprintf(fp, "  \"PVP_EVADE\": %s,\n", in_cfg->pvp_evade ? "true" : "false");
    fprintf(fp, "  \"PVP_ANSWER\": %s,\n", in_cfg->pvp_answer ? "true" : "false");
    fprintf(fp, "  \"LOW_HP_DODGE\": %s,\n", in_cfg->low_hp_dodge ? "true" : "false");
    fprintf(fp, "  \"HEALTH_BACK\": [\n    20,\n    30,\n    40\n  ],\n");
    fprintf(fp, "  \"BUY_LOOT_TOWN\": true,\n");
    fprintf(fp, "  \"BUY_LOOT_RIP\": true,\n");
    fprintf(fp, "  \"HP_BANK_CHECKER\": true,\n");
    fprintf(fp, "  \"SOSKA_CHECKER\": true,\n");
    fprintf(fp, "  \"DEATH_CHECKER\": true,\n");
    fprintf(fp, "  \"OVERWEIGHT_CHECKER\": true,\n");
    fprintf(fp, "  \"OVERWEIGHT_AFK\": %d,\n", in_cfg->overweight_afk > 0 ? in_cfg->overweight_afk : 80);
    fprintf(fp, "  \"SCHEDULE_BUYING\": \"10:30|13:30|20:20\",\n");
    fprintf(fp, "  \"SCHEDULE_MAIL\": \"10:00|15:00|20:00|05:00\",\n");
    fprintf(fp, "  \"SCHEDULE_REWARDS\": \"21:00\",\n");
    fprintf(fp, "  \"AUTOHUNT_BEFORE_TP\": %s\n", in_cfg->autohunt_before_tp ? "true" : "false");
    fprintf(fp, "}\n");

    fclose(fp);
    return true;
}

bool l2m_id_profile_set_region(const char* id_name, const char* region) {
    if (!id_name || id_name[0] == '\0' || !region || region[0] == '\0') return false;
    L2MIdConfig cfg;
    if (!l2m_id_profile_load(id_name, &cfg)) {
        memset(&cfg, 0, sizeof(cfg));
        snprintf(cfg.id_name, sizeof(cfg.id_name), "%s", id_name);
        cfg.peace_mode = true;
        cfg.pvp_evade = true;
        cfg.low_hp_dodge = true;
        cfg.overweight_afk = 80;
        cfg.autohunt_before_tp = true;
    }
    snprintf(cfg.region, sizeof(cfg.region), "%s", region);
    return l2m_id_profile_save(id_name, &cfg);
}

bool l2m_id_profile_get_region(const char* id_name, char* out_region, size_t max_len) {
    if (!id_name || !out_region || max_len == 0) return false;
    L2MIdConfig cfg;
    if (l2m_id_profile_load(id_name, &cfg)) {
        snprintf(out_region, max_len, "%s", cfg.region);
        return true;
    }
    return false;
}

bool l2m_enum_id_profiles(char out_ids[][64], int32_t max_count, int32_t* out_count) {
    if (!out_ids || max_count <= 0 || !out_count) return false;
    *out_count = 0;

#ifdef _WIN32
    char patterns[5][MAX_PATH] = {0};
    int pattern_count = 0;

    char exe_dir[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir))) {
        char* last_slash = strrchr(exe_dir, '\\');
        if (!last_slash) last_slash = strrchr(exe_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(patterns[pattern_count++], MAX_PATH, "%s/data/id/*.json", exe_dir);
            snprintf(patterns[pattern_count++], MAX_PATH, "%s/../data/id/*.json", exe_dir);
        }
    }
    snprintf(patterns[pattern_count++], MAX_PATH, "data/id/*.json");
    snprintf(patterns[pattern_count++], MAX_PATH, "../data/id/*.json");
    snprintf(patterns[pattern_count++], MAX_PATH, "bot/data/id/*.json");

    for (int p = 0; p < pattern_count; p++) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(patterns[p], &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    char id_buf[MAX_PATH] = {0};
                    snprintf(id_buf, sizeof(id_buf), "%s", fd.cFileName);
                    char* dot = strrchr(id_buf, '.');
                    if (dot) *dot = '\0';

                    if (id_buf[0] != '\0' && *out_count < max_count) {
                        snprintf(out_ids[*out_count], 64, "%.63s", id_buf);
                        (*out_count)++;
                    }
                }
            } while (FindNextFileA(hFind, &fd) && *out_count < max_count);
            FindClose(hFind);
            if (*out_count > 0) return true;
        }
    }
#endif
    return (*out_count > 0);
}
