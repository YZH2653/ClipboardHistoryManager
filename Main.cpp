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
const wchar_t* APP_VERSION = L"1.9.0.0";
const wchar_t* APP_UPDATE_DATE = L"2026-08-23";
const wchar_t* APP_AUTHOR = L"YZH2653";
const wchar_t* APP_AUTHOR_EMAIL = L"yzh2653@163.com";
const wchar_t* APP_GITHUB_URL = L"https://github.com/YZH2653/ClipboardHistoryManager";

// 页面状态
enum PageState
{
    PAGE_MAIN,
    PAGE_SETTINGS,
    PAGE_VERSION,
    PAGE_FEEDBACK,
    PAGE_CLEANUP_RULES,
    PAGE_CLEANUP_PREVIEW
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

// 批量选择状态
bool G_SelectMode = false;  // 是否进入选择模式
bool G_SelectAll = false;   // 全选状态
vector<int> G_SelectedItems;  // 选中的记录ID列表

// 清理规则编辑状态
bool G_ShowCleanupRuleEditDialog = false;  // 是否显示编辑对话框
CleanupRule G_EditingCleanupRule;          // 当前正在编辑的规则
bool G_IsNewCleanupRule = false;           // 是否是新规则

// 全局快捷键配置
#define HOTKEY_ID_TOGGLE_WINDOW 1  // 显示/隐藏窗口
#define HOTKEY_ID_QUICK_COPY    2  // 快速复制最近记录

UINT G_HotkeyToggleModifiers = MOD_CONTROL | MOD_ALT;  // Ctrl+Alt
UINT G_HotkeyToggleKey = 0x56;  // V 键
UINT G_HotkeyCopyModifiers = MOD_CONTROL | MOD_ALT;    // Ctrl+Alt
UINT G_HotkeyCopyKey = 0x43;    // C 键
bool G_HotkeysRegistered = false;

// 窗口尺寸
int G_WindowWidth = 800;
int G_WindowHeight = 600;

// 托盘图标
NOTIFYICONDATA G_Nid = {};
bool G_TrayIconAdded = false;
bool G_IsMinimizedToTray = false;

// 前向声明
static wstring GetAvailableFont ();

// GDI 对象缓存
struct GDICache
{
    // 字体缓存
    HFONT fontTitle;        // 标题字体 (36px Bold)
    HFONT fontSection;      // 段落字体 (26px)
    HFONT fontContent;      // 内容字体 (24px)
    HFONT fontButton;       // 按钮字体 (18px)
    HFONT fontSmall;        // 小字体 (16px)

    // 画刷缓存
    HBRUSH brushWhite;      // 白色背景
    HBRUSH brushLightGray;  // 浅灰背景
    HBRUSH brushHover;      // 悬停背景
    HBRUSH brushHighlight;  // 高亮背景

    // 画笔缓存
    HPEN penBorder;         // 边框画笔
    HPEN penDivider;        // 分割线画笔
    HPEN penHighlight;      // 高亮画笔

    // 初始化所有缓存对象
    void Initialize ()
    {
        wstring fontName = GetAvailableFont ();
        const wchar_t* fn = fontName.c_str ();

        // 创建字体
        fontTitle = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, fn);
        fontSection = CreateFont (26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, fn);
        fontContent = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, fn);
        fontButton = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, fn);
        fontSmall = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, fn);

        // 创建画刷
        brushWhite = CreateSolidBrush (RGB (255, 255, 255));
        brushLightGray = CreateSolidBrush (RGB (245, 245, 245));
        brushHover = CreateSolidBrush (RGB (230, 240, 255));
        brushHighlight = CreateSolidBrush (RGB (255, 200, 200));

        // 创建画笔
        penBorder = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
        penDivider = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
        penHighlight = CreatePen (PS_SOLID, 2, RGB (100, 149, 237));
    }

    // 释放所有缓存对象
    void Cleanup ()
    {
        // 释放字体
        if (fontTitle) DeleteObject (fontTitle);
        if (fontSection) DeleteObject (fontSection);
        if (fontContent) DeleteObject (fontContent);
        if (fontButton) DeleteObject (fontButton);
        if (fontSmall) DeleteObject (fontSmall);

        // 释放画刷
        if (brushWhite) DeleteObject (brushWhite);
        if (brushLightGray) DeleteObject (brushLightGray);
        if (brushHover) DeleteObject (brushHover);
        if (brushHighlight) DeleteObject (brushHighlight);

        // 释放画笔
        if (penBorder) DeleteObject (penBorder);
        if (penDivider) DeleteObject (penDivider);
        if (penHighlight) DeleteObject (penHighlight);
    }
};
GDICache G_GDICache;

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
    // 重置页面状态为主界面
    G_CurrentPage = PAGE_MAIN;

    // 重置选择模式状态
    G_SelectMode = false;
    G_SelectedItems.clear ();
    G_SelectAll = false;

    // 恢复窗口显示
    ShowWindow (hWnd, SW_RESTORE);
    SetForegroundWindow (hWnd);
    BringWindowToTop (hWnd);
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
    std::wcsftime (dateStr, 20, L"%Y-%m-%d", &timeInfo);

    // 格式化日期：MM-DD
    wchar_t shortDateStr[10];
    std::wcsftime (shortDateStr, 10, L"%m-%d", &timeInfo);

    // 格式化时间：HH:MM
    wchar_t timeStr[10];
    std::wcsftime (timeStr, 10, L"%H:%M", &timeInfo);

    // 格式化年月：YYYY-MM
    wchar_t yearMonthStr[10];
    std::wcsftime (yearMonthStr, 10, L"%Y-%m", &timeInfo);

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
            std::wcsftime (yearStr, 6, L"%Y", &timeInfo);
            return wcsstr (yearStr, searchText.c_str ()) != NULL;
        }

        // 尝试匹配月份
        if (searchText.length () == 2 || searchText.length () == 1)
        {
            wchar_t monthStr[4];
            std::wcsftime (monthStr, 4, L"%m", &timeInfo);
            return wcsstr (monthStr, searchText.c_str ()) != NULL;
        }

        // 尝试匹配日期
        if (searchText.length () == 2 || searchText.length () == 1)
        {
            wchar_t dayStr[4];
            std::wcsftime (dayStr, 4, L"%d", &timeInfo);
            return wcsstr (dayStr, searchText.c_str ()) != NULL;
        }
    }

    return false;
}

// 获取筛选后的记录（置顶优先，时间倒序）
// 筛选结果缓存
struct RecordCache
{
    vector<ClipRecord> filteredRecords;
    wstring lastSearchText;
    int lastRecordCount;
    bool isValid;

    RecordCache () : lastRecordCount (0), isValid (false) {}

    void Invalidate ()
    {
        isValid = false;
    }
};
RecordCache G_RecordCache;

// 获取筛选后的记录（带缓存）
vector<ClipRecord> GetFilteredRecords ()
{
    const vector<ClipRecord>& allRecords = G_ClipManager.GetRecords ();
    int currentCount = (int)allRecords.size ();

    // 检查缓存是否有效
    if (G_RecordCache.isValid
        && G_RecordCache.lastSearchText == G_SearchText
        && G_RecordCache.lastRecordCount == currentCount)
    {
        return G_RecordCache.filteredRecords;
    }

    // 缓存失效，重新计算
    vector<ClipRecord> result;
    result.reserve (currentCount);

    for (const auto& record : allRecords)
    {
        // 搜索过滤
        if (!G_SearchText.empty ())
        {
            bool textMatch = (record.content.find (G_SearchText) != wstring::npos);
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

    // 更新缓存
    G_RecordCache.filteredRecords = result;
    G_RecordCache.lastSearchText = G_SearchText;
    G_RecordCache.lastRecordCount = currentCount;
    G_RecordCache.isValid = true;

    return G_RecordCache.filteredRecords;
}

// 字体缓存
static wstring G_CachedFontName;
static bool G_FontInitialized = false;

// 获取系统可用字体（供 GDICache 使用）
static wstring GetAvailableFont ()
{
    // 如果已经缓存，直接返回
    if (G_FontInitialized && !G_CachedFontName.empty())
    {
        return G_CachedFontName;
    }
    // 扩展的优先字体列表（按优先级排序）
    const wchar_t* priorityFonts[] = {
        // 中文首选字体
        L"Microsoft YaHei UI",      // Windows 8/10/11 默认中文字体
        L"Microsoft YaHei",        // 旧版 Windows 中文字体
        L"SimSun",                 // 宋体
        L"SimHei",                 // 黑体
        L"KaiTi",                  // 楷体
        L"FangSong",               // 仿宋
        L"Segoe UI",               // Windows 默认英文字体
        L"Arial",                  // 通用字体
        L"Calibri",                // Office 套装字体
        L"Times New Roman",        // Times 衬线字体
        L"Consolas",               // 等宽字体

        // 其他备选字体
        L"Tahoma",                 // Windows 经典字体
        L"Verdana",                // Web 常用字体
        L"Trebuchet MS",           // 现代无衬线字体
        L"Lucida Sans",           // 清晰无衬线字体
        L"Helvetica Neue",         // macOS 常见字体

        // 最后的回退选项
        L"MS Sans Serif",          // Windows 95/98 字体
        L"System",                 // 系统默认字体
        L"Roman"                  // 基础衬线字体
    };

    // 字体样式测试
    LOGFONT lf = { 0 };
    lf.lfHeight = -12;  // 使用负值表示字体高度的单位
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    HDC hdc = GetDC (NULL);
    if (hdc == NULL)
    {
        return L"Microsoft YaHei UI";  // 默认字体
    }

    // 依次测试每个字体
    for (const wchar_t* fontName : priorityFonts)
    {
        wcscpy_s (lf.lfFaceName, fontName);
        HFONT hFont = CreateFontIndirect (&lf);
        if (hFont)
        {
            // 验证字体是否真的可用
            HFONT oldFont = (HFONT)SelectObject (hdc, hFont);
            if (oldFont)
            {
                // 尝试绘制中英文字符来测试字体
                TEXTMETRIC tm;
                if (GetTextMetrics (hdc, &tm) && tm.tmHeight > 0)
                {
                    // 额外的字体验证：尝试绘制实际的文本内容
                    const wchar_t* testText = L"测试Test";
                    RECT testRect = { 0, 0, 100, 50 };
                    DrawTextW (hdc, testText, -1, &testRect, DT_CALCRECT | DT_NOPREFIX);
                    if (testRect.right > 10)  // 文本宽度合理
                    {
                        SelectObject (hdc, oldFont);
                        DeleteObject (hFont);
                        ReleaseDC (NULL, hdc);
                        return fontName;  // 返回可用的字体
                    }
                }
                SelectObject (hdc, oldFont);
            }
            DeleteObject (hFont);
        }
    }

    ReleaseDC (NULL, hdc);

    // 如果所有字体都不可用，使用更安全的回退方案
    HDC hdcBackup = GetDC (NULL);
    if (hdcBackup)
    {
        // 尝试获取系统默认字体
        NONCLIENTMETRICSW ncmetrics;
        ncmetrics.cbSize = sizeof (NONCLIENTMETRICSW);
        if (SystemParametersInfoW (SPI_GETNONCLIENTMETRICS, sizeof (ncmetrics), &ncmetrics, 0))
        {
            HFONT defaultFont = CreateFontIndirectW (&ncmetrics.lfMessageFont);
            if (defaultFont)
            {
                HFONT oldFont = (HFONT)SelectObject (hdcBackup, defaultFont);
                if (oldFont)
                {
                    TEXTMETRIC tm;
                    if (GetTextMetrics (hdcBackup, &tm) && tm.tmHeight > 0)
                    {
                        SelectObject (hdcBackup, oldFont);
                        DeleteObject (defaultFont);
                        ReleaseDC (NULL, hdcBackup);
                        return L"系统默认字体";  // 使用系统默认字体
                    }
                    SelectObject (hdcBackup, oldFont);
                }
                DeleteObject (defaultFont);
            }
        }
        ReleaseDC (NULL, hdcBackup);
    }

    // 最终回退
    wstring fallbackFont = L"System";

    // 缓存结果
    G_CachedFontName = fallbackFont;
    G_FontInitialized = true;

    return fallbackFont;
}

// 创建字体对象的辅助函数
HFONT CreateFontHelper(int size, bool bold = false, const wchar_t* faceName = nullptr)
{
    if (!faceName)
    {
        faceName = GetAvailableFont().c_str();
    }
    return CreateFont(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, DEFAULT_PITCH, faceName);
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

    // 绘制搜索图标和输入文本（使用缓存字体）
    SetTextColor (hdc, RGB (150, 150, 150));
    SetBkMode (hdc, TRANSPARENT);
    SelectObject (hdc, G_GDICache.fontContent);
    TextOut (hdc, x + 15, y + 12, L"🔍", 1);

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

    // 绘制光标（获得焦点时显示）
    if (G_SearchFocused)
    {
        // 计算光标位置（使用缓存字体）
        SIZE textSize = { 0, 0 };
        if (!G_SearchText.empty ())
        {
            GetTextExtentPoint32 (hdc, G_SearchText.c_str (), G_SearchText.length (), &textSize);
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
    SelectObject (hdc, G_GDICache.penBorder);
    Rectangle (hdc, x, y, x + width, y + height);

    // 绘制文字（使用缓存字体）
    SetTextColor (hdc, RGB (80, 80, 80));
    SetBkMode (hdc, TRANSPARENT);
    SelectObject (hdc, G_GDICache.fontButton);

    SIZE textSize;
    GetTextExtentPoint32 (hdc, text.c_str (), text.length (), &textSize);
    int textX = x + (width - textSize.cx) / 2;
    int textY = y + (height - textSize.cy) / 2;
    TextOut (hdc, textX, textY, text.c_str (), text.length ());
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

    // 绘制齿轮图标（使用缓存字体）
    SetTextColor (hdc, RGB (100, 100, 100));
    SetBkMode (hdc, TRANSPARENT);
    SelectObject (hdc, G_GDICache.fontContent);
    TextOut (hdc, x + 6, y + 4, L"⚙", 1);
}

// 绘制垃圾桶按钮（选择模式切换）
void DrawDeleteModeButton (HDC hdc, int x, int y, bool isHovered, bool isActive)
{
    int size = 32;

    // 绘制背景
    COLORREF bgColor = isActive ? RGB (255, 200, 200) : (isHovered ? RGB (220, 220, 220) : RGB (240, 240, 240));
    HBRUSH bgBrush = CreateSolidBrush (bgColor);
    RECT bgRect = { x, y, x + size, y + size };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制垃圾桶图标（使用缓存字体）
    SetTextColor (hdc, isActive ? RGB (200, 50, 50) : RGB (100, 100, 100));
    SetBkMode (hdc, TRANSPARENT);
    SelectObject (hdc, G_GDICache.fontContent);
    TextOut (hdc, x + 6, y + 4, L"🗑", 1);
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
    SelectObject (hdc, G_GDICache.penBorder);
    Rectangle (hdc, x, y, x + width, y + height);

    // 绘制文字（使用缓存字体）
    SetTextColor (hdc, RGB (80, 80, 80));
    SetBkMode (hdc, TRANSPARENT);
    SelectObject (hdc, G_GDICache.fontButton);
    TextOut (hdc, x + 12, y + 7, L"← 返回", 5);
}

// 绘制设置页面
void DrawSettingsPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题（使用缓存字体）
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    SelectObject (hdc, G_GDICache.fontTitle);
    TextOut (hdc, 100, 12, L"设置", 2);

    // 使用缓存分割线画笔

    // 绘制分割线
    SelectObject (hdc, G_GDICache.penDivider);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);

    // 保存时间设置（使用缓存字体）
    SetTextColor (hdc, RGB (33, 33, 33));
    SelectObject (hdc, G_GDICache.fontSection);
    TextOut (hdc, 20, 80, L"保存时间", 4);

    // 绘制下拉菜单框（在右边）
    int dropdownWidth = 200;
    int dropdownHeight = 40;
    int dropdownX = G_WindowWidth - dropdownWidth - 40;
    int dropdownY = 75;

    // 绘制下拉框背景（使用缓存画刷）
    RECT bgRect = { dropdownX, dropdownY, dropdownX + dropdownWidth, dropdownY + dropdownHeight };
    FillRect (hdc, &bgRect, G_GDICache.brushWhite);

    // 绘制边框（使用缓存画笔）
    SelectObject (hdc, G_GDICache.penBorder);
    Rectangle (hdc, dropdownX, dropdownY, dropdownX + dropdownWidth, dropdownY + dropdownHeight);

    // 绘制当前选中的值（使用缓存字体）
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    SelectObject (hdc, G_GDICache.fontContent);
    TextOut (hdc, dropdownX + 15, dropdownY + 10, RETENTION_LABELS[G_SelectedRetentionIndex], wcslen (RETENTION_LABELS[G_SelectedRetentionIndex]));

    // 绘制下拉箭头
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, dropdownX + dropdownWidth - 25, dropdownY + 10, L"▼", 1);

    // 绘制分割线
    SelectObject (hdc, G_GDICache.penDivider);
    MoveToEx (hdc, 20, 140, NULL);
    LineTo (hdc, G_WindowWidth - 20, 140);

    // 开机自启设置
    // 开机自启设置（使用缓存字体）
    SetTextColor (hdc, RGB (33, 33, 33));
    SelectObject (hdc, G_GDICache.fontSection);
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

    // 绘制开关圆角边框
    HPEN togglePen = CreatePen (PS_SOLID, 1, toggleBgColor);
    HBRUSH nullBrush = (HBRUSH)GetStockObject (NULL_BRUSH);
    HPEN prevPen = (HPEN)SelectObject (hdc, togglePen);
    HBRUSH prevBrush = (HBRUSH)SelectObject (hdc, nullBrush);
    RoundRect (hdc, toggleX, toggleY, toggleX + toggleWidth, toggleY + toggleHeight, toggleHeight, toggleHeight);
    SelectObject (hdc, prevBrush);
    SelectObject (hdc, prevPen);
    DeleteObject (togglePen);

    // 绘制开关文字（使用缓存字体）
    SetBkMode (hdc, TRANSPARENT);
    SetTextColor (hdc, RGB (255, 255, 255));
    SelectObject (hdc, G_GDICache.fontSection);
    const wchar_t* toggleText = G_AutoStart ? L"开" : L"关";
    SIZE toggleTextSize;
    GetTextExtentPoint32 (hdc, toggleText, 1, &toggleTextSize);
    int toggleTextX = toggleX + (toggleWidth - toggleTextSize.cx) / 2;
    int toggleTextY = toggleY + (toggleHeight - toggleTextSize.cy) / 2;
    TextOut (hdc, toggleTextX, toggleTextY, toggleText, 1);

    // 绘制分割线
    SelectObject (hdc, G_GDICache.penDivider);
    MoveToEx (hdc, 20, 200, NULL);
    LineTo (hdc, G_WindowWidth - 20, 200);

    // 关闭时最小化到托盘设置（使用缓存字体）
    SetTextColor (hdc, RGB (33, 33, 33));
    SelectObject (hdc, G_GDICache.fontSection);
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

    // 绘制开关文字（使用缓存字体）
    SetBkMode (hdc, TRANSPARENT);
    SetTextColor (hdc, RGB (255, 255, 255));
    SelectObject (hdc, G_GDICache.fontSection);
    const wchar_t* minimizeToggleText = G_MinimizeToTray ? L"开" : L"关";
    SIZE minimizeToggleTextSize;
    GetTextExtentPoint32 (hdc, minimizeToggleText, 1, &minimizeToggleTextSize);
    int minimizeToggleTextX = minimizeToggleX + (minimizeToggleWidth - minimizeToggleTextSize.cx) / 2;
    int minimizeToggleTextY = minimizeToggleY + (minimizeToggleHeight - minimizeToggleTextSize.cy) / 2;
    TextOut (hdc, minimizeToggleTextX, minimizeToggleTextY, minimizeToggleText, 1);

    // 绘制分割线
    SelectObject (hdc, G_GDICache.penDivider);
    MoveToEx (hdc, 20, 260, NULL);
    LineTo (hdc, G_WindowWidth - 20, 260);

    // 清理规则入口（使用缓存字体）
    int cleanupY = 280;
    SelectObject (hdc, G_GDICache.fontSection);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 20, cleanupY, L"清理规则", 4);

    // 绘制箭头
    SetTextColor (hdc, RGB (150, 150, 150));
    TextOut (hdc, G_WindowWidth - 40, cleanupY, L"→", 1);

    // 绘制分割线
    MoveToEx (hdc, 20, cleanupY + 40, NULL);
    LineTo (hdc, G_WindowWidth - 20, cleanupY + 40);

    // 版本信息入口（使用缓存字体）
    int versionY = 340;
    SelectObject (hdc, G_GDICache.fontSection);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 20, versionY, L"版本信息", 4);

    // 绘制箭头
    SetTextColor (hdc, RGB (150, 150, 150));
    TextOut (hdc, G_WindowWidth - 40, versionY, L"→", 1);

    // 绘制分割线
    SelectObject (hdc, G_GDICache.penDivider);
    MoveToEx (hdc, 20, versionY + 40, NULL);
    LineTo (hdc, G_WindowWidth - 20, versionY + 40);

    // 问题反馈入口（使用缓存字体）
    int feedbackY = 400;
    SelectObject (hdc, G_GDICache.fontSection);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 20, feedbackY, L"问题反馈", 4);

    // 绘制箭头
    SetTextColor (hdc, RGB (150, 150, 150));
    TextOut (hdc, G_WindowWidth - 40, feedbackY, L"→", 1);

    // 绘制分割线
    SelectObject (hdc, G_GDICache.penDivider);
    MoveToEx (hdc, 20, feedbackY + 40, NULL);
    LineTo (hdc, G_WindowWidth - 20, feedbackY + 40);

    // GitHub 仓库地址（使用缓存字体）
    int githubY = G_WindowHeight - 60;
    SelectObject (hdc, G_GDICache.fontContent);
    SetTextColor (hdc, RGB (100, 100, 100));
    SetBkMode (hdc, TRANSPARENT);
    TextOut (hdc, 20, githubY, L"GitHub仓库地址:", 8);

    // 绘制可点击的链接
    SetTextColor (hdc, RGB (100, 149, 237));
    TextOut (hdc, 200, githubY, APP_GITHUB_URL, wcslen (APP_GITHUB_URL));

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
            HBRUSH optBgBrush = isSelected ? G_GDICache.brushHover : G_GDICache.brushWhite;
            RECT optBgRect = { dropdownX, optionY, dropdownX + dropdownWidth, optionY + optionHeight };
            FillRect (hdc, &optBgRect, optBgBrush);

            // 绘制边框（使用缓存画笔）
            SelectObject (hdc, G_GDICache.penBorder);
            Rectangle (hdc, dropdownX, optionY, dropdownX + dropdownWidth, optionY + optionHeight);

            // 绘制文字（使用缓存字体）
            COLORREF textColor = isSelected ? RGB (100, 149, 237) : RGB (33, 33, 33);
            SetTextColor (hdc, textColor);
            SetBkMode (hdc, TRANSPARENT);
            SelectObject (hdc, G_GDICache.fontContent);

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
        }
    }
}

// 绘制版本号页面
void DrawVersionPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
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

    HFONT contentFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
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
    TextOut (hdc, 40, contentY, L"• Ctrl+Alt+V 快速显示/隐藏窗口", 10);
    contentY += 35;
    TextOut (hdc, 40, contentY, L"• Ctrl+Alt+C 快速复制最近记录", 10);
    contentY += 35;
    TextOut (hdc, 40, contentY, L"• 全局快捷键，随时响应", 9);
    contentY += 35;
    TextOut (hdc, 40, contentY, L"• 更新版本号到1.9.0.0", 8);
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
    HFONT titleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
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

    HFONT contentFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
    SelectObject (hdc, contentFont);

    // 作者邮箱
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, contentY, L"作者邮箱", 4);
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, 130, contentY, APP_AUTHOR_EMAIL, wcslen (APP_AUTHOR_EMAIL));
    contentY += lineHeight + 25;

    // 反馈格式说明
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT hintFont = CreateFont (28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
    SelectObject (hdc, hintFont);
    TextOut (hdc, 20, contentY, L"请按以下格式写:", 8);
    DeleteObject (hintFont);
    contentY += lineHeight;

    // 反馈格式内容
    HFONT formatFont = CreateFont (22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
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

// 绘制清理规则管理页面
void DrawCleanupRulesPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
    SelectObject (hdc, titleFont);
    TextOut (hdc, 100, 12, L"清理规则", 4);
    DeleteObject (titleFont);

    // 绘制分割线
    HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, linePen);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);
    DeleteObject (linePen);

    // 获取清理规则管理器
    CleanupRuleManager& ruleManager = G_Storage.GetCleanupRuleManager ();
    vector<CleanupRule> rules = ruleManager.GetRules ();

    // 绘制规则列表
    int ruleY = 70;
    int ruleHeight = 80;
    int ruleMargin = 10;

    for (int i = 0; i < (int)rules.size (); i++)
    {
        const CleanupRule& rule = rules[i];

        // 绘制规则背景
        COLORREF bgColor = rule.enabled ? RGB (255, 255, 255) : RGB (245, 245, 245);
        HBRUSH bgBrush = CreateSolidBrush (bgColor);
        RECT bgRect = { 20, ruleY, G_WindowWidth - 20, ruleY + ruleHeight };
        FillRect (hdc, &bgRect, bgBrush);
        DeleteObject (bgBrush);

        // 绘制边框
        HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (220, 220, 220));
        SelectObject (hdc, borderPen);
        Rectangle (hdc, 20, ruleY, G_WindowWidth - 20, ruleY + ruleHeight);
        DeleteObject (borderPen);

        // 绘制规则名称
        SetTextColor (hdc, rule.enabled ? RGB (33, 33, 33) : RGB (150, 150, 150));
        SetBkMode (hdc, TRANSPARENT);
        HFONT nameFont = CreateFont (22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
        SelectObject (hdc, nameFont);
        TextOut (hdc, 30, ruleY + 10, rule.name.c_str (), rule.name.length ());
        DeleteObject (nameFont);

        // 绘制规则类型
        wstring typeText;
        switch (rule.type)
        {
        case RULE_BY_TIME:
            typeText = L"按时间清理 - 保留 " + to_wstring (rule.days) + L" 天";
            break;
        case RULE_BY_COUNT:
            typeText = L"按数量清理 - 保留 " + to_wstring (rule.maxRecords) + L" 条";
            break;
        case RULE_BY_SIZE:
            typeText = L"按大小清理 - 保留 " + to_wstring (rule.maxSizeMB) + L" MB";
            break;
        }

        HFONT typeFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
        SelectObject (hdc, typeFont);
        SetTextColor (hdc, rule.enabled ? RGB (100, 100, 100) : RGB (180, 180, 180));
        TextOut (hdc, 30, ruleY + 40, typeText.c_str (), typeText.length ());
        DeleteObject (typeFont);

        // 绘制启用/禁用状态
        wstring statusText = rule.enabled ? L"已启用" : L"已禁用";
        COLORREF statusColor = rule.enabled ? RGB (74, 144, 217) : RGB (200, 200, 200);
        HFONT statusFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
        SelectObject (hdc, statusFont);
        SetTextColor (hdc, statusColor);
        TextOut (hdc, G_WindowWidth - 100, ruleY + 10, statusText.c_str (), statusText.length ());
        DeleteObject (statusFont);

        ruleY += ruleHeight + ruleMargin;
    }

    // 如果没有规则，显示提示
    if (rules.empty ())
    {
        HFONT hintFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
        SelectObject (hdc, hintFont);
        SetTextColor (hdc, RGB (150, 150, 150));
        TextOut (hdc, G_WindowWidth / 2 - 150, G_WindowHeight / 2, L"暂无清理规则", 6);
        DeleteObject (hintFont);
    }

    // 绘制预览按钮
    int previewBtnX = G_WindowWidth - 210;
    int previewBtnY = G_WindowHeight - 50;
    DrawButton (hdc, previewBtnX, previewBtnY, 80, 30, L"预览清理", false);

    // 绘制添加规则按钮
    int addBtnX = G_WindowWidth - 120;
    int addBtnY = G_WindowHeight - 50;
    DrawButton (hdc, addBtnX, addBtnY, 80, 30, L"添加规则", false);
}

// 绘制复选框
void DrawCheckbox (HDC hdc, int x, int y, int size, bool checked, bool hovered)
{
    // 绘制复选框背景
    COLORREF bgColor = hovered ? RGB (240, 240, 240) : RGB (255, 255, 255);
    HBRUSH bgBrush = CreateSolidBrush (bgColor);
    RECT bgRect = { x, y, x + size, y + size };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制边框
    COLORREF borderColor = checked ? RGB (100, 149, 237) : RGB (180, 180, 180);
    HPEN borderPen = CreatePen (PS_SOLID, 2, borderColor);
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + size, y + size);
    DeleteObject (borderPen);

    // 如果选中，绘制勾选标记
    if (checked)
    {
        HPEN checkPen = CreatePen (PS_SOLID, 2, RGB (100, 149, 237));
        SelectObject (hdc, checkPen);

        // 绘制勾选标记
        int startX = x + 4;
        int startY = y + size / 2;
        int midX = x + size / 3;
        int midY = y + size - 4;
        int endX = x + size - 4;
        int endY = y + 4;

        MoveToEx (hdc, startX, startY, NULL);
        LineTo (hdc, midX, midY);
        LineTo (hdc, endX, endY);

        DeleteObject (checkPen);
    }
}

// 绘制清理规则编辑对话框
void DrawCleanupRuleEditDialog (HDC hdc, const CleanupRule& rule, bool isNew)
{
    // 绘制对话框背景
    COLORREF bgColor = RGB (255, 255, 255);
    HBRUSH bgBrush = CreateSolidBrush (bgColor);
    RECT bgRect = { 50, 50, G_WindowWidth - 50, G_WindowHeight - 50 };
    FillRect (hdc, &bgRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制边框
    HPEN borderPen = CreatePen (PS_SOLID, 2, RGB (100, 149, 237));
    SelectObject (hdc, borderPen);
    Rectangle (hdc, 50, 50, G_WindowWidth - 50, G_WindowHeight - 50);
    DeleteObject (borderPen);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
    SelectObject (hdc, titleFont);
    wstring title = isNew ? L"添加清理规则" : L"编辑清理规则";
    TextOut (hdc, 70, 70, title.c_str (), title.length ());
    DeleteObject (titleFont);

    // 绘制分割线
    HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, linePen);
    MoveToEx (hdc, 50, 110, NULL);
    LineTo (hdc, G_WindowWidth - 50, 110);
    DeleteObject (linePen);

    // 绘制规则名称标签
    HFONT labelFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
    SelectObject (hdc, labelFont);
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 70, 130, L"规则名称:", 5);

    // 绘制规则名称输入框
    int inputX = 180;
    int inputY = 125;
    int inputWidth = G_WindowWidth - 250;
    int inputHeight = 30;

    COLORREF inputBgColor = RGB (255, 255, 255);
    HBRUSH inputBgBrush = CreateSolidBrush (inputBgColor);
    RECT inputRect = { inputX, inputY, inputX + inputWidth, inputY + inputHeight };
    FillRect (hdc, &inputRect, inputBgBrush);
    DeleteObject (inputBgBrush);

    HPEN inputBorderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
    SelectObject (hdc, inputBorderPen);
    Rectangle (hdc, inputX, inputY, inputX + inputWidth, inputY + inputHeight);
    DeleteObject (inputBorderPen);

    // 绘制规则名称文本
    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, inputX + 10, inputY + 5, rule.name.c_str (), rule.name.length ());

    // 绘制规则类型标签
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 70, 180, L"规则类型:", 5);

    // 绘制规则类型下拉框
    int dropdownX = 180;
    int dropdownY = 175;
    int dropdownWidth = 200;
    int dropdownHeight = 30;

    COLORREF dropdownBgColor = RGB (255, 255, 255);
    HBRUSH dropdownBgBrush = CreateSolidBrush (dropdownBgColor);
    RECT dropdownRect = { dropdownX, dropdownY, dropdownX + dropdownWidth, dropdownY + dropdownHeight };
    FillRect (hdc, &dropdownRect, dropdownBgBrush);
    DeleteObject (dropdownBgBrush);

    HPEN dropdownBorderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
    SelectObject (hdc, dropdownBorderPen);
    Rectangle (hdc, dropdownX, dropdownY, dropdownX + dropdownWidth, dropdownY + dropdownHeight);
    DeleteObject (dropdownBorderPen);

    // 绘制规则类型文本
    wstring typeText;
    switch (rule.type)
    {
    case RULE_BY_TIME:
        typeText = L"按时间清理";
        break;
    case RULE_BY_COUNT:
        typeText = L"按数量清理";
        break;
    case RULE_BY_SIZE:
        typeText = L"按大小清理";
        break;
    }

    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, dropdownX + 10, dropdownY + 5, typeText.c_str (), typeText.length ());

    // 绘制下拉箭头
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, dropdownX + dropdownWidth - 25, dropdownY + 5, L"▼", 1);

    // 绘制规则参数标签
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 70, 230, L"规则参数:", 5);

    // 绘制规则参数输入框
    int paramX = 180;
    int paramY = 225;
    int paramWidth = 150;
    int paramHeight = 30;

    COLORREF paramBgColor = RGB (255, 255, 255);
    HBRUSH paramBgBrush = CreateSolidBrush (paramBgColor);
    RECT paramRect = { paramX, paramY, paramX + paramWidth, paramY + paramHeight };
    FillRect (hdc, &paramRect, paramBgBrush);
    DeleteObject (paramBgBrush);

    HPEN paramBorderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
    SelectObject (hdc, paramBorderPen);
    Rectangle (hdc, paramX, paramY, paramX + paramWidth, paramY + paramHeight);
    DeleteObject (paramBorderPen);

    // 绘制规则参数文本
    wstring paramText;
    switch (rule.type)
    {
    case RULE_BY_TIME:
        paramText = to_wstring (rule.days);
        break;
    case RULE_BY_COUNT:
        paramText = to_wstring (rule.maxRecords);
        break;
    case RULE_BY_SIZE:
        paramText = to_wstring (rule.maxSizeMB);
        break;
    }

    SetTextColor (hdc, RGB (33, 33, 33));
    TextOut (hdc, paramX + 10, paramY + 5, paramText.c_str (), paramText.length ());

    // 绘制参数单位标签
    wstring unitText;
    switch (rule.type)
    {
    case RULE_BY_TIME:
        unitText = L"天";
        break;
    case RULE_BY_COUNT:
        unitText = L"条";
        break;
    case RULE_BY_SIZE:
        unitText = L"MB";
        break;
    }

    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, paramX + paramWidth + 10, paramY + 5, unitText.c_str (), unitText.length ());

    // 绘制启用/禁用复选框
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 70, 280, L"启用规则:", 5);

    int checkboxX = 180;
    int checkboxY = 275;
    int checkboxSize = 20;
    DrawCheckbox (hdc, checkboxX, checkboxY, checkboxSize, rule.enabled, false);

    // 绘制删除图片复选框
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 70, 320, L"删除图片:", 5);

    int deleteImgCheckboxX = 180;
    int deleteImgCheckboxY = 315;
    DrawCheckbox (hdc, deleteImgCheckboxX, deleteImgCheckboxY, checkboxSize, rule.deleteImages, false);

    // 绘制保存按钮
    int saveBtnX = G_WindowWidth - 200;
    int saveBtnY = G_WindowHeight - 100;
    DrawButton (hdc, saveBtnX, saveBtnY, 80, 30, L"保存", false);

    // 绘制取消按钮
    int cancelBtnX = G_WindowWidth - 110;
    int cancelBtnY = G_WindowHeight - 100;
    DrawButton (hdc, cancelBtnX, cancelBtnY, 80, 30, L"取消", false);

    DeleteObject (labelFont);
}

// 绘制清理规则预览界面
void DrawCleanupRulePreviewPage (HDC hdc)
{
    // 绘制返回按钮
    DrawBackButton (hdc, 20, 10, false);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
    SelectObject (hdc, titleFont);
    TextOut (hdc, 100, 12, L"清理预览", 4);
    DeleteObject (titleFont);

    // 绘制分割线
    HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, linePen);
    MoveToEx (hdc, 20, 55, NULL);
    LineTo (hdc, G_WindowWidth - 20, 55);
    DeleteObject (linePen);

    // 获取清理规则管理器
    CleanupRuleManager& ruleManager = G_Storage.GetCleanupRuleManager ();
    vector<ClipRecord>& records = const_cast<vector<ClipRecord>&> (G_ClipManager.GetRecords ());
    vector<ClipRecord> toDelete = ruleManager.PreviewCleanup (records);

    // 绘制预览信息
    int contentY = 70;
    int lineHeight = 40;

    HFONT infoFont = CreateFont (24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
    SelectObject (hdc, infoFont);

    // 绘制将要清理的记录数量
    SetTextColor (hdc, RGB (33, 33, 33));
    wstring countText = L"将要清理 " + to_wstring (toDelete.size ()) + L" 条记录";
    TextOut (hdc, 20, contentY, countText.c_str (), countText.length ());
    contentY += lineHeight;

    // 绘制清理后的记录数量
    wstring remainingText = L"清理后剩余 " + to_wstring (records.size () - toDelete.size ()) + L" 条记录";
    TextOut (hdc, 20, contentY, remainingText.c_str (), remainingText.length ());
    contentY += lineHeight;

    // 绘制将要释放的存储空间
    long long totalSize = 0;
    for (const auto& record : toDelete)
    {
        if (record.type == CLIP_IMAGE && !record.filePath.empty ())
        {
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            if (GetFileAttributesExW (record.filePath.c_str (), GetFileExInfoStandard, &fileInfo))
            {
                totalSize += ((long long)fileInfo.nFileSizeHigh << 32) + fileInfo.nFileSizeLow;
            }
        }
        else
        {
            totalSize += record.content.length () * sizeof (wchar_t);
        }
    }

    wstring sizeText = L"将要释放 " + to_wstring (totalSize / 1024) + L" KB 存储空间";
    TextOut (hdc, 20, contentY, sizeText.c_str (), sizeText.length ());
    contentY += lineHeight + 20;

    // 绘制将要清理的记录列表
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, 20, contentY, L"将要清理的记录:", 8);
    contentY += 30;

    // 绘制记录列表
    int listY = contentY;
    int listHeight = 30;
    int listMargin = 5;

    for (int i = 0; i < (int)toDelete.size () && i < 10; i++)
    {
        const ClipRecord& record = toDelete[i];

        // 绘制记录背景
        COLORREF bgColor = RGB (255, 255, 255);
        HBRUSH bgBrush = CreateSolidBrush (bgColor);
        RECT bgRect = { 20, listY, G_WindowWidth - 20, listY + listHeight };
        FillRect (hdc, &bgRect, bgBrush);
        DeleteObject (bgBrush);

        // 绘制记录边框
        HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (220, 220, 220));
        SelectObject (hdc, borderPen);
        Rectangle (hdc, 20, listY, G_WindowWidth - 20, listY + listHeight);
        DeleteObject (borderPen);

        // 绘制记录预览
        wstring preview = record.preview;
        if (preview.length () > 50)
        {
            preview = preview.substr (0, 50) + L"...";
        }

        SetTextColor (hdc, RGB (33, 33, 33));
        HFONT previewFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
        SelectObject (hdc, previewFont);
        TextOut (hdc, 30, listY + 5, preview.c_str (), preview.length ());
        DeleteObject (previewFont);

        listY += listHeight + listMargin;
    }

    // 如果超过10条，显示更多提示
    if (toDelete.size () > 10)
    {
        wstring moreText = L"... 还有 " + to_wstring (toDelete.size () - 10) + L" 条记录";
        SetTextColor (hdc, RGB (150, 150, 150));
        TextOut (hdc, 30, listY, moreText.c_str (), moreText.length ());
    }

    // 绘制执行清理按钮
    int executeBtnX = G_WindowWidth - 200;
    int executeBtnY = G_WindowHeight - 60;
    DrawButton (hdc, executeBtnX, executeBtnY, 100, 30, L"执行清理", false);

    // 绘制取消按钮
    int cancelBtnX = G_WindowWidth - 90;
    int cancelBtnY = G_WindowHeight - 60;
    DrawButton (hdc, cancelBtnX, cancelBtnY, 80, 30, L"取消", false);

    DeleteObject (infoFont);
}

// 绘制卡片
void DrawCard (HDC hdc, int x, int y, int width, const ClipRecord& record, bool isHovered, bool isSelected = false)
{
    // 绘制卡片背景
    COLORREF bgColor = isSelected ? RGB (230, 240, 255) : (isHovered ? RGB (245, 248, 255) : RGB (255, 255, 255));
    HBRUSH cardBg = CreateSolidBrush (bgColor);
    RECT cardRect = { x, y, x + width, y + 100 };
    FillRect (hdc, &cardRect, cardBg);
    DeleteObject (cardBg);

    // 绘制边框
    COLORREF borderColor = isSelected ? RGB (100, 149, 237) : (isHovered ? RGB (100, 149, 237) : RGB (220, 220, 220));
    HPEN borderPen = CreatePen (PS_SOLID, 1, borderColor);
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + 100);
    DeleteObject (borderPen);

    // 绘制复选框（在选择模式下显示）
    if (G_SelectMode)
    {
        int checkboxSize = 20;
        int checkboxX = x + 15;
        int checkboxY = y + (100 - checkboxSize) / 2;
        DrawCheckbox (hdc, checkboxX, checkboxY, checkboxSize, isSelected, isHovered);
    }

    // 绘制左侧彩色条
    int accentX = G_SelectMode ? x + 45 : x + 4;
    COLORREF accentColor = record.isPinned ? RGB (255, 165, 0) : RGB (100, 149, 237);
    HBRUSH accentBrush = CreateSolidBrush (accentColor);
    RECT accentRect = { accentX, y, accentX + 4, y + 100 };
    FillRect (hdc, &accentRect, accentBrush);
    DeleteObject (accentBrush);

    // 绘制内容预览
    int contentX = G_SelectMode ? x + 55 : x + 15;
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT contentFont = CreateFont (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
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
    std::wcsftime (timeStr, 32, L"%Y-%m-%d %H:%M", &timeInfo);
    SetTextColor (hdc, RGB (150, 150, 150));
    HFONT timeFont = CreateFont (18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, GetAvailableFont().c_str());
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

// 检查记录是否被选中
bool IsItemSelected (int recordId)
{
    for (int id : G_SelectedItems)
    {
        if (id == recordId)
        {
            return true;
        }
    }
    return false;
}

// 切换记录的选中状态
void ToggleItemSelection (int recordId)
{
    for (auto it = G_SelectedItems.begin (); it != G_SelectedItems.end (); ++it)
    {
        if (*it == recordId)
        {
            G_SelectedItems.erase (it);
            return;
        }
    }
    G_SelectedItems.push_back (recordId);
}

// 全选/取消全选
void ToggleSelectAll (const vector<ClipRecord>& records)
{
    if (G_SelectAll)
    {
        // 取消全选
        G_SelectedItems.clear ();
        G_SelectAll = false;
    }
    else
    {
        // 全选
        G_SelectedItems.clear ();
        for (const auto& record : records)
        {
            G_SelectedItems.push_back (record.id);
        }
        G_SelectAll = true;
    }
}

// 批量删除选中的记录
void BatchDeleteSelected (HWND hWnd)
{
    if (G_SelectedItems.empty ())
    {
        return;
    }

    // 显示确认对话框
    wstring message = L"确定要删除选中的 " + to_wstring (G_SelectedItems.size ()) + L" 条记录吗？";
    int result = MessageBoxW (hWnd, message.c_str (), L"确认删除", MB_YESNO | MB_ICONQUESTION);

    if (result == IDYES)
    {
        vector<ClipRecord>& allRecords = const_cast<vector<ClipRecord>&> (G_ClipManager.GetRecords ());

        // 删除选中的记录
        for (int selectedId : G_SelectedItems)
        {
            for (auto it = allRecords.begin (); it != allRecords.end (); ++it)
            {
                if (it->id == selectedId)
                {
                    G_Storage.DeleteRecordFile (*it);
                    allRecords.erase (it);
                    break;
                }
            }
        }

        // 保存更改
        G_Storage.SaveRecords (allRecords);

        // 清空选中状态
        G_SelectedItems.clear ();
        G_SelectAll = false;
        G_SelectMode = false;

        // 使缓存失效并刷新界面
        G_RecordCache.Invalidate ();
        InvalidateRect (hWnd, NULL, TRUE);
    }
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

    case WM_HOTKEY:
    {
        if (wParam == HOTKEY_ID_TOGGLE_WINDOW)
        {
            // 切换窗口显示/隐藏
            if (G_IsMinimizedToTray || !IsWindowVisible (hWnd))
            {
                RestoreFromTray (hWnd);
            }
            else
            {
                MinimizeToTray (hWnd);
            }
            return 0;
        }
        else if (wParam == HOTKEY_ID_QUICK_COPY)
        {
            // 快速复制最近一条记录
            const vector<ClipRecord>& records = G_ClipManager.GetRecords ();
            if (!records.empty ())
            {
                // 找到最近一条非图片记录
                for (const auto& record : records)
                {
                    if (record.type == CLIP_TEXT && !record.content.empty ())
                    {
                        G_ClipManager.CopyToClipboard (record.content);
                        break;
                    }
                }
            }
            return 0;
        }
        return 0;
    }

    case WM_DESTROY:
        // 注销全局快捷键
        UnregisterHotKey (hWnd, HOTKEY_ID_TOGGLE_WINDOW);
        UnregisterHotKey (hWnd, HOTKEY_ID_QUICK_COPY);

        // 移除剪贴板监听
        RemoveClipboardFormatListener (hWnd);

        // 清理 GDI 对象缓存
        G_GDICache.Cleanup ();

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

        // 使缓存失效
        G_RecordCache.Invalidate ();

        // 刷新窗口显示
        InvalidateRect (hWnd, NULL, TRUE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hWnd, &ps);

        // 双缓冲：创建内存 DC 和位图
        HDC memDC = CreateCompatibleDC (hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap (hdc, G_WindowWidth, G_WindowHeight);
        HBITMAP oldBitmap = (HBITMAP)SelectObject (memDC, memBitmap);

        // 在内存 DC 上绘制背景
        FillRect (memDC, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW + 1));

        if (G_CurrentPage == PAGE_MAIN)
        {
            // 主页面
            // 计算布局
            int contentWidth = G_WindowWidth - 40;
            int searchWidth = contentWidth;
            int cardWidth = contentWidth;

            // 绘制标题（使用缓存字体）
            SetTextColor (memDC, RGB (33, 33, 33));
            SetBkMode (memDC, TRANSPARENT);
            SelectObject (memDC, G_GDICache.fontTitle);
            TextOut (memDC, 20, 12, L"历史剪贴板管理器", 8);

            // 绘制设置按钮
            DrawSettingsButton (memDC, G_WindowWidth - 52, 10, false);

            // 绘制垃圾桶按钮（选择模式切换）
            DrawDeleteModeButton (memDC, G_WindowWidth - 92, 10, false, G_SelectMode);

            // 绘制搜索框
            DrawSearchBox (memDC, 20, 50, searchWidth);

            // 绘制分割线（使用缓存画笔）
            SelectObject (memDC, G_GDICache.penDivider);
            MoveToEx (memDC, 20, 100, NULL);
            LineTo (memDC, 20 + searchWidth, 100);

            // 绘制全选复选框和批量删除按钮（在选择模式下）
            vector<ClipRecord> records = GetFilteredRecords ();
            if (G_SelectMode)
            {
                // 绘制全选复选框
                int checkboxSize = 20;
                int checkboxX = 20;
                int checkboxY = 105;
                DrawCheckbox (memDC, checkboxX, checkboxY, checkboxSize, G_SelectAll, false);

                // 绘制全选文字（使用缓存字体）
                SetTextColor (memDC, RGB (33, 33, 33));
                SetBkMode (memDC, TRANSPARENT);
                SelectObject (memDC, G_GDICache.fontButton);
                TextOut (memDC, checkboxX + 25, checkboxY + 2, L"全选", 2);

                // 绘制选中计数
                wstring countText = L"已选中 " + to_wstring (G_SelectedItems.size ()) + L" 条";
                SetTextColor (memDC, RGB (100, 100, 100));
                TextOut (memDC, checkboxX + 80, checkboxY + 2, countText.c_str (), countText.length ());

                // 绘制批量删除按钮
                int deleteBtnX = G_WindowWidth - 120;
                int deleteBtnY = checkboxY;
                bool hasSelected = !G_SelectedItems.empty ();
                DrawButton (memDC, deleteBtnX, deleteBtnY, 80, 26, L"删除选中", !hasSelected);
            }

            // 绘制历史记录列表
            int cardY = G_SelectMode ? 135 : 110 - G_ScrollOffset;
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
                bool isSelected = IsItemSelected (records[i].id);
                DrawCard (memDC, 20, cardY, cardWidth, records[i], isHovered, isSelected);
                cardY += cardHeight + cardMargin;
            }

            // 如果没有记录，显示提示
            if (records.empty ())
            {
                SelectObject (memDC, G_GDICache.fontContent);
                SetTextColor (memDC, RGB (150, 150, 150));
                wstring hintText = G_SearchText.empty () ? L"暂无历史记录，请复制内容测试" : L"未找到匹配的记录";
                TextOut (memDC, G_WindowWidth / 2 - 150, G_WindowHeight / 2, hintText.c_str (), hintText.length ());
            }

            // 底部快捷键提示
            SelectObject (memDC, G_GDICache.fontSmall);
            SetTextColor (memDC, RGB (150, 150, 150));
            wstring hotkeyHint = L"快捷键: Ctrl+Alt+V 显示/隐藏窗口  |  Ctrl+Alt+C 快速复制最近记录";
            SIZE hotkeySize;
            GetTextExtentPoint32 (memDC, hotkeyHint.c_str (), hotkeyHint.length (), &hotkeySize);
            TextOut (memDC, (G_WindowWidth - hotkeySize.cx) / 2, G_WindowHeight - 30, hotkeyHint.c_str (), hotkeyHint.length ());
        }
        else if (G_CurrentPage == PAGE_SETTINGS)
        {
            // 设置页面
            DrawSettingsPage (memDC);
        }
        else if (G_CurrentPage == PAGE_VERSION)
        {
            // 版本号页面
            DrawVersionPage (memDC);
        }
        else if (G_CurrentPage == PAGE_FEEDBACK)
        {
            // 问题反馈页面
            DrawFeedbackPage (memDC);
        }
        else if (G_CurrentPage == PAGE_CLEANUP_RULES)
        {
            // 清理规则管理页面
            DrawCleanupRulesPage (memDC);

            // 如果显示编辑对话框，绘制编辑对话框
            if (G_ShowCleanupRuleEditDialog)
            {
                DrawCleanupRuleEditDialog (memDC, G_EditingCleanupRule, G_IsNewCleanupRule);
            }
        }
        else if (G_CurrentPage == PAGE_CLEANUP_PREVIEW)
        {
            // 清理规则预览页面
            DrawCleanupRulePreviewPage (memDC);
        }

        // 双缓冲：将内存 DC 内容拷贝到屏幕
        BitBlt (hdc, 0, 0, G_WindowWidth, G_WindowHeight, memDC, 0, 0, SRCCOPY);

        // 清理双缓冲资源
        SelectObject (memDC, oldBitmap);
        DeleteObject (memBitmap);
        DeleteDC (memDC);

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

            // 检查是否点击了垃圾桶按钮（选择模式切换）
            if (x >= G_WindowWidth - 92 && x <= G_WindowWidth - 60 && y >= 10 && y <= 42)
            {
                G_SelectMode = !G_SelectMode;
                if (!G_SelectMode)
                {
                    // 退出选择模式时清空选中状态
                    G_SelectedItems.clear ();
                    G_SelectAll = false;
                }
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

            // 检查是否点击了全选复选框（在选择模式下）
            vector<ClipRecord> records = GetFilteredRecords ();
            if (G_SelectMode)
            {
                int checkboxSize = 20;
                int checkboxX = 20;
                int checkboxY = 105;

                if (x >= checkboxX && x <= checkboxX + checkboxSize && y >= checkboxY && y <= checkboxY + checkboxSize)
                {
                    ToggleSelectAll (records);
                    InvalidateRect (hWnd, NULL, TRUE);
                    return 0;
                }

                // 检查是否点击了批量删除按钮
                int deleteBtnX = G_WindowWidth - 120;
                int deleteBtnY = checkboxY;
                if (x >= deleteBtnX && x <= deleteBtnX + 80 && y >= deleteBtnY && y <= deleteBtnY + 26)
                {
                    BatchDeleteSelected (hWnd);
                    return 0;
                }
            }

            // 检查是否点击了卡片
            int cardY = G_SelectMode ? 135 : 110 - G_ScrollOffset;
            int cardHeight = 100;
            int cardMargin = 10;

        for (int i = 0; i < (int)records.size (); i++)
        {
            if (y >= cardY && y < cardY + cardHeight && x >= 20 && x <= G_WindowWidth - 20)
            {
                // 检查是否点击了复选框（在选择模式下）
                if (G_SelectMode)
                {
                    int checkboxSize = 20;
                    int checkboxX = 35;
                    int checkboxY = cardY + (cardHeight - checkboxSize) / 2;

                    if (x >= checkboxX && x <= checkboxX + checkboxSize && y >= checkboxY && y <= checkboxY + checkboxSize)
                    {
                        ToggleItemSelection (records[i].id);
                        G_SelectAll = (G_SelectedItems.size () == records.size ());
                        InvalidateRect (hWnd, NULL, TRUE);
                        return 0;
                    }
                }

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
                        G_RecordCache.Invalidate ();
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
                        G_RecordCache.Invalidate ();
                        InvalidateRect (hWnd, NULL, TRUE);
                        return 0;
                    }
                }

                // 点击卡片本身，在非选择模式下复制内容
                if (!G_SelectMode)
                {
                    wstring copyText = records[i].content;
                    G_ClipManager.CopyToClipboard (copyText);
                }

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

            // 检查是否点击了清理规则入口
            if (x >= 20 && x <= G_WindowWidth - 20 && y >= 280 && y <= 320)
            {
                G_CurrentPage = PAGE_CLEANUP_RULES;
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了版本信息入口
            if (x >= 20 && x <= G_WindowWidth - 20 && y >= 340 && y <= 380)
            {
                G_CurrentPage = PAGE_VERSION;
                G_DropdownOpen = false;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了问题反馈入口
            if (x >= 20 && x <= G_WindowWidth - 20 && y >= 400 && y <= 440)
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
        else if (G_CurrentPage == PAGE_CLEANUP_RULES)
        {
            // 清理规则管理页面点击处理

            // 如果显示编辑对话框，处理编辑对话框的点击
            if (G_ShowCleanupRuleEditDialog)
            {
                // 检查是否点击了取消按钮
                int cancelBtnX = G_WindowWidth - 110;
                int cancelBtnY = G_WindowHeight - 100;
                if (x >= cancelBtnX && x <= cancelBtnX + 80 && y >= cancelBtnY && y <= cancelBtnY + 30)
                {
                    G_ShowCleanupRuleEditDialog = false;
                    InvalidateRect (hWnd, NULL, TRUE);
                    return 0;
                }

                // 检查是否点击了保存按钮
                int saveBtnX = G_WindowWidth - 200;
                int saveBtnY = G_WindowHeight - 100;
                if (x >= saveBtnX && x <= saveBtnX + 80 && y >= saveBtnY && y <= saveBtnY + 30)
                {
                    // 保存规则
                    CleanupRuleManager& ruleManager = G_Storage.GetCleanupRuleManager ();
                    if (G_IsNewCleanupRule)
                    {
                        ruleManager.AddRule (G_EditingCleanupRule);
                    }
                    else
                    {
                        ruleManager.UpdateRule (G_EditingCleanupRule);
                    }

                    G_ShowCleanupRuleEditDialog = false;
                    InvalidateRect (hWnd, NULL, TRUE);
                    return 0;
                }

                // 检查是否点击了启用/禁用复选框
                int checkboxX = 180;
                int checkboxY = 275;
                int checkboxSize = 20;
                if (x >= checkboxX && x <= checkboxX + checkboxSize && y >= checkboxY && y <= checkboxY + checkboxSize)
                {
                    G_EditingCleanupRule.enabled = !G_EditingCleanupRule.enabled;
                    InvalidateRect (hWnd, NULL, TRUE);
                    return 0;
                }

                // 检查是否点击了删除图片复选框
                int deleteImgCheckboxX = 180;
                int deleteImgCheckboxY = 315;
                if (x >= deleteImgCheckboxX && x <= deleteImgCheckboxX + checkboxSize && y >= deleteImgCheckboxY && y <= deleteImgCheckboxY + checkboxSize)
                {
                    G_EditingCleanupRule.deleteImages = !G_EditingCleanupRule.deleteImages;
                    InvalidateRect (hWnd, NULL, TRUE);
                    return 0;
                }

                return 0;
            }

            // 检查是否点击了返回按钮
            if (x >= 20 && x <= 80 && y >= 8 && y <= 38)
            {
                G_CurrentPage = PAGE_SETTINGS;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了添加规则按钮
            int addBtnX = G_WindowWidth - 120;
            int addBtnY = G_WindowHeight - 50;
            if (x >= addBtnX && x <= addBtnX + 80 && y >= addBtnY && y <= addBtnY + 30)
            {
                // 显示添加规则对话框
                G_EditingCleanupRule = CleanupRule ();
                G_EditingCleanupRule.name = L"新规则";
                G_EditingCleanupRule.enabled = true;
                G_EditingCleanupRule.type = RULE_BY_TIME;
                G_EditingCleanupRule.days = 30;
                G_EditingCleanupRule.maxRecords = 1000;
                G_EditingCleanupRule.maxSizeMB = 100;
                G_EditingCleanupRule.deleteImages = true;
                G_IsNewCleanupRule = true;
                G_ShowCleanupRuleEditDialog = true;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了预览按钮
            int previewBtnX = G_WindowWidth - 210;
            int previewBtnY = G_WindowHeight - 50;
            if (x >= previewBtnX && x <= previewBtnX + 80 && y >= previewBtnY && y <= previewBtnY + 30)
            {
                // 显示预览页面
                G_CurrentPage = PAGE_CLEANUP_PREVIEW;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了规则列表中的规则
            CleanupRuleManager& ruleManager = G_Storage.GetCleanupRuleManager ();
            vector<CleanupRule> rules = ruleManager.GetRules ();
            int ruleY = 70;
            int ruleHeight = 80;
            int ruleMargin = 10;

            for (int i = 0; i < (int)rules.size (); i++)
            {
                if (x >= 20 && x <= G_WindowWidth - 20 && y >= ruleY && y < ruleY + ruleHeight)
                {
                    // 显示编辑规则对话框
                    G_EditingCleanupRule = rules[i];
                    G_IsNewCleanupRule = false;
                    G_ShowCleanupRuleEditDialog = true;
                    InvalidateRect (hWnd, NULL, TRUE);
                    return 0;
                }
                ruleY += ruleHeight + ruleMargin;
            }
        }
        else if (G_CurrentPage == PAGE_CLEANUP_PREVIEW)
        {
            // 清理规则预览页面点击处理

            // 检查是否点击了返回按钮
            if (x >= 20 && x <= 80 && y >= 8 && y <= 38)
            {
                G_CurrentPage = PAGE_CLEANUP_RULES;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了执行清理按钮
            int executeBtnX = G_WindowWidth - 200;
            int executeBtnY = G_WindowHeight - 60;
            if (x >= executeBtnX && x <= executeBtnX + 100 && y >= executeBtnY && y <= executeBtnY + 30)
            {
                // 执行清理
                CleanupRuleManager& ruleManager = G_Storage.GetCleanupRuleManager ();
                vector<ClipRecord>& records = const_cast<vector<ClipRecord>&> (G_ClipManager.GetRecords ());
                int deletedCount = ruleManager.ExecuteCleanup (records);

                // 保存记录
                G_Storage.SaveRecords (records);
                G_RecordCache.Invalidate ();

                // 显示提示
                wstring message = L"已清理 " + to_wstring (deletedCount) + L" 条记录";
                MessageBoxW (hWnd, message.c_str (), L"清理完成", MB_OK);

                // 返回清理规则页面
                G_CurrentPage = PAGE_CLEANUP_RULES;
                InvalidateRect (hWnd, NULL, TRUE);
                return 0;
            }

            // 检查是否点击了取消按钮
            int cancelBtnX = G_WindowWidth - 90;
            int cancelBtnY = G_WindowHeight - 60;
            if (x >= cancelBtnX && x <= cancelBtnX + 80 && y >= cancelBtnY && y <= cancelBtnY + 30)
            {
                // 返回清理规则页面
                G_CurrentPage = PAGE_CLEANUP_RULES;
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

    case WM_RBUTTONDOWN:
    {
        // 右键菜单已移除，使用垃圾桶按钮切换选择模式
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

    // 初始化 GDI 对象缓存
    G_GDICache.Initialize ();

    // 添加托盘图标
    AddTrayIcon (hWnd);

    // 注册全局快捷键
    G_HotkeysRegistered = RegisterHotKey (hWnd, HOTKEY_ID_TOGGLE_WINDOW, G_HotkeyToggleModifiers, G_HotkeyToggleKey);
    RegisterHotKey (hWnd, HOTKEY_ID_QUICK_COPY, G_HotkeyCopyModifiers, G_HotkeyCopyKey);

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
