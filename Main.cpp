#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <iostream>
#include <fstream>
#include "ClipboardManager.h"
#include "Storage.h"
#include "UIManager.h"
using namespace std;

// 窗口类名
const wchar_t* CLASS_NAME = L"ClipboardHistoryClass";

// 全局对象
ClipboardManager G_ClipManager;
Storage G_Storage;
UIManager G_UIManager;

// 设置参数
int G_RetentionDays = 3;    // 保留天数
int G_MaxRecords = 1000;    // 最大记录数

// 窗口过程函数
LRESULT CALLBACK WindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        // 移除剪贴板监听
        RemoveClipboardFormatListener (hWnd);

        // 保存记录到文件
        G_Storage.SaveRecords (G_ClipManager.GetRecords ());

        PostQuitMessage (0);
        return 0;

    case WM_CLIPBOARDUPDATE:
    {
        // 处理剪贴板更新
        G_ClipManager.OnClipboardUpdate ();

        // 删除过期记录
        G_Storage.DeleteExpiredRecords (
            const_cast<vector<ClipRecord>&> (G_ClipManager.GetRecords ()),
            G_RetentionDays
        );

        // 保存记录到文件
        G_Storage.SaveRecords (G_ClipManager.GetRecords ());

        // 刷新窗口显示
        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hWnd, &ps);

        // 获取客户区
        RECT clientRect;
        GetClientRect (hWnd, &clientRect);

        // 绘制UI
        G_UIManager.OnPaint (hdc, clientRect);

        EndPaint (hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        int x = LOWORD (lParam);
        int y = HIWORD (lParam);
        G_UIManager.OnMouseMove (x, y);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD (lParam);
        int y = HIWORD (lParam);
        G_UIManager.OnLButtonDown (x, y);
        return 0;
    }

    case WM_CHAR:
    {
        wchar_t ch = (wchar_t)wParam;
        G_UIManager.OnChar (ch);
        return 0;
    }

    case WM_KEYDOWN:
    {
        int keyCode = (int)wParam;
        G_UIManager.OnKeyDown (keyCode);
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

    // 初始化存储系统
    G_Storage.Initialize ();

    // 加载历史记录
    vector<ClipRecord> records;
    G_Storage.LoadRecords (records);
    for (const auto& record : records)
    {
        G_ClipManager.AddRecord (record);
    }

    // 加载设置
    G_Storage.LoadSettings (G_RetentionDays, G_MaxRecords);
    wcout << L"保留天数: " << G_RetentionDays << endl;
    wcout << L"最大记录数: " << G_MaxRecords << endl;

    // 删除过期记录
    int deletedCount = G_Storage.DeleteExpiredRecords (records, G_RetentionDays);
    if (deletedCount > 0)
    {
        G_Storage.SaveRecords (records);
    }

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

    // 初始化UI管理器
    G_UIManager.Initialize (hWnd, &G_ClipManager);

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
