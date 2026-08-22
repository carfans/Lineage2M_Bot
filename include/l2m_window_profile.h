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

/**
 * @brief 从 JSON 文件加载窗口与角色配置文件 (例如 data/window_profiles.json)
 * @param file_path 配置文件路径 (为 NULL 时采用默认路径)
 * @param out_list 输出窗口配置列表
 * @return 是否加载成功
 */
bool l2m_window_profiles_load(
    const char* file_path,
    L2MWindowProfileList* out_list
);

/**
 * @brief 保存窗口与角色配置列表至 JSON 文件
 * @param file_path 配置文件路径
 * @param list 待保存的配置列表
 * @return 是否保存成功
 */
bool l2m_window_profiles_save(
    const char* file_path,
    const L2MWindowProfileList* list
);

/**
 * @brief 初始化一套默认的窗口与角色配置文件
 * @param out_list 输出默认配置列表
 */
void l2m_window_profiles_init_default(L2MWindowProfileList* out_list);

/**
 * @brief 为指定的窗口实例智能匹配最匹配的角色与语言配置
 * @param list 窗口配置列表
 * @param win_inst 目标窗口实例
 * @param window_index 当前窗口在枚举列表中的序号
 * @param out_profile 输出匹配到的配置项
 * @return 是否成功匹配
 */
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

/**
 * @brief 按窗口标题/标识快速保存或更新其绑定的角色名称与语言地区配置
 * @param window_title 窗口标题或唯一名称
 * @param character_name 角色名称 (如 "Andyusa", "狂风舞者")
 * @param region 语言地区 ("CN", "EN", "JP", "RU")
 * @param notes 备注说明
 * @return 是否保存成功
 */
bool l2m_save_window_profile_by_title(
    const char* window_title,
    const char* character_name,
    const char* region,
    const char* notes
);

/**
 * @brief 根据窗口标题查询其专属绑定的角色与语言配置
 * @param window_title 窗口标题
 * @param out_prof 输出匹配到的配置项
 * @return 是否找到匹配配置
 */
bool l2m_load_window_profile_by_title(
    const char* window_title,
    L2MWindowProfile* out_prof
);

/**
 * @brief 对系统中运行的多开游戏窗口执行一键自动排版与四开对齐
 * @param mode 对齐模式 (如 L2M_ALIGN_GRID_2X2 四开网格)
 * @param target_client_w 目标客户区宽度 (默认为 960)
 * @param target_client_h 目标客户区高度 (默认为 540)
 * @param out_aligned_count 输出实际完成对齐的窗口数量
 * @return 是否执行成功
 */
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

/**
 * @brief 从 data/id/<id_name>.json 读取指定角色/窗口的配置文件
 * @param id_name 角色或窗口名称 (例如 "Andyusa")
 * @param out_cfg 输出配置信息
 * @return 是否读取成功
 */
bool l2m_id_profile_load(
    const char* id_name,
    L2MIdConfig* out_cfg
);

/**
 * @brief 保存或更新角色/窗口配置至 data/id/<id_name>.json
 * @param id_name 角色或窗口名称
 * @param in_cfg 待保存的配置信息
 * @return 是否保存成功
 */
bool l2m_id_profile_save(
    const char* id_name,
    const L2MIdConfig* in_cfg
);

/**
 * @brief 快速为 data/id/<id_name>.json 设置或更新绑定的语言地区 (如 "CN", "EN", "JP", "RU")
 * @param id_name 角色或窗口名称
 * @param region 语言地区代码
 * @return 是否保存成功
 */
bool l2m_id_profile_set_region(
    const char* id_name,
    const char* region
);

/**
 * @brief 获取指定角色/窗口配置文件中绑定的语言地区代码
 * @param id_name 角色或窗口名称
 * @param out_region 输出语言地区代码
 * @param max_len 缓冲区最大长度
 * @return 是否获取成功
 */
bool l2m_id_profile_get_region(
    const char* id_name,
    char* out_region,
    size_t max_len
);

/**
 * @brief 枚举 data/id/ 目录下所有的角色/窗口配置文件列表
 * @param out_ids 输出 ID 名称数组
 * @param max_count 数组最大容量
 * @param out_count 输出实际检测到的配置文件数量
 * @return 是否成功完成枚举
 */
bool l2m_enum_id_profiles(
    char out_ids[][64],
    int32_t max_count,
    int32_t* out_count
);

#ifdef __cplusplus
}
#endif

#endif /* L2M_WINDOW_PROFILE_H */
