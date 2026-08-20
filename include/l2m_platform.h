/**
 * @file l2m_platform.h
 * @brief Lineage2MBot Windows 平台句柄截屏与键鼠输入头文件
 */

#ifndef L2M_PLATFORM_H
#define L2M_PLATFORM_H

#include "l2m_types.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
/**
 * @brief 检查当前程序是否拥有 Administrator 管理员权限
 * @return 是否为管理员运行
 */
bool l2m_is_run_as_admin(void);

/**
 * @brief 若无管理员权限，自动以管理员身份重启提权
 * @return 是否已成功发起提权
 */
bool l2m_elevate_to_admin_if_needed(void);

/**
 * @brief 强力激活目标游戏窗口并切换至前台获得输入焦点
 * @param hwnd 目标游戏窗口句柄
 */
void l2m_force_activate_window(HWND hwnd);

/**
 * @brief 向目标游戏窗口发送硬件级扫描码按键 (DirectInput / RawInput 兼容)
 * @param hwnd 目标游戏窗口句柄
 * @param vk_code 虚拟键码 (如 '0', '1', VK_F1 等)
 * @return 是否发送成功
 */
bool l2m_send_key_press(HWND hwnd, WORD vk_code);

/**
 * @brief 通过 Windows GDI / PrintWindow / 屏幕 BitBlt 对目标窗口 HWND 进行多级鲁棒截屏
 * @param hwnd 目标窗口句柄
 * @param client_only 是否只截取客户区
 * @param out_img 输出图像缓冲区 (BGR 格式)
 * @return 是否截屏成功
 */
bool l2m_capture_window(HWND hwnd, bool client_only, L2MImageBuffer* out_img);

/**
 * @brief 在目标窗口下发物理光标移动与硬件级鼠标点击 (DirectInput / RawInput 兼容)
 * @param hwnd 目标窗口句柄
 * @param client_x 客户区 X 坐标
 * @param client_y 客户区 Y 坐标
 * @return 是否下发成功
 */
bool l2m_post_mouse_click(HWND hwnd, int32_t client_x, int32_t client_y);

/**
 * @brief 在目标窗口连续执行“先勾选、再确认关闭”两步操作
 * @param hwnd 目标窗口句柄
 * @param cb_x 勾选框 X 坐标
 * @param cb_y 勾选框 Y 坐标
 * @param btn_x 确认按钮 X 坐标
 * @param btn_y 确认按钮 Y 坐标
 * @param delay_ms 两步之间的延时 (毫秒, 如 250)
 * @return 是否执行成功
 */
bool l2m_post_popup_dismiss_flow(
    HWND hwnd,
    int32_t cb_x, int32_t cb_y,
    int32_t btn_x, int32_t btn_y,
    uint32_t delay_ms
);

/**
 * @brief 触发回城/瞬移指令 (切换并激活游戏窗口 -> 模拟按下 0 快捷键 -> 物理点击回城卷轴点位)
 * @param hwnd 目标游戏窗口句柄
 * @return 是否执行成功
 */
bool l2m_execute_teleport_home(HWND hwnd);

#endif

#ifdef __cplusplus
}
#endif

#endif /* L2M_PLATFORM_H */
