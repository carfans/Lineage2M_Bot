/**
 * @file l2m_gui.h
 * @brief Lineage2MBot 纯 C Win32 原生图形桌面界面头文件
 */

#ifndef L2M_GUI_H
#define L2M_GUI_H

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include "l2m_types.h"
#include "l2m_vision.h"
#include "l2m_hp.h"
#include "l2m_popup.h"
#include "l2m_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化通用控件库 */
bool l2m_gui_init_common_controls(void);

/* 创建并显示主监控窗口 */
HWND l2m_create_main_window(HINSTANCE hInstance, int nCmdShow);

/* 创建并弹出可视化调试对话框 */
void l2m_open_debug_dialog(HWND hParentWnd, HWND hTargetGameWnd);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* L2M_GUI_H */
