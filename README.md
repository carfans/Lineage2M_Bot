# Lineage2M_Bot
Lineage2M Multi-Client Monitor &amp; Visual Debugger

[English](./README.md) | [简体中文](./README_CN.md)

---

**Lineage2MBot** is a **100% pure C native, high-concurrency, microsecond-response multi-client monitoring workbench and visual debugging system** tailored specifically for *Lineage 2M*.

This project is completely decoupled from Python runtimes and bloated dependencies such as PyQt5. Built directly on native Win32 Unicode APIs, it operates as a single standalone executable (`.exe` size only ~160 KB), uses < 10 MB RAM, consumes < 0.1% of a single CPU core, and supports concurrent high-frequency multi-window inspection and hardware-level mouse/keyboard injection.

---

## 🌟 Core Features Overview

### 1. 📊 Pure C Native Multi-Client High-Frequency Monitoring Workbench (`Lineage2MBot_GUI.exe`)
- **Real-Time Multi-Client Monitoring Table (`ListView`)**:
  - Automatically discovers and lists all active Lineage 2M game windows across the operating system;
  - 7 full-dimension columns updated in real-time: **Game Window HWND / Resolution / Language & Region / HP (%) / Popup Defense Interceptions / Running Status / Recent Action Log**;
  - Double-click any client row to instantly launch its dedicated visual debugger.
- **Microsecond Concurrent Background Worker Thread (`MultiClientWorkerThread`)**:
  - A comprehensive visual decision cycle (HP detection + popup defense) executes in only **0.3 microseconds**;
  - Concurrently polls all checked client instances with ultra-low CPU power consumption;
  - Supports independent popup defense (automatically checks "Do not show again" and clicks close) and low-HP emergency escape protection.
- **🛑 Global Emergency Stop Hotkey [Ctrl + Q]**:
  - Registers a Windows OS kernel-level global hotkey with top-tier priority;
  - Instantly terminates all background multi-client inspections and releases physical mouse/keyboard control from anywhere, accompanied by an audible warning beep and highlighted red error logs.

---

### 2. 🔬 Pure C Native Visual Interactive Debugger
- **960x540 HD Double-Buffered Rendering Canvas**:
  - Leverages GDI double buffering and `HALFTONE` smooth interpolation for flicker-free real-time game stream rendering;
  - Clicking or dragging anywhere on the canvas instantly calculates inverse coordinate mapping to absolute image coordinates and real-time RGB values.
- **🔍 11x11 Pixel-Level Magnifier Preview (`L2M_Zoom_View`)**:
  - An independent dedicated canvas rendering the **11x11 pixel grid** surrounding the current sample point at **10x magnification**;
  - Features a yellow boundary box and a red center crosshair for pinpoint pixel precision;
  - Microsecond two-way synchronization with canvas clicks, CBT dropdown selections, and live color comparisons.
- **🌐 Multi-Language CBT Feature Sampling Point Manager**:
  - Supports one-click switching between `CN` (Simplified Chinese), `EN` (English), `JP` (Japanese), and `RU` (Russian);
  - Native vertical scrollbar (`WS_VSCROLL`) dropdown list supporting smooth mouse wheel scrolling across hundreds of feature points;
  - **[🎯 Pick Point]**: Populates input fields with coordinates and RGB values picked from the main canvas;
  - **[🔬 Compare Test]**: Calculates real-time color delta against the live frame with diagnostic output `✅ Match Passed (Delta <= Tolerance)`;
  - **[💾 Save Feature Point]**: Persists modified or newly added sample points directly into the corresponding language JSON configuration;
  - **[🗑️ Delete Point]**: Removes the selected feature point from the configuration file.
- **Dynamic Named Popup Management & Multidimensional Diagnostics**:
  - Full support for **Custom Named Popup Profiles** (not limited to fixed 3 categories), allowing users to create, rename, edit descriptions, and delete any popup profile (e.g., top-left tips, resurrection confirms, event banners);
  - **Deep CBT Sampling Point Linking**: Enables popups to link specific CBT feature points, verifying real-time pixel RGB differences alongside geometric bounds;
  - Real-time HUD rendering: yellow scan ROI box, green confirmation button bounds, red click crosshair, and cyan checkbox bounds;
  - **[💾 Save / Update]**: Supports one-click creation of new popup keys or updating existing ROI, descriptions, and linked CBTs back into JSON;
  - **[➕ New] / [🗑️ Delete]**: Supports instant input reset and permanent deletion of popup profiles;
  - **[🖱️ Test Close]**: Simulates the two-step action: "Check box first, then click Confirm/Close".
- **Screenshot Capture & Offline Image Debugging**:
  - **[💾 Save Screenshot]**: Prompts a save dialog to export the current frame as a lossless 24-bit BMP image;
  - **[📂 Load Image]**: Opens local historical screenshots for offline coordinate picking, popup diagnostics, HP calculation, and CBT feature point comparisons.

---

### 3. 🧠 Computer Vision & Game State Analysis Engine
- **Popup Defense & Button Recognition Engine ([`src/game/popup_engine.c`])**:
  - **Dark Background Heuristic**: Evaluates dark gray/blue pixel ratios (threshold >= 45%) and average luminance (< 90) to eliminate false positives in normal gameplay scenes;
  - **Button Color & Connected-Component Analysis**: Accurately detects orange confirm/jump buttons (RGB `[220, 115, 10]`) and gray close buttons (RGB `[80, 85, 90]`);
  - **"Do Not Show Again" Checkbox Locator**: Intelligent corner detection to locate the exact center for checking;
  - **Full Popup Automated Inspection**: `l2m_detect_all_popups` iterates across all active named popup profiles across the screen.
- **HP Bar Analysis Engine ([`src/game/hp_engine.c`])**:
  - Dual red/orange tolerance mask horizontal boundary scanning for exact HP percentage calculation.
- **Low-Level Vision Operator Library ([`src/vision/`])**:
  - Pure C color space conversions, multi-core color mask extraction, binary morphological operations (dilation, erosion, closing), two-pass connected-component labeling, and bounding box fitting.

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
