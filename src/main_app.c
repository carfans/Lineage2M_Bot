/**
 * @file main_app.c
 * @brief Lineage2MBot 纯 C 原生桌面应用程序主入口 (Unicode 原生版)
 */

#include <windows.h>
#include "../include/l2m_gui.h"
#include "../include/l2m_api.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    /* 1. 初始化 C 核心引擎 */
    l2m_init_engine();

    /* 2. 创建并显示原生主窗口 */
    HWND hWnd = l2m_create_main_window(hInstance, nCmdShow);
    if (!hWnd) {
        MessageBoxW(NULL, L"创建主窗口失败！", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    /* 3. Windows 标准 Unicode 消息循环 */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* 4. 退出清理 */
    l2m_shutdown_engine();
    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)lpCmdLine;
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return wWinMain(GetModuleHandleW(NULL), NULL, GetCommandLineW(), SW_SHOW);
}
