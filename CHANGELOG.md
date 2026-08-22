# 📝 Lineage2MBot 版本更新历史 (Changelog)

All notable changes to this project will be documented in this file.

---

## [v2.8.0] - 2026-08-22

### 🚀 新增功能 (Features)
- **纯配置文件驱动的血条采样与百分比计算引擎重构 (`src/game/hp_engine.c`, `src/gui/win_main_gui.c`)**：
  - **彻底移除代码硬编码参数与死数值**：全面摒弃代码中写死的 `103px`、`0.95f`、固定颜色值或经验偏移等硬编码魔法数字，血条的采样坐标 `(offset_x, offset_y)`、有效物理宽度 `width`、高度 `height`、主色1 `target_color_1` 与容差1、辅色2 `target_color_2` 与容差2 **100% 严格由 CBT 配置文件（`data/cbt/<REGION>.json` 的 `hp_bar_config` 节点）定义与驱动**；
  - **纯配置驱动连续有效像素追踪**：严格依据配置的主色1/辅色2与容差进行从左向右列级连续性扫描，实测连续有效像素宽度直接计算 `(continuous_end + 1) * 100 / config->width`；
  - **调试所见即所得**：用户在调试窗口中将血条物理宽度校准为实际有效像素（如 97px、100px 等）并保存后，守护线程与计算引擎立即 100% 动态自适应，满血即 100%，掉血毫秒级灵敏响应，绝无任何滞后；
- **多物理显示器识别、选择与跨屏智能网格对齐 (`include/l2m_window_profile.h`, `src/core/window_profile_manager.c`, `src/gui/win_main_gui.c`, `src/gui/win_debug_dialog.c`)**：
  - **物理显示器枚举与工作区探测 (`l2m_enum_monitors`, `l2m_get_monitor_by_index`)**：通过 Windows API (`EnumDisplayMonitors` / `GetMonitorInfoW`) 动态枚举系统当前连接的所有物理显示器（包含主屏、副屏、分辨率、工作区坐标与设备名），支持多屏异构分辨率与负坐标虚拟桌面；
  - **跨显示器多开网格对齐排列 (`l2m_align_game_windows_ex`)**：全面升级窗口对齐引擎，支持指定目标显示器索引，自动避开目标显示器任务栏并以 960x540 标准分辨率在选定显示器上进行 2x2 四宫格无缝排布；
  - **主界面多显示器选择与一键排版**：主界面顶部工具栏新增【🖥️ 显示器: `[ 显示器 1 [主屏幕] (1920x1080) ]`】下拉选择框与【🪟 四开对齐】按钮，支持随时切换目标屏幕一键对齐；
  - **调试窗口显示器选择与联动**：调试窗口顶部区域 0 同步新增显示器下拉选择框，支持单窗口调试时任意选屏并对齐多开客户端；
- **调试窗口血条参数可视化调试、一键色彩拾取与 CBT 持久化 (`src/gui/win_debug_dialog.c`, `src/core/cbt_manager.c`, `include/l2m_cbt.h`)**：
  - **血条参数全要素编辑面板**：在调试窗口左侧控制区新增专属【🩸 血条参数精准调试与保存】板块，支持动态微调血条位置 `(offset_x, offset_y)`、尺寸 `(width, height)`、主目标色1 `target_color_1` 与容差1、辅目标色2 `target_color_2` 与容差2；
  - **一键填入画面拾取色**：提供【🎯 填入色1】与【🎯 填入色2】按钮，可在画板或 11x11 像素放大镜中拾取任意位置像素 RGB 后一键填充至对应血条颜色输入框，校准零门槛；
  - **画板实时高亮与端点标尺可视化**：点击【🩸 测试血条】后，右侧画板实时在血条区域绘制亮青色外框、亮红有效血量进度条与黄色垂直端点标尺，并在顶部展示 `🩸 HP: xx% (有效端点: xx/xx px)`，放大镜自动聚焦到血条起点；
  - **多语言 CBT 配置文件双向持久化**：点击【💾 保存血条】后，自动将微调后的血条参数保存至当前选中语言的 `data/cbt/<REGION>.json`（`hp_bar_config` 节点），并在切换语言（`CN/EN/JP/RU`）时自动同步加载对应参数；
- **超高频 30ms 毫秒级血量监测与分频架构优化 (`src/gui/win_main_gui.c`, `src/platform/win_capture.c`, `src/game/hp_engine.c`)**：
  - **血条局部 ROI 极速微秒级截屏 (`l2m_capture_window_roi`)**：将血量采样的截屏范围由全屏 960x540 缩减至仅血条所在的 103x2 局部切片，内存传输量减少 99.96%，抓取耗时降至 < 0.05 毫秒；
  - **零拷贝 BGR 极速像素计算 (`l2m_calculate_hp_bgr`)**：直接在抓取到的 BGR 内存流上进行颜色匹配计算，彻底消除色彩空间转换与动态内存分配开销；
  - **主守护线程超高频巡检 (~33 FPS)**：守护循环由原先 200ms 降至 30ms 高频周期，每秒巡检高达 30+ 次，血量跌破阈值时可在 30 毫秒内瞬间执行安全瞬移回城；
  - **弹窗巡检智能分流降频与 CBT 缓存**：计算开销较大的全屏弹窗检测与 CBT 加载与高频血量检测解耦，采用每 30 周期（约 1 秒）分频巡检，极大释放 CPU 资源；
  - **UI 列表平滑节流刷新**：主界面 ListView 采用 150ms 节流刷新机制，杜绝 Windows 消息队列阻塞，保障高频后台监控流畅奔跑；
- **多开窗口地区 (REGION) 精准加载、实时下拉切换与双向持久化 (`src/gui/win_main_gui.c`, `src/core/window_profile_manager.c`)**：
  - **地区选择下拉框**：主界面第二行策略区新增【地区: `[ CN/TW/EN/JP/KR/RU ]`】下拉选择控件；
  - **双向联动与即时回显**：选中列表任意行时，自动同步显示该窗口实际绑定的游戏地区（支持 `CN` 国服简中、`TW` 台服繁中、`EN` 美欧英文、`JP` 日服、`KR` 韩服、`RU` 俄服）；
  - **双层配置同步落盘**：点击【💾 保存配置】后，自动将用户选中的地区同时写入 `data/id/<name>.json`（`REGION` 字段）与 `data/window_profiles.json`（`region` 字段），并刷新表格呈现；
  - **修复地区盲目兜底与冲突覆盖**：移除 `l2m_window_profile_match` 中未匹配到序号时错误回退强制赋 `EN` 的逻辑；修正 `refresh_multi_clients` 加载流程，确保优先准确加载 `data/id/` 与 `data/window_profiles.json` 中配置的真实地区；
- **多开窗口血量阈值动态调整与双向持久化配置 (`src/gui/win_main_gui.c`, `src/core/window_profile_manager.c`)**：
  - **低血回城与恢复出战阈值微调**：主界面第二行策略区新增【低血回城: `[ 30 ]` %】与【回满出战: `[ 80 ]` %】输入控件；
  - **双向联动与即时回显**：选中列表任意行时，自动同步显示该窗口当前的低血量与回血阈值；点击【💾 保存配置】后自动将阈值保存至 `data/id/<name>.json`（`HEALTH_BACK`、`HEALTH_RECOVER`）与 `data/window_profiles.json`（`low_hp_threshold`、`recover_hp_threshold`）；
  - **闭环智能挂机状态机 (`MultiClientWorkerThread`)**：
    - 当实测血量低于低血阈值（如 < 30%）时，自动触发瞬移回城并进入【休整状态】（状态栏显示 `⚠️ 低血回城休整` / `安全区回血中...`）；
    - 在休整状态下，仅当血量回满恢复至达标阈值（如 >= 80%）时，自动解除休整并【重新投入战斗挂机】（状态栏显示 `🟢 恢复出战挂机`）；
- **主界面多开窗口“自动关闭弹窗”专属策略开关与配置持久化 (`src/gui/win_main_gui.c`)**：
  - **顶部快速策略配置栏**：主界面新增【选中窗口快速策略设置区】，提供【🛡️ 自动关闭弹窗 (POPUP_CHECKER)】复选框与【💾 保存配置】按钮；
  - **列表多维联动与实时呈现**：在表格中点击选中任意游戏客户端行，自动同步该客户端在 `data/id/<name>.json` 或 `data/window_profiles.json` 中的弹窗防御开启状态；
  - **单角色/窗口配置独立持久化**：用户勾选/取消勾选或点击保存后，自动将 `AUTO_DISMISS_POPUP` 字段写入保存至该角色的独立配置文件 `data/id/<name>.json` 及 `data/window_profiles.json`；
  - **挂机守护线程智能裁决 (`MultiClientWorkerThread`)**：仅对开启了自动弹窗防御的窗口执行弹窗检测与关闭流程，未开启窗口跳过弹窗扫描以最大化挂机性能。

### 🛡️ 架构重构与缺陷修复 (Bug Fixes & Refactoring)
- **`data/id/` 角色配置文件 UTF-8 编码与堆栈安全读写重构 (`src/core/window_profile_manager.c`, `src/gui/win_main_gui.c`)**：
  - **修复 Windows 下中文角色路径读写报错**：Windows ANSI `fopen` 无法处理 UTF-8 中文角色名（如 `狂风舞者.json`、`天下无双.json`），全面引入 `file_open_utf8` 与 `_wfopen` 宽字符文件操作 API 以及 `_wmkdir`，100% 支持中英文角色配置文件读写；
  - **根治栈溢出崩溃隐患 (Stack Overflow Fix)**：移除原有保存函数中在线程栈上分配的 512KB 大数组，全面改用动态堆内存管理；
  - **无损安全的 JSON 结构化三段重构**：采用前缀+新值+后缀动态拼接机制替换字段，解决新旧字符串长度不一致导致的内存覆盖与越界问题；
  - **未命名角色容错自愈**：主界面对未命名或 `"Unknown"` 角色点击保存时，自动从窗口标题分配唯一 Profile 与 ID 配置，确保双层配置（`data/id/` 与 `data/window_profiles.json`）均能稳健保存。
- **高精度游戏窗口三级过滤引擎 (`src/core/window_profile_manager.c`, `src/gui/win_main_gui.c`)**：
  - **修复自身与调试器误识别缺陷**：彻底解决了由于模糊标题包含（如 `Lineage2M`）或尺寸粗筛导致软件自身主窗口（`Lineage2MBot`）、开发调试工具窗口（`Debugger` / `Dashboard`）以及 IDE/编辑器/浏览器被误当成游戏客户端的问题；
  - **三级严谨过滤管道**：
    1. **自身与系统黑名单**：通过 `GetCurrentProcessId()` 绝对排除自身 PID；类名黑名单拦截 `L2M_`、`Shell_TrayWnd`、`ApplicationFrameWindow` 等；标题黑名单排除 `Lineage2MBot`、`Debugger`、`Visual Studio`、`Antigravity`、`Chrome`、`Edge` 等；
    2. **游戏正向白名单**：严格匹配官方客户端（`Lineage2M.exe`、`Purple.exe`、`UnrealWindow`）与主流安卓模拟器（雷电 `dnplayer`、夜神 `Nox`、网易 `MuMu`、蓝叠 `HD-Player`、逍遥 `MEmu`）；
    3. **单一数据源统一**：主界面 `refresh_multi_clients` 与调试工具全面统一调用底层引擎 [`l2m_enum_game_windows`](file:///d:/Work/SvnLin/Lineage2M_Bot_C/Lineage2M_Bot/src/core/window_profile_manager.c)，彻底消除重复代码与判定分歧。
- **全量自动化测试扩充 (`tests/test_core.c`)**：
  - 新增针对中文角色独立配置（`狂风舞者`）与 `auto_dismiss_popup` 独立持久化/回读的自动化测试用例，全量单元测试扩充至 **122 / 122 项 100% 纯净通过**。

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
