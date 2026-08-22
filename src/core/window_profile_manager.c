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

/* 辅助：跨平台与 Windows 宽字符 UTF-8 兼容的安全文件打开函数 */
static FILE* file_open_utf8(const char* utf8_path, const char* mode) {
    if (!utf8_path || !mode) return NULL;
#ifdef _WIN32
    wchar_t wpath[MAX_PATH] = {0};
    wchar_t wmode[32] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, wpath, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 32);
    return _wfopen(wpath, wmode);
#else
    return fopen(utf8_path, mode);
#endif
}

/* 辅助：确保父目录存在 (支持 UTF-8 中文路径与 Windows 宽字符) */
static void ensure_parent_dir_exists_local(const char* filepath) {
    if (!filepath) return;
#ifdef _WIN32
    wchar_t wtemp[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wtemp, MAX_PATH);
    for (wchar_t* p = wtemp; *p; p++) {
        if (*p == L'/' || *p == L'\\') {
            wchar_t old = *p;
            *p = L'\0';
            if (wcslen(wtemp) > 0 && !(wcslen(wtemp) == 2 && wtemp[1] == L':')) {
                _wmkdir(wtemp);
            }
            *p = old;
        }
    }
#else
    char temp[MAX_PATH];
    strncpy(temp, filepath, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    for (char* p = temp; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char old = *p;
            *p = '\0';
            if (strlen(temp) > 0) mkdir(temp, 0755);
            *p = old;
        }
    }
#endif
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
            FILE* fp = file_open_utf8(candidate, "rb");
            if (fp) { fclose(fp); snprintf(out_path, max_len, "%s", candidate); return true; }

            snprintf(candidate, sizeof(candidate), "%s/../data/window_profiles.json", exe_dir);
            fp = file_open_utf8(candidate, "rb");
            if (fp) { fclose(fp); snprintf(out_path, max_len, "%s", candidate); return true; }
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
        FILE* fp = file_open_utf8(candidates[i], "rb");
        if (fp) {
            fclose(fp);
            strncpy(out_path, candidates[i], max_len - 1);
            out_path[max_len - 1] = '\0';
            return true;
        }
    }

#ifdef _WIN32
    if (exe_dir[0]) {
        snprintf(out_path, max_len - 1, "%s/../data/window_profiles.json", exe_dir);
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

static bool str_contains_nocase_w(const wchar_t* haystack, const wchar_t* needle) {
    if (!haystack || !needle || needle[0] == L'\0') return false;
    size_t n_len = wcslen(needle);
    size_t h_len = wcslen(haystack);
    if (h_len < n_len) return false;
    for (size_t i = 0; i <= h_len - n_len; i++) {
        if (_wcsnicmp(&haystack[i], needle, n_len) == 0) return true;
    }
    return false;
}

static bool str_contains_nocase_a(const char* haystack, const char* needle) {
    if (!haystack || !needle || needle[0] == '\0') return false;
    size_t n_len = strlen(needle);
    size_t h_len = strlen(haystack);
    if (h_len < n_len) return false;
    for (size_t i = 0; i <= h_len - n_len; i++) {
        if (_strnicmp(&haystack[i], needle, n_len) == 0) return true;
    }
    return false;
}

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    EnumWinContext* ctx = (EnumWinContext*)lParam;
    if (ctx->count >= ctx->max_count) return FALSE;

    if (!IsWindowVisible(hwnd)) return TRUE;

    /* 1. 过滤子窗口、无宿主/所有者窗口 */
    if (GetWindow(hwnd, GW_OWNER) != NULL) return TRUE;
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    if (style & WS_CHILD) return TRUE;

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int cw = rcClient.right - rcClient.left;
    int ch = rcClient.bottom - rcClient.top;

    if (cw < 300 || ch < 200) return TRUE;

    /* 2. 检查 PID，绝对排除本程序自身进程的所有窗口 */
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) return TRUE;

    /* 获取进程可执行文件名 */
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

    /* 3. 绝对排除进程名包含自身 Bot、IDE、代码编辑器或系统进程 */
    if (str_contains_nocase_a(procName, "Lineage2MBot") ||
        str_contains_nocase_a(procName, "devenv") ||
        str_contains_nocase_a(procName, "Code") ||
        str_contains_nocase_a(procName, "antigravity") ||
        str_contains_nocase_a(procName, "cursor") ||
        str_contains_nocase_a(procName, "sublime") ||
        str_contains_nocase_a(procName, "notepad") ||
        str_contains_nocase_a(procName, "explorer")) {
        return TRUE;
    }

    /* 4. 获取窗口类名与标题 */
    wchar_t wClass[128] = {0};
    GetClassNameW(hwnd, wClass, sizeof(wClass)/sizeof(wchar_t));

    wchar_t wTitle[256] = {0};
    GetWindowTextW(hwnd, wTitle, sizeof(wTitle)/sizeof(wchar_t));
    if (wcslen(wTitle) == 0) return TRUE;

    /* 5. 绝对排除类名黑名单 (自身 UI 类名、Windows 外壳等) */
    if (wcsstr(wClass, L"L2M_") ||
        wcsstr(wClass, L"Shell_TrayWnd") ||
        wcsstr(wClass, L"Progman") ||
        wcsstr(wClass, L"WorkerW") ||
        wcsstr(wClass, L"ApplicationFrameWindow") ||
        wcsstr(wClass, L"Windows.UI.Core.CoreWindow")) {
        return TRUE;
    }

    /* 6. 绝对排除标题黑名单 (软件自身、调试器、工作台、开发工具、浏览器) */
    if (str_contains_nocase_w(wTitle, L"Lineage2MBot") ||
        str_contains_nocase_w(wTitle, L"Debugger") ||
        str_contains_nocase_w(wTitle, L"Dashboard") ||
        str_contains_nocase_w(wTitle, L"调试器") ||
        str_contains_nocase_w(wTitle, L"工作台") ||
        str_contains_nocase_w(wTitle, L"特征采集") ||
        str_contains_nocase_w(wTitle, L"Visual Studio") ||
        str_contains_nocase_w(wTitle, L"Antigravity") ||
        str_contains_nocase_w(wTitle, L"Google Chrome") ||
        str_contains_nocase_w(wTitle, L"Microsoft Edge") ||
        str_contains_nocase_w(wTitle, L"Firefox") ||
        str_contains_nocase_w(wTitle, L"Git") ||
        str_contains_nocase_w(wTitle, L"PowerShell")) {
        return TRUE;
    }

    /* 7. 游戏客户端正向白名单判定 (必须符合以下特征之一) */
    bool is_game = false;

    /* A. 游戏/模拟器进程名直接匹配 */
    if (str_contains_nocase_a(procName, "Lineage2M") ||
        str_contains_nocase_a(procName, "Purple") ||
        str_contains_nocase_a(procName, "L2M") ||
        str_contains_nocase_a(procName, "dnplayer") ||
        str_contains_nocase_a(procName, "Nox") ||
        str_contains_nocase_a(procName, "MuMu") ||
        str_contains_nocase_a(procName, "HD-Player") ||
        str_contains_nocase_a(procName, "MEmu")) {
        is_game = true;
    }
    /* B. 窗口类名符合虚幻引擎或主流模拟器特征 */
    else if (wcsstr(wClass, L"UnrealWindow") ||
             wcsstr(wClass, L"LDPlayerMainFrame") ||
             wcsstr(wClass, L"NoxWindowClass") ||
             wcsstr(wClass, L"MuMuMainFrame") ||
             wcsstr(wClass, L"Qt5QWindowIcon")) {
        is_game = true;
    }
    /* C. 窗口标题符合纯正游戏标题 */
    else if (str_contains_nocase_w(wTitle, L"Lineage2M") ||
             str_contains_nocase_w(wTitle, L"Lineage 2M") ||
             str_contains_nocase_w(wTitle, L"PURPLE") ||
             wcsstr(wTitle, L"리니지2M") ||
             wcsstr(wTitle, L"天堂2M")) {
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

    /* 1. 主号默认配置 (CN / 国服简中) */
    L2MWindowProfile* p1 = &out_list->profiles[out_list->count++];
    strncpy(p1->profile_id, "profile_01", sizeof(p1->profile_id) - 1);
    strncpy(p1->profile_name, "主号 (CN / Andyusa)", sizeof(p1->profile_name) - 1);
    strncpy(p1->character_name, "Andyusa", sizeof(p1->character_name) - 1);
    strncpy(p1->region, "CN", sizeof(p1->region) - 1);
    p1->match_rule = L2M_WIN_MATCH_INDEX;
    p1->match_window_index = 0;
    p1->auto_detect_region = false;
    p1->auto_dismiss_popup = true;
    p1->low_hp_threshold = 30;
    p1->recover_hp_threshold = 80;
    p1->enabled = true;
    strncpy(p1->notes, "第1个游戏窗口，默认加载国服简中配置", sizeof(p1->notes) - 1);

    /* 2. 二号默认配置 (CN / 国服简中) */
    L2MWindowProfile* p2 = &out_list->profiles[out_list->count++];
    strncpy(p2->profile_id, "profile_02", sizeof(p2->profile_id) - 1);
    strncpy(p2->profile_name, "二号 (CN / 中文角色)", sizeof(p2->profile_name) - 1);
    strncpy(p2->character_name, "狂风舞者", sizeof(p2->character_name) - 1);
    strncpy(p2->region, "CN", sizeof(p2->region) - 1);
    p2->match_rule = L2M_WIN_MATCH_INDEX;
    p2->match_window_index = 1;
    p2->auto_detect_region = false;
    p2->auto_dismiss_popup = true;
    p2->low_hp_threshold = 30;
    p2->recover_hp_threshold = 80;
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
    p3->auto_dismiss_popup = true;
    p3->low_hp_threshold = 30;
    p3->recover_hp_threshold = 80;
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

    FILE* fp = file_open_utf8(full_path, "rb");
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
                prof.auto_dismiss_popup = true;
                prof.low_hp_threshold = 30;
                prof.recover_hp_threshold = 80;
                prof.enabled = true;

                json_extract_str(obj_content, "profile_id", prof.profile_id, sizeof(prof.profile_id));
                json_extract_str(obj_content, "profile_name", prof.profile_name, sizeof(prof.profile_name));
                json_extract_str(obj_content, "character_name", prof.character_name, sizeof(prof.character_name));
                json_extract_str(obj_content, "region", prof.region, sizeof(prof.region));
                json_extract_str(obj_content, "custom_cbt_path", prof.custom_cbt_path, sizeof(prof.custom_cbt_path));
                json_extract_str(obj_content, "match_title_keyword", prof.match_title_keyword, sizeof(prof.match_title_keyword));
                json_extract_int(obj_content, "match_window_index", &prof.match_window_index);
                json_extract_bool(obj_content, "auto_detect_region", &prof.auto_detect_region);
                json_extract_bool(obj_content, "auto_dismiss_popup", &prof.auto_dismiss_popup);
                json_extract_int(obj_content, "low_hp_threshold", &prof.low_hp_threshold);
                json_extract_int(obj_content, "recover_hp_threshold", &prof.recover_hp_threshold);
                json_extract_bool(obj_content, "enabled", &prof.enabled);
                json_extract_str(obj_content, "notes", prof.notes, sizeof(prof.notes));

                if (prof.low_hp_threshold <= 0 || prof.low_hp_threshold > 95) prof.low_hp_threshold = 30;
                if (prof.recover_hp_threshold <= 0 || prof.recover_hp_threshold > 100) prof.recover_hp_threshold = 80;

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

    FILE* fp = file_open_utf8(full_path, "wb");
    if (!fp) return false;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"window_profiles\": [\n");

    for (int i = 0; i < list->count; i++) {
        const L2MWindowProfile* p = &list->profiles[i];
        const char* rule_str = (p->match_rule == L2M_WIN_MATCH_TITLE_KEYWORD) ? "TITLE_KEYWORD" :
                               ((p->match_rule == L2M_WIN_MATCH_HWND) ? "HWND" :
                               ((p->match_rule == L2M_WIN_MATCH_AUTO) ? "AUTO" : "INDEX"));

        int low_hp = (p->low_hp_threshold > 0) ? p->low_hp_threshold : 30;
        int rec_hp = (p->recover_hp_threshold > 0) ? p->recover_hp_threshold : 80;

        fprintf(fp, "    {\n");
        fprintf(fp, "      \"profile_id\": \"%s\",\n", p->profile_id);
        fprintf(fp, "      \"profile_name\": \"%s\",\n", p->profile_name);
        fprintf(fp, "      \"character_name\": \"%s\",\n", p->character_name);
        fprintf(fp, "      \"region\": \"%s\",\n", p->region);
        fprintf(fp, "      \"match_rule\": \"%s\",\n", rule_str);
        fprintf(fp, "      \"match_window_index\": %d,\n", p->match_window_index);
        fprintf(fp, "      \"match_title_keyword\": \"%s\",\n", p->match_title_keyword);
        fprintf(fp, "      \"auto_detect_region\": %s,\n", p->auto_detect_region ? "true" : "false");
        fprintf(fp, "      \"auto_dismiss_popup\": %s,\n", p->auto_dismiss_popup ? "true" : "false");
        fprintf(fp, "      \"low_hp_threshold\": %d,\n", low_hp);
        fprintf(fp, "      \"recover_hp_threshold\": %d,\n", rec_hp);
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

    /* 1. 优先尝试按窗口标题关键字匹配 (TITLE_KEYWORD) */
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

        /* 1.1 尝试按 character_name 或 profile_name 匹配窗口标题 */
        for (int i = 0; i < list->count; i++) {
            const L2MWindowProfile* p = &list->profiles[i];
            if (p->enabled) {
                if (p->character_name[0] && strstr(win_inst->window_title, p->character_name)) {
                    *out_profile = *p;
                    return true;
                }
                if (p->profile_name[0] && strstr(win_inst->window_title, p->profile_name)) {
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

#ifdef _WIN32
typedef struct {
    L2MMonitorList* list;
    int index;
} MonitorEnumContext;

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hdcMon, LPRECT lprcMon, LPARAM dwData) {
    (void)hdcMon; (void)lprcMon;
    MonitorEnumContext* ctx = (MonitorEnumContext*)dwData;
    if (!ctx || !ctx->list || ctx->index >= MAX_MONITORS) return TRUE;

    MONITORINFOEXW mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);

    if (GetMonitorInfoW(hMon, (LPMONITORINFO)&mi)) {
        L2MMonitorInfo* info = &ctx->list->monitors[ctx->index];
        memset(info, 0, sizeof(L2MMonitorInfo));
        info->monitor_index = ctx->index;
        info->is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        if (info->is_primary) {
            ctx->list->primary_index = ctx->index;
        }

        info->x = mi.rcMonitor.left;
        info->y = mi.rcMonitor.top;
        info->width = mi.rcMonitor.right - mi.rcMonitor.left;
        info->height = mi.rcMonitor.bottom - mi.rcMonitor.top;

        info->work_x = mi.rcWork.left;
        info->work_y = mi.rcWork.top;
        info->work_width = mi.rcWork.right - mi.rcWork.left;
        info->work_height = mi.rcWork.bottom - mi.rcWork.top;

        WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, info->name, sizeof(info->name) - 1, NULL, NULL);

        snprintf(info->desc, sizeof(info->desc), "🖥️ 显示器 %d %s(%dx%d)",
                 ctx->index + 1,
                 info->is_primary ? "[主屏幕] " : "",
                 info->width, info->height);

        ctx->index++;
        ctx->list->count = ctx->index;
    }
    return TRUE;
}
#endif

bool l2m_enum_monitors(L2MMonitorList* out_list) {
    if (!out_list) return false;
    memset(out_list, 0, sizeof(L2MMonitorList));

#ifdef _WIN32
    MonitorEnumContext ctx;
    ctx.list = out_list;
    ctx.index = 0;

    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&ctx);

    /* 兜底保障：若枚举未返回，使用主屏幕系统参数 */
    if (out_list->count == 0) {
        L2MMonitorInfo* info = &out_list->monitors[0];
        info->monitor_index = 0;
        info->is_primary = true;
        snprintf(info->name, sizeof(info->name), "DISPLAY_DEFAULT");
        snprintf(info->desc, sizeof(info->desc), "🖥️ 显示器 1 [主屏幕] (%dx%d)",
                 GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        info->x = 0;
        info->y = 0;
        info->width = GetSystemMetrics(SM_CXSCREEN);
        info->height = GetSystemMetrics(SM_CYSCREEN);

        RECT rcWork;
        if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0)) {
            info->work_x = rcWork.left;
            info->work_y = rcWork.top;
            info->work_width = rcWork.right - rcWork.left;
            info->work_height = rcWork.bottom - rcWork.top;
        } else {
            info->work_x = 0; info->work_y = 0;
            info->work_width = info->width; info->work_height = info->height;
        }
        out_list->count = 1;
        out_list->primary_index = 0;
    }
    return true;
#else
    return false;
#endif
}

bool l2m_get_monitor_by_index(int32_t monitor_index, L2MMonitorInfo* out_info) {
    if (!out_info) return false;
    L2MMonitorList list;
    if (!l2m_enum_monitors(&list)) return false;

    if (monitor_index >= 0 && monitor_index < list.count) {
        *out_info = list.monitors[monitor_index];
        return true;
    }
    if (list.primary_index >= 0 && list.primary_index < list.count) {
        *out_info = list.monitors[list.primary_index];
        return true;
    }
    if (list.count > 0) {
        *out_info = list.monitors[0];
        return true;
    }
    return false;
}

bool l2m_align_game_windows_ex(
    L2MWindowAlignMode mode,
    int32_t target_client_w,
    int32_t target_client_h,
    int32_t monitor_index,
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

    /* 获取目标物理显示器的可用工作区 (避开对应屏幕任务栏，支持负坐标) */
    L2MMonitorList mon_list;
    l2m_enum_monitors(&mon_list);

    L2MMonitorInfo mon_info;
    if (monitor_index >= 0 && monitor_index < mon_list.count) {
        mon_info = mon_list.monitors[monitor_index];
    } else if (mon_list.primary_index >= 0 && mon_list.primary_index < mon_list.count) {
        mon_info = mon_list.monitors[mon_list.primary_index];
    } else {
        mon_info = mon_list.monitors[0];
    }

    int base_x = mon_info.work_x;
    int base_y = mon_info.work_y;
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

        int target_x = base_x;
        int target_y = base_y;

        if (mode == L2M_ALIGN_GRID_2X2) {
            /* 2x2 四宫格对齐排列 */
            int row = i / 2;
            int col = i % 2;
            target_x = base_x + col * frame_w;
            target_y = base_y + row * frame_h;
        } else if (mode == L2M_ALIGN_HORIZONTAL) {
            /* 水平横向排列 */
            target_x = base_x + i * frame_w;
            target_y = base_y;
        } else if (mode == L2M_ALIGN_VERTICAL) {
            /* 垂直纵向排列 */
            target_x = base_x;
            target_y = base_y + i * frame_h;
        }

        /* 移动并调整窗口尺寸 */
        SetWindowPos(hwnd, HWND_TOP, target_x, target_y, frame_w, frame_h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        aligned_cnt++;
    }

    if (out_aligned_count) *out_aligned_count = aligned_cnt;
    return (aligned_cnt > 0);
#else
    (void)mode; (void)target_client_w; (void)target_client_h; (void)monitor_index; (void)out_aligned_count;
    return false;
#endif
}

bool l2m_align_game_windows(
    L2MWindowAlignMode mode,
    int32_t target_client_w,
    int32_t target_client_h,
    int32_t* out_aligned_count
) {
    return l2m_align_game_windows_ex(mode, target_client_w, target_client_h, -1, out_aligned_count);
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
            FILE* fp = file_open_utf8(candidate, "rb");
            if (fp) { fclose(fp); snprintf(out_path, max_len, "%s", candidate); return true; }

            snprintf(candidate, sizeof(candidate), "%s/../data/id/%s.json", exe_dir, id_name);
            fp = file_open_utf8(candidate, "rb");
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
        FILE* fp = file_open_utf8(candidate, "rb");
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

    FILE* fp = file_open_utf8(path, "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 2 || size > 512 * 1024) {
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

    /* 提取 AUTO_DISMISS_POPUP 字段 (默认 true) */
    out_cfg->auto_dismiss_popup = true;
    if (!json_extract_bool(buffer, "AUTO_DISMISS_POPUP", &out_cfg->auto_dismiss_popup)) {
        json_extract_bool(buffer, "POPUP_CHECKER", &out_cfg->auto_dismiss_popup);
    }

    /* 提取低血量阈值与 HEALTH_BACK 数组 (默认 30) */
    out_cfg->low_hp_threshold = 30;
    out_cfg->health_back[0] = 30;
    out_cfg->health_back[1] = 40;
    out_cfg->health_back[2] = 50;

    char* hb_pos = strstr(buffer, "\"HEALTH_BACK\"");
    if (!hb_pos) hb_pos = strstr(buffer, "\"low_hp_threshold\"");
    if (hb_pos) {
        char* colon = strchr(hb_pos, ':');
        if (colon) {
            char* p = colon + 1;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
            if (*p == '[') {
                int val1 = 0, val2 = 0, val3 = 0;
                if (sscanf(p + 1, "%d, %d, %d", &val1, &val2, &val3) >= 1) {
                    if (val1 > 0) {
                        out_cfg->low_hp_threshold = val1;
                        out_cfg->health_back[0] = val1;
                        out_cfg->health_back[1] = (val2 > 0) ? val2 : val1 + 10;
                        out_cfg->health_back[2] = (val3 > 0) ? val3 : val1 + 20;
                    }
                }
            } else {
                int val = atoi(p);
                if (val > 0) {
                    out_cfg->low_hp_threshold = val;
                    out_cfg->health_back[0] = val;
                }
            }
        }
    }
    if (out_cfg->low_hp_threshold <= 0 || out_cfg->low_hp_threshold > 95) out_cfg->low_hp_threshold = 30;

    /* 提取恢复出战血量阈值 (默认 80) */
    out_cfg->recover_hp_threshold = 80;
    if (!json_extract_int(buffer, "HEALTH_RECOVER", &out_cfg->recover_hp_threshold)) {
        if (!json_extract_int(buffer, "HEALTH_RESTORE", &out_cfg->recover_hp_threshold)) {
            json_extract_int(buffer, "recover_hp_threshold", &out_cfg->recover_hp_threshold);
        }
    }
    if (out_cfg->recover_hp_threshold <= 0 || out_cfg->recover_hp_threshold > 100) out_cfg->recover_hp_threshold = 80;

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

    int low_hp = (in_cfg->low_hp_threshold > 0) ? in_cfg->low_hp_threshold : 30;
    int rec_hp = (in_cfg->recover_hp_threshold > 0) ? in_cfg->recover_hp_threshold : 80;

    /* 尝试读取原文件内容 */
    FILE* fp_in = file_open_utf8(path, "rb");
    if (fp_in) {
        fseek(fp_in, 0, SEEK_END);
        long size = ftell(fp_in);
        fseek(fp_in, 0, SEEK_SET);

        if (size > 0 && size < 512 * 1024) {
            char* original_json = (char*)malloc(size + 1);
            if (original_json) {
                size_t read_bytes = fread(original_json, 1, size, fp_in);
                original_json[read_bytes] = '\0';
                fclose(fp_in);
                fp_in = NULL;

                /* 动态分配新缓冲区，容量为 原文件大小 + 4KB 冗余 */
                size_t new_cap = size + 4096;
                char* new_json = (char*)malloc(new_cap);
                if (new_json) {
                    new_json[0] = '\0';

                    const char* bool_str = in_cfg->auto_dismiss_popup ? "true" : "false";
                    char* pop_pos = strstr(original_json, "\"AUTO_DISMISS_POPUP\"");
                    if (!pop_pos) pop_pos = strstr(original_json, "\"POPUP_CHECKER\"");

                    if (pop_pos) {
                        char* colon = strchr(pop_pos, ':');
                        if (colon) {
                            char* val_start = colon + 1;
                            while (*val_start == ' ' || *val_start == '\t') val_start++;
                            char* comma = strchr(val_start, ',');
                            char* brace = strchr(val_start, '}');
                            char* line_end = strchr(val_start, '\n');
                            char* val_end = comma;
                            if (!val_end || (brace && brace < val_end)) val_end = brace;
                            if (!val_end || (line_end && line_end < val_end)) val_end = line_end;

                            if (val_end) {
                                size_t prefix_len = val_start - original_json;
                                strncpy(new_json, original_json, prefix_len);
                                new_json[prefix_len] = '\0';
                                strcat(new_json, bool_str);
                                strcat(new_json, val_end);
                            }
                        }
                    }

                    /* 如果没找到弹窗字段，在首个 { 后面插入 */
                    if (new_json[0] == '\0') {
                        char* first_brace = strchr(original_json, '{');
                        if (first_brace) {
                            size_t prefix_len = (first_brace + 1) - original_json;
                            strncpy(new_json, original_json, prefix_len);
                            new_json[prefix_len] = '\0';
                            strcat(new_json, "\n  \"AUTO_DISMISS_POPUP\": ");
                            strcat(new_json, bool_str);
                            strcat(new_json, ",");
                            strcat(new_json, first_brace + 1);
                        }
                    }

                    /* 如果指定了有效的 region，更新 REGION 字段 */
                    if (in_cfg->region[0] != '\0' && new_json[0] != '\0') {
                        char* reg_pos = strstr(new_json, "\"REGION\"");
                        if (!reg_pos) reg_pos = strstr(new_json, "\"region\"");
                        if (reg_pos) {
                            char* colon = strchr(reg_pos, ':');
                            char* q1 = colon ? strchr(colon, '"') : NULL;
                            char* q2 = q1 ? strchr(q1 + 1, '"') : NULL;
                            if (q1 && q2) {
                                char* temp_reg = (char*)malloc(new_cap);
                                if (temp_reg) {
                                    size_t pre_len = (q1 + 1) - new_json;
                                    strncpy(temp_reg, new_json, pre_len);
                                    temp_reg[pre_len] = '\0';
                                    strcat(temp_reg, in_cfg->region);
                                    strcat(temp_reg, q2);
                                    strcpy(new_json, temp_reg);
                                    free(temp_reg);
                                }
                            }
                        }
                    }

                    /* 更新或插入 HEALTH_RECOVER 字段 */
                    if (new_json[0] != '\0') {
                        char* rec_pos = strstr(new_json, "\"HEALTH_RECOVER\"");
                        if (!rec_pos) rec_pos = strstr(new_json, "\"HEALTH_RESTORE\"");
                        if (rec_pos) {
                            char* colon = strchr(rec_pos, ':');
                            if (colon) {
                                char* val_start = colon + 1;
                                while (*val_start == ' ' || *val_start == '\t') val_start++;
                                char* comma = strchr(val_start, ',');
                                char* brace = strchr(val_start, '}');
                                char* line_end = strchr(val_start, '\n');
                                char* val_end = comma;
                                if (!val_end || (brace && brace < val_end)) val_end = brace;
                                if (!val_end || (line_end && line_end < val_end)) val_end = line_end;

                                if (val_end) {
                                    char* temp_rec = (char*)malloc(new_cap);
                                    if (temp_rec) {
                                        size_t prefix_len = val_start - new_json;
                                        strncpy(temp_rec, new_json, prefix_len);
                                        temp_rec[prefix_len] = '\0';
                                        char num_str[16];
                                        snprintf(num_str, sizeof(num_str), "%d", rec_hp);
                                        strcat(temp_rec, num_str);
                                        strcat(temp_rec, val_end);
                                        strcpy(new_json, temp_rec);
                                        free(temp_rec);
                                    }
                                }
                            }
                        } else {
                            /* 插入 HEALTH_RECOVER */
                            char* first_brace = strchr(new_json, '{');
                            if (first_brace) {
                                char* temp_rec = (char*)malloc(new_cap);
                                if (temp_rec) {
                                    size_t prefix_len = (first_brace + 1) - new_json;
                                    strncpy(temp_rec, new_json, prefix_len);
                                    temp_rec[prefix_len] = '\0';
                                    char rec_entry[64];
                                    snprintf(rec_entry, sizeof(rec_entry), "\n  \"HEALTH_RECOVER\": %d,", rec_hp);
                                    strcat(temp_rec, rec_entry);
                                    strcat(temp_rec, first_brace + 1);
                                    strcpy(new_json, temp_rec);
                                    free(temp_rec);
                                }
                            }
                        }
                    }

                    /* 更新或插入 HEALTH_BACK 字段 */
                    if (new_json[0] != '\0') {
                        char* hb_pos = strstr(new_json, "\"HEALTH_BACK\"");
                        if (hb_pos) {
                            char* colon = strchr(hb_pos, ':');
                            if (colon) {
                                char* val_start = colon + 1;
                                while (*val_start == ' ' || *val_start == '\t' || *val_start == '\r' || *val_start == '\n') val_start++;
                                char* val_end = NULL;
                                if (*val_start == '[') {
                                    val_end = strchr(val_start, ']');
                                    if (val_end) val_end++;
                                } else {
                                    char* comma = strchr(val_start, ',');
                                    char* brace = strchr(val_start, '}');
                                    char* line_end = strchr(val_start, '\n');
                                    val_end = comma;
                                    if (!val_end || (brace && brace < val_end)) val_end = brace;
                                    if (!val_end || (line_end && line_end < val_end)) val_end = line_end;
                                }

                                if (val_end) {
                                    char* temp_hb = (char*)malloc(new_cap);
                                    if (temp_hb) {
                                        size_t prefix_len = val_start - new_json;
                                        strncpy(temp_hb, new_json, prefix_len);
                                        temp_hb[prefix_len] = '\0';
                                        char hb_str[128];
                                        snprintf(hb_str, sizeof(hb_str), "[\n    %d,\n    %d,\n    %d\n  ]",
                                                 low_hp, (low_hp + 10 <= 100) ? low_hp + 10 : low_hp, (low_hp + 20 <= 100) ? low_hp + 20 : low_hp);
                                        strcat(temp_hb, hb_str);
                                        strcat(temp_hb, val_end);
                                        strcpy(new_json, temp_hb);
                                        free(temp_hb);
                                    }
                                }
                            }
                        } else {
                            /* 插入 HEALTH_BACK */
                            char* first_brace = strchr(new_json, '{');
                            if (first_brace) {
                                char* temp_hb = (char*)malloc(new_cap);
                                if (temp_hb) {
                                    size_t prefix_len = (first_brace + 1) - new_json;
                                    strncpy(temp_hb, new_json, prefix_len);
                                    temp_hb[prefix_len] = '\0';
                                    char hb_entry[128];
                                    snprintf(hb_entry, sizeof(hb_entry), "\n  \"HEALTH_BACK\": [\n    %d,\n    %d,\n    %d\n  ],",
                                             low_hp, (low_hp + 10 <= 100) ? low_hp + 10 : low_hp, (low_hp + 20 <= 100) ? low_hp + 20 : low_hp);
                                    strcat(temp_hb, hb_entry);
                                    strcat(temp_hb, first_brace + 1);
                                    strcpy(new_json, temp_hb);
                                    free(temp_hb);
                                }
                            }
                        }
                    }

                    /* 写入文件 */
                    if (new_json[0] != '\0') {
                        FILE* fp_out = file_open_utf8(path, "wb");
                        if (fp_out) {
                            fwrite(new_json, 1, strlen(new_json), fp_out);
                            fclose(fp_out);
                            free(new_json);
                            free(original_json);
                            return true;
                        }
                    }
                    free(new_json);
                }
                free(original_json);
            }
        } else {
            fclose(fp_in);
        }
    }

    /* 若原文件不存在或更新失败，生成完整的标准 data/id/<id_name>.json 模板 */
    FILE* fp = file_open_utf8(path, "wb");
    if (!fp) return false;

    const char* reg_str = (in_cfg->region[0]) ? in_cfg->region : "EN";

    fprintf(fp, "{\n");
    fprintf(fp, "  \"REGION\": \"%s\",\n", reg_str);
    fprintf(fp, "  \"AUTO_DISMISS_POPUP\": %s,\n", in_cfg->auto_dismiss_popup ? "true" : "false");
    fprintf(fp, "  \"HEALTH_RECOVER\": %d,\n", rec_hp);
    fprintf(fp, "  \"PEACE_MODE\": %s,\n", in_cfg->peace_mode ? "true" : "false");
    fprintf(fp, "  \"PVP_EVADE\": %s,\n", in_cfg->pvp_evade ? "true" : "false");
    fprintf(fp, "  \"PVP_ANSWER\": %s,\n", in_cfg->pvp_answer ? "true" : "false");
    fprintf(fp, "  \"LOW_HP_DODGE\": %s,\n", in_cfg->low_hp_dodge ? "true" : "false");
    fprintf(fp, "  \"HEALTH_BACK\": [\n    %d,\n    %d,\n    %d\n  ],\n",
            low_hp, (low_hp + 10 <= 100) ? low_hp + 10 : low_hp, (low_hp + 20 <= 100) ? low_hp + 20 : low_hp);
    fprintf(fp, "  \"BUY_LOOT_TOWN\": false,\n");
    fprintf(fp, "  \"BUY_LOOT_RIP\": false,\n");
    fprintf(fp, "  \"HP_BANK_CHECKER\": false,\n");
    fprintf(fp, "  \"SOSKA_CHECKER\": false,\n");
    fprintf(fp, "  \"DEATH_CHECKER\": false,\n");
    fprintf(fp, "  \"OVERWEIGHT_CHECKER\": false,\n");
    fprintf(fp, "  \"OVERWEIGHT_AFK\": %d,\n", in_cfg->overweight_afk > 0 ? in_cfg->overweight_afk : 80);
    fprintf(fp, "  \"SCHEDULE_BUYING\": \"10:30|13:30|20:20\",\n");
    fprintf(fp, "  \"SCHEDULE_MAIL\": \"10:00|15:00|20:00|05:00\",\n");
    fprintf(fp, "  \"SCHEDULE_REWARDS\": \"21:00\",\n");
    fprintf(fp, "  \"AUTOHUNT_BEFORE_TP\": %s\n", in_cfg->autohunt_before_tp ? "true" : "false");
    fprintf(fp, "}\n");

    fclose(fp);
    return true;
}

bool l2m_id_profile_set_auto_dismiss_popup(const char* id_name, bool enabled) {
    if (!id_name || id_name[0] == '\0') return false;
    L2MIdConfig cfg;
    if (!l2m_id_profile_load(id_name, &cfg)) {
        memset(&cfg, 0, sizeof(cfg));
        snprintf(cfg.id_name, sizeof(cfg.id_name), "%s", id_name);
        snprintf(cfg.region, sizeof(cfg.region), "CN");
    }
    cfg.auto_dismiss_popup = enabled;
    return l2m_id_profile_save(id_name, &cfg);
}

bool l2m_id_profile_get_auto_dismiss_popup(const char* id_name, bool* out_enabled) {
    if (!id_name || !out_enabled) return false;
    L2MIdConfig cfg;
    if (l2m_id_profile_load(id_name, &cfg)) {
        *out_enabled = cfg.auto_dismiss_popup;
        return true;
    }
    *out_enabled = true;
    return false;
}

bool l2m_id_profile_set_hp_thresholds(const char* id_name, int32_t low_hp, int32_t recover_hp) {
    if (!id_name || id_name[0] == '\0') return false;
    if (low_hp < 5) low_hp = 5;
    if (low_hp > 90) low_hp = 90;
    if (recover_hp < low_hp + 10) recover_hp = low_hp + 10;
    if (recover_hp > 100) recover_hp = 100;

    L2MIdConfig cfg;
    if (!l2m_id_profile_load(id_name, &cfg)) {
        memset(&cfg, 0, sizeof(cfg));
        snprintf(cfg.id_name, sizeof(cfg.id_name), "%s", id_name);
        snprintf(cfg.region, sizeof(cfg.region), "CN");
    }
    cfg.low_hp_threshold = low_hp;
    cfg.recover_hp_threshold = recover_hp;
    cfg.health_back[0] = low_hp;
    cfg.health_back[1] = (low_hp + 10 <= 100) ? low_hp + 10 : low_hp;
    cfg.health_back[2] = (low_hp + 20 <= 100) ? low_hp + 20 : low_hp;
    return l2m_id_profile_save(id_name, &cfg);
}

bool l2m_id_profile_get_hp_thresholds(const char* id_name, int32_t* out_low_hp, int32_t* out_recover_hp) {
    if (!id_name || !out_low_hp || !out_recover_hp) return false;
    L2MIdConfig cfg;
    if (l2m_id_profile_load(id_name, &cfg)) {
        *out_low_hp = (cfg.low_hp_threshold > 0) ? cfg.low_hp_threshold : 30;
        *out_recover_hp = (cfg.recover_hp_threshold > 0) ? cfg.recover_hp_threshold : 80;
        return true;
    }
    *out_low_hp = 30;
    *out_recover_hp = 80;
    return false;
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
    wchar_t patterns[5][MAX_PATH] = {0};
    int pattern_count = 0;

    wchar_t exe_dir[MAX_PATH] = {0};
    if (GetModuleFileNameW(NULL, exe_dir, MAX_PATH)) {
        wchar_t* last_slash = wcsrchr(exe_dir, L'\\');
        if (!last_slash) last_slash = wcsrchr(exe_dir, L'/');
        if (last_slash) {
            *last_slash = L'\0';
            swprintf(patterns[pattern_count++], MAX_PATH, L"%ls/data/id/*.json", exe_dir);
            swprintf(patterns[pattern_count++], MAX_PATH, L"%ls/../data/id/*.json", exe_dir);
        }
    }
    swprintf(patterns[pattern_count++], MAX_PATH, L"data/id/*.json");
    swprintf(patterns[pattern_count++], MAX_PATH, L"../data/id/*.json");
    swprintf(patterns[pattern_count++], MAX_PATH, L"bot/data/id/*.json");

    for (int p = 0; p < pattern_count; p++) {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(patterns[p], &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    wchar_t wname[MAX_PATH] = {0};
                    wcsncpy(wname, fd.cFileName, MAX_PATH - 1);
                    wchar_t* dot = wcsrchr(wname, L'.');
                    if (dot) *dot = L'\0';

                    char utf8_id[64] = {0};
                    WideCharToMultiByte(CP_UTF8, 0, wname, -1, utf8_id, sizeof(utf8_id) - 1, NULL, NULL);

                    if (utf8_id[0] != '\0') {
                        bool exists = false;
                        for (int i = 0; i < *out_count; i++) {
                            if (strcmp(out_ids[i], utf8_id) == 0) { exists = true; break; }
                        }
                        if (!exists && *out_count < max_count) {
                            snprintf(out_ids[*out_count], sizeof(out_ids[0]), "%s", utf8_id);
                            (*out_count)++;
                        }
                    }
                }
            } while (FindNextFileW(hFind, &fd) && *out_count < max_count);
            FindClose(hFind);
            if (*out_count > 0) return true;
        }
    }
#endif
    return (*out_count > 0);
}
