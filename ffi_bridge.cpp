#include <windows.h>
#include <cstdint>
#include "ClipboardManager.h"
#include "Storage.h"
#include <string>
#include <vector>
using namespace std;

// 全局后端实例
static ClipboardManager G_ClipManager;
static Storage G_Storage;
static vector<ClipRecord> G_Records;
static bool G_Initialized = false;
static HWND G_HiddenWnd = NULL;
static HANDLE G_ClipThread = NULL;

// 宽字符串转UTF-8
static string WstringToUtf8 (const wstring& ws)
{
    if (ws.empty ()) return "";
    int len = WideCharToMultiByte (CP_UTF8, 0, ws.c_str (),
        (int)ws.size (), NULL, 0, NULL, NULL);
    string result (len, 0);
    WideCharToMultiByte (CP_UTF8, 0, ws.c_str (),
        (int)ws.size (), &result[0], len, NULL, NULL);
    return result;
}

// UTF-8转宽字符串
static wstring Utf8ToWstring (const string& s)
{
    if (s.empty ()) return L"";
    int len = MultiByteToWideChar (CP_UTF8, 0, s.c_str (),
        (int)s.size (), NULL, 0);
    wstring result (len, 0);
    MultiByteToWideChar (CP_UTF8, 0, s.c_str (),
        (int)s.size (), &result[0], len);
    return result;
}

// 隐藏窗口过程
static LRESULT CALLBACK HiddenWndProc (
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_CLIPBOARDUPDATE)
    {
        G_ClipManager.OnClipboardUpdate ();
        G_Records = G_ClipManager.GetRecords ();
        G_Storage.SaveRecords (G_Records);
        return 0;
    }
    if (msg == WM_DESTROY)
    {
        PostQuitMessage (0);
        return 0;
    }
    return DefWindowProc (hWnd, msg, wParam, lParam);
}

// 剪贴板监听线程
static DWORD WINAPI ClipThreadProc (LPVOID lpParam)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof (WNDCLASSEX);
    wc.lpfnWndProc = HiddenWndProc;
    wc.hInstance = GetModuleHandle (NULL);
    wc.lpszClassName = L"ClipMonitor";
    RegisterClassEx (&wc);

    G_HiddenWnd = CreateWindowEx (0, L"ClipMonitor", L"",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL,
        GetModuleHandle (NULL), NULL);

    if (!G_HiddenWnd) return 1;

    AddClipboardFormatListener (G_HiddenWnd);

    MSG msg;
    while (GetMessage (&msg, NULL, 0, 0))
    {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }

    RemoveClipboardFormatListener (G_HiddenWnd);
    DestroyWindow (G_HiddenWnd);
    return 0;
}

// FFI 记录结构
struct FFIRecord
{
    int64_t Id;
    int Type;
    char Content[10001];
    char Preview[101];
    char FilePath[512];
    int64_t Timestamp;
    bool IsPinned;
};

// FFI 设置结构
struct FFISettings
{
    int RetentionDays;
    int MaxRecords;
    bool AutoStart;
    bool MinimizeToTray;
};

// 初始化后端
extern "C" __declspec(dllexport)
bool FfiInitialize (const char* rootDirUtf8)
{
    if (G_Initialized) return true;

    wstring rootDir = Utf8ToWstring (string (rootDirUtf8));

    G_Storage.SetRootDir (rootDir);
    if (!G_Storage.Initialize ()) return false;

    G_ClipManager.SetRootDir (rootDir);
    G_ClipManager.InitializeGdiplus ();

    int retention = 3, maxRec = 1000;
    G_Storage.LoadSettings (retention, maxRec);
    G_ClipManager.SetMaxRecords (maxRec);

    G_Storage.LoadRecords (G_Records);
    for (auto& r : G_Records)
    {
        G_ClipManager.AddRecord (r);
    }
    G_Records = G_ClipManager.GetRecords ();

    G_Storage.DeleteExpiredRecords (G_Records, retention);
    G_Records = G_ClipManager.GetRecords ();

    G_ClipThread = CreateThread (
        NULL, 0, ClipThreadProc, NULL, 0, NULL);

    G_Initialized = true;
    return true;
}

// 关闭后端
extern "C" __declspec(dllexport)
void FfiShutdown ()
{
    if (!G_Initialized) return;

    if (G_HiddenWnd)
    {
        PostMessage (G_HiddenWnd, WM_CLOSE, 0, 0);
    }
    if (G_ClipThread)
    {
        WaitForSingleObject (G_ClipThread, 3000);
        CloseHandle (G_ClipThread);
        G_ClipThread = NULL;
    }

    G_Storage.SaveRecords (G_Records);
    G_ClipManager.ShutdownGdiplus ();
    G_Initialized = false;
}

// 获取所有记录
extern "C" __declspec(dllexport)
int FfiGetRecords (FFIRecord* outRecords, int maxCount)
{
    int count = min ((int)G_Records.size (), maxCount);
    for (int i = 0; i < count; i++)
    {
        const auto& r = G_Records[i];
        outRecords[i].Id = r.id;
        outRecords[i].Type = (r.type == CLIP_TEXT) ? 0 : 1;

        string content = WstringToUtf8 (r.content);
        strncpy (outRecords[i].Content, content.c_str (), 10000);
        outRecords[i].Content[10000] = '\0';

        string preview = WstringToUtf8 (r.preview);
        strncpy (outRecords[i].Preview, preview.c_str (), 100);
        outRecords[i].Preview[100] = '\0';

        string path = WstringToUtf8 (r.filePath);
        strncpy (outRecords[i].FilePath, path.c_str (), 511);
        outRecords[i].FilePath[511] = '\0';

        outRecords[i].Timestamp = r.timestamp;
        outRecords[i].IsPinned = r.isPinned;
    }
    return count;
}

// 复制文本到剪贴板
extern "C" __declspec(dllexport)
bool FfiCopyToClipboard (const char* textUtf8)
{
    wstring wtext = Utf8ToWstring (string (textUtf8));
    return G_ClipManager.CopyToClipboard (wtext);
}

// 通过ID复制记录内容
extern "C" __declspec(dllexport)
bool FfiCopyRecord (int64_t id)
{
    for (const auto& r : G_Records)
    {
        if (r.id == id)
        {
            return G_ClipManager.CopyToClipboard (r.content);
        }
    }
    return false;
}

// 删除记录
extern "C" __declspec(dllexport)
bool FfiDeleteRecord (int64_t id)
{
    for (auto it = G_Records.begin (); it != G_Records.end (); ++it)
    {
        if (it->id == id)
        {
            G_Storage.DeleteRecordFile (*it);
            G_Records.erase (it);
            G_Storage.SaveRecords (G_Records);
            return true;
        }
    }
    return false;
}

// 切换置顶
extern "C" __declspec(dllexport)
bool FfiTogglePin (int64_t id)
{
    for (auto& r : G_Records)
    {
        if (r.id == id)
        {
            r.isPinned = !r.isPinned;
            G_Storage.SaveRecords (G_Records);
            return r.isPinned;
        }
    }
    return false;
}

// 批量删除
extern "C" __declspec(dllexport)
int FfiBatchDelete (const int64_t* ids, int count)
{
    int deleted = 0;
    for (int i = 0; i < count; i++)
    {
        for (auto it = G_Records.begin (); it != G_Records.end (); ++it)
        {
            if (it->id == ids[i])
            {
                G_Storage.DeleteRecordFile (*it);
                G_Records.erase (it);
                deleted++;
                break;
            }
        }
    }
    if (deleted > 0) G_Storage.SaveRecords (G_Records);
    return deleted;
}

// 清空所有记录
extern "C" __declspec(dllexport)
void FfiClearAll ()
{
    for (auto& r : G_Records)
    {
        G_Storage.DeleteRecordFile (r);
    }
    G_Records.clear ();
    G_Storage.SaveRecords (G_Records);
}

// 获取设置
extern "C" __declspec(dllexport)
void FfiGetSettings (FFISettings* settings)
{
    int retention = 3, maxRec = 1000;
    bool autoStart = false, minimizeToTray = true;
    G_Storage.LoadSettings (retention, maxRec);
    G_Storage.LoadAutoStartSetting (autoStart);
    G_Storage.LoadMinimizeToTraySetting (minimizeToTray);
    settings->RetentionDays = retention;
    settings->MaxRecords = maxRec;
    settings->AutoStart = autoStart;
    settings->MinimizeToTray = minimizeToTray;
}

// 保存设置
extern "C" __declspec(dllexport)
void FfiSaveSettings (const FFISettings* settings)
{
    G_Storage.SaveSettings (settings->RetentionDays, settings->MaxRecords);
    G_Storage.SaveAutoStartSetting (settings->AutoStart);
    G_Storage.SaveMinimizeToTraySetting (settings->MinimizeToTray);
    G_ClipManager.SetMaxRecords (settings->MaxRecords);
    Storage::SetAutoStart (settings->AutoStart);
}
