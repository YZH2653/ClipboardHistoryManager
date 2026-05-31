#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include "UIManager.h"
#include <algorithm>
#include <sstream>
using namespace std;

// 构造函数
UIManager::UIManager ()
    : m_hWnd (NULL), m_clipManager (NULL), m_hoverCardIndex (-1),
      m_hoverButtonIndex (-1), m_scrollOffset (0)
{
}

// 析构函数
UIManager::~UIManager ()
{
}

// 初始化界面
bool UIManager::Initialize (HWND hWnd, ClipboardManager* clipManager)
{
    m_hWnd = hWnd;
    m_clipManager = clipManager;
    return true;
}

// 绘制界面
void UIManager::OnPaint (HDC hdc, RECT clientRect)
{
    // 绘制背景
    HBRUSH bgBrush = CreateSolidBrush (RGB (245, 245, 245));
    FillRect (hdc, &clientRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制标题
    SetTextColor (hdc, RGB (51, 51, 51));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, titleFont);
    TextOut (hdc, CARD_PADDING, 10, L"历史剪贴板管理器", 8);
    DeleteObject (titleFont);

    // 绘制搜索框
    DrawSearchBox (hdc, CARD_PADDING, 40, clientRect.right - CARD_PADDING * 2);

    // 绘制历史卡片
    vector<ClipRecord> records = GetFilteredRecords ();
    int cardY = 40 + SEARCH_BOX_HEIGHT + CARD_MARGIN;

    HFONT cardFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    HFONT smallFont = CreateFont (12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");

    for (int i = 0; i < (int)records.size (); i++)
    {
        int cardX = CARD_PADDING;
        int cardWidth = clientRect.right - CARD_PADDING * 2;

        // 检查是否超出可视区域
        if (cardY + CARD_HEIGHT - m_scrollOffset > clientRect.bottom)
        {
            break;
        }

        // 调整Y坐标（考虑滚动）
        int drawY = cardY - m_scrollOffset;
        if (drawY + CARD_HEIGHT < 0)
        {
            cardY += CARD_HEIGHT + CARD_MARGIN;
            continue;
        }

        // 绘制卡片
        bool isHovered = (i == m_hoverCardIndex);
        DrawCard (hdc, cardX, drawY, cardWidth, records[i], isHovered);

        cardY += CARD_HEIGHT + CARD_MARGIN;
    }

    DeleteObject (cardFont);
    DeleteObject (smallFont);

    // 如果没有记录，显示提示
    if (records.empty ())
    {
        HFONT hintFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        SelectObject (hdc, hintFont);
        SetTextColor (hdc, RGB (150, 150, 150));
        TextOut (hdc, clientRect.right / 2 - 80, clientRect.bottom / 2, L"暂无历史记录", 6);
        DeleteObject (hintFont);
    }
}

// 绘制搜索框
void UIManager::DrawSearchBox (HDC hdc, int x, int y, int width)
{
    // 绘制搜索框背景
    HBRUSH searchBg = CreateSolidBrush (RGB (255, 255, 255));
    RECT searchRect = { x, y, x + width, y + SEARCH_BOX_HEIGHT };
    FillRect (hdc, &searchRect, searchBg);
    DeleteObject (searchBg);

    // 绘制边框
    HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (200, 200, 200));
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + SEARCH_BOX_HEIGHT);
    DeleteObject (borderPen);

    // 绘制搜索图标
    SetTextColor (hdc, RGB (150, 150, 150));
    SetBkMode (hdc, TRANSPARENT);
    TextOut (hdc, x + 10, y + 10, L"🔍", 1);

    // 绘制输入文本
    SetTextColor (hdc, RGB (51, 51, 51));
    if (m_searchText.empty ())
    {
        SetTextColor (hdc, RGB (180, 180, 180));
        TextOut (hdc, x + 35, y + 10, L"搜索历史记录...", 7);
    }
    else
    {
        TextOut (hdc, x + 35, y + 10, m_searchText.c_str (), m_searchText.length ());
    }
}

// 绘制历史卡片
void UIManager::DrawCard (HDC hdc, int x, int y, int width, const ClipRecord& record, bool isHovered)
{
    // 绘制卡片背景
    COLORREF bgColor = isHovered ? RGB (240, 248, 255) : RGB (255, 255, 255);
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

    // 绘制置顶标记
    if (record.isPinned)
    {
        SetTextColor (hdc, RGB (255, 165, 0));
        SetBkMode (hdc, TRANSPARENT);
        TextOut (hdc, x + CARD_PADDING, y + 5, L"📌", 1);
    }

    // 绘制类型图标
    wstring typeIcon = (record.type == CLIP_TEXT) ? L"📝" : L"🖼️";
    SetTextColor (hdc, RGB (100, 100, 100));
    SetBkMode (hdc, TRANSPARENT);
    TextOut (hdc, x + CARD_PADDING + (record.isPinned ? 20 : 0), y + 5, typeIcon.c_str (), typeIcon.length ());

    // 绘制时间
    time_t timestamp = record.timestamp;
    struct tm timeInfo;
    localtime_s (&timeInfo, &timestamp);
    wchar_t timeStr[32];
    wcsftime (timeStr, 32, L"%Y-%m-%d %H:%M", &timeInfo);
    SetTextColor (hdc, RGB (150, 150, 150));
    TextOut (hdc, x + width - 120, y + 5, timeStr, wcslen (timeStr));

    // 绘制内容预览
    SetTextColor (hdc, RGB (51, 51, 51));
    wstring preview = record.preview;
    if (preview.length () > 80)
    {
        preview = preview.substr (0, 80) + L"...";
    }
    TextOut (hdc, x + CARD_PADDING, y + 30, preview.c_str (), preview.length ());

    // 绘制操作按钮
    int buttonY = y + CARD_HEIGHT - BUTTON_HEIGHT - 10;
    int buttonX = x + width - BUTTON_WIDTH * 3 - BUTTON_MARGIN * 2;

    // 复制按钮
    DrawButton (hdc, buttonX, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT, L"复制", false);

    // 置顶按钮
    wstring pinText = record.isPinned ? L"取消置顶" : L"置顶";
    DrawButton (hdc, buttonX + BUTTON_WIDTH + BUTTON_MARGIN, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT, pinText, false);

    // 删除按钮
    DrawButton (hdc, buttonX + (BUTTON_WIDTH + BUTTON_MARGIN) * 2, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT, L"删除", false);
}

// 绘制按钮
void UIManager::DrawButton (HDC hdc, int x, int y, int width, int height, const wstring& text, bool isHovered)
{
    // 绘制按钮背景
    COLORREF btnColor = isHovered ? RGB (70, 130, 180) : RGB (100, 149, 237);
    HBRUSH btnBg = CreateSolidBrush (btnColor);
    RECT btnRect = { x, y, x + width, y + height };
    FillRect (hdc, &btnRect, btnBg);
    DeleteObject (btnBg);

    // 绘制边框
    HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (70, 130, 180));
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + height);
    DeleteObject (borderPen);

    // 绘制文字
    SetTextColor (hdc, RGB (255, 255, 255));
    SetBkMode (hdc, TRANSPARENT);
    int textX = x + (width - text.length () * 7) / 2;
    int textY = y + (height - 14) / 2;
    TextOut (hdc, textX, textY, text.c_str (), text.length ());
}

// 绘制文字（自动换行）
void UIManager::DrawTextWithWrap (HDC hdc, const wstring& text, int x, int y, int width, int maxLength)
{
    wstring displayText = text;
    if (displayText.length () > maxLength)
    {
        displayText = displayText.substr (0, maxLength) + L"...";
    }
    TextOut (hdc, x, y, displayText.c_str (), displayText.length ());
}

// 获取筛选后的记录
vector<ClipRecord> UIManager::GetFilteredRecords ()
{
    if (!m_clipManager)
    {
        return vector<ClipRecord> ();
    }

    const vector<ClipRecord>& allRecords = m_clipManager->GetRecords ();

    // 如果搜索框为空，返回所有记录
    if (m_searchText.empty ())
    {
        return allRecords;
    }

    // 筛选包含搜索文本的记录
    vector<ClipRecord> filtered;
    for (const auto& record : allRecords)
    {
        // 搜索预览文本
        if (record.preview.find (m_searchText) != wstring::npos)
        {
            filtered.push_back (record);
            continue;
        }

        // 搜索完整内容（仅文字类型）
        if (record.type == CLIP_TEXT && record.content.find (m_searchText) != wstring::npos)
        {
            filtered.push_back (record);
        }
    }

    return filtered;
}

// 处理鼠标移动
void UIManager::OnMouseMove (int x, int y)
{
    m_hoverCardIndex = -1;
    m_hoverButtonIndex = -1;

    // 计算鼠标所在的卡片
    int cardY = 40 + SEARCH_BOX_HEIGHT + CARD_MARGIN - m_scrollOffset;
    vector<ClipRecord> records = GetFilteredRecords ();

    for (int i = 0; i < (int)records.size (); i++)
    {
        if (y >= cardY && y < cardY + CARD_HEIGHT)
        {
            m_hoverCardIndex = i;
            break;
        }
        cardY += CARD_HEIGHT + CARD_MARGIN;
    }

    // 刷新窗口
    InvalidateRect (m_hWnd, NULL, TRUE);
}

// 处理鼠标左键点击
void UIManager::OnLButtonDown (int x, int y)
{
    // 检查是否点击了搜索框
    RECT clientRect;
    GetClientRect (m_hWnd, &clientRect);
    if (y >= 40 && y < 40 + SEARCH_BOX_HEIGHT)
    {
        // 激活搜索框
        SetFocus (m_hWnd);
        return;
    }

    // 检查是否点击了卡片
    vector<ClipRecord> records = GetFilteredRecords ();
    int cardY = 40 + SEARCH_BOX_HEIGHT + CARD_MARGIN - m_scrollOffset;

    for (int i = 0; i < (int)records.size (); i++)
    {
        if (y >= cardY && y < cardY + CARD_HEIGHT)
        {
            // 检查是否点击了按钮
            int cardX = CARD_PADDING;
            int cardWidth = clientRect.right - CARD_PADDING * 2;
            int buttonX = cardX + cardWidth - BUTTON_WIDTH * 3 - BUTTON_MARGIN * 2;
            int buttonY = cardY + CARD_HEIGHT - BUTTON_HEIGHT - 10;

            if (x >= buttonX && x < buttonX + BUTTON_WIDTH)
            {
                // 复制按钮
                wstring copyText = records[i].content;
                if (!copyText.empty ())
                {
                    OpenClipboard (m_hWnd);
                    EmptyClipboard ();
                    HGLOBAL hMem = GlobalAlloc (GMEM_MOVEABLE, (copyText.length () + 1) * sizeof (wchar_t));
                    wchar_t* pMem = (wchar_t*)GlobalLock (hMem);
                    wcscpy_s (pMem, copyText.length () + 1, copyText.c_str ());
                    GlobalUnlock (hMem);
                    SetClipboardData (CF_UNICODETEXT, hMem);
                    CloseClipboard ();
                }
                return;
            }

            if (x >= buttonX + BUTTON_WIDTH + BUTTON_MARGIN && x < buttonX + (BUTTON_WIDTH + BUTTON_MARGIN) * 2)
            {
                // 置顶按钮
                // TODO: 实现置顶功能
                return;
            }

            if (x >= buttonX + (BUTTON_WIDTH + BUTTON_MARGIN) * 2 && x < buttonX + (BUTTON_WIDTH + BUTTON_MARGIN) * 3)
            {
                // 删除按钮
                // TODO: 实现删除功能
                return;
            }

            break;
        }
        cardY += CARD_HEIGHT + CARD_MARGIN;
    }
}

// 处理键盘输入
void UIManager::OnChar (wchar_t ch)
{
    if (ch == VK_BACK)
    {
        // 退格键
        if (!m_searchText.empty ())
        {
            m_searchText.pop_back ();
        }
    }
    else if (ch >= 32)
    {
        // 可打印字符
        m_searchText += ch;
    }

    // 刷新窗口
    InvalidateRect (m_hWnd, NULL, TRUE);
}

// 处理按键
void UIManager::OnKeyDown (int keyCode)
{
    if (keyCode == VK_BACK)
    {
        // 退格键
        if (!m_searchText.empty ())
        {
            m_searchText.pop_back ();
        }
        InvalidateRect (m_hWnd, NULL, TRUE);
    }
    else if (keyCode == VK_DOWN)
    {
        // 向下滚动
        m_scrollOffset += 30;
        InvalidateRect (m_hWnd, NULL, TRUE);
    }
    else if (keyCode == VK_UP)
    {
        // 向上滚动
        m_scrollOffset = max (0, m_scrollOffset - 30);
        InvalidateRect (m_hWnd, NULL, TRUE);
    }
}

// 获取搜索框内容
const wstring& UIManager::GetSearchText () const
{
    return m_searchText;
}

// 清空搜索框
void UIManager::ClearSearch ()
{
    m_searchText.clear ();
    InvalidateRect (m_hWnd, NULL, TRUE);
}
