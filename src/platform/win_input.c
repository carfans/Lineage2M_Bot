/**
 * @file win_input.c
 * @brief Windows 平台高可靠键鼠模拟输入、管理员提权与硬件级按键实现 (DirectInput 硬件扫描码 + 物理鼠标 + 强力窗口激活)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/l2m_platform.h"

#ifdef _WIN32

/* 检查是否为 Administrator 管理员运行 */
bool l2m_is_run_as_admin(void) {
    BOOL is_admin = FALSE;
    PSID admin_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_group)) {
        CheckTokenMembership(NULL, admin_group, &is_admin);
        FreeSid(admin_group);
    }
    return (is_admin == TRUE);
}

/* 若无管理员权限，自动以管理员身份重启提权 */
bool l2m_elevate_to_admin_if_needed(void) {
    if (l2m_is_run_as_admin()) {
        return false; /* 已经是管理员，无需提权 */
    }

    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFOW sei;
        memset(&sei, 0, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas"; /* 触发 UAC 提权提示 */
        sei.lpFile = szPath;
        sei.nShow = SW_NORMAL;

        if (ShellExecuteExW(&sei)) {
            ExitProcess(0); /* 退出当前普通用户进程，交由管理员进程接管 */
            return true;
        }
    }
    return false;
}

/* 强力激活目标游戏窗口并切换至前台获得输入焦点 */
void l2m_force_activate_window(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;

    /* 若最小化则恢复 */
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
        Sleep(50);
    }

    HWND hCurFore = GetForegroundWindow();
    DWORD curThread = GetCurrentThreadId();
    DWORD foreThread = GetWindowThreadProcessId(hCurFore, NULL);
    DWORD targetThread = GetWindowThreadProcessId(hwnd, NULL);

    /* 附加输入队列，突破 Windows 前台焦点限制 */
    if (curThread != targetThread) AttachThreadInput(curThread, targetThread, TRUE);
    if (foreThread != 0 && foreThread != targetThread) AttachThreadInput(foreThread, targetThread, TRUE);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    if (curThread != targetThread) AttachThreadInput(curThread, targetThread, FALSE);
    if (foreThread != 0 && foreThread != targetThread) AttachThreadInput(foreThread, targetThread, FALSE);

    Sleep(80);
}

/* 向目标游戏窗口发送硬件级扫描码按键 (DirectInput / RawInput 兼容) */
bool l2m_send_key_press(HWND hwnd, WORD vk_code) {
    if (hwnd && IsWindow(hwnd)) {
        l2m_force_activate_window(hwnd);
    }

    /* 1. 获取键盘标准硬件扫描码 (如 '0' -> 0x0B) */
    WORD scan_code = (WORD)MapVirtualKeyW(vk_code, MAPVK_VK_TO_VSC);
    if (scan_code == 0) {
        if (vk_code == '0') scan_code = 0x0B;
        else if (vk_code == '1') scan_code = 0x02;
        else if (vk_code == '8') scan_code = 0x09;
        else scan_code = vk_code;
    }

    /* 2. 第一重保障：SendInput 硬件扫描码注入 */
    INPUT inputs[2];
    memset(inputs, 0, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = 0;
    inputs[0].ki.wScan = scan_code;
    inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 0;
    inputs[1].ki.wScan = scan_code;
    inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

    SendInput(1, &inputs[0], sizeof(INPUT));
    Sleep(50);
    SendInput(1, &inputs[1], sizeof(INPUT));

    /* 3. 第二重保障：keybd_event 硬件级辅助 */
    keybd_event(0, (BYTE)scan_code, KEYEVENTF_SCANCODE, 0);
    Sleep(40);
    keybd_event(0, (BYTE)scan_code, KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0);

    /* 4. 第三重保障：HWND 消息直发 */
    if (hwnd && IsWindow(hwnd)) {
        LPARAM lpDown = 0x00000001 | ((LPARAM)scan_code << 16);
        LPARAM lpUp = 0xC0000001 | ((LPARAM)scan_code << 16);
        PostMessageW(hwnd, WM_KEYDOWN, vk_code, lpDown);
        Sleep(40);
        PostMessageW(hwnd, WM_KEYUP, vk_code, lpUp);
    }

    return true;
}

/* 物理级鼠标移动与硬件点击 (支持预激活防吞噬 + 原地双重真实按压) */
static void execute_physical_mouse_click(int abs_x, int abs_y) {
    int v_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int v_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int v_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int v_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (v_w <= 0) v_w = GetSystemMetrics(SM_CXSCREEN);
    if (v_h <= 0) v_h = GetSystemMetrics(SM_CYSCREEN);

    /* 转换为 0~65535 虚拟桌面绝对归一化坐标 */
    DWORD norm_x = (DWORD)(((int64_t)(abs_x - v_left) * 65536) / v_w);
    DWORD norm_y = (DWORD)(((int64_t)(abs_y - v_top) * 65536) / v_h);

    /* 1. 先用系统 API 物理移至该像素坐标 */
    SetCursorPos(abs_x, abs_y);

    /* 2. 再用 SendInput + mouse_event 注入绝对移动事件，触发系统与游戏的光标更新 */
    INPUT input_move;
    memset(&input_move, 0, sizeof(INPUT));
    input_move.type = INPUT_MOUSE;
    input_move.mi.dx = norm_x;
    input_move.mi.dy = norm_y;
    input_move.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &input_move, sizeof(INPUT));
    mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, norm_x, norm_y, 0, 0);

    /* 3. 悬停停顿 (80ms)：确保 Direct3D/虚幻引擎渲染线程将光标 HitTest 判定更新为按钮 Hover/聚焦状态 */
    Sleep(80);

    INPUT input_down;
    memset(&input_down, 0, sizeof(INPUT));
    input_down.type = INPUT_MOUSE;
    input_down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    INPUT input_up;
    memset(&input_up, 0, sizeof(INPUT));
    input_up.type = INPUT_MOUSE;
    input_up.mi.dwFlags = MOUSEEVENTF_LEFTUP;

    /* 4. 第一击：预激活击穿（短按 40ms 抬起，消耗 Windows 激活吞噬 WM_MOUSEACTIVATE 机制） */
    SendInput(1, &input_down, sizeof(INPUT));
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(40);
    SendInput(1, &input_up, sizeof(INPUT));
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

    /* 5. 焦点过渡微停顿 (60ms) */
    Sleep(60);

    /* 6. 第二击：真实物理按压（原地充足保持 130ms，100% 触发按钮的按下动画与 Click 业务逻辑） */
    SendInput(1, &input_down, sizeof(INPUT));
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(130);
    SendInput(1, &input_up, sizeof(INPUT));
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

    /* 7. 抬起后恢复短暂停顿 */
    Sleep(50);
}

bool l2m_post_mouse_click(HWND hwnd, int32_t client_x, int32_t client_y) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    /* 1. 强力激活并置前目标游戏窗口，并给予 100ms 建立输入焦点 */
    l2m_force_activate_window(hwnd);
    Sleep(100);

    /* 2. 自适应换算客户区物理像素 (将 960x540 标准参考系自适应映射到真实窗口客户区) */
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int real_w = rcClient.right - rcClient.left;
    int real_h = rcClient.bottom - rcClient.top;

    int actual_x = client_x;
    int actual_y = client_y;
    if (real_w > 0 && real_h > 0 && (real_w != 960 || real_h != 540)) {
        actual_x = (int)(((int64_t)client_x * real_w) / 960);
        actual_y = (int)(((int64_t)client_y * real_h) / 540);
    }

    /* 3. 转换为桌面绝对物理屏幕坐标 */
    POINT screen_pt = {actual_x, actual_y};
    ClientToScreen(hwnd, &screen_pt);

    /* 4. 探测坐标处接收输入的实际子窗口并置焦 */
    POINT client_pt = {actual_x, actual_y};
    HWND hTarget = RealChildWindowFromPoint(hwnd, client_pt);
    if (!hTarget || !IsWindow(hTarget)) {
        hTarget = ChildWindowFromPoint(hwnd, client_pt);
    }
    if (!hTarget || !IsWindow(hTarget)) {
        hTarget = hwnd;
    }

    if (hTarget && IsWindow(hTarget)) {
        SetFocus(hTarget);
        SetActiveWindow(hTarget);
    }

    /* 计算相对于目标子窗口客户区的相对坐标 */
    POINT child_pt = {actual_x, actual_y};
    if (hTarget != hwnd) {
        MapWindowPoints(hwnd, hTarget, &child_pt, 1);
    }
    LPARAM lparam_target = MAKELPARAM((WORD)child_pt.x, (WORD)child_pt.y);

    /* 5. 同步发送一次子窗口鼠标移动通知 */
    SendMessageW(hTarget, WM_SETCURSOR, (WPARAM)hTarget, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    SendMessageW(hTarget, WM_MOUSEMOVE, 0, lparam_target);

    /* 6. 执行强大的双重确认物理硬件单击 (预激活防吞噬 + 原地真实长按) */
    execute_physical_mouse_click(screen_pt.x, screen_pt.y);

    /* 7. 后续异步补发一次消息确认 (双重保障) */
    PostMessageW(hTarget, WM_LBUTTONDOWN, MK_LBUTTON, lparam_target);
    Sleep(50);
    PostMessageW(hTarget, WM_LBUTTONUP, 0, lparam_target);

    return true;
}

bool l2m_post_popup_dismiss_flow(
    HWND hwnd,
    int32_t cb_x, int32_t cb_y,
    int32_t btn_x, int32_t btn_y,
    uint32_t delay_ms
) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    /* 激活窗口确保输入生效 */
    l2m_force_activate_window(hwnd);
    Sleep(120);

    /* 1. 第一步：点击“不再显示”复选框 */
    if (cb_x > 0 && cb_y > 0) {
        l2m_post_mouse_click(hwnd, cb_x, cb_y);
        Sleep(delay_ms >= 300 ? delay_ms : 380);
    }

    /* 2. 第二步：点击确认/关闭按钮 */
    if (btn_x > 0 && btn_y > 0) {
        l2m_post_mouse_click(hwnd, btn_x, btn_y);
    }

    return true;
}

bool l2m_execute_teleport_home(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    /* 1. 强力切换并激活目标游戏窗口至前台 */
    l2m_force_activate_window(hwnd);
    Sleep(120);

    /* 2. 核心操作：模拟按下快捷键 '0' (回家快捷键，采用 DirectInput 硬件扫描码 0x0B) */
    l2m_send_key_press(hwnd, '0');
    Sleep(100);

    /* 3. 辅助操作：物理鼠标点击普通战斗模式回城卷轴坐标 (217, 487) */
    l2m_post_mouse_click(hwnd, 217, 487);
    Sleep(100);

    /* 4. 辅助操作：物理鼠标点击节能黑屏模式回城卷轴坐标 (910, 436) */
    l2m_post_mouse_click(hwnd, 910, 436);
    Sleep(80);

    /* 5. 再次补发快捷键 '0' 确保 100% 触发回城 */
    l2m_send_key_press(hwnd, '0');

    return true;
}

#endif
