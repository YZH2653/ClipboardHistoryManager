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

// 版本号
const wchar_t* APP_VERSION = L"1.3.0.0";
const wchar_t* APP_UPDATE_DATE = L"2026-06-05";
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
    wstring FullPath (path);
    size_t pos = FullPath.find_last_of (L"\\");
    if (pos != wstring::npos)
    {
        return FullPath.substr (0, pos);
    }
    return FullPath;
}

// 获取筛选后的记录（置顶优先，时间倒序）
// 检查搜索文本是否匹配时间
bool MatchTimeFilter (const wstring & SearchText, time_t Timestamp)
{
    // 转换时间为字符串
    struct tm TimeInfo;
    localtime_s (&TimeInfo, &Timestamp);

    // 格式化日期：YYYY-MM-DD
    wchar_t DateStr[20];
    wcsftime (DateStr, 20, L"%Y-%m-%d", &TimeInfo);

    // 格式化日期：MM-DD
    wchar_t ShortDateStr[10];
    wcsftime (ShortDateStr, 10, L"%m-%d", &TimeInfo);

    // 格式化时间：HH:MM
    wchar_t TimeStr[10];
    wcsftime (TimeStr, 10, L"%H:%M", &TimeInfo);

    // 格式化年月：YYYY-MM
    wchar_t YearMonthStr[10];
    wcsftime (YearMonthStr, 10, L"%Y-%m", &TimeInfo);

    // 检查是否匹配日期格式（YYYY-MM-DD）
    if (SearchText.length () == 10 && SearchText[4] == L'-' && SearchText[7] == L'-')
    {
        return wcsstr (DateStr, SearchText.c_str ()) != NULL;
    }

    // 检查是否匹配短日期格式（MM-DD）
    if (SearchText.length () == 5 && SearchText[2] == L'-')
    {
        return wcsstr (ShortDateStr, SearchText.c_str ()) != NULL;
    }

    // 检查是否匹配时间格式（HH:MM）
    if (SearchText.length () == 5 && SearchText[2] == L':')
    {
        return wcsstr (TimeStr, SearchText.c_str ()) != NULL;
    }

    // 检查是否匹配年月格式（YYYY-MM）
    if (SearchText.length () == 7 && SearchText[4] == L'-')
    {
        return wcsstr (YearMonthStr, SearchText.c_str ()) != NULL;
    }

    // 检查是否只包含数字（可能是年份、月份、日期、小时）
    bool IsNumber = true;
    for (wchar_t Ch : SearchText)
    {
        if (!iswdigit (Ch) && Ch != L'-')
        {
            IsNumber = false;
            break;
        }
    }

    if (IsNumber)
    {
        // 尝试匹配年份
        if (SearchText.length () == 4)
        {
            wchar_t YearStr[6];
            wcsftime (YearStr, 6, L"%Y", &TimeInfo);
            return wcsstr (YearStr, SearchText.c_str ()) != NULL;
        }

        // 尝试匹配月份
        if (SearchText.length () == 2 || SearchText.length () == 1)
        {
            wchar_t MonthStr[4];
            wcsftime (MonthStr, 4, L"%m", &TimeInfo);
            return wcsstr (MonthStr, SearchText.c_str ()) != NULL;
        }

        // 尝试匹配日期
        if (SearchText.length () == 2 || SearchText.length () == 1)
        {
            wchar_t DayStr[4];
            wcsftime (DayStr, 4, L"%d", &TimeInfo);
            return wcsstr (DayStr, SearchText.c_str ()) != NULL;
        }
    }

    return false;
}

// 获取筛选后的记录（置顶优先，时间倒序）
vector<ClipRecord> GetFilteredRecords ()
{
    vector<ClipRecord> result;
    const vector<ClipRecord> & allRecords = G_ClipManager.GetRecords ();

    for (const auto & record : allRecords)
    {
        // 搜索过滤
        if (!G_SearchText.empty ())
        {
            bool TextMatch = (record.preview.find (G_SearchText) != wstring::npos ||
                             record.content.find (G_SearchText) != wstring::npos);
            bool TimeMatch = MatchTimeFilter (G_SearchText, record.timestamp);

            // 文字搜索或时间搜索
            if (!TextMatch && !TimeMatch)
            {
                continue;
            }
        }
        result.push_back (record);
    }

    // 排序：置顶优先，然后按时间倒序
    sort (result.begin (), result.end (), [] (const ClipRecord & a, const ClipRecord & b)
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
    COLORREF BgColor = G_SearchFocused ? RGB (255, 255, 255) : RGB (245, 245, 245);
    HBRUSH BgBrush = CreateSolidBrush (BgColor);
    RECT BgRect = { x, y, x + width, y + 50 };
    FillRect (hdc, &BgRect, BgBrush);
    DeleteObject (BgBrush);

    // 绘制边框（获得焦点时高亮）
    COLORREF BorderColor = G_SearchFocused ? RGB (100, 149, 237) : RGB (200, 200, 200);
    HPEN BorderPen = CreatePen (PS_SOLID, 2, BorderColor);
    SelectObject (hdc, BorderPen);
    Rectangle (hdc, x, y, x + width, y + 50);
    DeleteObject (BorderPen);

    // 绘制搜索图标
    SetTextColor (hdc, RGB (150, 150, 150));
    SetBkMode (hdc, TRANSPARENT);
    HFONT IconFont = CreateFont (22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
    SelectObject (hdc, IconFont);
    TextOut (hdc, x + 15, y + 12, L"🔍", 1);
    DeleteObject (IconFont);

    // 绘制输入文本
    HFONT InputFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, InputFont);

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

    DeleteObject (InputFont);

    // 绘制光标（获得焦点时显示）
    if (G_SearchFocused)
    {
        // 计算光标位置
        SIZE TextSize = { 0, 0 };
        if (!G_SearchText.empty ())
        {
            HFONT TempFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, TempFont);
            GetTextExtentPoint32 (hdc, G_SearchText.c_str (), G_SearchText.length (), &TextSize);
            DeleteObject (TempFont);
        }

        // 绘制光标竖线
        HPEN CursorPen = CreatePen (PS_SOLID, 2, RGB (33, 33, 33));
        SelectObject (hdc, CursorPen);
        MoveToEx (hdc, x + 50 + TextSize.cx + 2, y + 10, NULL);
        LineTo (hdc, x + 50 + TextSize.cx + 2, y + 40);
        DeleteObject (CursorPen);
    }
}

// 绘制按钮
void DrawButton (HDC hdc, int x, int y, int width, int height, const wstring & text, bool IsHovered)
{
    // 绘制背景
    COLORREF BgColor = IsHovered ? RGB (200, 200, 200) : RGB (230, 230, 230);
    HBRUSH BgBrush = CreateSolidBrush (BgColor);
    RECT BgRect = { x, y, x + width, y + height };
    FillRect (hdc, &BgRect, BgBrush);
    DeleteObject (BgBrush);

    // 绘制边框
    HPEN BorderPen = CreatePen (PS_SOLID, 1, RGB (180, 180, 180));
    SelectObject (hdc, BorderPen);
    Rectangle (hdc, x, y, x + width, y + height);
    DeleteObject (BorderPen);

    // 绘制文字
    SetTextColor (hdc, RGB (80, 80, 80));
    SetBkMode (hdc, TRANSPARENT);
    HFONT BtnFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, BtnFont);

    SIZE TextSize;
    GetTextExtentPoint32 (hdc, text.c_str (), text.length (), &TextSize);
    int TextX = x + (width - TextSize.cx) / 2;
    int TextY = y + (height - TextSize.cy) / 2;
    TextOut (hdc, TextX, TextY, text.c_str (), text.length ());

    DeleteObject (BtnFont);
}

// 绘制设置按钮（齿轮图标）
void DrawSettingsButton (HDC hdc, int x, int y, bool IsHovered)
{
    int size = 32;

    // 绘制背景
    COLORREF BgColor = IsHovered ? RGB (220, 220, 220) : RGB (240, 240, 240);
    HBRUSH BgBrush = CreateSolidBrush (BgColor);
    RECT BgRect = { x, y, x + size, y + size };
    FillRect (hdc, &BgRect, BgBrush);
    DeleteObject (BgBrush);

    // 绘制齿轮图标
    SetTextColor (hdc, RGB (100, 100, 100));
    SetBkMode (hdc, TRANSPARENT);
    HFONT IconFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
    SelectObject (hdc, IconFont);
    TextOut (hdc, x + 6, y + 4, L"⚙", 1);
    DeleteObject (IconFont);
}

// 绘制返回按钮
void DrawBackButton (HDC hdc, int x, int y, bool IsHovered)
{
    int width = 60;
    int height = 30;

    // 绘制背景
    COLORREF BgColor = IsHovered ? RGB (200, 200, 200) : RGB (230, 230, 230);
    HBRUSH BgBrush = CreateSolidBrush (BgColor);
    RECT BgRect = { x, y, x + width, y + height };
    FillRect (hdc, &BgRect, BgBrush);
    DeleteObject (BgBrush);

    // 绘制边框
    HPEN BorderPen = CreatePen (PS_SOLID, 1, RGB (180, 180, 180));
    SelectObject (hdc, BorderPen);
    Rectangle (hdc, x, y, x + width, y + height);
    DeleteObject (BorderPen);

    // 绘制文字
    SetTextColor (hdc, RGB (80, 80, 80));
    SetBkMode (hdc, TRANSPARENT);
    HFONT BtnFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, BtnFont);
    TextOut (hdc, x + 12, y + 7, L"← 返回", 5);
    DeleteObject (BtnFont);
}

// 绘制设置页面
void DrawSettingsPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT TitleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, TitleFont);
    TextOut (hdc, 100, 12, L"设置", 2);
    DeleteObject (TitleFont);

    // 创建分割线画笔（整个函数复用）
    HPEN LinePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));

    // 绘制分割线
    SelectObject (hdc, LinePen);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);

    // 保存时间设置
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT SectionFont = CreateFont (26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, SectionFont);
    TextOut (hdc, 20, 80, L"保存时间", 4);
    DeleteObject (SectionFont);

    // 绘制下拉菜单框（在右边）
    int DropdownWidth = 200;
    int DropdownHeight = 40;
    int DropdownX = G_WindowWidth - DropdownWidth - 40;
    int DropdownY = 75;

    // 绘制下拉框背景
    COLORREF BgColor = RGB (255, 255, 255);
    HBRUSH BgBrush = CreateSolidBrush (BgColor);
    RECT BgRect = { DropdownX, DropdownY, DropdownX + DropdownWidth, DropdownY + DropdownHeight };
    FillRect (hdc, &BgRect, BgBrush);
    DeleteObject (BgBrush);

    // 绘制边框（临时切换画笔，用完恢复）
    HPEN BorderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
    HPEN PrevPen = (HPEN)SelectObject (hdc, BorderPen);
    Rectangle (hdc, DropdownX, DropdownY, DropdownX + DropdownWidth, DropdownY + DropdownHeight);
    SelectObject (hdc, PrevPen);
    DeleteObject (BorderPen);

    // 绘制当前选中的值
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT ValueFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, ValueFont);
    TextOut (hdc, DropdownX + 15, DropdownY + 10, RETENTION_LABELS[G_SelectedRetentionIndex], wcslen (RETENTION_LABELS[G_SelectedRetentionIndex]));

    // 绘制下拉箭头
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, DropdownX + DropdownWidth - 25, DropdownY + 10, L"▼", 1);
    DeleteObject (ValueFont);

    // 绘制分割线
    MoveToEx (hdc, 20, 140, NULL);
    LineTo (hdc, G_WindowWidth - 20, 140);

    // 开机自启设置
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT AutoStartFont = CreateFont (26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, AutoStartFont);
    TextOut (hdc, 20, 158, L"开机自启", 4);

    // 绘制开关按钮
    int ToggleWidth = 70;
    int ToggleHeight = 36;
    int ToggleX = G_WindowWidth - ToggleWidth - 40;
    int ToggleY = 153;

    // 开关背景色
    COLORREF ToggleBgColor = G_AutoStart ? RGB (74, 144, 217) : RGB (200, 200, 200);
    HBRUSH ToggleBgBrush = CreateSolidBrush (ToggleBgColor);
    RECT ToggleRect = { ToggleX, ToggleY, ToggleX + ToggleWidth, ToggleY + ToggleHeight };
    FillRect (hdc, &ToggleRect, ToggleBgBrush);
    DeleteObject (ToggleBgBrush);

    // 绘制开关圆角边框（临时切换画笔和画刷，用完恢复）
    HPEN TogglePen = CreatePen (PS_SOLID, 1, ToggleBgColor);
    HBRUSH NullBrush = (HBRUSH)GetStockObject (NULL_BRUSH);
    PrevPen = (HPEN)SelectObject (hdc, TogglePen);
    HBRUSH PrevBrush = (HBRUSH)SelectObject (hdc, NullBrush);
    RoundRect (hdc, ToggleX, ToggleY, ToggleX + ToggleWidth, ToggleY + ToggleHeight, ToggleHeight, ToggleHeight);
    SelectObject (hdc, PrevBrush);
    SelectObject (hdc, PrevPen);
    DeleteObject (TogglePen);

    // 绘制开关文字
    SetBkMode (hdc, TRANSPARENT);
    SetTextColor (hdc, RGB (255, 255, 255));
    HFONT ToggleFont = CreateFont (22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, ToggleFont);
    const wchar_t* ToggleText = G_AutoStart ? L"开" : L"关";
    SIZE ToggleTextSize;
    GetTextExtentPoint32 (hdc, ToggleText, 1, &ToggleTextSize);
    int ToggleTextX = ToggleX + (ToggleWidth - ToggleTextSize.cx) / 2;
    int ToggleTextY = ToggleY + (ToggleHeight - ToggleTextSize.cy) / 2;
    TextOut (hdc, ToggleTextX, ToggleTextY, ToggleText, 1);
    DeleteObject (ToggleFont);
    DeleteObject (AutoStartFont);

    // 绘制分割线
    MoveToEx (hdc, 20, 200, NULL);
    LineTo (hdc, G_WindowWidth - 20, 200);

    // 版本信息入口
    int VersionY = 220;
    HFONT VersionFont = CreateFont (26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, VersionFont);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 20, VersionY, L"版本信息", 4);

    // 绘制箭头
    SetTextColor (hdc, RGB (150, 150, 150));
    TextOut (hdc, G_WindowWidth - 40, VersionY, L"→", 1);

    // 绘制分割线
    MoveToEx (hdc, 20, VersionY + 40, NULL);
    LineTo (hdc, G_WindowWidth - 20, VersionY + 40);

    // 问题反馈入口
    int FeedbackY = 280;
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 20, FeedbackY, L"问题反馈", 4);

    // 绘制箭头
    SetTextColor (hdc, RGB (150, 150, 150));
    TextOut (hdc, G_WindowWidth - 40, FeedbackY, L"→", 1);
    DeleteObject (VersionFont);

    // 绘制分割线
    MoveToEx (hdc, 20, FeedbackY + 40, NULL);
    LineTo (hdc, G_WindowWidth - 20, FeedbackY + 40);

    // GitHub 仓库地址
    int GithubY = G_WindowHeight - 60;
    HFONT GithubFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, GithubFont);
    SetTextColor (hdc, RGB (100, 100, 100));
    SetBkMode (hdc, TRANSPARENT);
    TextOut (hdc, 20, GithubY, L"GitHub仓库地址:", 8);

    // 绘制可点击的链接
    SetTextColor (hdc, RGB (100, 149, 237));
    TextOut (hdc, 200, GithubY, APP_GITHUB_URL, wcslen (APP_GITHUB_URL));
    DeleteObject (GithubFont);

    // 最后绘制下拉菜单选项列表（确保在最上层）
    if (G_DropdownOpen)
    {
        int OptionHeight = 45;
        int ListY = DropdownY + DropdownHeight;

        for (int i = 0; i < RETENTION_COUNT; i++)
        {
            int OptionY = ListY + i * OptionHeight;
            bool IsSelected = (i == G_SelectedRetentionIndex);

            // 绘制选项背景（当前选中的高亮显示）
            COLORREF OptBgColor = IsSelected ? RGB (230, 240, 255) : RGB (255, 255, 255);
            HBRUSH OptBgBrush = CreateSolidBrush (OptBgColor);
            RECT OptBgRect = { DropdownX, OptionY, DropdownX + DropdownWidth, OptionY + OptionHeight };
            FillRect (hdc, &OptBgRect, OptBgBrush);
            DeleteObject (OptBgBrush);

            // 绘制边框
            HPEN OptBorderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
            HPEN SavedPen = (HPEN)SelectObject (hdc, OptBorderPen);
            Rectangle (hdc, DropdownX, OptionY, DropdownX + DropdownWidth, OptionY + OptionHeight);
            SelectObject (hdc, SavedPen);
            DeleteObject (OptBorderPen);

            // 绘制文字（当前选中的加一个✓标记）
            COLORREF TextColor = IsSelected ? RGB (100, 149, 237) : RGB (33, 33, 33);
            SetTextColor (hdc, TextColor);
            SetBkMode (hdc, TRANSPARENT);
            HFONT OptionFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, OptionFont);

            SIZE TextSize;
            GetTextExtentPoint32 (hdc, RETENTION_LABELS[i], wcslen (RETENTION_LABELS[i]), &TextSize);
            int TextX = DropdownX + (DropdownWidth - TextSize.cx) / 2;
            int TextY = OptionY + (OptionHeight - TextSize.cy) / 2;
            TextOut (hdc, TextX, TextY, RETENTION_LABELS[i], wcslen (RETENTION_LABELS[i]));

            // 如果是当前选中的，在右边显示✓
            if (IsSelected)
            {
                TextOut (hdc, DropdownX + DropdownWidth - 30, TextY, L"✓", 1);
            }

            DeleteObject (OptionFont);
        }
    }

    // 清理分割线画笔
    DeleteObject (LinePen);
}

// 绘制版本号页面
void DrawVersionPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT TitleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, TitleFont);
    TextOut (hdc, 100, 12, L"版本信息", 4);
    DeleteObject (TitleFont);

    // 绘制分割线
    HPEN LinePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, LinePen);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);
    DeleteObject (LinePen);

    // 版本信息内容
    int ContentY = 80;
    int LineHeight = 50;

    HFONT ContentFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, ContentFont);

    // 版本号
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, ContentY, L"版本号", 3);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 120, ContentY, APP_VERSION, wcslen (APP_VERSION));
    ContentY += LineHeight;

    // 更新日期
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, ContentY, L"更新日期", 4);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 120, ContentY, APP_UPDATE_DATE, wcslen (APP_UPDATE_DATE));
    ContentY += LineHeight;

    // 更新内容
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, ContentY, L"更新内容", 4);
    ContentY += LineHeight;

    // 更新内容列表
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 40, ContentY, L"• 新增设置页面", 8);
    ContentY += 35;
    TextOut (hdc, 40, ContentY, L"• 支持保存时间配置", 10);
    ContentY += 35;
    TextOut (hdc, 40, ContentY, L"• 显示版本信息", 8);
    ContentY += LineHeight + 20;

    // 作者
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, ContentY, L"作者", 2);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 120, ContentY, APP_AUTHOR, wcslen (APP_AUTHOR));

    DeleteObject (ContentFont);
}

// 绘制问题反馈页面
void DrawFeedbackPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT TitleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, TitleFont);
    TextOut (hdc, 100, 12, L"问题反馈", 4);
    DeleteObject (TitleFont);

    // 绘制分割线
    HPEN LinePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, LinePen);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);
    DeleteObject (LinePen);

    // 问题反馈内容
    int ContentY = 80;
    int LineHeight = 45;

    HFONT ContentFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, ContentFont);

    // 作者邮箱
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, ContentY, L"作者邮箱", 4);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 130, ContentY, APP_AUTHOR_EMAIL, wcslen (APP_AUTHOR_EMAIL));
    ContentY += LineHeight + 25;

    // 反馈格式说明
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT HintFont = CreateFont (28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, HintFont);
    TextOut (hdc, 20, ContentY, L"请按以下格式写:", 8);
    DeleteObject (HintFont);
    ContentY += LineHeight;

    // 反馈格式内容
    HFONT FormatFont = CreateFont (22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, FormatFont);

    SetTextColor (hdc, RGB (80, 80, 80));
    TextOut (hdc, 40, ContentY, L"软件版本(可在版本号中查看):", 15);
    ContentY += 35;
    TextOut (hdc, 40, ContentY, L"出现问题的时间:", 8);
    ContentY += 35;
    TextOut (hdc, 40, ContentY, L"描述问题(可用图片表示):", 12);

    DeleteObject (FormatFont);
    DeleteObject (ContentFont);
}

// 绘制卡片
void DrawCard (HDC hdc, int x, int y, int width, const ClipRecord & record, bool IsHovered)
{
    // 绘制卡片背景
    COLORREF BgColor = IsHovered ? RGB (245, 248, 255) : RGB (255, 255, 255);
    HBRUSH CardBg = CreateSolidBrush (BgColor);
    RECT CardRect = { x, y, x + width, y + 100 };
    FillRect (hdc, &CardRect, CardBg);
    DeleteObject (CardBg);

    // 绘制边框
    COLORREF BorderColor = IsHovered ? RGB (100, 149, 237) : RGB (220, 220, 220);
    HPEN BorderPen = CreatePen (PS_SOLID, 1, BorderColor);
    SelectObject (hdc, BorderPen);
    Rectangle (hdc, x, y, x + width, y + 100);
    DeleteObject (BorderPen);

    // 绘制左侧彩色条
    COLORREF AccentColor = record.isPinned ? RGB (255, 165, 0) : RGB (100, 149, 237);
    HBRUSH AccentBrush = CreateSolidBrush (AccentColor);
    RECT AccentRect = { x, y, x + 4, y + 100 };
    FillRect (hdc, &AccentRect, AccentBrush);
    DeleteObject (AccentBrush);

    // 绘制内容预览
    int ContentX = x + 15;
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT ContentFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, ContentFont);

    wstring Preview = record.preview;
    if (Preview.length () > 60)
    {
        Preview = Preview.substr (0, 60) + L"...";
    }
    TextOut (hdc, ContentX, y + 12, Preview.c_str (), Preview.length ());
    DeleteObject (ContentFont);

    // 绘制时间
    time_t Timestamp = record.timestamp;
    struct tm TimeInfo;
    localtime_s (&TimeInfo, &Timestamp);
    wchar_t TimeStr[32];
    wcsftime (TimeStr, 32, L"%Y-%m-%d %H:%M", &TimeInfo);
    SetTextColor (hdc, RGB (150, 150, 150));
    HFONT TimeFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, TimeFont);
    TextOut (hdc, ContentX, y + 65, TimeStr, wcslen (TimeStr));
    DeleteObject (TimeFont);

    // 绘制操作按钮
    int ButtonY = y + 100 - 32;
    int ButtonX = x + width - 200;

    // 复制按钮
    DrawButton (hdc, ButtonX, ButtonY, 55, 26, L"复制", false);

    // 置顶按钮
    wstring PinText = record.isPinned ? L"取消" : L"置顶";
    DrawButton (hdc, ButtonX + 65, ButtonY, 55, 26, PinText, false);

    // 删除按钮
    DrawButton (hdc, ButtonX + 130, ButtonY, 55, 26, L"删除", false);
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
        vector<ClipRecord> & records = const_cast<vector<ClipRecord> &> (G_ClipManager.GetRecords ());
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
            int ContentWidth = G_WindowWidth - 40;
            int SearchWidth = ContentWidth;
            int CardWidth = ContentWidth;

            // 绘制标题
            SetTextColor (hdc, RGB (33, 33, 33));
            SetBkMode (hdc, TRANSPARENT);
            HFONT TitleFont = CreateFont (32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
            SelectObject (hdc, TitleFont);
            TextOut (hdc, 20, 12, L"历史剪贴板管理器", 8);
            DeleteObject (TitleFont);

            // 绘制设置按钮
            DrawSettingsButton (hdc, G_WindowWidth - 52, 10, false);

            // 绘制搜索框
            DrawSearchBox (hdc, 20, 50, SearchWidth);

            // 绘制分割线
            HPEN LinePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
            SelectObject (hdc, LinePen);
            MoveToEx (hdc, 20, 100, NULL);
            LineTo (hdc, 20 + SearchWidth, 100);
            DeleteObject (LinePen);

            // 绘制历史记录列表
            vector<ClipRecord> records = GetFilteredRecords ();
            int CardY = 110 - G_ScrollOffset;
            int CardHeight = 100;
            int CardMargin = 10;

            for (int i = 0; i < (int)records.size (); i++)
            {
                // 检查是否超出可视区域
                if (CardY + CardHeight > G_WindowHeight - 20)
                {
                    break;
                }

                // 跳过在可视区域上方的卡片
                if (CardY + CardHeight < 0)
                {
                    CardY += CardHeight + CardMargin;
                    continue;
                }

                // 绘制卡片
                bool IsHovered = (i == G_HoverIndex);
                DrawCard (hdc, 20, CardY, CardWidth, records[i], IsHovered);
                CardY += CardHeight + CardMargin;
            }

            // 如果没有记录，显示提示
            if (records.empty ())
            {
                HFONT HintFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
                SelectObject (hdc, HintFont);
                SetTextColor (hdc, RGB (150, 150, 150));
                wstring HintText = G_SearchText.empty () ? L"暂无历史记录，请复制内容测试" : L"未找到匹配的记录";
                TextOut (hdc, G_WindowWidth / 2 - 150, G_WindowHeight / 2, HintText.c_str (), HintText.length ());
                DeleteObject (HintFont);
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
        int CardY = 110 - G_ScrollOffset;
        int CardHeight = 100;
        int CardMargin = 10;
        vector<ClipRecord> records = GetFilteredRecords ();
        int NewHoverIndex = -1;

        for (int i = 0; i < (int)records.size (); i++)
        {
            if (y >= CardY && y < CardY + CardHeight && x >= 20 && x <= G_WindowWidth - 20)
            {
                NewHoverIndex = i;
                break;
            }
            CardY += CardHeight + CardMargin;
        }

        // 只有当悬停索引改变时才刷新窗口
        if (NewHoverIndex != G_HoverIndex)
        {
            G_HoverIndex = NewHoverIndex;
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
            int CardY = 110 - G_ScrollOffset;
            int CardHeight = 100;
            int CardMargin = 10;
            vector<ClipRecord> records = GetFilteredRecords ();

        for (int i = 0; i < (int)records.size (); i++)
        {
            if (y >= CardY && y < CardY + CardHeight && x >= 20 && x <= G_WindowWidth - 20)
            {
                // 检查是否点击了按钮
                int ButtonX = 20 + G_WindowWidth - 40 - 200;
                int ButtonY = CardY + CardHeight - 32;

                if (y >= ButtonY && y <= ButtonY + 26)
                {
                    // 复制按钮
                    if (x >= ButtonX && x <= ButtonX + 55)
                    {
                        wstring CopyText = records[i].content;
                        G_ClipManager.CopyToClipboard (CopyText);
                        return 0;
                    }

                    // 置顶按钮
                    if (x >= ButtonX + 65 && x <= ButtonX + 120)
                    {
                        int RecordId = records[i].id;
                        vector<ClipRecord> & allRecords = const_cast<vector<ClipRecord> &> (G_ClipManager.GetRecords ());
                        for (auto & record : allRecords)
                        {
                            if (record.id == RecordId)
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
                    if (x >= ButtonX + 130 && x <= ButtonX + 185)
                    {
                        int RecordId = records[i].id;
                        vector<ClipRecord> & allRecords = const_cast<vector<ClipRecord> &> (G_ClipManager.GetRecords ());
                        for (auto it = allRecords.begin (); it != allRecords.end (); ++it)
                        {
                            if (it->id == RecordId)
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
                wstring CopyText = records[i].content;
                G_ClipManager.CopyToClipboard (CopyText);

                // 取消搜索框焦点
                G_SearchFocused = false;

                break;
            }
            CardY += CardHeight + CardMargin;
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
            int DropdownWidth = 200;
            int DropdownHeight = 40;
            int DropdownX = G_WindowWidth - DropdownWidth - 40;
            int DropdownY = 75;

            if (x >= DropdownX && x <= DropdownX + DropdownWidth && y >= DropdownY && y <= DropdownY + DropdownHeight)
            {
                G_DropdownOpen = !G_DropdownOpen;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 如果下拉菜单打开，检查是否点击了选项
            if (G_DropdownOpen)
            {
                int OptionHeight = 45;
                int ListY = DropdownY + DropdownHeight;

                for (int i = 0; i < RETENTION_COUNT; i++)
                {
                    int OptionY = ListY + i * OptionHeight;
                    if (x >= DropdownX && x <= DropdownX + DropdownWidth && y >= OptionY && y <= OptionY + OptionHeight)
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
            int ToggleWidth = 70;
            int ToggleHeight = 36;
            int ToggleX = G_WindowWidth - ToggleWidth - 40;
            int ToggleY = 153;

            if (x >= ToggleX && x <= ToggleX + ToggleWidth && y >= ToggleY && y <= ToggleY + ToggleHeight)
            {
                G_AutoStart = !G_AutoStart;
                Storage::SetAutoStart (G_AutoStart);
                G_Storage.SaveAutoStartSetting (G_AutoStart);
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了版本信息入口
            if (x >= 20 && x <= G_WindowWidth - 20 && y >= 220 && y <= 260)
            {
                G_CurrentPage = PAGE_VERSION;
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了问题反馈入口
            if (x >= 20 && x <= G_WindowWidth - 20 && y >= 280 && y <= 320)
            {
                G_CurrentPage = PAGE_FEEDBACK;
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了 GitHub 链接
            int GithubY = G_WindowHeight - 60;
            if (x >= 180 && x <= 180 + 500 && y >= GithubY && y <= GithubY + 25)
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
        int Delta = GET_WHEEL_DELTA_WPARAM (wParam);
        G_ScrollOffset -= Delta / 3;

        // 限制滚动范围
        int MaxScroll = max (0, (int)GetFilteredRecords ().size () * 110 - (G_WindowHeight - 130));
        G_ScrollOffset = max (0, min (G_ScrollOffset, MaxScroll));

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

        wchar_t Ch = (wchar_t)wParam;

        if (Ch == VK_BACK)
        {
            // 退格键
            if (!G_SearchText.empty ())
            {
                G_SearchText.pop_back ();
            }
        }
        else if (Ch >= 32)
        {
            // 可打印字符
            G_SearchText += Ch;
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

        int KeyCode = (int)wParam;

        if (KeyCode == VK_BACK)
        {
            // 退格键
            if (!G_SearchText.empty ())
            {
                G_SearchText.pop_back ();
            }
            InvalidateRect (hWnd, NULL, TRUE);
        }
        else if (KeyCode == VK_DOWN)
        {
            // 向下滚动
            G_ScrollOffset += 40;
            int MaxScroll = max (0, (int)GetFilteredRecords ().size () * 110 - (G_WindowHeight - 130));
            G_ScrollOffset = min (G_ScrollOffset, MaxScroll);
            InvalidateRect (hWnd, NULL, TRUE);
        }
        else if (KeyCode == VK_UP)
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
    wstring ExeDir = GetExeDir ();
    SetCurrentDirectoryW (ExeDir.c_str ());

    // 初始化存储系统
    G_Storage.SetRootDir (ExeDir);
    G_Storage.Initialize ();

    // 设置剪贴板管理器的根目录
    G_ClipManager.SetRootDir (ExeDir);

    // 加载历史记录
    vector<ClipRecord> records;
    G_Storage.LoadRecords (records);
    for (const auto & record : records)
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
    int DeletedCount = G_Storage.DeleteExpiredRecords (records, G_RetentionDays);
    if (DeletedCount > 0)
    {
        G_Storage.SaveRecords (records);
    }

    // 获取模块句柄
    HINSTANCE HInstance = GetModuleHandle (NULL);

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = HInstance;
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
        NULL, NULL, HInstance, NULL
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

    // 显示窗口
    ShowWindow (hWnd, SW_SHOW);
    UpdateWindow (hWnd);

    // 消息循环
    MSG msg = {};
    while (GetMessage (&msg, NULL, 0, 0))
    {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }

    return 0;
}
