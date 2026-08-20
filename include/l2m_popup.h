/**
 * @file l2m_popup.h
 * @brief Lineage2MBot 游戏弹窗分析与关闭按钮定位引擎头文件
 */

#ifndef L2M_POPUP_H
#define L2M_POPUP_H

#include "l2m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 弹窗暗色背景色先验校验 (区分暗色蒙版与高亮自然野外场景)
 * @param crop_rgb 弹窗扫描 ROI 切片
 * @param popup_type 弹窗类型 (top_left, center, fullscreen)
 * @param out_bg_info 输出背景色校验统计信息
 * @return true: 符合弹窗暗底特征; false: 不符合 (高亮自然场景或噪点)
 */
bool l2m_check_popup_background(
    const L2MImageBuffer* crop_rgb,
    L2MPopupType popup_type,
    L2MPopupBgInfo* out_bg_info
);

/**
 * @brief 在切片区域中定位“不再显示该提示”复选框 (Checkbox)
 * @param crop_rgb 左上角弹窗切片
 * @param base_x 切片在全局画面中的 X 偏移
 * @param base_y 切片在全局画面中的 Y 偏移
 * @param ref_btn_y 底部确认按钮的 Y 坐标
 * @param ref_btn_x 底部确认按钮的 X 坐标
 * @param out_cb_pos 输出勾选框点击坐标
 * @return 是否找到勾选框
 */
bool l2m_find_checkbox(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    int32_t ref_btn_y,
    int32_t ref_btn_x,
    L2MPoint* out_cb_pos
);

/**
 * @brief 提取左上角提示弹窗的按钮与勾选框 (锁定左侧灰色确认, 规避右侧橙色跳转)
 * @param crop_rgb 左上角切片
 * @param base_x 全局 X 偏移
 * @param base_y 全局 Y 偏移
 * @param out_result 输出弹窗检测结果
 * @return 是否成功识别
 */
bool l2m_detect_top_left_popup(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MPopupResult* out_result
);

/**
 * @brief 提取中间标准模态弹窗或全屏弹窗的橙黄色确认按钮
 * @param crop_rgb 弹窗切片
 * @param base_x 全局 X 偏移
 * @param base_y 全局 Y 偏移
 * @param popup_type 弹窗类型 (center / fullscreen)
 * @param out_result 输出弹窗检测结果
 * @return 是否成功识别
 */
bool l2m_detect_standard_popup(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MPopupType popup_type,
    L2MPopupResult* out_result
);

/**
 * @brief 弹窗本体多维结构特征检测 (面板轮廓、标题栏、文本行投影与叉号特征)
 * @param crop_rgb 弹窗扫描 ROI 切片
 * @param base_x 全局 X 偏移
 * @param base_y 全局 Y 偏移
 * @param popup_type 弹窗类型 (top_left, center, fullscreen)
 * @param out_features 输出特征分析结果
 * @return 是否检测到至少一项有效弹窗结构特征
 */
bool l2m_detect_popup_features(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MPopupType popup_type,
    L2MPopupFeatureInfo* out_features
);

/**
 * @brief 统一弹窗检测主入口 (带背景色先验校验与特征检测)
 * @param crop_rgb 弹窗切片
 * @param base_x 全局 X 偏移
 * @param base_y 全局 Y 偏移
 * @param popup_type 弹窗类型 (top_left, center, fullscreen)
 * @param validate_bg 是否启用背景色先验校验 (建议为 true)
 * @param out_result 输出详细检测结果
 * @return 是否成功识别到弹窗与关闭按钮
 */
bool l2m_detect_popup(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    L2MPopupType popup_type,
    bool validate_bg,
    L2MPopupResult* out_result
);

/**
 * @brief 基于指定命名弹窗配置项进行高精度检测
 * @param crop_rgb 弹窗切片
 * @param base_x 全局 X 偏移
 * @param base_y 全局 Y 偏移
 * @param item 命名弹窗特征配置项 (const L2MPopupItem*)
 * @param validate_bg 是否启用背景色先验校验
 * @param out_result 输出详细检测结果
 * @return 是否成功识别
 */
bool l2m_detect_popup_by_item(
    const L2MImageBuffer* crop_rgb,
    int32_t base_x,
    int32_t base_y,
    const void* item,
    bool validate_bg,
    L2MPopupResult* out_result
);

/**
 * @brief 在全屏画面中根据 CBT 配置自动巡检所有已启用的命名弹窗
 * @param full_frame_rgb 完整画面 (如 960x540 RGB)
 * @param cbt_cfg CBT 配置 (const L2MCbtConfig*)
 * @param out_result 输出检测结果
 * @return 是否检测到任意已启用的命名弹窗
 */
bool l2m_detect_all_popups(
    const L2MImageBuffer* full_frame_rgb,
    const void* cbt_cfg,
    L2MPopupResult* out_result
);

#ifdef __cplusplus
}
#endif

#endif /* L2M_POPUP_H */
