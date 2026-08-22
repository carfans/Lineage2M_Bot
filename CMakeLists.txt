cmake_minimum_required(VERSION 3.10)
project(Lineage2MBot C)

if(MSVC)
    # 强制 MSVC 使用 UTF-8 读取源码
    add_compile_options(/utf-8)
    
    # 屏蔽不安全函数的警告
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    
    # 【新增这一行】：强制 Windows API 使用 Unicode (宽字符) 版本
    add_compile_definitions(UNICODE _UNICODE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(CORE_SOURCES
    src/core/image_buffer.c
    src/core/logger.c
    src/core/cbt_manager.c
    src/core/window_profile_manager.c
    src/vision/color_mask.c
    src/vision/morphology.c
    src/vision/contour.c
    src/game/hp_engine.c
    src/game/popup_engine.c
    src/game/map_zone_engine.c
    src/platform/win_capture.c
    src/platform/win_input.c
    src/api/l2m_api.c
)

set(GUI_SOURCES
    src/gui/win_debug_dialog.c
    src/gui/win_main_gui.c
    src/main_app.c
)


# 1. 编译动态链接库 Lineage2MBot.dll
add_library(Lineage2MBot SHARED ${CORE_SOURCES})
target_compile_definitions(Lineage2MBot PRIVATE L2M_BUILD_DLL)
if(WIN32)
    target_link_libraries(Lineage2MBot gdi32 user32 psapi)
endif()

# 2. 编译纯 C 独立桌面应用程序 Lineage2MBot_GUI.exe
add_executable(Lineage2MBot_GUI WIN32 ${CORE_SOURCES} ${GUI_SOURCES})
target_compile_definitions(Lineage2MBot_GUI PRIVATE L2M_USE_STATIC)
if(WIN32)
    target_link_libraries(Lineage2MBot_GUI gdi32 user32 comctl32 comdlg32 psapi)
endif()

