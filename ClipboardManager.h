#pragma once
#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <string>
#include <vector>
#include <ctime>
using namespace std;

// 剪贴板内容类型
enum ClipType
{
    CLIP_TEXT = 0,   // 文字
    CLIP_IMAGE = 1   // 图片
};

// 历史记录结构
struct ClipRecord
{
    int id;                  // 唯一标识
    ClipType type;           // 内容类型
    wstring content;         // 文字内容
    wstring preview;         // 预览文本（前100字符）
    wstring filePath;        // 文件路径（图片）
    time_t timestamp;        // 创建时间
    bool isPinned;           // 是否置顶
};

// 剪贴板管理器类
class ClipboardManager
{
public:
    ClipboardManager ();
    ~ClipboardManager ();

    // 初始化剪贴板监听
    bool Initialize (HWND hWnd);

    // 设置程序根目录
    void SetRootDir (const wstring& rootDir);

    // 处理剪贴板更新
    bool OnClipboardUpdate ();

    // 添加记录（从外部加载）
    void AddRecord (const ClipRecord& record);

    // 获取所有历史记录
    const vector<ClipRecord>& GetRecords () const;

    // 获取记录数量
    int GetRecordCount () const;

private:
    // 捕获文字内容
    bool CaptureText ();

    // 捕获图片内容
    bool CaptureImage ();

    // 生成唯一ID
    int GenerateId ();

    HWND m_hWnd;              // 窗口句柄
    wstring m_rootDir;        // 程序根目录
    vector<ClipRecord> m_records;  // 历史记录
    int m_nextId;             // 下一个ID
};
