#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <iostream>
#include <fstream>
#include "ClipboardManager.h"
#include "Storage.h"
// #include "UIManager.h"  // 暂时注释UI管理器
using namespace std;

// 窗口类名
const wchar_t* CLASS_NAME = L"ClipboardHistoryClass";

// 全局对象
ClipboardManager G_ClipManager;
Storage G_Storage;
// UIManager G_UIManager;  // 暂时注释UI管理器

// 设置参数
int G_RetentionDays = 3;    // 保留天数
int G_MaxRecords = 1000;    // 最大记录数

// 获取exe所在目录
wstring GetExeDir ()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW (NULL, path, MAX_PATH);
    wstring fullPath (path);
    size_t pos = fullPath.find_last_of (L"\\");
    if (pos != wstring::npos)
    {
        return fullPath.substr (0, pos);
    }
    return fullPath;
}

// 绘制卡片
void DrawCard (HDC hdc, int x, int y, int width, const ClipRecord& record)
{
    // 绘制卡片背景
    HBRUSH cardBg = CreateSolidBrush (RGB (255, 255, 255));
    RECT cardRect = { x, y, x + width, y + 80 };
    FillRect (hdc, &cardRect, cardBg);
    DeleteObject (cardBg);

    // 绘制边框
    HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (220, 220, 220));
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + 80);
    DeleteObject (borderPen);

    // 绘制内容预览
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT contentFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, contentFont);

    wstring preview = record.preview;
    if (preview.length () > 50)
    {
        preview = preview.substr (0, 50) + L"...";
    }
    TextOut (hdc, x + 10, y + 10, preview.c_str (), preview.length ());
    DeleteObject (contentFont);

    // 绘制时间
    time_t timestamp = record.timestamp;
    struct tm timeInfo;
    localtime_s (&timeInfo, &timestamp);
    wchar_t timeStr[32];
    wcsftime (timeStr, 32, L"%Y-%m-%d %H:%M", &timeInfo);
    SetTextColor (hdc, RGB (150, 150, 150));
    HFONT timeFont = CreateFont (12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, timeFont);
    TextOut (hdc, x + 10, y + 55, timeStr, wcslen (timeStr));
    DeleteObject (timeFont);
}

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
        vector<ClipRecord>& records = const_cast<vector<ClipRecord>&> (G_ClipManager.GetRecords ());
        G_Storage.DeleteExpiredRecords (records, G_RetentionDays);

        // 保存记录到文件
        G_Storage.SaveRecords (records);

        // 刷新窗口显示
        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hWnd, &ps);

        // 绘制背景
        FillRect (hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW + 1));

        // 绘制标题
        SetTextColor (hdc, RGB (33, 33, 33));
        SetBkMode (hdc, TRANSPARENT);
        HFONT titleFont = CreateFont (18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        SelectObject (hdc, titleFont);
        TextOut (hdc, 20, 15, L"历史剪贴板管理器", 8);
        DeleteObject (titleFont);

        // 绘制记录数量
        HFONT countFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        SelectObject (hdc, countFont);
        wstring info = L"共 " + to_wstring (G_ClipManager.GetRecordCount ()) + L" 条记录";
        TextOut (hdc, 20, 50, info.c_str (), info.length ());
        DeleteObject (countFont);

        // 绘制分割线
        HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
        SelectObject (hdc, linePen);
        MoveToEx (hdc, 20, 75, NULL);
        LineTo (hdc, 780, 75);
        DeleteObject (linePen);

        // 绘制历史记录列表
        const vector<ClipRecord>& records = G_ClipManager.GetRecords ();
        int cardY = 85;
        int cardWidth = 760;

        for (int i = 0; i < (int)records.size () && i < 6; i++)
        {
            // 检查是否超出可视区域
            if (cardY + 90 > 580)
            {
                break;
            }

            // 绘制卡片
            DrawCard (hdc, 20, cardY, cardWidth, records[i]);
            cardY += 90;
        }

        // 如果没有记录，显示提示
        if (records.empty ())
        {
            HFONT hintFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, hintFont);
            SetTextColor (hdc, RGB (150, 150, 150));
            TextOut (hdc, 300, 250, L"暂无历史记录，请复制内容测试", 14);
            DeleteObject (hintFont);
        }

        EndPaint (hWnd, &ps);
        return 0;
    }
    }
    return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

// 主函数
int main ()
{
    // 获取exe所在目录并切换到该目录
    wstring exeDir = GetExeDir ();
    SetCurrentDirectoryW (exeDir.c_str ());

    // 重定向控制台输出（调试用）
    AllocConsole ();
    freopen ("CONOUT$", "w", stdout);

    wcout << L"历史剪贴板管理器启动中..." << endl;
    wcout << L"程序目录: " << exeDir << endl;

    // 初始化存储系统
    G_Storage.SetRootDir (exeDir);
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

    // 显示窗口
    ShowWindow (hWnd, SW_SHOW);
    UpdateWindow (hWnd);
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
