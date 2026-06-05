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

// 诊断日志相关
static int g_callbackCount = 0;
static string g_logFilePath = "";

// 初始化日志文件路径
static void InitLogPath ()
{
    if (g_logFilePath.empty ())
    {
        char TempPath[MAX_PATH];
        GetTempPathA (MAX_PATH, TempPath);
        g_logFilePath = string (TempPath) + "clipboard_debug.log";
    }
}

// 输出日志到控制台和文件
static void DebugLog (const string & msg)
{
    // 输出到控制台
    OutputDebugStringA (msg.c_str ());
    OutputDebugStringA ("\n");

    // 追加写入文件
    ofstream LogFile (g_logFilePath, ios::app);
    if (LogFile.is_open ())
    {
        LogFile << msg << endl;
        LogFile.close ();
    }
}

// 获取当前时间戳（毫秒级）
static string GetTimestamp ()
{
    auto now = chrono::system_clock::now ();
    auto time = chrono::system_clock::to_time_t (now);
    auto ms = chrono::duration_cast<chrono::milliseconds> (now.time_since_epoch ()) % 1000;

    struct tm TimeInfo;
    localtime_s (&TimeInfo, &time);

    ostringstream oss;
    oss << put_time (&TimeInfo, "%H:%M:%S")
        << "." << setfill ('0') << setw (3) << ms.count ();
    return oss.str ();
}

// 获取格式名称
static string GetFormatName (UINT format)
{
    switch (format)
    {
    case CF_TEXT: return "CF_TEXT";
    case CF_BITMAP: return "CF_BITMAP";
    case CF_UNICODETEXT: return "CF_UNICODETEXT";
    case CF_DIB: return "CF_DIB";
    case CF_DIBV5: return "CF_DIBV5";
    case CF_HDROP: return "CF_HDROP";
    case CF_LOCALE: return "CF_LOCALE";
    default:
    {
        char name[256];
        int len = GetClipboardFormatNameA (format, name, 256);
        if (len > 0)
        {
            return string (name, len);
        }
        return "Unknown(" + to_string (format) + ")";
    }
    }
}

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
void ClipboardManager::SetRootDir (const wstring & RootDir)
{
    m_rootDir = RootDir;
}

// 设置最大记录数
void ClipboardManager::SetMaxRecords (int MaxRecords)
{
    m_maxRecords = MaxRecords;
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
bool ClipboardManager::IsValidContent (const wstring & content)
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
    int NonPrintableCount = 0;
    for (wchar_t ch : content)
    {
        if (ch < 32 && ch != L'\n' && ch != L'\r' && ch != L'\t')
        {
            NonPrintableCount++;
        }
    }
    if (NonPrintableCount > (int)content.length () / 10)
    {
        return false;
    }

    return true;
}

// 检查是否与最近记录重复
bool ClipboardManager::IsDuplicate (const wstring & content)
{
    if (m_records.empty ())
    {
        return false;
    }

    // 检查是否与最近5条记录中的任意一条相同（防止短时间内重复）
    int CheckCount = min (5, (int)m_records.size ());
    for (int i = 0; i < CheckCount; i++)
    {
        if (m_records[i].type == CLIP_TEXT && m_records[i].content == content)
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
bool ClipboardManager::CopyToClipboard (const wstring & content)
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
    // 初始化日志
    InitLogPath ();

    // 递增回调计数器
    g_callbackCount++;

    // 获取诊断信息
    string Timestamp = GetTimestamp ();
    DWORD ThreadId = GetCurrentThreadId ();
    UINT CurrentSeq = GetClipboardSequenceNumber ();

    // 输出回调基本信息
    ostringstream LogMsg;
    LogMsg << "[" << Timestamp << "] 回调 #" << g_callbackCount
           << " | 线程=0x" << hex << ThreadId
           << " | 序列号=" << dec << CurrentSeq;
    DebugLog (LogMsg.str ());

    // 枚举剪贴板格式
    if (OpenClipboard (m_hWnd))
    {
        ostringstream FormatLog;
        FormatLog << "[" << Timestamp << "] 格式枚举: ";

        UINT format = 0;
        bool first = true;
        while ((format = EnumClipboardFormats (format)) != 0)
        {
            if (!first)
            {
                FormatLog << ", ";
            }
            FormatLog << format << " (" << GetFormatName (format) << ")";
            first = false;
        }

        DebugLog (FormatLog.str ());
        CloseClipboard ();
    }

    bool result = false;
    string SkipReason = "";

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
    else
    {
        SkipReason = "没有可识别的格式";
    }

    // 输出保存结果
    ostringstream ResultLog;
    ResultLog << "[" << Timestamp << "] ";
    if (result)
    {
        ResultLog << "保存成功";
    }
    else
    {
        ResultLog << "跳过保存";
        if (!SkipReason.empty ())
        {
            ResultLog << " (" << SkipReason << ")";
        }
    }
    DebugLog (ResultLog.str ());
    DebugLog ("---");

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
    int PreviewLen = min (100, (int)record.content.length ());
    record.preview = record.content.substr (0, PreviewLen);
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
    string Timestamp = GetTimestamp ();

    // 打开剪贴板
    if (!OpenClipboard (m_hWnd))
    {
        DebugLog ("[" + Timestamp + "] CaptureImage: 打开剪贴板失败");
        return false;
    }

    // 获取DIB句柄
    HANDLE hData = GetClipboardData (CF_DIB);
    if (hData == NULL)
    {
        DebugLog ("[" + Timestamp + "] CaptureImage: 获取CF_DIB数据失败");
        CloseClipboard ();
        return false;
    }

    // 锁定内存
    void* pData = GlobalLock (hData);
    if (pData == NULL)
    {
        DebugLog ("[" + Timestamp + "] CaptureImage: 锁定内存失败");
        CloseClipboard ();
        return false;
    }

    // 获取DIB信息
    BITMAPINFO* pBmi = (BITMAPINFO*)pData;

    // 输出图片信息
    int ImgWidth = pBmi->bmiHeader.biWidth;
    int ImgHeight = pBmi->bmiHeader.biHeight;
    ostringstream ImgInfo;
    ImgInfo << "[" << Timestamp << "] CaptureImage: 图片尺寸="
            << ImgWidth << "x" << ImgHeight
            << ", 位深=" << pBmi->bmiHeader.biBitCount;
    DebugLog (ImgInfo.str ());

    // 计算图片数据的简单哈希（取前1024字节做快速比较）
    string CurrentHash = "";
    {
        // 读取DIB数据的前1024字节计算哈希
        BYTE* pixels = (BYTE*)pData + pBmi->bmiHeader.biSize;
        int DataSize = min (1024, (int)GlobalSize (hData));
        unsigned int hash = 0;
        for (int i = 0; i < DataSize; i++)
        {
            hash = hash * 31 + pixels[i];
        }
        ostringstream HashOss;
        HashOss << hex << hash;
        CurrentHash = HashOss.str ();
    }
    DebugLog ("[" + Timestamp + "] CaptureImage: 哈希=" + CurrentHash + ", 上次哈希=" + m_lastImageHash);

    // 检查是否与上次捕获的图片相同（哈希相同且时间间隔小于10秒）
    time_t now = time (NULL);
    if (!m_lastImageHash.empty () && CurrentHash == m_lastImageHash
        && (now - m_lastImageTime) < 10)
    {
        DebugLog ("[" + Timestamp + "] CaptureImage: 与上次图片内容相同且间隔<10秒，跳过");
        GlobalUnlock (hData);
        CloseClipboard ();
        return false;
    }

    // 使用GDI+从DIB数据创建Bitmap
    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromBITMAPINFO (pBmi, pData);

    // 确保images目录存在
    wstring ImagesDir = m_rootDir + L"\\clips\\images";
    CreateDirectoryW (ImagesDir.c_str (), NULL);

    // 生成文件路径（绝对路径）
    int id = GenerateId ();
    wstring FilePath = m_rootDir + L"\\clips\\images\\" + to_wstring (id) + L".png";

    // 保存为PNG
    CLSID PngClsid;
    bool FoundPngEncoder = false;
    // 获取PNG编码器CLSID
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize (&num, &size);
    if (size == 0)
    {
        DebugLog ("[" + Timestamp + "] CaptureImage: 获取图片编码器失败");
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
            PngClsid = pImageCodecInfo[i].Clsid;
            FoundPngEncoder = true;
            break;
        }
    }
    free (pImageCodecInfo);

    if (!FoundPngEncoder)
    {
        DebugLog ("[" + Timestamp + "] CaptureImage: 未找到PNG编码器");
        delete pBitmap;
        GlobalUnlock (hData);
        CloseClipboard ();
        return false;
    }

    // 保存图片
    Gdiplus::Status status = pBitmap->Save (FilePath.c_str (), &PngClsid, NULL);

    // 输出保存结果
    char FilePathA[MAX_PATH];
    WideCharToMultiByte (CP_ACP, 0, FilePath.c_str (), -1, FilePathA, MAX_PATH, NULL, NULL);
    ostringstream SaveLog;
    SaveLog << "[" << Timestamp << "] CaptureImage: 保存图片到 "
            << "id=" << id << ", 状态=" << (int)status
            << ", 路径=" << FilePathA;
    DebugLog (SaveLog.str ());

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
    record.filePath = FilePath;
    record.timestamp = time (NULL);
    record.isPinned = false;

    // 更新上次图片信息（用于去重）
    m_lastImageHash = CurrentHash;
    m_lastImageTime = time (NULL);

    // 添加记录
    AddRecord (record);

    return true;
}

// 添加记录
void ClipboardManager::AddRecord (const ClipRecord & record)
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
const vector<ClipRecord> & ClipboardManager::GetRecords () const
{
    return m_records;
}

// 获取记录数量
int ClipboardManager::GetRecordCount () const
{
    return m_records.size ();
}
