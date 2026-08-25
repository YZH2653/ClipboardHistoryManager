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
#include <chrono>
#include <iomanip>
using namespace std;

// 构造函数
ClipboardManager::ClipboardManager ()
    : m_hWnd (NULL), m_nextId (1), m_maxRecords (1000), m_gdiplusToken (0), m_lastImageTime (0)
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
        return false;
    }

    return true;
}

// 设置程序根目录
void ClipboardManager::SetRootDir (const wstring& rootDir)
{
    m_rootDir = rootDir;
}

// 设置最大记录数
void ClipboardManager::SetMaxRecords (int maxRecords)
{
    m_maxRecords = maxRecords;
}

// 初始化GDI+
void ClipboardManager::InitializeGdiplus ()
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup (&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

// 清理GDI+
void ClipboardManager::ShutdownGdiplus ()
{
    if (m_gdiplusToken != 0)
    {
        Gdiplus::GdiplusShutdown (m_gdiplusToken);
        m_gdiplusToken = 0;
    }
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

    // 过滤乱码内容（包含大量非打印字符）
    int nonPrintableCount = 0;
    for (wchar_t ch : content)
    {
        if (ch < 32 && ch != L'\n' && ch != L'\r' && ch != L'\t')
        {
            nonPrintableCount++;
        }
    }
    if (nonPrintableCount > (int)content.length () / 10)
    {
        return false;
    }

    return true;
}

// 检查是否与历史记录重复
bool ClipboardManager::IsDuplicate (const wstring& content)
{
    if (m_records.empty ())
    {
        return false;
    }

    // 检查所有记录，防止重复
    for (const auto& record : m_records)
    {
        if (record.type == CLIP_TEXT && record.content == content)
        {
            return true;
        }
    }

    return false;
}

// 清理超出限制的记录
void ClipboardManager::CleanupOldRecords ()
{
    if (m_maxRecords <= 0)
    {
        return;
    }

    // 如果超过最大记录数，删除末尾（最旧）的记录
    while ((int)m_records.size () > m_maxRecords)
    {
        // 从末尾开始查找非置顶记录
        for (auto it = m_records.rbegin (); it != m_records.rend (); ++it)
        {
            if (!it->isPinned)
            {
                // 删除对应的图片文件
                if (it->type == CLIP_IMAGE && !it->filePath.empty ())
                {
                    DeleteFileW (it->filePath.c_str ());
                }
                // 删除记录
                m_records.erase (--(it.base ()));
                break;
            }
        }
    }
}

// 复制内容到剪贴板
bool ClipboardManager::CopyToClipboard (const wstring& content)
{
    if (content.empty ())
    {
        return false;
    }

    // 打开剪贴板
    if (!OpenClipboard (m_hWnd))
    {
        return false;
    }

    // 清空剪贴板
    EmptyClipboard ();

    // 分配内存
    HGLOBAL hMem = GlobalAlloc (GMEM_MOVEABLE, (content.length () + 1) * sizeof (wchar_t));
    if (hMem == NULL)
    {
        CloseClipboard ();
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

    return true;
}

// 处理剪贴板更新
bool ClipboardManager::OnClipboardUpdate ()
{
    bool result = false;

    // 如果同时存在图片和文字，优先捕获图片（截图工具会同时写入多种格式）
    if (IsClipboardFormatAvailable (CF_DIB))
    {
        result = CaptureImage ();
    }
    // 只有在没有图片时才捕获文字
    else if (IsClipboardFormatAvailable (CF_UNICODETEXT))
    {
        result = CaptureText ();
    }

    return result;
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

    // 检查是否与上次捕获的内容相同（防止重复）
    if (content == m_lastContent)
    {
        return false;
    }
    m_lastContent = content;

    // 校验内容合法性
    if (!IsValidContent (content))
    {
        return false;
    }

    // 检查是否与最近记录重复
    if (IsDuplicate (content))
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

    // 获取DIB数据
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

    // 获取BITMAPINFO指针
    BITMAPINFO* pBmi = (BITMAPINFO*)pData;

    // 计算图片数据的简单哈希（取前1024字节做快速比较）
    string currentHash = "";
    {
        BYTE* pixels = (BYTE*)pData + pBmi->bmiHeader.biSize;
        int dataSize = min (1024, (int)GlobalSize (hData));
        unsigned int hash = 0;
        for (int i = 0; i < dataSize; i++)
        {
            hash = hash * 31 + pixels[i];
        }
        ostringstream hashOss;
        hashOss << hex << hash;
        currentHash = hashOss.str ();
    }

    // 检查是否与上次捕获的图片相同（哈希相同且时间间隔小于10秒）
    time_t now = time (NULL);
    if (!m_lastImageHash.empty () && currentHash == m_lastImageHash
        && (now - m_lastImageTime) < 10)
    {
        GlobalUnlock (hData);
        CloseClipboard ();
        return false;
    }

    // 使用GDI+从DIB数据创建Bitmap
    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromBITMAPINFO (pBmi, pData);
    if (pBitmap == NULL)
    {
        GlobalUnlock (hData);
        CloseClipboard ();
        return false;
    }

    // 确保images目录存在
    wstring imagesDir = m_rootDir + L"\\clips\\images";
    CreateDirectoryW (imagesDir.c_str (), NULL);

    // 生成文件路径（绝对路径）
    int id = GenerateId ();
    wstring filePath = m_rootDir + L"\\clips\\images\\" + to_wstring (id) + L".png";

    // 保存为PNG
    CLSID pngClsid;
    bool foundPngEncoder = false;
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize (&num, &size);
    if (size == 0)
    {
        delete pBitmap;
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
            foundPngEncoder = true;
            break;
        }
    }
    free (pImageCodecInfo);

    if (!foundPngEncoder)
    {
        delete pBitmap;
        GlobalUnlock (hData);
        CloseClipboard ();
        return false;
    }

    // 保存图片
    Gdiplus::Status status = pBitmap->Save (filePath.c_str (), &pngClsid, NULL);

    // 清理资源
    delete pBitmap;
    GlobalUnlock (hData);
    CloseClipboard ();

    if (status != Gdiplus::Ok)
    {
        return false;
    }

    // 创建记录
    ClipRecord record;
    record.id = id;
    record.type = CLIP_IMAGE;
    record.content = L"[图片]";
    record.preview = L"[图片]";
    record.filePath = filePath;
    record.timestamp = time (NULL);
    record.isPinned = false;

    // 更新上次图片信息（用于去重）
    m_lastImageHash = currentHash;
    m_lastImageTime = time (NULL);

    // 添加记录
    AddRecord (record);

    return true;
}

// 添加记录
void ClipboardManager::AddRecord (const ClipRecord& record)
{
    // 插入到开头（最新的在前面）
    m_records.insert (m_records.begin (), record);

    // 清理超出限制的记录
    CleanupOldRecords ();
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

// 清空内部记录
void ClipboardManager::ClearRecords ()
{
    m_records.clear ();
    m_lastContent.clear ();
}
