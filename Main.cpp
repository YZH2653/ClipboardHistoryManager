#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "ClipboardManager.h"
#include "Storage.h"
using namespace std;

// 自定义消息：托盘图标
#define WM_TRAYICON (WM_USER + 1)

// 窗口类名
const wchar_t* CLASS_NAME = L"ClipboardHistoryClass";

// 全局对象
ClipboardManager G_ClipManager;
Storage G_Storage;

// 版本号
const wchar_t* APP_VERSION = L"1.4.0.0";
const wchar_t* APP_UPDATE_DATE = L"2026-06-07";
const wchar_t* APP_AUTHOR = L"YZH2653";
const wchar_t* APP_AUTHOR_EMAIL = L"yzh2653@163.com";
const wchar_t* APP_GITHUB_URL = L"https://github.com/YZH2653/ClipboardHistoryManager";

// 页面状态
enum PageState
{
    PAGE_MAIN,
    PAGE_SETTINGS,
    PAGE_VERSION,
    PAGE_FEEDBACK
};
PageState G_CurrentPage = PAGE_MAIN;

// 设置参数
int G_RetentionDays = 3;    // 保留天数
int G_MaxRecords = 1000;    // 最大记录数

// 保存时间选项
const int RETENTION_OPTIONS[] = {3, 5, 7, 30, -1};  // -1 表示永久
const wchar_t* RETENTION_LABELS[] = {L"3天", L"5天", L"7天", L"30天", L"永久"};
const int RETENTION_COUNT = 5;
int G_SelectedRetentionIndex = 0;  // 当前选中的保存时间索引
bool G_DropdownOpen = false;  // 下拉菜单是否打开
bool G_AutoStart = false;  // 开机自启状态
bool G_MinimizeToTray = true;  // 关闭时最小化到托盘

// 界面状态
wstring G_SearchText;       // 搜索文本
int G_ScrollOffset = 0;     // 滚动偏移量
int G_HoverIndex = -1;      // 鼠标悬停的卡片索引
bool G_SearchFocused = false;  // 搜索框是否获得焦点
int G_CursorPos = 0;        // 光标位置

// 窗口尺寸
int G_WindowWidth = 800;
int G_WindowHeight = 600;

// 托盘图标
NOTIFYICONDATA G_Nid = {};
bool G_TrayIconAdded = false;
bool G_IsMinimizedToTray = false;

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

// 添加托盘图标
void AddTrayIcon (HWND hWnd)
{
    G_Nid.cbSize = sizeof (NOTIFYICONDATA);
    G_Nid.hWnd = hWnd;
    G_Nid.uID = 1;
    G_Nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    G_Nid.uCallbackMessage = WM_TRAYICON;
    G_Nid.hIcon = LoadIcon (GetModuleHandle (NULL), MAKEINTRESOURCE (1));
    if (G_Nid.hIcon == NULL)
    {
        // 如果没有资源图标，使用默认图标
        G_Nid.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    }
    wcscpy_s (G_Nid.szTip, L"历史剪贴板管理器");
    Shell_NotifyIcon (NIM_ADD, &G_Nid);
    G_TrayIconAdded = true;
}

// 删除托盘图标
void RemoveTrayIcon ()
{
    if (G_TrayIconAdded)
    {
        Shell_NotifyIcon (NIM_DELETE, &G_Nid);
        G_TrayIconAdded = false;
    }
}

// 最小化到托盘
void MinimizeToTray (HWND hWnd)
{
    ShowWindow (hWnd, SW_HIDE);
    G_IsMinimizedToTray = true;
}

// 从托盘恢复窗口
void RestoreFromTray (HWND hWnd)
{
    ShowWindow (hWnd, SW_SHOW);
    SetForegroundWindow (hWnd);
    G_IsMinimizedToTray = false;
    InvalidateRect (hWnd, NULL, TRUE);
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
    RECT bgRect = { x, y, x + width, y + 50 };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制边框（获得焦点时高亮）
    COLORREF borderColor = G_SearchFocused ? RGB (100, 149, 237) : RGB (200, 200, 200);
    HPEN borderPen = CreatePen (PS_SOLID, 2, borderColor);
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + 50);
    DeleteObject (borderPen);

    // 绘制搜索图标
    SetTextColor (hdc, RGB (150, 150, 150));
    SetBkMode (hdc, TRANSPARENT);
    HFONT iconFont = CreateFont (22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
    SelectObject (hdc, iconFont);
    TextOut (hdc, x + 15, y + 12, L"🔍", 1);
    DeleteObject (iconFont);

    // 绘制输入文本
    HFONT inputFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, inputFont);

    if (G_SearchText.empty () && !G_SearchFocused)
    {
        SetTextColor (hdc, RGB (180, 180, 180));
        TextOut (hdc, x + 50, y + 14, L"搜索历史记录...", 7);
    }
    else
    {
        SetTextColor (hdc, RGB (33, 33, 33));
        TextOut (hdc, x + 50, y + 14, G_SearchText.c_str (), G_SearchText.length ());
    }

    DeleteObject (inputFont);

    // 绘制光标（获得焦点时显示）
    if (G_SearchFocused)
    {
        // 计算光标位置
        SIZE textSize = { 0, 0 };
        if (!G_SearchText.empty ())
        {
            HFONT tempFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, tempFont);
            GetTextExtentPoint32 (hdc, G_SearchText.c_str (), G_SearchText.length (), &textSize);
            DeleteObject (tempFont);
        }

        // 绘制光标竖线
        HPEN cursorPen = CreatePen (PS_SOLID, 2, RGB (33, 33, 33));
        SelectObject (hdc, cursorPen);
        MoveToEx (hdc, x + 50 + textSize.cx + 2, y + 10, NULL);
        LineTo (hdc, x + 50 + textSize.cx + 2, y + 40);
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
    HFONT btnFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, btnFont);

    SIZE textSize;
    GetTextExtentPoint32 (hdc, text.c_str (), text.length (), &textSize);
    int textX = x + (width - textSize.cx) / 2;
    int textY = y + (height - textSize.cy) / 2;
    TextOut (hdc, textX, textY, text.c_str (), text.length ());

    DeleteObject (btnFont);
}

// 绘制设置按钮（齿轮图标）
void DrawSettingsButton (HDC hdc, int x, int y, bool isHovered)
{
    int size = 32;

    // 绘制背景
    COLORREF bgColor = isHovered ? RGB (220, 220, 220) : RGB (240, 240, 240);
    HBRUSH bgBrush = CreateSolidBrush (bgColor);
    RECT bgRect = { x, y, x + size, y + size };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制齿轮图标
    SetTextColor (hdc, RGB (100, 100, 100));
    SetBkMode (hdc, TRANSPARENT);
    HFONT iconFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
    SelectObject (hdc, iconFont);
    TextOut (hdc, x + 6, y + 4, L"⚙", 1);
    DeleteObject (iconFont);
}

// 绘制返回按钮
void DrawBackButton (HDC hdc, int x, int y, bool isHovered)
{
    int width = 60;
    int height = 30;

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
    HFONT btnFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, btnFont);
    TextOut (hdc, x + 12, y + 7, L"← 返回", 5);
    DeleteObject (btnFont);
}

// 绘制设置页面
void DrawSettingsPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, titleFont);
    TextOut (hdc, 100, 12, L"设置", 2);
    DeleteObject (titleFont);

    // 创建分割线画笔（整个函数复用）
    HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));

    // 绘制分割线
    SelectObject (hdc, linePen);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);

    // 保存时间设置
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT sectionFont = CreateFont (26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, sectionFont);
    TextOut (hdc, 20, 80, L"保存时间", 4);
    DeleteObject (sectionFont);

    // 绘制下拉菜单框（在右边）
    int dropdownWidth = 200;
    int dropdownHeight = 40;
    int dropdownX = G_WindowWidth - dropdownWidth - 40;
    int dropdownY = 75;

    // 绘制下拉框背景
    COLORREF bgColor = RGB (255, 255, 255);
    HBRUSH bgBrush = CreateSolidBrush (bgColor);
    RECT bgRect = { dropdownX, dropdownY, dropdownX + dropdownWidth, dropdownY + dropdownHeight };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制边框（临时切换画笔，用完恢复）
    HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
    HPEN prevPen = (HPEN)SelectObject (hdc, borderPen);
    Rectangle (hdc, dropdownX, dropdownY, dropdownX + dropdownWidth, dropdownY + dropdownHeight);
    SelectObject (hdc, prevPen);
    DeleteObject (borderPen);

    // 绘制当前选中的值
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT valueFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, valueFont);
    TextOut (hdc, dropdownX + 15, dropdownY + 10, RETENTION_LABELS[G_SelectedRetentionIndex], wcslen (RETENTION_LABELS[G_SelectedRetentionIndex]));

    // 绘制下拉箭头
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, dropdownX + dropdownWidth - 25, dropdownY + 10, L"▼", 1);
    DeleteObject (valueFont);

    // 绘制分割线
    MoveToEx (hdc, 20, 140, NULL);
    LineTo (hdc, G_WindowWidth - 20, 140);

    // 开机自启设置
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT autoStartFont = CreateFont (26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, autoStartFont);
    TextOut (hdc, 20, 158, L"开机自启", 4);

    // 绘制开关按钮
    int toggleWidth = 70;
    int toggleHeight = 36;
    int toggleX = G_WindowWidth - toggleWidth - 40;
    int toggleY = 153;

    // 开关背景色
    COLORREF toggleBgColor = G_AutoStart ? RGB (74, 144, 217) : RGB (200, 200, 200);
    HBRUSH toggleBgBrush = CreateSolidBrush (toggleBgColor);
    RECT toggleRect = { toggleX, toggleY, toggleX + toggleWidth, toggleY + toggleHeight };
    FillRect (hdc, &toggleRect, toggleBgBrush);
    DeleteObject (toggleBgBrush);

    // 绘制开关圆角边框（临时切换画笔和画刷，用完恢复）
    HPEN togglePen = CreatePen (PS_SOLID, 1, toggleBgColor);
    HBRUSH nullBrush = (HBRUSH)GetStockObject (NULL_BRUSH);
    prevPen = (HPEN)SelectObject (hdc, togglePen);
    HBRUSH prevBrush = (HBRUSH)SelectObject (hdc, nullBrush);
    RoundRect (hdc, toggleX, toggleY, toggleX + toggleWidth, toggleY + toggleHeight, toggleHeight, toggleHeight);
    SelectObject (hdc, prevBrush);
    SelectObject (hdc, prevPen);
    DeleteObject (togglePen);

    // 绘制开关文字
    SetBkMode (hdc, TRANSPARENT);
    SetTextColor (hdc, RGB (255, 255, 255));
    HFONT toggleFont = CreateFont (22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, toggleFont);
    const wchar_t* toggleText = G_AutoStart ? L"开" : L"关";
    SIZE toggleTextSize;
    GetTextExtentPoint32 (hdc, toggleText, 1, &toggleTextSize);
    int toggleTextX = toggleX + (toggleWidth - toggleTextSize.cx) / 2;
    int toggleTextY = toggleY + (toggleHeight - toggleTextSize.cy) / 2;
    TextOut (hdc, toggleTextX, toggleTextY, toggleText, 1);
    DeleteObject (toggleFont);
    DeleteObject (autoStartFont);

    // 绘制分割线
    MoveToEx (hdc, 20, 200, NULL);
    LineTo (hdc, G_WindowWidth - 20, 200);

    // 关闭时最小化到托盘设置
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT minimizeFont = CreateFont (26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, minimizeFont);
    TextOut (hdc, 20, 218, L"关闭时最小化到托盘", 9);

    // 绘制开关按钮
    int minimizeToggleWidth = 70;
    int minimizeToggleHeight = 36;
    int minimizeToggleX = G_WindowWidth - minimizeToggleWidth - 40;
    int minimizeToggleY = 213;

    // 开关背景色
    COLORREF minimizeToggleBgColor = G_MinimizeToTray ? RGB (74, 144, 217) : RGB (200, 200, 200);
    HBRUSH minimizeToggleBgBrush = CreateSolidBrush (minimizeToggleBgColor);
    RECT minimizeToggleRect = { minimizeToggleX, minimizeToggleY, minimizeToggleX + minimizeToggleWidth, minimizeToggleY + minimizeToggleHeight };
    FillRect (hdc, &minimizeToggleRect, minimizeToggleBgBrush);
    DeleteObject (minimizeToggleBgBrush);

    // 绘制开关圆角边框
    HPEN minimizeTogglePen = CreatePen (PS_SOLID, 1, minimizeToggleBgColor);
    HPEN prevPen2 = (HPEN)SelectObject (hdc, minimizeTogglePen);
    HBRUSH prevBrush2 = (HBRUSH)SelectObject (hdc, nullBrush);
    RoundRect (hdc, minimizeToggleX, minimizeToggleY, minimizeToggleX + minimizeToggleWidth, minimizeToggleY + minimizeToggleHeight, minimizeToggleHeight, minimizeToggleHeight);
    SelectObject (hdc, prevBrush2);
    SelectObject (hdc, prevPen2);
    DeleteObject (minimizeTogglePen);

    // 绘制开关文字
    SetBkMode (hdc, TRANSPARENT);
    SetTextColor (hdc, RGB (255, 255, 255));
    HFONT minimizeToggleFont = CreateFont (22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, minimizeToggleFont);
    const wchar_t* minimizeToggleText = G_MinimizeToTray ? L"开" : L"关";
    SIZE minimizeToggleTextSize;
    GetTextExtentPoint32 (hdc, minimizeToggleText, 1, &minimizeToggleTextSize);
    int minimizeToggleTextX = minimizeToggleX + (minimizeToggleWidth - minimizeToggleTextSize.cx) / 2;
    int minimizeToggleTextY = minimizeToggleY + (minimizeToggleHeight - minimizeToggleTextSize.cy) / 2;
    TextOut (hdc, minimizeToggleTextX, minimizeToggleTextY, minimizeToggleText, 1);
    DeleteObject (minimizeToggleFont);
    DeleteObject (minimizeFont);

    // 绘制分割线
    MoveToEx (hdc, 20, 260, NULL);
    LineTo (hdc, G_WindowWidth - 20, 260);

    // 版本信息入口
    int versionY = 280;
    HFONT versionFont = CreateFont (26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, versionFont);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 20, versionY, L"版本信息", 4);

    // 绘制箭头
    SetTextColor (hdc, RGB (150, 150, 150));
    TextOut (hdc, G_WindowWidth - 40, versionY, L"→", 1);

    // 绘制分割线
    MoveToEx (hdc, 20, versionY + 40, NULL);
    LineTo (hdc, G_WindowWidth - 20, versionY + 40);

    // 问题反馈入口
    int feedbackY = 340;
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 20, feedbackY, L"问题反馈", 4);

    // 绘制箭头
    SetTextColor (hdc, RGB (150, 150, 150));
    TextOut (hdc, G_WindowWidth - 40, feedbackY, L"→", 1);
    DeleteObject (versionFont);

    // 绘制分割线
    MoveToEx (hdc, 20, feedbackY + 40, NULL);
    LineTo (hdc, G_WindowWidth - 20, feedbackY + 40);

    // GitHub 仓库地址
    int githubY = G_WindowHeight - 60;
    HFONT githubFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, githubFont);
    SetTextColor (hdc, RGB (100, 100, 100));
    SetBkMode (hdc, TRANSPARENT);
    TextOut (hdc, 20, githubY, L"GitHub仓库地址:", 8);

    // 绘制可点击的链接
    SetTextColor (hdc, RGB (100, 149, 237));
    TextOut (hdc, 200, githubY, APP_GITHUB_URL, wcslen (APP_GITHUB_URL));
    DeleteObject (githubFont);

    // 最后绘制下拉菜单选项列表（确保在最上层）
    if (G_DropdownOpen)
    {
        int optionHeight = 45;
        int listY = dropdownY + dropdownHeight;

        for (int i = 0; i < RETENTION_COUNT; i++)
        {
            int optionY = listY + i * optionHeight;
            bool isSelected = (i == G_SelectedRetentionIndex);

            // 绘制选项背景（当前选中的高亮显示）
            COLORREF optBgColor = isSelected ? RGB (230, 240, 255) : RGB (255, 255, 255);
            HBRUSH optBgBrush = CreateSolidBrush (optBgColor);
            RECT optBgRect = { dropdownX, optionY, dropdownX + dropdownWidth, optionY + optionHeight };
            FillRect (hdc, &optBgRect, optBgBrush);
            DeleteObject (optBgBrush);

            // 绘制边框
            HPEN optBorderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
            HPEN savedPen = (HPEN)SelectObject (hdc, optBorderPen);
            Rectangle (hdc, dropdownX, optionY, dropdownX + dropdownWidth, optionY + optionHeight);
            SelectObject (hdc, savedPen);
            DeleteObject (optBorderPen);

            // 绘制文字（当前选中的加一个✓标记）
            COLORREF textColor = isSelected ? RGB (100, 149, 237) : RGB (33, 33, 33);
            SetTextColor (hdc, textColor);
            SetBkMode (hdc, TRANSPARENT);
            HFONT optionFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, optionFont);

            SIZE textSize;
            GetTextExtentPoint32 (hdc, RETENTION_LABELS[i], wcslen (RETENTION_LABELS[i]), &textSize);
            int textX = dropdownX + (dropdownWidth - textSize.cx) / 2;
            int textY = optionY + (optionHeight - textSize.cy) / 2;
            TextOut (hdc, textX, textY, RETENTION_LABELS[i], wcslen (RETENTION_LABELS[i]));

            // 如果是当前选中的，在右边显示✓
            if (isSelected)
            {
                TextOut (hdc, dropdownX + dropdownWidth - 30, textY, L"✓", 1);
            }

            DeleteObject (optionFont);
        }
    }

    // 清理分割线画笔
    DeleteObject (linePen);
}

// 绘制版本号页面
void DrawVersionPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, titleFont);
    TextOut (hdc, 100, 12, L"版本信息", 4);
    DeleteObject (titleFont);

    // 绘制分割线
    HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, linePen);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);
    DeleteObject (linePen);

    // 版本信息内容
    int contentY = 80;
    int lineHeight = 50;

    HFONT contentFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, contentFont);

    // 版本号
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, contentY, L"版本号", 3);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 120, contentY, APP_VERSION, wcslen (APP_VERSION));
    contentY += lineHeight;

    // 更新日期
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, contentY, L"更新日期", 4);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 120, contentY, APP_UPDATE_DATE, wcslen (APP_UPDATE_DATE));
    contentY += lineHeight;

    // 更新内容
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, contentY, L"更新内容", 4);
    contentY += lineHeight;

    // 更新内容列表
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 40, contentY, L"• 新增设置页面", 8);
    contentY += 35;
    TextOut (hdc, 40, contentY, L"• 支持保存时间配置", 10);
    contentY += 35;
    TextOut (hdc, 40, contentY, L"• 显示版本信息", 8);
    contentY += lineHeight + 20;

    // 作者
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, contentY, L"作者", 2);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 120, contentY, APP_AUTHOR, wcslen (APP_AUTHOR));

    DeleteObject (contentFont);
}

// 绘制问题反馈页面
void DrawFeedbackPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, titleFont);
    TextOut (hdc, 100, 12, L"问题反馈", 4);
    DeleteObject (titleFont);

    // 绘制分割线
    HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, linePen);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);
    DeleteObject (linePen);

    // 问题反馈内容
    int contentY = 80;
    int lineHeight = 45;

    HFONT contentFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, contentFont);

    // 作者邮箱
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, contentY, L"作者邮箱", 4);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 130, contentY, APP_AUTHOR_EMAIL, wcslen (APP_AUTHOR_EMAIL));
    contentY += lineHeight + 25;

    // 反馈格式说明
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT hintFont = CreateFont (28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, hintFont);
    TextOut (hdc, 20, contentY, L"请按以下格式写:", 8);
    DeleteObject (hintFont);
    contentY += lineHeight;

    // 反馈格式内容
    HFONT formatFont = CreateFont (22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, formatFont);

    SetTextColor (hdc, RGB (80, 80, 80));
    TextOut (hdc, 40, contentY, L"软件版本(可在版本号中查看):", 15);
    contentY += 35;
    TextOut (hdc, 40, contentY, L"出现问题的时间:", 8);
    contentY += 35;
    TextOut (hdc, 40, contentY, L"描述问题(可用图片表示):", 12);

    DeleteObject (formatFont);
    DeleteObject (contentFont);
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
    HFONT contentFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
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
    HFONT timeFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
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
    case WM_TRAYICON:
    {
        if (lParam == WM_LBUTTONDBLCLK)
        {
            // 双击托盘图标恢复窗口
            RestoreFromTray (hWnd);
        }
        else if (lParam == WM_RBUTTONDOWN)
        {
            // 右键托盘图标显示菜单
            POINT pt;
            GetCursorPos (&pt);

            HMENU hMenu = CreatePopupMenu ();
            AppendMenu (hMenu, MF_STRING, 1, L"显示窗口");
            AppendMenu (hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu (hMenu, MF_STRING, 2, L"退出");

            SetForegroundWindow (hWnd);
            int cmd = TrackPopupMenu (hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu (hMenu);

            if (cmd == 1)
            {
                RestoreFromTray (hWnd);
            }
            else if (cmd == 2)
            {
                RemoveTrayIcon ();
                PostQuitMessage (0);
            }
        }
        return 0;
    }

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
        if (wParam == SIZE_MINIMIZED)
        {
            // 最小化时隐藏到托盘
            MinimizeToTray (hWnd);
            return 0;
        }

        // 窗口大小改变时更新尺寸
        G_WindowWidth = LOWORD (lParam);
        G_WindowHeight = HIWORD (lParam);
        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_CLOSE:
    {
        // 关闭时最小化到托盘（而不是退出）
        if (G_MinimizeToTray)
        {
            MinimizeToTray (hWnd);
            return 0;
        }
        // 如果未开启最小化到托盘，正常关闭
        DestroyWindow (hWnd);
        return 0;
    }

    case WM_DESTROY:
        // 移除剪贴板监听
        RemoveClipboardFormatListener (hWnd);

        // 清理GDI+
        G_ClipManager.ShutdownGdiplus ();

        // 删除托盘图标
        RemoveTrayIcon ();

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

        if (G_CurrentPage == PAGE_MAIN)
        {
            // 主页面
            // 计算布局
            int contentWidth = G_WindowWidth - 40;
            int searchWidth = contentWidth;
            int cardWidth = contentWidth;

            // 绘制标题
            SetTextColor (hdc, RGB (33, 33, 33));
            SetBkMode (hdc, TRANSPARENT);
            HFONT titleFont = CreateFont (32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, titleFont);
            TextOut (hdc, 20, 12, L"历史剪贴板管理器", 8);
            DeleteObject (titleFont);

            // 绘制设置按钮
            DrawSettingsButton (hdc, G_WindowWidth - 52, 10, false);

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
                HFONT hintFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
                SelectObject (hdc, hintFont);
                SetTextColor (hdc, RGB (150, 150, 150));
                wstring hintText = G_SearchText.empty () ? L"暂无历史记录，请复制内容测试" : L"未找到匹配的记录";
                TextOut (hdc, G_WindowWidth / 2 - 150, G_WindowHeight / 2, hintText.c_str (), hintText.length ());
                DeleteObject (hintFont);
            }
        }
        else if (G_CurrentPage == PAGE_SETTINGS)
        {
            // 设置页面
            DrawSettingsPage (hdc);
        }
        else if (G_CurrentPage == PAGE_VERSION)
        {
            // 版本号页面
            DrawVersionPage (hdc);
        }
        else if (G_CurrentPage == PAGE_FEEDBACK)
        {
            // 问题反馈页面
            DrawFeedbackPage (hdc);
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
        int newHoverIndex = -1;

        for (int i = 0; i < (int)records.size (); i++)
        {
            if (y >= cardY && y < cardY + cardHeight && x >= 20 && x <= G_WindowWidth - 20)
            {
                newHoverIndex = i;
                break;
            }
            cardY += cardHeight + cardMargin;
        }

        // 只有当悬停索引改变时才刷新窗口
        if (newHoverIndex != G_HoverIndex)
        {
            G_HoverIndex = newHoverIndex;
            InvalidateRect (hWnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD (lParam);
        int y = HIWORD (lParam);

        if (G_CurrentPage == PAGE_MAIN)
        {
            // 主页面点击处理

            // 检查是否点击了设置按钮
            if (x >= G_WindowWidth - 52 && x <= G_WindowWidth - 20 && y >= 10 && y <= 42)
            {
                G_CurrentPage = PAGE_SETTINGS;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

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
        }
        else if (G_CurrentPage == PAGE_SETTINGS)
        {
            // 设置页面点击处理

            // 检查是否点击了返回按钮
            if (x >= 20 && x <= 80 && y >= 8 && y <= 38)
            {
                G_CurrentPage = PAGE_MAIN;
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了下拉菜单框（在右边）
            int dropdownWidth = 200;
            int dropdownHeight = 40;
            int dropdownX = G_WindowWidth - dropdownWidth - 40;
            int dropdownY = 75;

            if (x >= dropdownX && x <= dropdownX + dropdownWidth && y >= dropdownY && y <= dropdownY + dropdownHeight)
            {
                G_DropdownOpen = !G_DropdownOpen;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 如果下拉菜单打开，检查是否点击了选项
            if (G_DropdownOpen)
            {
                int optionHeight = 45;
                int listY = dropdownY + dropdownHeight;

                for (int i = 0; i < RETENTION_COUNT; i++)
                {
                    int optionY = listY + i * optionHeight;
                    if (x >= dropdownX && x <= dropdownX + dropdownWidth && y >= optionY && y <= optionY + optionHeight)
                    {
                        // 点击当前选中的选项，只关闭下拉菜单
                        if (i == G_SelectedRetentionIndex)
                        {
                            G_DropdownOpen = false;
                        }
                        else
                        {
                            // 点击其他选项，更新设置
                            G_SelectedRetentionIndex = i;
                            G_RetentionDays = RETENTION_OPTIONS[i];
                            G_Storage.SaveSettings (G_RetentionDays, G_MaxRecords);
                            G_DropdownOpen = false;
                        }
                        InvalidateRect (hWnd, NULL, TRUE);
                        return 0;
                    }
                }
            }

            // 检查是否点击了开机自启开关
            int toggleWidth = 70;
            int toggleHeight = 36;
            int toggleX = G_WindowWidth - toggleWidth - 40;
            int toggleY = 153;

            if (x >= toggleX && x <= toggleX + toggleWidth && y >= toggleY && y <= toggleY + toggleHeight)
            {
                G_AutoStart = !G_AutoStart;
                Storage::SetAutoStart (G_AutoStart);
                G_Storage.SaveAutoStartSetting (G_AutoStart);
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了关闭时最小化到托盘开关
            int minimizeToggleWidth = 70;
            int minimizeToggleHeight = 36;
            int minimizeToggleX = G_WindowWidth - minimizeToggleWidth - 40;
            int minimizeToggleY = 213;

            if (x >= minimizeToggleX && x <= minimizeToggleX + minimizeToggleWidth && y >= minimizeToggleY && y <= minimizeToggleY + minimizeToggleHeight)
            {
                G_MinimizeToTray = !G_MinimizeToTray;
                G_Storage.SaveMinimizeToTraySetting (G_MinimizeToTray);
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了版本信息入口
            if (x >= 20 && x <= G_WindowWidth - 20 && y >= 280 && y <= 320)
            {
                G_CurrentPage = PAGE_VERSION;
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了问题反馈入口
            if (x >= 20 && x <= G_WindowWidth - 20 && y >= 340 && y <= 380)
            {
                G_CurrentPage = PAGE_FEEDBACK;
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了 GitHub 链接
            int githubY = G_WindowHeight - 60;
            if (x >= 180 && x <= 180 + 500 && y >= githubY && y <= githubY + 25)
            {
                // 打开默认浏览器
                ShellExecuteW (NULL, L"open", APP_GITHUB_URL, NULL, NULL, SW_SHOWNORMAL);
                return 0;
            }
        }
        else if (G_CurrentPage == PAGE_VERSION)
        {
            // 版本号页面点击处理

            // 检查是否点击了返回按钮
            if (x >= 20 && x <= 80 && y >= 8 && y <= 38)
            {
                G_CurrentPage = PAGE_SETTINGS;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }
        }
        else if (G_CurrentPage == PAGE_FEEDBACK)
        {
            // 问题反馈页面点击处理

            // 检查是否点击了返回按钮
            if (x >= 20 && x <= 80 && y >= 8 && y <= 38)
            {
                G_CurrentPage = PAGE_SETTINGS;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }
        }

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
int main (int argc, char* argv[])
{
    // 检查是否有 --minimized 参数（开机自启时使用）
    bool startMinimized = false;
    for (int i = 1; i < argc; i++)
    {
        if (string (argv[i]) == "--minimized")
        {
            startMinimized = true;
            break;
        }
    }

    // 获取exe所在目录并切换到该目录
    wstring exeDir = GetExeDir ();
    SetCurrentDirectoryW (exeDir.c_str ());

    // 初始化存储系统
    G_Storage.SetRootDir (exeDir);
    G_Storage.Initialize ();

    // 设置剪贴板管理器的根目录
    G_ClipManager.SetRootDir (exeDir);

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

    // 根据加载的设置更新选中的保存时间索引
    for (int i = 0; i < RETENTION_COUNT; i++)
    {
        if (RETENTION_OPTIONS[i] == G_RetentionDays)
        {
            G_SelectedRetentionIndex = i;
            break;
        }
    }

    // 加载开机自启设置并同步注册表
    G_Storage.LoadAutoStartSetting (G_AutoStart);
    if (G_AutoStart)
    {
        // 确保注册表中有自启项
        Storage::SetAutoStart (true);
    }
    else
    {
        // 确保注册表中无自启项
        Storage::SetAutoStart (false);
    }

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
        return 0;
    }

    // 初始化剪贴板管理器
    if (!G_ClipManager.Initialize (hWnd))
    {
        return 0;
    }

    // 初始化GDI+
    G_ClipManager.InitializeGdiplus ();

    // 添加托盘图标
    AddTrayIcon (hWnd);

    // 加载最小化到托盘设置
    G_Storage.LoadMinimizeToTraySetting (G_MinimizeToTray);

    // 根据启动方式决定是否显示窗口
    if (startMinimized)
    {
        // 开机自启时最小化到托盘
        MinimizeToTray (hWnd);
    }
    else
    {
        // 正常启动显示窗口
        ShowWindow (hWnd, SW_SHOW);
        UpdateWindow (hWnd);
    }

    // 消息循环
    MSG msg = {};
    while (GetMessage (&msg, NULL, 0, 0))
    {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }

    return 0;
}
