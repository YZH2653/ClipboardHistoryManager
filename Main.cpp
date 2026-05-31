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
using namespace std;

// 窗口类名
const wchar_t* CLASS_NAME = L"ClipboardHistoryClass";

// 全局对象
ClipboardManager G_ClipManager;
Storage G_Storage;

// 设置参数
int G_RetentionDays = 3;    // 保留天数
int G_MaxRecords = 1000;    // 最大记录数

// 界面状态
wstring G_SearchText;       // 搜索文本
int G_ScrollOffset = 0;     // 滚动偏移量
int G_HoverIndex = -1;      // 鼠标悬停的卡片索引
bool G_SearchFocused = false;  // 搜索框是否获得焦点
int G_CursorPos = 0;        // 光标位置

// 窗口尺寸
int G_WindowWidth = 800;
int G_WindowHeight = 600;

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
// 检查搜索文本是否匹配时间
bool MatchTimeFilter (const wstring& searchText, time_t timestamp)
{
    // 转换时间为字符串
    struct tm timeInfo;
    localtime_s (&timeInfo, &timestamp);

    // 格式化日期：YYYY-MM-DD
    wchar_t dateStr[20];
    wcsftime (dateStr, 20, L"%Y-%m-%d", &timeInfo);

    // 格式化日期：MM-DD
    wchar_t shortDateStr[10];
    wcsftime (shortDateStr, 10, L"%m-%d", &timeInfo);

    // 格式化时间：HH:MM
    wchar_t timeStr[10];
    wcsftime (timeStr, 10, L"%H:%M", &timeInfo);

    // 格式化年月：YYYY-MM
    wchar_t yearMonthStr[10];
    wcsftime (yearMonthStr, 10, L"%Y-%m", &timeInfo);

    // 检查是否匹配日期格式（YYYY-MM-DD）
    if (searchText.length () == 10 && searchText[4] == L'-' && searchText[7] == L'-')
    {
        return wcsstr (dateStr, searchText.c_str ()) != NULL;
    }

    // 检查是否匹配短日期格式（MM-DD）
    if (searchText.length () == 5 && searchText[2] == L'-')
    {
        return wcsstr (shortDateStr, searchText.c_str ()) != NULL;
    }

    // 检查是否匹配时间格式（HH:MM）
    if (searchText.length () == 5 && searchText[2] == L':')
    {
        return wcsstr (timeStr, searchText.c_str ()) != NULL;
    }

    // 检查是否匹配年月格式（YYYY-MM）
    if (searchText.length () == 7 && searchText[4] == L'-')
    {
        return wcsstr (yearMonthStr, searchText.c_str ()) != NULL;
    }

    // 检查是否只包含数字（可能是年份、月份、日期、小时）
    bool isNumber = true;
    for (wchar_t ch : searchText)
    {
        if (!iswdigit (ch) && ch != L'-')
        {
            isNumber = false;
            break;
        }
    }

    if (isNumber)
    {
        // 尝试匹配年份
        if (searchText.length () == 4)
        {
            wchar_t yearStr[6];
            wcsftime (yearStr, 6, L"%Y", &timeInfo);
            return wcsstr (yearStr, searchText.c_str ()) != NULL;
        }

        // 尝试匹配月份
        if (searchText.length () == 2 || searchText.length () == 1)
        {
            wchar_t monthStr[4];
            wcsftime (monthStr, 4, L"%m", &timeInfo);
            return wcsstr (monthStr, searchText.c_str ()) != NULL;
        }

        // 尝试匹配日期
        if (searchText.length () == 2 || searchText.length () == 1)
        {
            wchar_t dayStr[4];
            wcsftime (dayStr, 4, L"%d", &timeInfo);
            return wcsstr (dayStr, searchText.c_str ()) != NULL;
        }
    }

    return false;
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
            bool textMatch = (record.preview.find (G_SearchText) != wstring::npos ||
                             record.content.find (G_SearchText) != wstring::npos);
            bool timeMatch = MatchTimeFilter (G_SearchText, record.timestamp);

            // 文字搜索或时间搜索
            if (!textMatch && !timeMatch)
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
    COLORREF bgColor = G_SearchFocused ? RGB (255, 255, 255) : RGB (245, 245, 245);
    HBRUSH bgBrush = CreateSolidBrush (bgColor);
    RECT bgRect = { x, y, x + width, y + 40 };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制边框（获得焦点时高亮）
    COLORREF borderColor = G_SearchFocused ? RGB (100, 149, 237) : RGB (200, 200, 200);
    HPEN borderPen = CreatePen (PS_SOLID, 2, borderColor);
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + 40);
    DeleteObject (borderPen);

    // 绘制搜索图标
    SetTextColor (hdc, RGB (150, 150, 150));
    SetBkMode (hdc, TRANSPARENT);
    HFONT iconFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
    SelectObject (hdc, iconFont);
    TextOut (hdc, x + 12, y + 10, L"🔍", 1);
    DeleteObject (iconFont);

    // 绘制输入文本
    HFONT inputFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, inputFont);

    if (G_SearchText.empty () && !G_SearchFocused)
    {
        SetTextColor (hdc, RGB (180, 180, 180));
        TextOut (hdc, x + 40, y + 12, L"搜索历史记录...", 7);
    }
    else
    {
        SetTextColor (hdc, RGB (33, 33, 33));
        TextOut (hdc, x + 40, y + 12, G_SearchText.c_str (), G_SearchText.length ());
    }

    DeleteObject (inputFont);

    // 绘制光标（获得焦点时显示）
    if (G_SearchFocused)
    {
        // 计算光标位置
        SIZE textSize = { 0, 0 };
        if (!G_SearchText.empty ())
        {
            HFONT tempFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, tempFont);
            GetTextExtentPoint32 (hdc, G_SearchText.c_str (), G_SearchText.length (), &textSize);
            DeleteObject (tempFont);
        }

        // 绘制光标竖线
        HPEN cursorPen = CreatePen (PS_SOLID, 2, RGB (33, 33, 33));
        SelectObject (hdc, cursorPen);
        MoveToEx (hdc, x + 40 + textSize.cx + 2, y + 8, NULL);
        LineTo (hdc, x + 40 + textSize.cx + 2, y + 32);
        DeleteObject (cursorPen);
    }
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
    HFONT btnFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
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
    RECT cardRect = { x, y, x + width, y + 100 };
    FillRect (hdc, &cardRect, cardBg);
    DeleteObject (cardBg);

    // 绘制边框
    COLORREF borderColor = isHovered ? RGB (100, 149, 237) : RGB (220, 220, 220);
    HPEN borderPen = CreatePen (PS_SOLID, 1, borderColor);
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + 100);
    DeleteObject (borderPen);

    // 绘制左侧彩色条
    COLORREF accentColor = record.isPinned ? RGB (255, 165, 0) : RGB (100, 149, 237);
    HBRUSH accentBrush = CreateSolidBrush (accentColor);
    RECT accentRect = { x, y, x + 4, y + 100 };
    FillRect (hdc, &accentRect, accentBrush);
    DeleteObject (accentBrush);

    // 绘制内容预览
    int contentX = x + 15;
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT contentFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, contentFont);

    wstring preview = record.preview;
    if (preview.length () > 60)
    {
        preview = preview.substr (0, 60) + L"...";
    }
    TextOut (hdc, contentX, y + 12, preview.c_str (), preview.length ());
    DeleteObject (contentFont);

    // 绘制时间
    time_t timestamp = record.timestamp;
    struct tm timeInfo;
    localtime_s (&timeInfo, &timestamp);
    wchar_t timeStr[32];
    wcsftime (timeStr, 32, L"%Y-%m-%d %H:%M", &timeInfo);
    SetTextColor (hdc, RGB (150, 150, 150));
    HFONT timeFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, timeFont);
    TextOut (hdc, contentX, y + 65, timeStr, wcslen (timeStr));
    DeleteObject (timeFont);

    // 绘制操作按钮
    int buttonY = y + 100 - 32;
    int buttonX = x + width - 200;

    // 复制按钮
    DrawButton (hdc, buttonX, buttonY, 55, 26, L"复制", false);

    // 置顶按钮
    wstring pinText = record.isPinned ? L"取消" : L"置顶";
    DrawButton (hdc, buttonX + 65, buttonY, 55, 26, pinText, false);

    // 删除按钮
    DrawButton (hdc, buttonX + 130, buttonY, 55, 26, L"删除", false);
}

// 窗口过程函数
LRESULT CALLBACK WindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_GETMINMAXINFO:
    {
        // 设置窗口最小尺寸
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 500;   // 最小宽度
        mmi->ptMinTrackSize.y = 400;   // 最小高度
        return 0;
    }

    case WM_SIZE:
    {
        // 窗口大小改变时更新尺寸
        G_WindowWidth = LOWORD (lParam);
        G_WindowHeight = HIWORD (lParam);
        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_DESTROY:
        // 移除剪贴板监听
        RemoveClipboardFormatListener (hWnd);

        // 清理GDI+
        G_ClipManager.ShutdownGdiplus ();

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

        // 计算布局
        int contentWidth = G_WindowWidth - 40;
        int searchWidth = contentWidth;
        int cardWidth = contentWidth;

        // 绘制标题
        SetTextColor (hdc, RGB (33, 33, 33));
        SetBkMode (hdc, TRANSPARENT);
        HFONT titleFont = CreateFont (24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        SelectObject (hdc, titleFont);
        TextOut (hdc, 20, 12, L"历史剪贴板管理器", 8);
        DeleteObject (titleFont);

        // 绘制搜索框
        DrawSearchBox (hdc, 20, 50, searchWidth);

        // 绘制分割线
        HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
        SelectObject (hdc, linePen);
        MoveToEx (hdc, 20, 100, NULL);
        LineTo (hdc, 20 + searchWidth, 100);
        DeleteObject (linePen);

        // 绘制历史记录列表
        vector<ClipRecord> records = GetFilteredRecords ();
        int cardY = 110 - G_ScrollOffset;
        int cardHeight = 100;
        int cardMargin = 10;

        for (int i = 0; i < (int)records.size (); i++)
        {
            // 检查是否超出可视区域
            if (cardY + cardHeight > G_WindowHeight - 20)
            {
                break;
            }

            // 跳过在可视区域上方的卡片
            if (cardY + cardHeight < 0)
            {
                cardY += cardHeight + cardMargin;
                continue;
            }

            // 绘制卡片
            bool isHovered = (i == G_HoverIndex);
            DrawCard (hdc, 20, cardY, cardWidth, records[i], isHovered);
            cardY += cardHeight + cardMargin;
        }

        // 如果没有记录，显示提示
        if (records.empty ())
        {
            HFONT hintFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, hintFont);
            SetTextColor (hdc, RGB (150, 150, 150));
            wstring hintText = G_SearchText.empty () ? L"暂无历史记录，请复制内容测试" : L"未找到匹配的记录";
            TextOut (hdc, G_WindowWidth / 2 - 120, G_WindowHeight / 2, hintText.c_str (), hintText.length ());
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
        int cardY = 110 - G_ScrollOffset;
        int cardHeight = 100;
        int cardMargin = 10;
        vector<ClipRecord> records = GetFilteredRecords ();
        G_HoverIndex = -1;

        for (int i = 0; i < (int)records.size (); i++)
        {
            if (y >= cardY && y < cardY + cardHeight && x >= 20 && x <= G_WindowWidth - 20)
            {
                G_HoverIndex = i;
                break;
            }
            cardY += cardHeight + cardMargin;
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
        if (y >= 50 && y < 90 && x >= 20 && x <= G_WindowWidth - 20)
        {
            G_SearchFocused = true;
            SetFocus (hWnd);
            InvalidateRect (hWnd, NULL, TRUE);
            return 0;
        }

        // 检查是否点击了卡片
        int cardY = 110 - G_ScrollOffset;
        int cardHeight = 100;
        int cardMargin = 10;
        vector<ClipRecord> records = GetFilteredRecords ();

        for (int i = 0; i < (int)records.size (); i++)
        {
            if (y >= cardY && y < cardY + cardHeight && x >= 20 && x <= G_WindowWidth - 20)
            {
                // 检查是否点击了按钮
                int buttonX = 20 + G_WindowWidth - 40 - 200;
                int buttonY = cardY + cardHeight - 32;

                if (y >= buttonY && y <= buttonY + 26)
                {
                    // 复制按钮
                    if (x >= buttonX && x <= buttonX + 55)
                    {
                        wstring copyText = records[i].content;
                        G_ClipManager.CopyToClipboard (copyText);
                        return 0;
                    }

                    // 置顶按钮
                    if (x >= buttonX + 65 && x <= buttonX + 120)
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
                    if (x >= buttonX + 130 && x <= buttonX + 185)
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

                // 取消搜索框焦点
                G_SearchFocused = false;

                break;
            }
            cardY += cardHeight + cardMargin;
        }

        // 点击其他地方，取消搜索框焦点
        G_SearchFocused = false;

        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        // 处理鼠标滚轮
        int delta = GET_WHEEL_DELTA_WPARAM (wParam);
        G_ScrollOffset -= delta / 3;

        // 限制滚动范围
        int maxScroll = max (0, (int)GetFilteredRecords ().size () * 110 - (G_WindowHeight - 130));
        G_ScrollOffset = max (0, min (G_ScrollOffset, maxScroll));

        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_CHAR:
    {
        // 只有搜索框获得焦点时才处理键盘输入
        if (!G_SearchFocused)
        {
            return 0;
        }

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
        // 只有搜索框获得焦点时才处理键盘输入
        if (!G_SearchFocused)
        {
            return 0;
        }

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
            G_ScrollOffset += 40;
            int maxScroll = max (0, (int)GetFilteredRecords ().size () * 110 - (G_WindowHeight - 130));
            G_ScrollOffset = min (G_ScrollOffset, maxScroll);
            InvalidateRect (hWnd, NULL, TRUE);
        }
        else if (keyCode == VK_UP)
        {
            // 向上滚动
            G_ScrollOffset -= 40;
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
    G_ClipManager.SetMaxRecords (G_MaxRecords);
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
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
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

    // 初始化GDI+
    G_ClipManager.InitializeGdiplus ();

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
