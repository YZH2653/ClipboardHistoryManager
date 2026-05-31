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
    // 绘制白色背景
    HBRUSH bgBrush = CreateSolidBrush (RGB (255, 255, 255));
    FillRect (hdc, &clientRect, bgBrush);
    DeleteObject (bgBrush);

    // 绘制标题
    SetTextColor (hdc, RGB (33, 33, 33));
    SetBkMode (hdc, TRANSPARENT);
    HFONT titleFont = CreateFont (22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, titleFont);
    TextOut (hdc, CARD_PADDING, 15, L"历史剪贴板", 5);
    DeleteObject (titleFont);

    // 绘制搜索框
    DrawSearchBox (hdc, CARD_PADDING, 55, clientRect.right - CARD_PADDING * 2);

    // 绘制分割线
    HPEN linePen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, linePen);
    MoveToEx (hdc, CARD_PADDING, 55 + SEARCH_BOX_HEIGHT + 10, NULL);
    LineTo (hdc, clientRect.right - CARD_PADDING, 55 + SEARCH_BOX_HEIGHT + 10);
    DeleteObject (linePen);

    // 绘制历史卡片
    vector<ClipRecord> records = GetFilteredRecords ();
    int cardY = 55 + SEARCH_BOX_HEIGHT + 20;

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

    // 如果没有记录，显示提示
    if (records.empty ())
    {
        HFONT hintFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        SelectObject (hdc, hintFont);
        SetTextColor (hdc, RGB (180, 180, 180));
        wstring hintText = m_searchText.empty () ? L"暂无历史记录" : L"未找到匹配的记录";
        TextOut (hdc, clientRect.right / 2 - 60, clientRect.bottom / 2, hintText.c_str (), hintText.length ());
        DeleteObject (hintFont);
    }
}

// 绘制搜索框
void UIManager::DrawSearchBox (HDC hdc, int x, int y, int width)
{
    // 绘制搜索框背景（浅灰色）
    HBRUSH searchBg = CreateSolidBrush (RGB (245, 245, 245));
    RECT searchRect = { x, y, x + width, y + SEARCH_BOX_HEIGHT };
    FillRect (hdc, &searchRect, searchBg);
    DeleteObject (searchBg);

    // 绘制圆角边框
    HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (230, 230, 230));
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + SEARCH_BOX_HEIGHT);
    DeleteObject (borderPen);

    // 绘制搜索图标
    SetTextColor (hdc, RGB (150, 150, 150));
    SetBkMode (hdc, TRANSPARENT);
    HFONT iconFont = CreateFont (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
    SelectObject (hdc, iconFont);
    TextOut (hdc, x + 12, y + 10, L"🔍", 1);
    DeleteObject (iconFont);

    // 绘制输入文本
    HFONT inputFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, inputFont);

    if (m_searchText.empty ())
    {
        SetTextColor (hdc, RGB (180, 180, 180));
        TextOut (hdc, x + 38, y + 12, L"搜索历史记录...", 7);
    }
    else
    {
        SetTextColor (hdc, RGB (33, 33, 33));
        TextOut (hdc, x + 38, y + 12, m_searchText.c_str (), m_searchText.length ());
    }

    DeleteObject (inputFont);
}

// 绘制历史卡片
void UIManager::DrawCard (HDC hdc, int x, int y, int width, const ClipRecord& record, bool isHovered)
{
    // 绘制卡片背景（纯白色）
    HBRUSH cardBg = CreateSolidBrush (RGB (255, 255, 255));
    RECT cardRect = { x, y, x + width, y + CARD_HEIGHT };
    FillRect (hdc, &cardRect, cardBg);
    DeleteObject (cardBg);

    // 绘制底部边框（浅灰色）
    HPEN linePen = CreatePen (PS_SOLID, 1, RGB (240, 240, 240));
    SelectObject (hdc, linePen);
    MoveToEx (hdc, x, y + CARD_HEIGHT, NULL);
    LineTo (hdc, x + width, y + CARD_HEIGHT);
    DeleteObject (linePen);

    // 绘制左侧彩色条（根据类型）
    COLORREF accentColor = (record.type == CLIP_TEXT) ? RGB (100, 149, 237) : RGB (255, 140, 0);
    HBRUSH accentBrush = CreateSolidBrush (accentColor);
    RECT accentRect = { x, y, x + 4, y + CARD_HEIGHT };
    FillRect (hdc, &accentRect, accentBrush);
    DeleteObject (accentBrush);

    // 绘制置顶标记
    int contentX = x + 15;
    if (record.isPinned)
    {
        SetTextColor (hdc, RGB (255, 165, 0));
        SetBkMode (hdc, TRANSPARENT);
        HFONT pinFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Emoji");
        SelectObject (hdc, pinFont);
        TextOut (hdc, contentX, y + 10, L"📌", 1);
        DeleteObject (pinFont);
        contentX += 20;
    }

    // 绘制时间（右上角）
    time_t timestamp = record.timestamp;
    struct tm timeInfo;
    localtime_s (&timeInfo, &timestamp);
    wchar_t timeStr[32];
    wcsftime (timeStr, 32, L"%m-%d %H:%M", &timeInfo);
    SetTextColor (hdc, RGB (180, 180, 180));
    SetBkMode (hdc, TRANSPARENT);
    HFONT timeFont = CreateFont (12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, timeFont);
    TextOut (hdc, x + width - 80, y + 10, timeStr, wcslen (timeStr));
    DeleteObject (timeFont);

    // 绘制内容预览
    SetTextColor (hdc, RGB (33, 33, 33));
    HFONT contentFont = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, contentFont);

    wstring preview = record.preview;
    if (preview.length () > 60)
    {
        preview = preview.substr (0, 60) + L"...";
    }
    TextOut (hdc, contentX, y + 35, preview.c_str (), preview.length ());
    DeleteObject (contentFont);

    // 绘制操作按钮（扁平化设计）
    int buttonY = y + CARD_HEIGHT - BUTTON_HEIGHT - 12;
    int buttonX = x + width - BUTTON_WIDTH * 3 - BUTTON_MARGIN * 2;

    // 复制按钮
    DrawButton (hdc, buttonX, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT, L"复制", false);

    // 置顶按钮
    wstring pinText = record.isPinned ? L"取消置顶" : L"置顶";
    DrawButton (hdc, buttonX + BUTTON_WIDTH + BUTTON_MARGIN, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT, pinText, false);

    // 删除按钮
    DrawButton (hdc, buttonX + (BUTTON_WIDTH + BUTTON_MARGIN) * 2, buttonY, BUTTON_WIDTH, BUTTON_HEIGHT, L"删除", false);
}

// 绘制按钮（扁平化设计）
void UIManager::DrawButton (HDC hdc, int x, int y, int width, int height, const wstring& text, bool isHovered)
{
    // 绘制按钮背景（浅灰色）
    COLORREF bgColor = isHovered ? RGB (230, 230, 230) : RGB (245, 245, 245);
    HBRUSH btnBg = CreateSolidBrush (bgColor);
    RECT btnRect = { x, y, x + width, y + height };
    FillRect (hdc, &btnRect, btnBg);
    DeleteObject (btnBg);

    // 绘制边框
    HPEN borderPen = CreatePen (PS_SOLID, 1, RGB (220, 220, 220));
    SelectObject (hdc, borderPen);
    Rectangle (hdc, x, y, x + width, y + height);
    DeleteObject (borderPen);

    // 绘制文字
    SetTextColor (hdc, RGB (80, 80, 80));
    SetBkMode (hdc, TRANSPARENT);
    HFONT btnFont = CreateFont (12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    SelectObject (hdc, btnFont);

    // 计算文字居中位置
    SIZE textSize;
    GetTextExtentPoint32 (hdc, text.c_str (), text.length (), &textSize);
    int textX = x + (width - textSize.cx) / 2;
    int textY = y + (height - textSize.cy) / 2;
    TextOut (hdc, textX, textY, text.c_str (), text.length ());

    DeleteObject (btnFont);
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
    int cardY = 55 + SEARCH_BOX_HEIGHT + 20 - m_scrollOffset;
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
    if (y >= 55 && y < 55 + SEARCH_BOX_HEIGHT)
    {
        // 激活搜索框
        SetFocus (m_hWnd);
        return;
    }

    // 检查是否点击了卡片
    vector<ClipRecord> records = GetFilteredRecords ();
    int cardY = 55 + SEARCH_BOX_HEIGHT + 20 - m_scrollOffset;

    for (int i = 0; i < (int)records.size (); i++)
    {
        if (y >= cardY && y < cardY + CARD_HEIGHT)
        {
            // 检查是否点击了按钮
            int cardX = CARD_PADDING;
            int cardWidth = clientRect.right - CARD_PADDING * 2;
            int buttonX = cardX + cardWidth - BUTTON_WIDTH * 3 - BUTTON_MARGIN * 2;
            int buttonY = cardY + CARD_HEIGHT - BUTTON_HEIGHT - 12;

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
