# 📝 Lineage2MBot 版本更新历史 (Changelog)

All notable changes to this project will be documented in this file.

---

## [v2.7.0] - 2026-08-21

### 🧹 优化与重构 (Refactoring & Cleanup)
- **全面清除未被使用的冗余 CBT 采样点与数据瘦身**：
  - 清理了 `data/cbt/CN.json`、`data/cbt/EN.json`、`data/cbt/JP.json`、`data/cbt/RU.json` 中遗留的 210+ 个冗余无用点位（包括历史背包遍历点位 `inventory_slot2~20`、副本按钮、旧坐标配置等）；
  - 配置文件大小由 30KB 瘦身至 3KB，行数由 2300+ 行精简至 140 行，显著提升 JSON 解析速度与内存效率；
  - 规范统一保留 3 大核心架构节点：
    1. `popup_scan_config`：命名弹窗扫描配置（`top_left_tip`, `center_modal`, `fullscreen_event`）；
    2. `map_box_config`：左上角小地图视觉与区域特征配置（Safe/Common/RedGrid/Heading）；
    3. `inventory_slot1_empty_1`、`home_scroll_button_no_energomode`、`home_scroll_button_energomode` 等核心关联采样点。
- **全量自动化测试与构建验证**：
  - 单元测试 **114 / 114 项 100% 纯净通过**，GUI 与 DLL 构建无警告无错误。

---

## [v2.6.0] - 2026-08-20

### 🚀 新增功能 (Features)
- **`data/id/` 独立角色与窗口配置文件深度对接与语言绑定 (`src/core/window_profile_manager.c`)**：
  - **标准化单文件管理**：支持以每个窗口/角色 ID 命名的 JSON 配置文件（如 `data/id/Andyusa.json`、`data/id/ChineseHero.json`）；
  - **核心字段 `"REGION"` 绑定**：支持对每个独立配置文件读写 `"REGION": "EN"` / `"REGION": "CN"` / `"REGION": "JP"` / `"REGION": "RU"` 语言属性；
  - **C 语言读写接口**：
    - `l2m_id_profile_load`：加载指定角色配置；
    - `l2m_id_profile_save`：保存/更新指定角色配置；
    - `l2m_id_profile_set_region`：快速更新并持久化指定角色的语言地区；
    - `l2m_id_profile_get_region`：快速获取角色语言代码；
    - `l2m_enum_id_profiles`：扫描并枚举 `data/id/` 目录下的所有角色配置文件列表。
- **Win32 调试器与 `data/id/` 配置文件无缝联动 (`src/gui/win_debug_dialog.c`)**：
  - 切换目标游戏窗口时，自动从 `data/id/<name>.json` 读取该角色的 `"REGION"` 语言绑定并同步加载对应 `data/cbt/<REGION>.json`；
  - 点击【💾 绑定保存】时，同步将用户手动选择的语言地区持久化更新回 `data/id/<name>.json` 文件中。
- **全量自动化测试扩充 (`tests/test_core.c`)**：
  - 增加对 `data/id/Andyusa.json` 的加载验证、新文件写入回读与目录扫描测试，全量单元测试达到 **114 / 114 项 100% 通过**。

---

## [v2.5.0] - 2026-08-20

### 🚀 新增功能 (Features)
- **多开窗口一键自动排版与四开网格对齐 (`l2m_align_game_windows`)**：
  - 支持将运行中的 Lineage 2M 游戏窗口一键按 **2x2 四宫格** 规整平铺；
  - 自动读取屏幕工作区（避开 Windows 任务栏），精确通过 `AdjustWindowRectEx` 计算标题栏与边框，将各窗口客户区尺寸固定为标准的 960x540 分辨率，消除窗口重叠与边缘遮挡；
  - 调试器顶栏新增【🪟 四开对齐】按钮，支持多开窗口一键整齐排版。
- **手动选择并直接保存至窗口名称对应配置文件 (`l2m_save_window_profile_by_title`)**：
  - 移除画面自动识别猜测语言逻辑，100% 尊重用户在 UI 上的手动配置；
  - 在语言与 CBT 区域新增【💾 绑定保存】按钮，允许用户将当前选中的窗口名称、手动指定的语言地区（CN/EN/JP/RU）及角色名称直接保存写入 `window_profiles.json`；
  - 提供 `l2m_load_window_profile_by_title` 实现按窗口名称的专属配置快速索引与回读。
- **全量自动化测试扩充 (`tests/test_core.c`)**：
  - 新增针对窗口名称保存回读与四开网格排列计算的测试用例，单元测试达到 **105 / 105 项 100% 通过**。

---

## [v2.4.0] - 2026-08-20

### 🚀 新增功能 (Features)
- **多开游戏窗口实例管理与角色配置绑定引擎 (`src/core/window_profile_manager.c`)**：
  - **解耦窗口标题与角色名称**：彻底解决中文客户端、模拟器（PURPLE/多开模拟器）窗口标题缺少角色名或乱码导致无法识别角色配置的痛点；
  - **窗口多开配置持久化 (`data/window_profiles.json`)**：支持通过窗口序号（`INDEX`）、窗口标题关键字（`TITLE_KEYWORD`）、或固定 `HWND` 将特定窗口实例与指定角色名称（支持中文、英文、特殊字符）及语言地区（CN/EN/JP/RU）绑定；
  - **画面语言自适应智能检测 (`l2m_auto_detect_screen_region`)**：当窗口未手动绑定语言时，通过分析游戏画面特征（左上角 Safe/Common 指示词与 CBT 特征点命中率），自动识别当前窗口为 CN、EN、JP 或 RU。
- **Win32 调试器顶栏多开窗口切换集成 (`src/gui/win_debug_dialog.c`)**：
  - 顶栏新增【🎮 窗口/角色】下拉选择框与【🔄 刷新】按钮；
  - 实时列出系统中所有运行的 Lineage 2M 游戏窗口实例（显示窗口序号、角色名、语言地区、客户区尺寸与 HWND）；
  - 切换下拉框时自动绑定目标窗口 HWND，自动同步切换对应的语言 CBT 配置文件（如 CN.json、EN.json），并实时截屏刷新。
- **全量自动化测试扩充 (`tests/test_core.c`)**：
  - 新增 18 项针对多开窗口枚举、配置持久化读写、多语言/中文角色匹配与自适应画面语言识别的测试用例，全量测试达到 **101 / 101 项 100% 通过**。

---

## [v2.3.0] - 2026-08-20

### 🚀 新增功能 (Features)
- **精准小地图视觉特征引擎升级 (`src/game/map_zone_engine.c`)**：
  - **🛡️ 蓝色 Safe 安全区域**：锁定小地图左上角徽标，精准提取高纯度天蓝色/青蓝色特征（$B \ge 85 \land B \ge R + 20 \land B \ge G - 15$），置信度高达 80~99 分；
  - **🌾 浅咖色 Common 普通区域**：提取左上角黄褐色/浅咖色（$R > G > B \land R \ge 105 \land G \ge 65 \land B \le 115$）特征，精准区分普通野外刷怪区；
  - **⚔️ 红色不可记忆网格区域**：全局扫描纯红色高饱和网格线条（$R \ge 130 \land R \ge G \times 1.5$），精准告警禁记区与 PVP 战斗区；
  - **🎯 中心玩家指示器与扇形视角朝向分析**：定位小地图正中心橙色圆圈与白色箭头，通过分析扇形橙色渐变视角的连通域重心，实时解算出玩家面朝方向角度（0.0° ~ 360.0°，正北为 0°）；
  - **画板直观渲染**：在调试器画板上根据区域类型渲染天蓝色/浅咖色/鲜红色边框、红网格线、橙色玩家圆环与视向引导射线。
- **地图区域坐标与多维判据参数 JSON 完整管理 (`map_box_config`)**：
  - 在 `include/l2m_zone.h` 与 `include/l2m_cbt.h` 中引入 `L2MMapZoneConfig` 结构体；
  - 支持将地图扫描 ROI（`x, y, width, height`）、徽标子 ROI（`badge_offset_x: 2, badge_offset_y: 2, badge_width: 65, badge_height: 30`）、蓝色安全阈值（`min_blue_ratio`）、浅咖色普通阈值（`min_brown_ratio`）、红色网格阈值（`min_red_ratio`）与暗底亮度门限（`max_bg_brightness`）完整持久化到 JSON 文件中。

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
- **修复调试器放大镜视窗被输出信息框遮挡问题**：
  - 重构左侧控制面板垂直 Y 轴排版，将放大镜画板尺寸修正为标准的 `112x112`（完整容纳 11x11 像素网格 10x 放大图），并将诊断输出信息框下移至 `y=388`，彻底消除层叠遮挡。
- **消除弹窗检测与诊断报告中的中文乱码**：
  - 在 `win_debug_dialog.c` 中实现 `utf8_to_wide` 专用转换函数，将底层引擎输出的 UTF-8 格式字符串显式转换为 Unicode `wchar_t*`，彻底解决 `swprintf` 使用 `%hs` 导致的 ANSI/GBK 解码乱码。
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
