#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <iostream>
#include <fstream>
#include "ClipboardManager.h"
using namespace std;

// 窗口类名
const wchar_t* CLASS_NAME = L"ClipboardHistoryClass";

// 全局剪贴板管理器
ClipboardManager G_ClipManager;

// 创建存储目录
void CreateStorageDirs ()
{
    CreateDirectory (L"clips", NULL);
    CreateDirectory (L"clips/images", NULL);
}

// 窗口过程函数
LRESULT CALLBACK WindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        // 移除剪贴板监听
        RemoveClipboardFormatListener (hWnd);
        PostQuitMessage (0);
        return 0;

    case WM_CLIPBOARDUPDATE:
        // 处理剪贴板更新
        G_ClipManager.OnClipboardUpdate ();
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hWnd, &ps);
        // 绘制背景
        FillRect (hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW + 1));

        // 显示记录数量
        SetTextColor (hdc, RGB (0, 0, 0));
        SetBkMode (hdc, TRANSPARENT);
        wstring info = L"历史记录数量: " + to_wstring (G_ClipManager.GetRecordCount ());
        TextOut (hdc, 20, 20, info.c_str (), info.length ());

        EndPaint (hWnd, &ps);
        return 0;
    }
    }
    return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

// 主函数
int main ()
{
    // 重定向控制台输出（调试用）
    AllocConsole ();
    freopen ("CONOUT$", "w", stdout);

    wcout << L"历史剪贴板管理器启动中..." << endl;

    // 创建存储目录
    CreateStorageDirs ();
    wcout << L"存储目录创建完成" << endl;

    // 获取模块句柄
    HINSTANCE hInstance = GetModuleHandle (NULL);

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    RegisterClass (&wc);

    // 创建窗口
    HWND hWnd = CreateWindowEx (
        0,
        CLASS_NAME,
        L"历史剪贴板管理器",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hWnd == NULL)
    {
        wcout << L"窗口创建失败" << endl;
        return 0;
    }

    // 初始化剪贴板管理器
    if (!G_ClipManager.Initialize (hWnd))
    {
        wcout << L"剪贴板监听初始化失败" << endl;
        return 0;
    }

    // 显示窗口
    ShowWindow (hWnd, SW_SHOW);
    wcout << L"窗口显示成功" << endl;
    wcout << L"请尝试复制文字或图片进行测试..." << endl;

    // 消息循环
    MSG msg = {};
    while (GetMessage (&msg, NULL, 0, 0))
    {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }

    return 0;
}
