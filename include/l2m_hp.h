/**
 * @file l2m_hp.h
 * @brief Lineage2MBot 血条像素采样与血量百分比计算引擎头文件
 */

#ifndef L2M_HP_H
#define L2M_HP_H

#include "l2m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从截取的血条 ROI 区域 (RGB) 中快速计算当前血量百分比
 * @param crop_rgb 血条切片图像 (宽度约为 100~150 像素, 高度 2~4 像素)
 * @param config 血条颜色与几何配置参数
 * @param out_result 输出计算结果 (血量百分比, 采样端点, 实测均值等)
 * @return 是否成功完成计算
 */
bool l2m_calculate_hp(
    const L2MImageBuffer* crop_rgb,
    const L2MHpConfig* config,
    L2MHpResult* out_result
);

/**
 * @brief 在整张游戏画面 (960x540) 中按配置直接提取血条切片并计算血量
 * @param full_screen_rgb 游戏全局 RGB 画面
 * @param config 血条配置
 * @param out_result 输出计算结果
 * @return 是否成功完成计算
 */
bool l2m_calculate_hp_from_fullscreen(
    const L2MImageBuffer* full_screen_rgb,
    const L2MHpConfig* config,
    L2MHpResult* out_result
);

/**
 * @brief 从截取的血条 ROI 区域 (直接传入 BGR 格式，零转换开销) 中计算当前血量
 * @param crop_bgr 血条切片图像 (BGR 格式)
 * @param config 血条配置
 * @param out_result 输出计算结果
 * @return 是否成功完成计算
 */
bool l2m_calculate_hp_bgr(
    const L2MImageBuffer* crop_bgr,
    const L2MHpConfig* config,
    L2MHpResult* out_result
);

#ifdef __cplusplus
}
#endif

#endif /* L2M_HP_H */
