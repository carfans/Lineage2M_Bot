# Lineage2M_Bot
Lineage2M Multi-Client Monitor &amp; Visual Debugger

[English](./README.md) | [简体中文](./README_CN.md) | [Changelog](./CHANGELOG.md)

---

**Lineage2MBot** is a **100% pure C native, high-concurrency, microsecond-response multi-client monitoring workbench and visual debugging system** tailored specifically for *Lineage 2M*.

This project is completely decoupled from Python runtimes and bloated dependencies such as PyQt5. Built directly on native Win32 Unicode APIs, it operates as a single standalone executable (`.exe` size only ~160 KB), uses < 10 MB RAM, consumes < 0.1% of a single CPU core, and supports concurrent high-frequency multi-window inspection and hardware-level mouse/keyboard injection.

---

## 🌟 Core Features Overview

### 1. 📊 Pure C Native Multi-Client High-Frequency Monitoring Workbench (`Lineage2MBot_GUI.exe`)
- **Real-Time Multi-Client Monitoring Table (`ListView`)**:
  - Automatically discovers and lists all active Lineage 2M game windows across the operating system;
  - 7 full-dimension columns updated in real-time: **Game Window HWND / Resolution / Region / HP (%) / Auto Popup Defense / Running Status / Recent Action Log**;
  - Double-click any client row to instantly launch its dedicated visual debugger.
- **🖥️ Multi-Monitor Detection & Cross-Display Smart Grid Alignment**:
  - Dynamically enumerates all connected physical displays (primary, secondary screens, workspace bounds, and heterogeneous resolutions);
  - Integrates a 【🖥️ Monitor】 dropdown and 【🪟 4-Grid Align】 button on both the main dashboard and debugger to arrange game windows in a seamless 2x2 grid on any selected display.
- **🌐 Region Selection & Dual-Layer Bidirectional Persistence**:
  - Supports live dropdown switching between `CN`, `TW`, `EN`, `JP`, `KR`, and `RU`;
  - Clicking 【💾 Save Config】 immediately writes region, popup defense flags, low HP thresholds, and recover HP thresholds into both `data/id/<name>.json` and `data/window_profiles.json`.
- **🩸 30ms Ultra-High-Frequency HP Monitoring & Closed-Loop Combat State Machine**:
  - **Microsecond ROI Local Capture (`l2m_capture_window_roi`)** + **Zero-Copy BGR Instant Calculation** achieving ~33 FPS continuous monitoring;
  - **Closed-Loop Combat/Rest State Machine**: When HP drops below threshold (e.g. < 30%), triggers instant teleport home and enters resting state; automatically resumes combat once HP recovers (e.g. >= 80%);
  - **Frequency-Divided Popup Inspection**: Full-screen popup detection and CBT loading are decoupled to run once every second (~30 cycles), minimizing CPU consumption.
- **🛑 Global Emergency Stop Hotkey [Ctrl + Q]**:
  - Registers a Windows OS kernel-level global hotkey with top-tier priority;
  - Instantly terminates all background multi-client inspections and releases physical mouse/keyboard control from anywhere.

---

### 2. 🔬 Pure C Native Visual Interactive Debugger
- **960x540 HD Double-Buffered Rendering Canvas**:
  - Leverages GDI double buffering and `HALFTONE` smooth interpolation for flicker-free real-time game stream rendering;
  - Clicking or dragging anywhere on the canvas instantly calculates inverse coordinate mapping to absolute image coordinates and real-time RGB values.
- **🩸 Visual HP Bar Debugging, One-Click Color Picking & CBT Persistence**:
  - Dedicated HP Bar Parameter Panel: fine-tune offset `(offset_x, offset_y)`, physical width `width`, height `height`, primary color/tolerance, and secondary color/tolerance;
  - **One-Click Color Picking**: Populates primary/secondary target color inputs directly from sampled canvas/magnifier pixels;
  - **Real-Time HUD Overlay**: Renders cyan bounding box, bright red effective HP progress bar, and yellow vertical endpoint ruler;
  - **Multi-Language CBT Sync**: Saves calibrated parameters directly into `data/cbt/<REGION>.json` under `"hp_bar_config"`.
- **🔍 11x11 Pixel-Level Magnifier Preview (`L2M_Zoom_View`)**:
  - An independent dedicated canvas rendering the **11x11 pixel grid** surrounding the current sample point at **10x magnification**;
  - Features a yellow boundary box and a red center crosshair for pinpoint pixel precision;
  - Microsecond two-way synchronization with canvas clicks, CBT dropdown selections, and live color comparisons.
- **🌐 Multi-Language CBT Feature Sampling Point Manager**:
  - Supports one-click switching between `CN`, `EN`, `JP`, and `RU`;
  - Native vertical scrollbar (`WS_VSCROLL`) dropdown list supporting smooth mouse wheel scrolling across hundreds of feature points;
  - Supports point picking, live comparison testing, saving, and deletion.
- **Dynamic Named Popup Management & Multidimensional Diagnostics**:
  - Full support for custom named popup profiles with editable scan ROI, descriptions, linked CBT points, and deletion;
  - Live HUD rendering with yellow scan ROI, green confirm button, red click target, and cyan checkbox;
  - Supports 【🖱️ Test Close】 to simulate check-then-confirm actions.
- **Screenshot Capture & Offline Image Debugging**:
  - Supports exporting lossless 24-bit BMP images or loading local historical screenshots for offline diagnostics.

---

### 3. 🧠 Computer Vision & Pure Configuration-Driven Engine
- **Pure Configuration-Driven HP Analysis Engine ([`src/game/hp_engine.c`])**:
  - **Zero Hardcoded Constants**: Blood bar dimensions, offsets, colors, and tolerances are 100% driven by JSON configuration;
  - **Left-to-Right Continuity Scanning**: Column-wise continuous scanning with 1px anti-aliasing gap tolerance, truncating immediately upon 2 consecutive dark background columns;
  - Pure linear calculation `(continuous_end + 1) * 100 / config->width`, providing instant 100% full-HP response and millisecond drop detection.
- **Popup Defense & Button Recognition Engine ([`src/game/popup_engine.c`])**:
  - **Dark Background Heuristic**: Evaluates dark gray/blue pixel ratios and average luminance to eliminate false positives in normal gameplay scenes;
  - **Button Color & Connected-Component Analysis**: Accurately detects orange confirm/jump buttons and gray close buttons;
  - **"Do Not Show Again" Checkbox Locator**: Intelligent corner detection to locate the exact center for checking.
- **High-Precision 3-Stage Window Filter Pipeline ([`src/core/window_profile_manager.c`])**:
  - Own-PID / Class / Title blacklist + Official client/emulator whitelist + Dimension checks, 100% eliminating self and debugger misidentification.

---

### 4. 🎮 Platform Drivers & DirectInput Hardware-Level Input ([`src/platform/`])
- **DirectX Zero-Blackout Screen Capture Pipeline (`win_capture.c`)**:
  - Primary Strategy: `ClientToScreen` coordinate transformation + global desktop DC `BitBlt(SRCCOPY | CAPTUREBLT)`, ensuring 100% stable capture of DirectX 11/12 and GPU SwapChain hardware-accelerated rendering;
  - Fallback Strategy: Automatic seamless fallback to `PrintWindow(PW_RENDERFULLCONTENT)` when solid black frames are detected.
- **DirectInput Hardware Scan Codes & Adaptive Physical Mouse Driver (`win_input.c`)**:
  - **Force Window Activation (`l2m_force_activate_window`)**: Combines `AttachThreadInput` queue attachment + `SetWindowPos(HWND_TOPMOST)` + `SetForegroundWindow` to reliably bring target windows to the foreground;
  - **Home / Teleport Shortcut Simulation**: Emits **Keyboard Hardware Scan Code 0x0B (Key '0')** to bypass DirectInput and RawInput restrictions;
  - **Window DPI/Resolution Adaptation & Absolute Physical Cursor Injection**:
    - Automatically translates 960x540 reference coordinates to actual client pixel dimensions;
    - Uses normalized 0~65535 absolute coordinates via `SendInput(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE)`;
    - Holds key press for **80ms** across 3D rendering frames, combined with nested child window penetration (`RealChildWindowFromPoint` & `MapWindowPoints`);
  - **Administrator Privilege Detection**: Automatically inspects execution privileges on startup to prevent Windows UIPI isolation from silently intercepting inputs.

---

## 📁 Directory Structure

```text
Lineage2MBot/
├── CMakeLists.txt                 # CMake build configuration
├── build.bat                      # Windows MinGW GCC one-click build batch script
├── build.ps1                      # PowerShell automated build script
├── README.md                      # System architecture & manual (Chinese)
├── README_EN.md                   # System architecture & manual (English)
├── Lineage2MBot_GUI.exe           # 🌟 Pure C native desktop GUI executable
├── Lineage2MBot.dll               # 🌟 Pure C core dynamic link library (Exported C API)
├── Lineage2MBot.exe               # Console CLI & benchmarking utility
├── include/                       # Public C headers
│   ├── l2m_types.h                # Basic data types (RGB, Point, Rect, ImageBuffer)
│   ├── l2m_vision.h               # Vision operators (Color masks, morphology, contours, BMP I/O)
│   ├── l2m_hp.h                   # HP bar pixel analysis engine interface
│   ├── l2m_popup.h                # Popup detection & background verification engine interface
│   ├── l2m_cbt.h                  # Multi-language CBT sampling points & popup configuration interface
│   ├── l2m_platform.h             # Screen capture, physical mouse, DirectInput hardware key interfaces
│   ├── l2m_gui.h                  # GUI window & debugger interface declarations
│   └── l2m_api.h                  # DLL exported C API declarations
├── src/                           # Pure C source code
│   ├── core/                      # Image buffer, BMP codec, logging & CBT JSON manager
│   │   ├── image_buffer.c         # Memory allocation, RGB/BGR/Gray conversion, BMP codec
│   │   ├── logger.c               # Pure C formatted logging
│   │   └── cbt_manager.c          # Zero-dependency JSON parser/serializer & color delta comparison
│   ├── vision/                    # Computer vision algorithms
│   │   ├── color_mask.c           # High-performance color masking
│   │   ├── morphology.c           # Dilation, erosion, and closing operations
│   │   └── contour.c              # Connected-component contour extraction & bounding analysis
│   ├── game/                      # Game business logic engines
│   │   ├── popup_engine.c         # Popup background validation, button & checkbox detection
│   │   └── hp_engine.c            # HP percentage horizontal scanning & mean sampling
│   ├── platform/                  # Windows platform drivers
│   │   ├── win_capture.c          # Physical screen BitBlt + DirectX hardware-accelerated capture
│   │   └── win_input.c            # Force activation, hardware scan code 0, SendInput physical mouse
│   ├── gui/                       # Pure C Win32 native desktop interface
│   │   ├── win_main_gui.c         # Multi-client monitor workbench (ListView, multi-client scheduler, Ctrl+Q)
│   │   └── win_debug_dialog.c     # Visual debugger (Double-buffered canvas, 11x11 magnifier, CBT manager)
│   ├── api/                       # DLL exported API implementation
│   └── main_app.c                 # Main application entry (wWinMain Unicode message loop)
├── examples/                      # Console benchmark examples
└── python_bindings/               # Python ctypes bridge module
```

---

## 🛠️ Build & Compilation Guide

### Method 1: Run the Automated Build Script (Recommended)
This repository includes automated scripts that automatically locate MinGW GCC:
- **CMD / Batch**: Run directly from the `Lineage2MBot` directory:
  ```cmd
  build.bat
  ```
- **PowerShell**: Run in PowerShell terminal:
  ```powershell
  .\build.ps1
  ```

### Method 2: Manual Compilation via MinGW-w64 GCC
If `gcc` is not in your global `PATH`, use the absolute path to your MinGW GCC (e.g. `C:\ProgramData\mingw64\mingw64\bin\gcc.exe`):
```cmd
"C:\ProgramData\mingw64\mingw64\bin\gcc.exe" -O3 -Wall -std=c99 -DL2M_USE_STATIC -municode -o bin/Lineage2MBot_GUI.exe src/core/*.c src/vision/*.c src/game/*.c src/platform/*.c src/api/*.c src/gui/*.c src/main_app.c -Iinclude -lgdi32 -luser32 -lcomctl32 -lcomdlg32 -mwindows
```

### Method 3: Build via CMake
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

## 🚀 Hotkeys & Quick Reference

| Shortcut / Action | Description |
| :--- | :--- |
| **`Ctrl + Q`** | **[Global Emergency Stop]** Instantly terminates inspection across all multi-client instances and releases physical mouse/keyboard control |
| **Double-Click Row** | Double-click any client row in the monitor table to open its dedicated **Visual Debugger** |
| **Canvas Click** | Click anywhere on the 960x540 canvas to sync coordinates and color to the **11x11 Magnifier** with microsecond latency |
| **Key `0` (Scan code `0x0B`)** | Game default home/teleport shortcut. Automatically activated when clicking **[🏠 Home/Teleport]** |

> **Note**: If your game client (NCSoft PURPLE / Lineage 2M) is running as Administrator, please **run `Lineage2MBot_GUI.exe` as Administrator** to ensure DirectInput scan codes and physical mouse events are not blocked by Windows UIPI privilege isolation.
