#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include "ClipboardManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <GdiPlus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace std;

// 构造函数
ClipboardManager::ClipboardManager ()
    : m_hWnd (NULL), m_nextId (1), m_isListening (true)
{
}

// 析构函数
ClipboardManager::~ClipboardManager ()
{
}

// 初始化剪贴板监听
bool ClipboardManager::Initialize (HWND hWnd)
{
    m_hWnd = hWnd;

    // 注册剪贴板监听
    if (!AddClipboardFormatListener (m_hWnd))
    {
        wcout << L"注册剪贴板监听失败" << endl;
        return false;
    }

    wcout << L"剪贴板监听初始化成功" << endl;
    return true;
}

// 设置程序根目录
void ClipboardManager::SetRootDir (const wstring& rootDir)
{
    m_rootDir = rootDir;
}

// 暂停/恢复监听
void ClipboardManager::SetListening (bool listening)
{
    m_isListening = listening;
}

// 校验内容合法性
bool ClipboardManager::IsValidContent (const wstring& content)
{
    // 过滤空内容
    if (content.empty ())
    {
        return false;
    }

    // 过滤过短内容（少于2个字符）
    if (content.length () < 2)
    {
        return false;
    }

    // 过滤系统命令和错误信息
    wstring lowerContent = content;
    transform (lowerContent.begin (), lowerContent.end (), lowerContent.begin (), ::towlower);

    // 过滤PowerShell错误信息
    if (lowerContent.find (L"out-null") != wstring::npos ||
        lowerContent.find (L"command") != wstring::npos ||
        lowerContent.find (L"error") != wstring::npos ||
        lowerContent.find (L"failed") != wstring::npos)
    {
        return false;
    }

    // 过滤系统路径和命令
    if (lowerContent.find (L"c:\\windows") != wstring::npos ||
        lowerContent.find (L"c:/windows") != wstring::npos ||
        lowerContent.find (L"system32") != wstring::npos)
    {
        return false;
    }

    // 过滤乱码内容（包含大量非打印字符）
    int nonPrintableCount = 0;
    for (wchar_t ch : content)
    {
        if (ch < 32 && ch != L'\n' && ch != L'\r' && ch != L'\t')
        {
            nonPrintableCount++;
        }
    }
    if (nonPrintableCount > content.length () / 10)
    {
        return false;
    }

    return true;
}

// 复制内容到剪贴板
bool ClipboardManager::CopyToClipboard (const wstring& content)
{
    if (content.empty ())
    {
        return false;
    }

    // 暂停监听，防止重复记录
    SetListening (false);

    // 打开剪贴板
    if (!OpenClipboard (m_hWnd))
    {
        SetListening (true);
        return false;
    }

    // 清空剪贴板
    EmptyClipboard ();

    // 分配内存
    HGLOBAL hMem = GlobalAlloc (GMEM_MOVEABLE, (content.length () + 1) * sizeof (wchar_t));
    if (hMem == NULL)
    {
        CloseClipboard ();
        SetListening (true);
        return false;
    }

    // 复制内容
    wchar_t* pMem = (wchar_t*)GlobalLock (hMem);
    wcscpy_s (pMem, content.length () + 1, content.c_str ());
    GlobalUnlock (hMem);

    // 设置剪贴板数据
    SetClipboardData (CF_UNICODETEXT, hMem);

    // 关闭剪贴板
    CloseClipboard ();

    // 恢复监听
    SetListening (true);

    wcout << L"已复制到剪贴板" << endl;
    return true;
}

// 处理剪贴板更新
bool ClipboardManager::OnClipboardUpdate ()
{
    // 如果暂停监听，跳过处理
    if (!m_isListening)
    {
        return false;
    }

    // 检查是否为文字内容
    if (IsClipboardFormatAvailable (CF_UNICODETEXT))
    {
        return CaptureText ();
    }

    // 检查是否为图片内容
    if (IsClipboardFormatAvailable (CF_DIB))
    {
        return CaptureImage ();
    }

    return false;
}

// 捕获文字内容
bool ClipboardManager::CaptureText ()
{
    // 打开剪贴板
    if (!OpenClipboard (m_hWnd))
    {
        return false;
    }

    // 获取文字句柄
    HANDLE hData = GetClipboardData (CF_UNICODETEXT);
    if (hData == NULL)
    {
        CloseClipboard ();
        return false;
    }

    // 锁定内存获取文字指针
    wchar_t* pText = (wchar_t*)GlobalLock (hData);
    if (pText == NULL)
    {
        CloseClipboard ();
        return false;
    }

    // 获取文字长度
    int length = wcslen (pText);

    // 检查字符数限制（最大10000字符）
    if (length > 10000)
    {
        length = 10000;
    }

    // 创建内容字符串
    wstring content (pText, length);

    // 解锁内存
    GlobalUnlock (hData);

    // 关闭剪贴板
    CloseClipboard ();

    // 校验内容合法性
    if (!IsValidContent (content))
    {
        wcout << L"过滤无效内容: " << content.substr (0, 50) << endl;
        return false;
    }

    // 检查是否与最后一条记录相同（避免重复）
    if (!m_records.empty () && m_records[0].content == content)
    {
        return false;
    }

    // 创建记录
    ClipRecord record;
    record.id = GenerateId ();
    record.type = CLIP_TEXT;
    record.content = content;
    record.timestamp = time (NULL);
    record.isPinned = false;

    // 生成预览文本（前100字符）
    int previewLen = min (100, (int)record.content.length ());
    record.preview = record.content.substr (0, previewLen);
    if ((int)record.content.length () > 100)
    {
        record.preview += L"...";
    }

    // 添加记录
    AddRecord (record);

    wcout << L"捕获文字内容: " << record.preview << endl;
    return true;
}

// 捕获图片内容
bool ClipboardManager::CaptureImage ()
{
    // 打开剪贴板
    if (!OpenClipboard (m_hWnd))
    {
        return false;
    }

    // 获取DIB句柄
    HANDLE hData = GetClipboardData (CF_DIB);
    if (hData == NULL)
    {
        CloseClipboard ();
        return false;
    }

    // 锁定内存
    void* pData = GlobalLock (hData);
    if (pData == NULL)
    {
        CloseClipboard ();
        return false;
    }

    // 获取DIB信息
    BITMAPINFO* pBmi = (BITMAPINFO*)pData;

    // 初始化GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup (&gdiplusToken, &gdiplusStartupInput, NULL);

    // 创建GDI+位图
    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromHBITMAP (
        CreateDIBSection (NULL, pBmi, DIB_RGB_COLORS, NULL, NULL, 0),
        NULL
    );

    // 生成文件路径（绝对路径）
    int id = GenerateId ();
    wstring filePath = m_rootDir + L"\\clips\\images\\" + to_wstring (id) + L".png";

    // 保存为PNG
    CLSID pngClsid;
    // 获取PNG编码器CLSID
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize (&num, &size);
    if (size == 0)
    {
        GlobalUnlock (hData);
        CloseClipboard ();
        return false;
    }

    Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)malloc (size);
    Gdiplus::GetImageEncoders (num, size, pImageCodecInfo);

    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp (pImageCodecInfo[i].MimeType, L"image/png") == 0)
        {
            pngClsid = pImageCodecInfo[i].Clsid;
            break;
        }
    }
    free (pImageCodecInfo);

    // 保存图片
    pBitmap->Save (filePath.c_str (), &pngClsid, NULL);

    // 创建记录
    ClipRecord record;
    record.id = id;
    record.type = CLIP_IMAGE;
    record.content = L"[图片]";
    record.preview = L"[图片]";
    record.filePath = filePath;
    record.timestamp = time (NULL);
    record.isPinned = false;

    // 清理资源
    delete pBitmap;
    Gdiplus::GdiplusShutdown (gdiplusToken);
    GlobalUnlock (hData);
    CloseClipboard ();

    // 添加记录
    AddRecord (record);

    wcout << L"捕获图片内容: " << filePath << endl;
    return true;
}

// 添加记录
void ClipboardManager::AddRecord (const ClipRecord& record)
{
    // 插入到开头（最新的在前面）
    m_records.insert (m_records.begin (), record);

    wcout << L"添加记录，当前总数: " << m_records.size () << endl;
}

// 生成唯一ID
int ClipboardManager::GenerateId ()
{
    return m_nextId++;
}

// 获取所有历史记录
const vector<ClipRecord>& ClipboardManager::GetRecords () const
{
    return m_records;
}

// 获取记录数量
int ClipboardManager::GetRecordCount () const
{
    return m_records.size ();
}
