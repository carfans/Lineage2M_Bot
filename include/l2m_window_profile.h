/**
 * @file l2m_window_profile.h
 * @brief Lineage2MBot 游戏窗口实例枚举、多开配置与多语言角色绑定管理器头文件
 */

#ifndef L2M_WINDOW_PROFILE_H
#define L2M_WINDOW_PROFILE_H

#include "l2m_types.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WINDOW_PROFILES 32
#define MAX_GAME_WINDOWS    32

/* 窗口匹配方式枚举 */
typedef enum {
    L2M_WIN_MATCH_INDEX = 0,        /* 按多开窗口枚举序号匹配 (第1个、第2个窗口) */
    L2M_WIN_MATCH_TITLE_KEYWORD = 1,/* 按窗口标题包含的关键字匹配 */
    L2M_WIN_MATCH_HWND = 2,         /* 按固定 HWND 运行时绑定 */
    L2M_WIN_MATCH_AUTO = 3          /* 自动识别画面语言与角色特征 */
} L2MWindowMatchRule;

/* 运行中的游戏窗口实例信息 */
typedef struct {
#ifdef _WIN32
    HWND hwnd;                      /* 窗口句柄 */
#else
    void* hwnd;
#endif
    uint32_t pid;                   /* 进程 PID */
    char process_name[64];          /* 进程名称 (如 Lineage2M.exe / Purple.exe) */
    char window_title[256];         /* 窗口标题 (UTF-8 编码) */
    int32_t client_width;           /* 客户区宽度 */
    int32_t client_height;          /* 客户区高度 */
    bool is_minimized;              /* 是否最小化 */
    bool is_active;                 /* 是否为前台焦点窗口 */
} L2MWindowInstance;

/* 游戏窗口与角色配置项 (对应 JSON window_profiles 节点) */
typedef struct {
    char profile_id[64];            /* 配置唯一标识符 (如 "profile_01", "andy_main") */
    char profile_name[64];          /* 显示名称/别名 (如 "主号-Andy", "二号-挂机法师") */
    char character_name[64];        /* 游戏内角色名称 (支持中文/英文/任意字符，如 "Andyusa", "剑刃舞者") */
    char region[16];                /* 语言地区代码 ("CN", "EN", "JP", "RU", "TW", "KR") */
    char custom_cbt_path[256];      /* 可选自定义 CBT 文件路径 (为空时默认采用 data/cbt/<REGION>.json) */

    L2MWindowMatchRule match_rule;  /* 窗口匹配规则 */
    int32_t match_window_index;     /* 规则为 INDEX 时的目标窗口索引 (0, 1, 2...) */
    char match_title_keyword[128];  /* 规则为 TITLE_KEYWORD 时的标题关键字 */

    bool auto_detect_region;        /* 当未明确指定或匹配时，是否根据画面特征自动检测语言地区 */
    bool auto_dismiss_popup;        /* 是否自动检测并关闭弹窗 (默认 true) */
    int32_t low_hp_threshold;       /* 低血量回城逃生阈值百分比 (默认 30%, 范围 5~90) */
    int32_t recover_hp_threshold;   /* 恢复出战血量阈值百分比 (默认 80%, 范围 20~100) */
    bool enabled;                   /* 是否启用该配置 */
    char notes[128];                /* 备注信息 */
} L2MWindowProfile;

/* 窗口配置列表集合 */
typedef struct {
    int32_t count;
    L2MWindowProfile profiles[MAX_WINDOW_PROFILES];
} L2MWindowProfileList;

/**
 * @brief 枚举当前系统中运行的所有 Lineage2M / PURPLE 游戏窗口实例
 * @param out_windows 输出窗口实例数组
 * @param max_count 数组最大容量
 * @param out_count 输出实际检测到的游戏窗口数量
 * @return 是否成功完成枚举
 */
bool l2m_enum_game_windows(
    L2MWindowInstance* out_windows,
    int32_t max_count,
    int32_t* out_count
);

bool l2m_window_profiles_load(
    const char* file_path,
    L2MWindowProfileList* out_list
);

bool l2m_window_profiles_save(
    const char* file_path,
    const L2MWindowProfileList* list
);

void l2m_window_profiles_init_default(L2MWindowProfileList* out_list);

bool l2m_window_profile_match(
    const L2MWindowProfileList* list,
    const L2MWindowInstance* win_inst,
    int32_t window_index,
    L2MWindowProfile* out_profile
);

/* 窗口排列对齐模式 */
typedef enum {
    L2M_ALIGN_GRID_2X2 = 0,     /* 四开 2x2 四宫格网格对齐 */
    L2M_ALIGN_HORIZONTAL = 1,   /* 横向水平平铺 */
    L2M_ALIGN_VERTICAL = 2      /* 纵向垂直平铺 */
} L2MWindowAlignMode;

#ifndef MAX_MONITORS
#define MAX_MONITORS 16
#endif

/* 显示器信息描述结构体 */
typedef struct {
    int32_t monitor_index;       /* 0-indexed 显示器序号 (0, 1, 2...) */
    char name[64];              /* 显示器设备名称 (如 "\\.\DISPLAY1", "DISPLAY1") */
    char desc[128];             /* 友好的中文描述 (如 "🖥️ 显示器 1 [主屏幕] (1920x1080)") */
    bool is_primary;            /* 是否为主显示器 */
    int32_t x;                  /* 虚拟桌面绝对屏幕 X 原点 (支持负坐标) */
    int32_t y;                  /* 虚拟桌面绝对屏幕 Y 原点 */
    int32_t width;              /* 分辨率宽度 */
    int32_t height;             /* 分辨率高度 */
    int32_t work_x;             /* 可用工作区左上角 X (已扣除 Windows 任务栏) */
    int32_t work_y;             /* 可用工作区左上角 Y */
    int32_t work_width;         /* 可用工作区宽度 */
    int32_t work_height;        /* 可用工作区高度 */
} L2MMonitorInfo;

/* 系统多显示器列表 */
typedef struct {
    L2MMonitorInfo monitors[MAX_MONITORS];
    int32_t count;               /* 实际检测到的显示器数量 */
    int32_t primary_index;       /* 主显示器在列表中的索引 */
} L2MMonitorList;

/* 枚举系统当前连接的所有物理显示器 */
bool l2m_enum_monitors(L2MMonitorList* out_list);

/* 根据索引获取特定显示器信息 */
bool l2m_get_monitor_by_index(int32_t monitor_index, L2MMonitorInfo* out_info);

bool l2m_save_window_profile_by_title(
    const char* window_title,
    const char* character_name,
    const char* region,
    const char* notes
);

bool l2m_load_window_profile_by_title(
    const char* window_title,
    L2MWindowProfile* out_prof
);

/* 支持指定目标物理显示器的多开窗口智能对齐排列 */
bool l2m_align_game_windows_ex(
    L2MWindowAlignMode mode,
    int32_t target_client_w,
    int32_t target_client_h,
    int32_t monitor_index,
    int32_t* out_aligned_count
);

/* 保持向后兼容的标准接口 (默认在主屏幕或当前屏幕对齐) */
bool l2m_align_game_windows(
    L2MWindowAlignMode mode,
    int32_t target_client_w,
    int32_t target_client_h,
    int32_t* out_aligned_count
);

/* data/id/<name>.json 角色与窗口配置项 */
typedef struct {
    char id_name[64];           /* 角色/窗口标识 (例如 "Andyusa", "狂风舞者") */
    char file_path[256];        /* 文件实际路径 */
    char region[16];            /* 语言地区 ("EN", "CN", "JP", "RU") */
    bool auto_dismiss_popup;    /* 是否自动检测并关闭弹窗 (默认 true) */
    int32_t low_hp_threshold;   /* 低血量回城阈值百分比 (默认 30) */
    int32_t recover_hp_threshold; /* 恢复出战血量阈值百分比 (默认 80) */
    bool peace_mode;            /* 和平模式 */
    bool pvp_evade;             /* PVP 规避 */
    bool pvp_answer;            /* PVP 反击 */
    bool low_hp_dodge;          /* 低血量闪避 */
    int32_t health_back[3];     /* 回城血量阈值 (如 20, 30, 40) */
    bool buy_loot_town;         /* 回城补给 */
    bool buy_loot_rip;          /* 死亡后补给 */
    bool hp_bank_checker;       /* 血瓶监控 */
    bool death_checker;         /* 死亡监控 */
    bool overweight_checker;    /* 超重监控 */
    int32_t overweight_afk;     /* 超重阈值 (如 80) */
    bool autohunt_before_tp;    /* 瞬移前自动挂机 */
} L2MIdConfig;

bool l2m_id_profile_load(
    const char* id_name,
    L2MIdConfig* out_cfg
);

bool l2m_id_profile_save(
    const char* id_name,
    const L2MIdConfig* in_cfg
);

bool l2m_id_profile_set_region(
    const char* id_name,
    const char* region
);

bool l2m_id_profile_get_region(
    const char* id_name,
    char* out_region,
    size_t max_len
);

bool l2m_id_profile_set_auto_dismiss_popup(
    const char* id_name,
    bool enabled
);

bool l2m_id_profile_get_auto_dismiss_popup(
    const char* id_name,
    bool* out_enabled
);

bool l2m_id_profile_set_hp_thresholds(
    const char* id_name,
    int32_t low_hp,
    int32_t recover_hp
);

bool l2m_id_profile_get_hp_thresholds(
    const char* id_name,
    int32_t* out_low_hp,
    int32_t* out_recover_hp
);

bool l2m_enum_id_profiles(
    char out_ids[][64],
    int32_t max_count,
    int32_t* out_count
);

#ifdef __cplusplus
}
#endif

#endif /* L2M_WINDOW_PROFILE_H */
