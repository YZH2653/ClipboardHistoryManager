#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <iostream>
#include <fstream>
#include <algorithm>
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

// 界面状态
wstring G_SearchText;       // 搜索文本
int G_ScrollOffset = 0;     // 滚动偏移量
int G_HoverIndex = -1;      // 鼠标悬停的卡片索引
bool G_InternalCopy = false;  // 程序内部复制标记

// 布局常量
const int SEARCH_BOX_Y = 45;
const int SEARCH_BOX_HEIGHT = 35;
const int CARD_START_Y = 90;
const int CARD_HEIGHT = 80;
const int CARD_MARGIN = 8;
const int CARD_PADDING = 15;

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

// 获取筛选后的记录（置顶优先，时间倒序）
vector<ClipRecord> GetFilteredRecords ()
{
    vector<ClipRecord> result;
    const vector<ClipRecord>& allRecords = G_ClipManager.GetRecords ();

    for (const auto& record : allRecords)
    {
        // 搜索过滤
        if (!G_SearchText.empty ())
        {
            if (record.preview.find (G_SearchText) == wstring::npos &&
                record.content.find (G_SearchText) == wstring::npos)
            {
                continue;
            }
        }
        result.push_back (record);
    }

    // 排序：置顶优先，然后按时间倒序
    sort (result.begin (), result.end (), [] (const ClipRecord& a, const ClipRecord& b)
    {
        if (a.isPinned != b.isPinned)
        {
            return a.isPinned > b.isPinned;
        }
        return a.timestamp > b.timestamp;
    });

    return result;
}

// 绘制搜索框
void DrawSearchBox (HDC hdc, int x, int y, int width)
{
    // 绘制背景
    HBRUSH bgBrush = CreateSolidBrush (RGB (245, 245, 245));
    RECT bgRect = { x, y, x + width, y + SEARCH_BOX_HEIGHT };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制边框
    HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + SEARCH_BOX_HEIGHT);
    DeleteObject (borderPen);

    // 绘制搜索图标
    SetTextColor (hdc, RGB (150, 150, 150));
    SetBkMode (hdc, TRANSPARENT);
    HFONT iconFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
    SelectObject (hdc, iconFont);
    TextOut (hdc, x + 10, y + 8, L"🔍", 1);
    DeleteObject (iconFont);

    // 绘制输入文本
    HFONT inputFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, inputFont);

    if (G_SearchText.empty ())
    {
        SetTextColor (hdc, RGB (180, 180, 180));
        TextOut (hdc, x + 35, y + 9, L"搜索历史记录...", 7);
    }
    else
    {
        SetTextColor (hdc, RGB (33, 33, 33));
        TextOut (hdc, x + 35, y + 9, G_SearchText.c_str (), G_SearchText.length ());
    }

    DeleteObject (inputFont);
}

// 绘制按钮
void DrawButton (HDC hdc, int x, int y, int width, int height, const wstring& text, bool isHovered)
{
    // 绘制背景
    COLORREF bgColor = isHovered ? RGB (200, 200, 200) : RGB (230, 230, 230);
    HBRUSH bgBrush = CreateSolidBrush (bgColor);
    RECT bgRect = { x, y, x + width, y + height };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制边框
    HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (180, 180, 180));
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + height);
    DeleteObject (borderPen);

    // 绘制文字
    SetTextColor (hdc, RGB (80, 80, 80));
    SetBkMode (hdc, TRANSPARENT);
    HFONT btnFont = CreateFont (12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, btnFont);

    SIZE textSize;
    GetTextExtentPoint32 (hdc, text.c_str (), text.length (), &textSize);
    int textX = x + (width - textSize.cx) / 2;
    int textY = y + (height - textSize.cy) / 2;
    TextOut (hdc, textX, textY, text.c_str (), text.length ());

    DeleteObject (btnFont);
}

// 绘制卡片
void DrawCard (HDC hdc, int x, int y, int width, const ClipRecord& record, bool isHovered)
{
    // 绘制卡片背景
    COLORREF bgColor = isHovered ? RGB (245, 248, 255) : RGB (255, 255, 255);
    HBRUSH cardBg = CreateSolidBrush (bgColor);
    RECT cardRect = { x, y, x + width, y + CARD_HEIGHT };
    FillRect (hdc, &cardRect, cardBg);
    DeleteObject (cardBg);

    // 绘制边框
    COLORREF borderColor = isHovered ? RGB (100, 149, 237) : RGB (220, 220, 220);
    HPEN borderPen = CreatePen (PS_SOLID, 1, borderColor);
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + CARD_HEIGHT);
    DeleteObject (borderPen);

    // 绘制左侧彩色条
    COLORREF accentColor = record.isPinned ? RGB (255, 165, 0) : RGB (100, 149, 237);
    HBRUSH accentBrush = CreateSolidBrush (accentColor);
    RECT accentRect = { x, y, x + 4, y + CARD_HEIGHT };
    FillRect (hdc, &accentRect, accentBrush);
    DeleteObject (accentBrush);

    // 绘制内容预览
    int contentX = x + CARD_PADDING;
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT contentFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, contentFont);

    wstring preview = record.preview;
    if (preview.length () > 50)
    {
        preview = preview.substr (0, 50) + L"...";
    }
    TextOut (hdc, contentX, y + 10, preview.c_str (), preview.length ());
    DeleteObject (contentFont);

    // 绘制时间
    time_t timestamp = record.timestamp;
    struct tm timeInfo;
    localtime_s (&timeInfo, &timestamp);
    wchar_t timeStr[32];
    wcsftime (timeStr, 32, L"%m-%d %H:%M", &timeInfo);
    SetTextColor (hdc, RGB (150, 150, 150));
    HFONT timeFont = CreateFont (12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, timeFont);
    TextOut (hdc, contentX, y + 55, timeStr, wcslen (timeStr));
    DeleteObject (timeFont);

    // 绘制操作按钮
    int buttonY = y + CARD_HEIGHT - 28;
    int buttonX = x + width - 180;

    // 复制按钮
    DrawButton (hdc, buttonX, buttonY, 50, 22, L"复制", false);

    // 置顶按钮
    wstring pinText = record.isPinned ? L"取消" : L"置顶";
    DrawButton (hdc, buttonX + 58, buttonY, 50, 22, pinText, false);

    // 删除按钮
    DrawButton (hdc, buttonX + 116, buttonY, 50, 22, L"删除", false);
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
        TextOut (hdc, 20, 12, L"历史剪贴板管理器", 8);
        DeleteObject (titleFont);

        // 绘制搜索框
        DrawSearchBox (hdc, 20, SEARCH_BOX_Y, 760);

        // 绘制分割线
        HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
        SelectObject (hdc, linePen);
        MoveToEx (hdc, 20, SEARCH_BOX_Y + SEARCH_BOX_HEIGHT + 10, NULL);
        LineTo (hdc, 780, SEARCH_BOX_Y + SEARCH_BOX_HEIGHT + 10);
        DeleteObject (linePen);

        // 绘制历史记录列表
        vector<ClipRecord> records = GetFilteredRecords ();
        int cardY = CARD_START_Y - G_ScrollOffset;
        int cardWidth = 760;

        for (int i = 0; i < (int)records.size (); i++)
        {
            // 检查是否超出可视区域
            if (cardY + CARD_HEIGHT > 580)
            {
                break;
            }

            // 跳过在可视区域上方的卡片
            if (cardY + CARD_HEIGHT < 0)
            {
                cardY += CARD_HEIGHT + CARD_MARGIN;
                continue;
            }

            // 绘制卡片
            bool isHovered = (i == G_HoverIndex);
            DrawCard (hdc, 20, cardY, cardWidth, records[i], isHovered);
            cardY += CARD_HEIGHT + CARD_MARGIN;
        }

        // 如果没有记录，显示提示
        if (records.empty ())
        {
            HFONT hintFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, hintFont);
            SetTextColor (hdc, RGB (150, 150, 150));
            wstring hintText = G_SearchText.empty () ? L"暂无历史记录，请复制内容测试" : L"未找到匹配的记录";
            TextOut (hdc, 300, 250, hintText.c_str (), hintText.length ());
            DeleteObject (hintFont);
        }

        EndPaint (hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        int x = LOWORD (lParam);
        int y = HIWORD (lParam);

        // 计算鼠标所在的卡片索引
        int cardY = CARD_START_Y - G_ScrollOffset;
        vector<ClipRecord> records = GetFilteredRecords ();
        G_HoverIndex = -1;

        for (int i = 0; i < (int)records.size (); i++)
        {
            if (y >= cardY && y < cardY + CARD_HEIGHT && x >= 20 && x <= 780)
            {
                G_HoverIndex = i;
                break;
            }
            cardY += CARD_HEIGHT + CARD_MARGIN;
        }

        // 刷新窗口
        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD (lParam);
        int y = HIWORD (lParam);

        // 检查是否点击了搜索框
        if (y >= SEARCH_BOX_Y && y < SEARCH_BOX_Y + SEARCH_BOX_HEIGHT && x >= 20 && x <= 780)
        {
            SetFocus (hWnd);
            return 0;
        }

        // 检查是否点击了卡片
        int cardY = CARD_START_Y - G_ScrollOffset;
        vector<ClipRecord> records = GetFilteredRecords ();

        for (int i = 0; i < (int)records.size (); i++)
        {
            if (y >= cardY && y < cardY + CARD_HEIGHT && x >= 20 && x <= 780)
            {
                // 检查是否点击了按钮
                int buttonX = 20 + 760 - 180;
                int buttonY = cardY + CARD_HEIGHT - 28;

                if (y >= buttonY && y <= buttonY + 22)
                {
                    // 复制按钮
                    if (x >= buttonX && x <= buttonX + 50)
                    {
                        wstring copyText = records[i].content;
                        G_ClipManager.CopyToClipboard (copyText);
                        return 0;
                    }

                    // 置顶按钮
                    if (x >= buttonX + 58 && x <= buttonX + 108)
                    {
                        int recordId = records[i].id;
                        vector<ClipRecord>& allRecords = const_cast<vector<ClipRecord>&> (G_ClipManager.GetRecords ());
                        for (auto& record : allRecords)
                        {
                            if (record.id == recordId)
                            {
                                record.isPinned = !record.isPinned;
                                break;
                            }
                        }
                        G_Storage.SaveRecords (allRecords);
                        InvalidateRect (hWnd, NULL, TRUE);
                        return 0;
                    }

                    // 删除按钮
                    if (x >= buttonX + 116 && x <= buttonX + 166)
                    {
                        int recordId = records[i].id;
                        vector<ClipRecord>& allRecords = const_cast<vector<ClipRecord>&> (G_ClipManager.GetRecords ());
                        for (auto it = allRecords.begin (); it != allRecords.end (); ++it)
                        {
                            if (it->id == recordId)
                            {
                                G_Storage.DeleteRecordFile (*it);
                                allRecords.erase (it);
                                break;
                            }
                        }
                        G_Storage.SaveRecords (allRecords);
                        InvalidateRect (hWnd, NULL, TRUE);
                        return 0;
                    }
                }

                // 点击卡片本身，复制内容
                wstring copyText = records[i].content;
                G_ClipManager.CopyToClipboard (copyText);

                break;
            }
            cardY += CARD_HEIGHT + CARD_MARGIN;
        }

        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        // 处理鼠标滚轮
        int delta = GET_WHEEL_DELTA_WPARAM (wParam);
        G_ScrollOffset -= delta / 3;

        // 限制滚动范围
        int maxScroll = max (0, (int)GetFilteredRecords ().size () * (CARD_HEIGHT + CARD_MARGIN) - 490);
        G_ScrollOffset = max (0, min (G_ScrollOffset, maxScroll));

        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_CHAR:
    {
        wchar_t ch = (wchar_t)wParam;

        if (ch == VK_BACK)
        {
            // 退格键
            if (!G_SearchText.empty ())
            {
                G_SearchText.pop_back ();
            }
        }
        else if (ch >= 32)
        {
            // 可打印字符
            G_SearchText += ch;
        }

        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_KEYDOWN:
    {
        int keyCode = (int)wParam;

        if (keyCode == VK_BACK)
        {
            // 退格键
            if (!G_SearchText.empty ())
            {
                G_SearchText.pop_back ();
            }
            InvalidateRect (hWnd, NULL, TRUE);
        }
        else if (keyCode == VK_DOWN)
        {
            // 向下滚动
            G_ScrollOffset += 30;
            int maxScroll = max (0, (int)GetFilteredRecords ().size () * (CARD_HEIGHT + CARD_MARGIN) - 490);
            G_ScrollOffset = min (G_ScrollOffset, maxScroll);
            InvalidateRect (hWnd, NULL, TRUE);
        }
        else if (keyCode == VK_UP)
        {
            // 向上滚动
            G_ScrollOffset -= 30;
            G_ScrollOffset = max (0, G_ScrollOffset);
            InvalidateRect (hWnd, NULL, TRUE);
        }

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
