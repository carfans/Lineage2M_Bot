# 📝 Lineage2MBot 版本更新历史 (Changelog)

All notable changes to this project will be documented in this file.

---

## [v2.2.0] - 2026-08-20

### 🚀 新增功能 (Features)
- **动态命名弹窗管理器 (Named Popup Profile Manager)**：
  - 彻底解耦固定三分类弹窗限制，支持任意自定义命名弹窗（如 `top_left_tip`、`center_modal`、`fullscreen_event`、`resurrect_confirm`、`dungeon_enter` 等）；
  - 提供完整的增删改查 C 语言接口：`l2m_cbt_get_popup_item`、`l2m_cbt_set_popup_item`、`l2m_cbt_delete_popup_item`、`l2m_cbt_get_popup_count`。
- **弹窗深度链接 CBT 采样点数据**：
  - 在 `L2MPopupItem` 数据结构中新增 `linked_cbt_key` 字段并在 JSON 中持久化；
  - 调试器中增加“关联CBT”下拉选择框，实现弹窗扫描 ROI 与特定特征采样点的强联合校验。
- **调试器界面升级与弹窗名称管理**：
  - 界面新增【➕ 新建】、【💾 保存/更新】、【🗑️ 删除】专属操作流；
  - 支持直接在输入框中修改弹窗 Key 与中文描述，自动创建新配置或更新已有配置并同步写入 JSON。

### 🐛 缺陷修复与稳定性增强 (Bug Fixes & Improvements)
- **解决保存弹窗时提示“没有权限写文件”与路径失效问题**：
  - 在文件选择框（`GetOpenFileName` / `GetSaveFileName`）中加入 `OFN_NOCHANGEDIR` 标志，防止系统悄悄篡改当前进程工作目录；
  - 重构 `get_cbt_file_path`，优先通过 `GetModuleFileNameA` 动态探测 EXE 物理安装目录，生成绝对路径；
  - 增加 `ensure_parent_dir_exists`，在保存文件时若目标父目录（如 `data/cbt/`）不存在，自动递归创建。
- **彻底解决物理鼠标模拟点击无效与“激活吞噬”问题**：
  - **自适应窗口分辨率换算**：将 960x540 标准参考系自适应按比例映射至目标客户区物理真实分辨率；
  - **支持全局虚拟多屏桌面绝对坐标**：引入 `SM_CXVIRTUALSCREEN` / `SM_CYVIRTUALSCREEN` 归一化坐标与 `MOUSEEVENTF_VIRTUALDESK` 标志；
  - **移除冲突的 `PostMessage` 注入**：杜绝硬件事件与消息队列并发导致的双重按键冲突；
  - **实现双重确认击穿机制 (Double-Pump Click Pipeline)**：
    1. 第一击（短按 40ms）：预激活击穿，强制消耗 Windows `WM_MOUSEACTIVATE` 激活吞噬状态；
    2. 过渡停顿 (60ms)；
    3. 第二击（真实长按 130ms）：原地纯净按下抬起，100% 触发按钮的真实下压动画与 `OnClicked` 业务响应；
  - **窗口置顶与子视口置焦**：`l2m_force_activate_window` 强力置顶并赋予 120ms 焦点建立缓冲，同时将焦点赋予游戏渲染子窗口。

---

## [v2.1.0] - 2026-08-20

### 🚀 弹窗多维特征与先验背景识别增强
- **暗色背景先验色度校验**：统计暗灰/暗蓝像素占比（阈值 45%）与平均亮度（< 90），在野外正常游戏场景下直接拦截误报；
- **按钮色彩与黄金宽高比打分**：精准识别橙色跳转/确认按钮（RGB `[220, 115, 10]`）与灰色关闭按钮（RGB `[80, 85, 90]`）；
- **弹窗多维结构特征检测**：支持弹窗面板外接轮廓查找、顶部标题栏金色装饰提取、文本行水平投影分析以及关闭叉号 (X) 模板探测；
- **全屏一键自动巡检**：`l2m_detect_all_popups` 自动遍历当前启用的全部命名弹窗配置。

---

## [v2.0.0] - 2026-08-19

### 🌟 纯 C 原生重构与多开高频监控工作台
- **100% 纯 C 原生架构**：完全脱离 Python 与 PyQt5 依赖，单文件独立运行（体积仅 ~160 KB，内存 < 10 MB，CPU < 0.1%）；
- **多开客户端实时监控表格 (ListView)**：7 列全维度状态实时呈现与并发调度；
- **微秒级并发后台守护线程 (`MultiClientWorkerThread`)**：单次综合决策耗时仅 0.3 微秒；
- **全局安全紧急制动热键 [Ctrl + Q]**：Windows 内核级最高优先级热键制动；
- **960x540 高清双缓冲渲染画板与 11x11 像素放大镜视窗**；
- **多语言 CBT 特征采样点配置管理**（支持 CN / EN / JP / RU 切换与一键持久化保存）。
