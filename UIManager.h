#pragma once
#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <string>
#include <vector>
#include "ClipboardManager.h"
using namespace std;

// 界面管理器类
class UIManager
{
public:
    UIManager ();
    ~UIManager ();

    // 初始化界面
    bool Initialize (HWND hWnd, ClipboardManager* clipManager);

    // 绘制界面
    void OnPaint (HDC hdc, RECT clientRect);

    // 处理鼠标点击
    void OnMouseMove (int x, int y);
    void OnLButtonDown (int x, int y);

    // 处理键盘输入
    void OnChar (wchar_t ch);
    void OnKeyDown (int keyCode);

    // 获取搜索框内容
    const wstring& GetSearchText () const;

    // 清空搜索框
    void ClearSearch ();

    // 获取选中的记录ID（用于置顶/删除）
    int GetSelectedRecordId () const;

    // 获取操作类型（0=无, 1=置顶, 2=删除）
    int GetOperationType () const;

    // 清除操作状态
    void ClearOperation ();

    // 获取剪贴板管理器指针
    ClipboardManager* GetClipManager () const;

private:
    // 绘制搜索框
    void DrawSearchBox (HDC hdc, int x, int y, int width);

    // 绘制历史卡片
    void DrawCard (HDC hdc, int x, int y, int width, const ClipRecord& record, bool isHovered);

    // 绘制按钮
    void DrawButton (HDC hdc, int x, int y, int width, int height, const wstring& text, bool isHovered);

    // 绘制文字（自动换行）
    void DrawTextWithWrap (HDC hdc, const wstring& text, int x, int y, int width, int maxLength);

    // 获取筛选后的记录
    vector<ClipRecord> GetFilteredRecords ();

    HWND m_hWnd;                    // 窗口句柄
    ClipboardManager* m_clipManager;  // 剪贴板管理器指针
    wstring m_searchText;           // 搜索框内容
    int m_hoverCardIndex;           // 鼠标悬停的卡片索引
    int m_scrollOffset;             // 滚动偏移量

    // 操作状态
    int m_selectedRecordId;         // 选中的记录ID
    int m_operationType;            // 操作类型（0=无, 1=置顶, 2=删除）

    // 界面布局常量
    static const int SEARCH_BOX_HEIGHT = 40;
    static const int CARD_HEIGHT = 120;
    static const int CARD_MARGIN = 10;
    static const int CARD_PADDING = 15;
    static const int BUTTON_WIDTH = 60;
    static const int BUTTON_HEIGHT = 30;
    static const int BUTTON_MARGIN = 5;
};
