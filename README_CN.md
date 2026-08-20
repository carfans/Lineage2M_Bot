# 🎮 Lineage2MBot 纯 C 原生多开监控与调试系统 (100% Pure C Native)

[简体中文](./README_CN.md) | [English](./README.md)

---

**Lineage2MBot** 是专为《天堂 2M》（Lineage 2M）打造的 **100% 纯 C 原生高并发、微秒级响应多开监控工作台与可视化调试系统**。

本项目已完全脱离 Python 环境与 PyQt5 等臃肿依赖，采用 Win32 原生 Unicode API 构建，单文件独立运行（`.exe` 体积仅 ~160 KB），内存占用 < 10 MB，单核 CPU 占用 < 0.1%，支持多游戏窗口并发高频巡检与硬件级键鼠注入。

---

## 🌟 核心特性全景

### 1. 📊 纯 C 原生多开高频监控工作台 (`Lineage2MBot_GUI.exe`)
- **多开客户端实时监控表格 (ListView)**：
  - 自动扫描系统中所有在线的天堂 2M 游戏客户端；
  - 7 列全维度状态实时展示：**游戏窗口句柄 / 分辨率 / 语言地区 / 生命值 (HP%) / 弹窗防御拦截数 / 运行状态 / 最近操作记录**；
  - 表格双击任意客户端行，直达该客户端专属的可视化调试器。
- **微秒级并发后台守护线程 (`MultiClientWorkerThread`)**：
  - 单次血量与弹窗综合视觉决策仅需 **0.3 微秒**，以极低功耗并发轮询所有勾选客户端；
  - 支持多窗口独立弹窗防御（自动勾选“不再显示”并点击关闭）与低血量逃跑保护。
- **🛑 全局安全紧急制动热键 [Ctrl + Q]**：
  - 注册 Windows 操作系统内核级热键（最高优先级响应）；
  - 巡检挂机期间无论焦点在何处、鼠标是否被占用，按下 **`Ctrl + Q`** 瞬间强制切断所有多开巡检，并立即释放物理鼠标控制权，伴随蜂鸣声音告警与日志标红。

---

### 2. 🔬 纯 C 原生可视化交互调试器
- **960x540 高清双缓冲渲染画板**：
  - 采用 GDI 双缓冲与 `HALFTONE` 高清平滑拉伸，无闪烁渲染游戏实时画面；
  - 鼠标在画板任意位置点击或拖动，实时逆映射换算为图像绝对坐标与真实 RGB 色值。
- **🔍 采样点 11x11 像素级放大镜视窗 (`L2M_Zoom_View`)**：
  - 独立的小画板控件，以 **10 倍放大率** 实时展示当前采样点周围 `11x11` 像素的网格色彩分布；
  - 正中心带有黄色方框与红色十字准星，精准定位像素色彩，微调对齐点位一目了然；
  - 与大画板鼠标点击、CBT 特征点下拉选择、实测比对实现**微秒级全方位双向联动**。
- **🌐 多语言 CBT 特征采样点配置管理**：
  - 支持在 `CN` (简体中文) / `EN` (英文) / `JP` (日文) / `RU` (俄文) 之间一键切换；
  - 带**原生垂直滚动条 (`WS_VSCROLL`)** 的特征点下拉列表，支持鼠标滚轮顺畅浏览上百个点位；
  - **【🎯 填入拾取点】**：一键将大画板点击拾取的坐标与 RGB 填入输入框；
  - **【🔬 比对测试】**：实时计算当前画面该点位的实测颜色与色差，诊断输出 `✅ 匹配通过 (色差 <= 容差)`；
  - **【💾 保存特征点】**：直接将修改/新增的点位保存写回对应语言的 JSON 配置文件；
  - **【🗑️ 删除此点位】**：一键从配置文件中移除。
- **弹窗扫描区域诊断与持久化**：
  - 覆盖左上角提示弹窗、中间模态弹窗与全屏活动弹窗；
  - 画面实时绘制黄色扫描框、绿色确认按钮框、红色准星点击中心与青色复选框；
  - **【💾 保存弹窗】**：一键将调整后的扫描区域 ROI `(X, Y, Width, Height)` 写入 JSON 文件的 `popup_scan_config` 节点；
  - **【🖱️ 模拟关闭】**：真实测试“先勾选、后确认”两步动作。
- **截图保存与本地图片离线调试**：
  - **【💾 保存当前截图】**：弹出文件保存对话框，将当前画面无损保存为标准 24 位 BMP 图像；
  - **【📂 载入本地图片】**：支持打开本地历史截图，在离线状态下执行鼠标取点、弹窗诊断、血条计算与 CBT 特征点比对。

---

### 3. 🧠 计算机视觉与游戏状态分析引擎
- **弹窗防御与按钮识别引擎 ([`src/game/popup_engine.c`])**：
  - **暗色背景先验校验**：统计暗灰/暗蓝像素占比（阈值 45%）与平均亮度（< 90），在野外正常游戏场景下直接拦截误报；
  - **按钮色彩与连通域轮廓分析**：精准识别橙色跳转/确认按钮（RGB [220, 115, 10]）与灰色关闭按钮（RGB [80, 85, 90]）；
  - **“不再显示”复选框探测**：智能扫描方框角点并定位勾选中心。
- **血条识别引擎 ([`src/game/hp_engine.c`])**：
  - 双重红色/橙色容差掩码横向扫描端点，精确计算当前生命值百分比。
- **底层图像算子库 ([`src/vision/`])**：
  - 纯 C 色彩空间转换、多核色彩掩码提取、二值形态学膨胀/腐蚀/闭运算、两遍扫描连通域轮廓查找与外接矩形拟合。

---

### 4. 🎮 平台驱动与 DirectInput 硬件级输入穿透 ([`src/platform/`])
- **DirectX 显存无黑屏截屏管线 (`win_capture.c`)**：
  - 主策略：`ClientToScreen` 换算绝对物理屏幕坐标 + 全局屏幕 DC `BitBlt(SRCCOPY | CAPTUREBLT)`，100% 稳定截取 DirectX 11/12 / GPU SwapChain 硬件加速画面；
  - 回退策略：结合全黑自适应检测，自动秒级回退至 `PrintWindow(PW_RENDERFULLCONTENT)`。
- **DirectInput 硬件扫描码回家与物理鼠标驱动 (`win_input.c`)**：
  - **强力窗口激活 (`l2m_force_activate_window`)**：使用 `AttachThreadInput` 附加输入队列 + `SetWindowPos(HWND_TOPMOST)` + `SetForegroundWindow` 强制夺取前台焦点；
  - **回家快捷键模拟**：发送**键盘硬件扫描码 0x0B (数字键 '0')**，穿透 DirectInput / RawInput 限制；
  - **物理级鼠标注入**：`SetCursorPos` 驱动屏幕物理光标跳动 + `SendInput(INPUT_MOUSE)` 硬件级点击，并辅以后台消息流双重触发；
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
