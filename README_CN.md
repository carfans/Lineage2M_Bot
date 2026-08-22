# 🎮 Lineage2MBot 纯 C 原生多开监控与调试系统 (100% Pure C Native)

[简体中文](./README_CN.md) | [English](./README.md) | [更新历史](./CHANGELOG.md)

---

**Lineage2MBot** 是专为《天堂 2M》（Lineage 2M）打造的 **100% 纯 C 原生高并发、微秒级响应多开监控工作台与可视化调试系统**。

本项目已完全脱离 Python 环境与 PyQt5 等臃肿依赖，采用 Win32 原生 Unicode API 构建，单文件独立运行（`.exe` 体积仅 ~160 KB），内存占用 < 10 MB，单核 CPU 占用 < 0.1%，支持多游戏窗口并发高频巡检与硬件级键鼠注入。

---

## 🌟 核心特性全景

### 1. 📊 纯 C 原生多开高频监控工作台 (`Lineage2MBot_GUI.exe`)
- **多开客户端实时监控表格 (ListView)**：
  - 自动扫描系统中所有在线的天堂 2M 游戏客户端；
  - 7 列全维度状态实时展示：**游戏窗口句柄 / 分辨率 / 语言地区 / 生命值 (HP%) / 自动弹窗防御 / 运行状态 / 最近操作记录**；
  - 表格双击任意客户端行，直达该客户端专属的可视化调试器。
- **🖥️ 多物理显示器自动识别与跨屏智能网格排版**：
  - 动态枚举系统当前连接的所有物理显示器（主屏、副屏、工作区坐标与异构分辨率）；
  - 主界面与调试窗口均集成【🖥️ 显示器】下拉选择框与【🪟 四开对齐】按钮，支持多开窗口一键 2x2 四宫格无缝排布到任意选定屏幕。
- **🌐 地区切换与双层双向持久化**：
  - 支持在主界面直接下拉切换 `CN` (国服简中) / `TW` (台服繁中) / `EN` (美欧英文) / `JP` (日服) / `KR` (韩服) / `RU` (俄服)；
  - 点击【💾 保存配置】后自动将语言地区、弹窗防御、低血与回满阈值同步写入 `data/id/<name>.json` 与 `data/window_profiles.json`。
- **🩸 30ms 毫秒级极速血量监测与闭环挂机状态机**：
  - **局部微秒级截屏 (`l2m_capture_window_roi`)** + **零拷贝 BGR 极速计算**，血量检测周期低至 30ms (~33 FPS)；
  - **闭环休整与出战状态机**：血量跌破低血阈值（如 < 30%）毫秒级瞬间瞬移回城并进入安全区休整，血量恢复达标（如 >= 80%）自动解除休整重新投入战斗；
  - **分频智能降频**：计算量较大的全屏弹窗检测与 CBT 加载采用每秒分频巡检，极低功耗。
- **🛑 全局安全紧急制动热键 [Ctrl + Q]**：
  - 注册 Windows 操作系统内核级热键（最高优先级响应）；
  - 巡检挂机期间无论焦点在何处、鼠标是否被占用，按下 **`Ctrl + Q`** 瞬间强制切断所有多开巡检，并立即释放物理鼠标控制权。

---

### 2. 🔬 纯 C 原生可视化交互调试器
- **960x540 高清双缓冲渲染画板**：
  - 采用 GDI 双缓冲与 `HALFTONE` 高清平滑拉伸，无闪烁渲染游戏实时画面；
  - 鼠标在画板任意位置点击或拖动，实时逆映射换算为图像绝对坐标与真实 RGB 色值。
- **🩸 血条参数可视化调试、一键色彩拾取与 CBT 持久化**：
  - 专属血条参数编辑面板：支持动态微调血条起点 `(offset_x, offset_y)`、有效物理宽度 `width`、高度 `height`、主色1与容差、辅色2与容差；
  - **一键填入画面拾取色**：可在画板或放大镜中拾取任意像素颜色后一键填入主色/辅色输入框；
  - **画板实时高亮渲染**：点击【🩸 测试血条】后在大画板上实时渲染青色外框、红色有效血量进度条与黄色垂直端点标尺；
  - **多语言 CBT 库持久化**：点击【💾 保存血条】一键将最新参数写入当前选中语言的 `data/cbt/<REGION>.json`。
- **🔍 采样点 11x11 像素级放大镜视窗 (`L2M_Zoom_View`)**：
  - 独立的小画板控件，以 **10 倍放大率** 实时展示当前采样点周围 `11x11` 像素的网格色彩分布；
  - 正中心带有黄色方框与红色十字准星，精准定位像素色彩，微调对齐点位一目了然；
  - 与大画板鼠标点击、CBT 特征点下拉选择、血条起点定位实现**微秒级全方位双向联动**。
- **🌐 多语言 CBT 特征采样点配置管理**：
  - 支持在 `CN` / `EN` / `JP` / `RU` 之间一键切换；
  - 带原生垂直滚动条 (`WS_VSCROLL`) 的特征点下拉列表，支持鼠标滚轮顺畅浏览上百个点位；
  - 支持一键填入拾取点、比对测试、保存特征点与删除点位。
- **动态命名弹窗管理与多维特征诊断**：
  - 全面支持自定义命名弹窗管理，支持自由创建、重命名、编辑描述、设置扫描 ROI、关联 CBT 点位与删除；
  - 画面实时绘制黄色扫描框、绿色确认按钮框、红色准星点击中心与青色复选框；
  - 支持【🖱️ 模拟关闭】真实测试“先勾选不再显示、后确认关闭”两步动作。
- **截图保存与本地图片离线调试**：
  - 支持将当前画面无损保存为 24 位 BMP 图像，或载入本地历史截图离线调试。

---

### 3. 🧠 计算机视觉与纯配置驱动分析引擎
- **纯配置驱动血条采样与百分比计算引擎 ([`src/game/hp_engine.c`])**：
  - **彻底移除代码硬编码死参数**：血条坐标、尺寸、双色及容差 100% 严格由 CBT 配置文件驱动；
  - **连续有效列扫描与抗噪断点容差**：从左至右逐列严格扫描，支持跳过 1 像素抗锯齿微缝，遇到连续深黑底槽即时截断；
  - 物理有效像素宽度直接计算 `(continuous_end + 1) * 100 / config->width`，满血即 100%，掉血毫秒级灵敏响应。
- **弹窗防御与按钮识别引擎 ([`src/game/popup_engine.c`])**：
  - **暗色背景先验校验**：统计暗灰/暗蓝像素占比与平均亮度，在野外正常游戏场景下直接拦截误报；
  - **按钮色彩与连通域轮廓分析**：精准识别橙色确认按钮与灰色关闭按钮；
  - **“不再显示”复选框探测**：智能扫描方框角点并定位勾选中心。
- **高精度游戏窗口三级过滤管道 ([`src/core/window_profile_manager.c`])**：
  - 自身 PID / 类名 / 标题黑名单拦截 + 官方客户端与模拟器白名单匹配 + 尺寸校验，100% 杜绝自身与调试工具误识别。

---

### 4. 🎮 平台驱动与 DirectInput 硬件级输入穿透 ([`src/platform/`])
- **DirectX 显存无黑屏截屏管线 (`win_capture.c`)**：
  - 主策略：`ClientToScreen` 换算绝对物理屏幕坐标 + 全局屏幕 DC `BitBlt(SRCCOPY | CAPTUREBLT)`，100% 稳定截取 DirectX 11/12 / GPU SwapChain 硬件加速画面；
  - 回退策略：结合全黑自适应检测，自动秒级回退至 `PrintWindow(PW_RENDERFULLCONTENT)`。
- **DirectInput 硬件扫描码回家与自适应物理鼠标驱动 (`win_input.c`)**：
  - **强力窗口激活 (`l2m_force_activate_window`)**：使用 `AttachThreadInput` 附加输入队列 + `SetWindowPos(HWND_TOPMOST)` + `SetForegroundWindow` 强制夺取前台焦点；
  - **回家快捷键模拟**：发送**键盘硬件扫描码 0x0B (数字键 '0')**，穿透 DirectInput / RawInput 限制；
  - **窗口分辨率自适应与绝对物理光标注入**：
    - 自动将 960x540 标准参考系换算为真实窗口客户区物理尺寸；
    - 使用标准 0~65535 归一化坐标结合 `SendInput(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE)` 精准移动物理光标；
    - 按下后充足保持 **80ms** 跨越 3D 游戏渲染帧，配合 Direct3D/虚幻引擎内部渲染子窗口坐标穿透；
  - **管理员权限检测**：启动时自动检测运行权限，避免 Windows UIPI 权限隔离静默拦截键鼠输入。

---

## 📁 目录结构

```text
Lineage2MBot/
├── CMakeLists.txt                 # CMake 构建配置文件
├── build.bat                      # Windows MinGW GCC 一键编译构建脚本
├── build.ps1                      # PowerShell 自动化编译脚本
├── README.md                      # 系统架构与使用说明文档
├── Lineage2MBot_GUI.exe           # 🌟 纯 C 原生桌面主程序 (双击直接运行)
├── Lineage2MBot.dll               # 🌟 纯 C 核心动态链接库 (导出 C API)
├── Lineage2MBot.exe               # 控制台命令行与基准测试工具
├── include/                       # 公共 C 头文件
│   ├── l2m_types.h                # 基础数据类型 (RGB, Point, Rect, ImageBuffer)
│   ├── l2m_vision.h               # 视觉算子 (色彩掩码, 形态学, 轮廓, BMP 读写)
│   ├── l2m_hp.h                   # 血条像素分析引擎接口
│   ├── l2m_popup.h                # 弹窗分析与背景确认引擎接口
│   ├── l2m_cbt.h                  # 多语言 CBT 采样点与弹窗配置管理接口
│   ├── l2m_platform.h             # 截屏、物理鼠标、DirectInput 硬件按键接口
│   ├── l2m_gui.h                  # GUI 窗口与调试器接口声明
│   └── l2m_api.h                  # DLL 导出 C API 接口声明
├── src/                           # 纯 C 源代码
│   ├── core/                      # 图像内存缓冲区、BMP 读写、日志与 CBT JSON 管理
│   │   ├── image_buffer.c         # 内存分配、RGB/BGR/Gray 转换、BMP 编解码
│   │   ├── logger.c               # 纯 C 格式化日志输出
│   │   └── cbt_manager.c          # 零依赖 JSON 读写与色差比对
│   ├── vision/                    # 计算机视觉底层算法
│   │   ├── color_mask.c           # 高性能色彩掩码算子
│   │   ├── morphology.c           # 膨胀、腐蚀、闭运算算子
│   │   └── contour.c              # 连通域轮廓查找与几何分析
│   ├── game/                      # 游戏业务引擎
│   │   ├── popup_engine.c         # 弹窗背景校验、按钮识别与复选框探测
│   │   └── hp_engine.c            # 血条百分比端点扫描与均值采样
│   ├── platform/                  # Windows 平台层驱动
│   │   ├── win_capture.c          # 物理屏幕 BitBlt + DirectX 硬件加速截屏
│   │   └── win_input.c            # 强力置顶、硬件扫描码 0、SendInput 物理鼠标
│   ├── gui/                       # 纯 C Win32 原生桌面端界面
│   │   ├── win_main_gui.c         # 多开监控工作台 (ListView、多开并发调度、Ctrl+Q 制动)
│   │   └── win_debug_dialog.c     # 可视化调试器 (双缓冲画板、11x11 放大镜、CBT 管理)
│   ├── api/                       # DLL 导出接口实现
│   └── main_app.c                 # 应用程序主入口 (wWinMain Unicode 消息循环)
├── examples/                      # 控制台基准测试示例
└── python_bindings/               # Python ctypes 桥接模块
```

---

## 🛠️ 编译与构建指南

### 方式 1：直接运行一键编译脚本（推荐）
本项目自带自动查找 MinGW GCC 路径的编译脚本：
- **CMD / 批处理**：直接在 `Lineage2MBot` 目录下双击或在终端运行：
  ```cmd
  build.bat
  ```
- **PowerShell**：在终端运行：
  ```powershell
  .\build.ps1
  ```

### 方式 2：使用 MinGW-w64 GCC 手动编译
如果系统中尚未将 `gcc` 加入全局 `PATH` 环境变量，请使用本机的 MinGW 绝对路径（例如 `C:\ProgramData\mingw64\mingw64\bin\gcc.exe`）：
```cmd
"C:\ProgramData\mingw64\mingw64\bin\gcc.exe" -O3 -Wall -std=c99 -DL2M_USE_STATIC -municode -o bin/Lineage2MBot_GUI.exe src/core/*.c src/vision/*.c src/game/*.c src/platform/*.c src/api/*.c src/gui/*.c src/main_app.c -Iinclude -lgdi32 -luser32 -lcomctl32 -lcomdlg32 -mwindows
```

### 方式 3：使用 CMake 构建
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

## 🚀 快捷键与使用说明

| 快捷键 / 操作 | 功能描述 |
| :--- | :--- |
| **`Ctrl + Q`** | **【全局安全紧急制动】** 瞬间停止所有多开窗口的巡检挂机，并立即释放物理鼠标控制权 |
| **双击列表行** | 在多开监控工作台中双击任意客户端行，即可直接打开该窗口专属的**可视化调试器** |
| **大画板点击** | 在调试窗口的 960x540 画板上点击任意位置，左侧 **11x11 像素放大镜** 会微秒级同步放大呈现 |
| **`0` 键 (扫描码 0x0B)** | 游戏默认回家快捷键，点击【🏠 一键瞬移/回城】时会自动强力激活游戏窗口并模拟按下 |

> **提示**：若游戏客户端（NCSoft PURPLE / 天堂 2M）是以管理员身份运行的，请**右键以管理员身份运行 `Lineage2MBot_GUI.exe`**，以确保 DirectInput 硬件扫描码与物理鼠标操作不受 Windows UIPI 权限隔离拦截。
