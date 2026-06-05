#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include "Storage.h"
#include <windows.h>
#include <string>
#include <vector>
#include "sqlite3.h"
using namespace std;

// 获取exe所在目录的绝对路径
static string GetExeDirA ()
{
    char Path[MAX_PATH];
    GetModuleFileNameA (NULL, Path, MAX_PATH);
    string FullPath (Path);
    size_t Pos = FullPath.find_last_of ("\\");
    if (Pos != string::npos)
    {
        return FullPath.substr (0, Pos);
    }
    return FullPath;
}

// 全局变量
static string g_exeDir = "";
static sqlite3* g_db = NULL;

// 构造函数
Storage::Storage ()
    : m_initialized (false)
{
}

// 析构函数
Storage::~Storage ()
{
    if (g_db)
    {
        sqlite3_close (g_db);
        g_db = NULL;
    }
}

// 设置程序根目录
void Storage::SetRootDir (const wstring & rootDir)
{
    m_rootDir = rootDir;
    g_exeDir = GetExeDirA ();
}

// 初始化存储系统
bool Storage::Initialize ()
{
    EnsureDirectories ();

    string DbPath = g_exeDir + "\\clips\\history.db";

    // 打开数据库（如果不存在则创建）
    int Rc = sqlite3_open (DbPath.c_str (), &g_db);
    if (Rc)
    {
        return false;
    }

    // 创建表（如果不存在）
    const char* CreateTableSQL =
        "CREATE TABLE IF NOT EXISTS records ("
        "id INTEGER PRIMARY KEY,"
        "type INTEGER,"
        "content TEXT,"
        "preview TEXT,"
        "filePath TEXT,"
        "timestamp INTEGER,"
        "isPinned INTEGER"
        ");";

    char* ErrMsg = NULL;
    Rc = sqlite3_exec (g_db, CreateTableSQL, NULL, NULL, &ErrMsg);
    if (Rc)
    {
        if (ErrMsg) sqlite3_free (ErrMsg);
        return false;
    }

    // 创建设置表
    const char* CreateSettingsSQL =
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY,"
        "value INTEGER"
        ");";

    Rc = sqlite3_exec (g_db, CreateSettingsSQL, NULL, NULL, &ErrMsg);
    if (Rc)
    {
        if (ErrMsg) sqlite3_free (ErrMsg);
        return false;
    }

    m_initialized = true;
    return true;
}

// 确保存储目录存在
void Storage::EnsureDirectories ()
{
    string ClipsDir = g_exeDir + "\\clips";
    CreateDirectoryA (ClipsDir.c_str (), NULL);
}

// 保存记录到数据库
bool Storage::SaveRecords (const vector<ClipRecord> & records)
{
    if (!g_db) return false;

    // 开始事务
    sqlite3_exec (g_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    // 清空旧记录
    sqlite3_exec (g_db, "DELETE FROM records;", NULL, NULL, NULL);

    // 插入新记录
    const char* InsertSQL = "INSERT INTO records (id, type, content, preview, filePath, timestamp, isPinned) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* Stmt = NULL;

    int Rc = sqlite3_prepare_v2 (g_db, InsertSQL, -1, &Stmt, NULL);
    if (Rc)
    {
        sqlite3_exec (g_db, "ROLLBACK;", NULL, NULL, NULL);
        return false;
    }

    for (const auto & record : records)
    {
        sqlite3_reset (Stmt);
        sqlite3_clear_bindings (Stmt);

        sqlite3_bind_int (Stmt, 1, record.id);
        sqlite3_bind_int (Stmt, 2, (int)record.type);

        // 将宽字符串转换为UTF-8
        string ContentUTF8 = wstring_to_utf8 (record.content);
        string PreviewUtf8 = wstring_to_utf8 (record.preview);
        string FilePathUtf8 = wstring_to_utf8 (record.filePath);

        sqlite3_bind_text (Stmt, 3, ContentUTF8.c_str (), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (Stmt, 4, PreviewUtf8.c_str (), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (Stmt, 5, FilePathUtf8.c_str (), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64 (Stmt, 6, (sqlite3_int64)record.timestamp);
        sqlite3_bind_int (Stmt, 7, record.isPinned ? 1 : 0);

        sqlite3_step (Stmt);
    }

    sqlite3_finalize (Stmt);

    // 提交事务
    sqlite3_exec (g_db, "COMMIT;", NULL, NULL, NULL);

    return true;
}

// 从数据库加载记录
bool Storage::LoadRecords (vector<ClipRecord> & records)
{
    if (!g_db) return false;

    const char* SelectSQL = "SELECT id, type, content, preview, filePath, timestamp, isPinned FROM records ORDER BY timestamp DESC;";
    sqlite3_stmt* Stmt = NULL;

    int Rc = sqlite3_prepare_v2 (g_db, SelectSQL, -1, &Stmt, NULL);
    if (Rc)
    {
        return false;
    }

    records.clear ();

    while (sqlite3_step (Stmt) == SQLITE_ROW)
    {
        ClipRecord record;
        record.id = sqlite3_column_int (Stmt, 0);
        record.type = (ClipType)sqlite3_column_int (Stmt, 1);

        // 将UTF-8转换为宽字符串
        const char* Content = (const char*)sqlite3_column_text (Stmt, 2);
        const char* Preview = (const char*)sqlite3_column_text (Stmt, 3);
        const char* FilePath = (const char*)sqlite3_column_text (Stmt, 4);

        record.content = Content ? utf8_to_wstring (Content) : L"";
        record.preview = Preview ? utf8_to_wstring (Preview) : L"";
        record.filePath = FilePath ? utf8_to_wstring (FilePath) : L"";

        record.timestamp = (time_t)sqlite3_column_int64 (Stmt, 5);
        record.isPinned = sqlite3_column_int (Stmt, 6) == 1;

        records.push_back (record);
    }

    sqlite3_finalize (Stmt);
    return true;
}

// 删除过期记录
int Storage::DeleteExpiredRecords (vector<ClipRecord> & records, int retentionDays)
{
    if (retentionDays <= 0)
    {
        return 0;
    }

    time_t Now = time (NULL);
    time_t ExpireTime = Now - (retentionDays * 24 * 60 * 60);
    int DeletedCount = 0;

    auto It = records.begin ();
    while (It != records.end ())
    {
        if (!It->isPinned && It->timestamp < ExpireTime)
        {
            DeleteRecordFile (*It);
            It = records.erase (It);
            DeletedCount++;
        }
        else
        {
            ++It;
        }
    }

    return DeletedCount;
}

// 删除记录对应的图片文件
void Storage::DeleteRecordFile (const ClipRecord & record)
{
    if (record.type == CLIP_IMAGE && !record.filePath.empty ())
    {
        DeleteFile (record.filePath.c_str ());
    }
}

// 保存设置
bool Storage::SaveSettings (int retentionDays, int maxRecords)
{
    if (!g_db) return false;

    const char* UpsertSQL = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* Stmt = NULL;

    int Rc = sqlite3_prepare_v2 (g_db, UpsertSQL, -1, &Stmt, NULL);
    if (Rc) return false;

    // 保存retentionDays
    sqlite3_reset (Stmt);
    sqlite3_clear_bindings (Stmt);
    sqlite3_bind_text (Stmt, 1, "retentionDays", -1, SQLITE_STATIC);
    sqlite3_bind_int (Stmt, 2, retentionDays);
    sqlite3_step (Stmt);

    // 保存maxRecords
    sqlite3_reset (Stmt);
    sqlite3_clear_bindings (Stmt);
    sqlite3_bind_text (Stmt, 1, "maxRecords", -1, SQLITE_STATIC);
    sqlite3_bind_int (Stmt, 2, maxRecords);
    sqlite3_step (Stmt);

    sqlite3_finalize (Stmt);
    return true;
}

// 加载设置
bool Storage::LoadSettings (int & retentionDays, int & maxRecords)
{
    if (!g_db)
    {
        retentionDays = 3;
        maxRecords = 1000;
        return true;
    }

    const char* SelectSQL = "SELECT key, value FROM settings;";
    sqlite3_stmt* Stmt = NULL;

    int Rc = sqlite3_prepare_v2 (g_db, SelectSQL, -1, &Stmt, NULL);
    if (Rc)
    {
        retentionDays = 3;
        maxRecords = 1000;
        return true;
    }

    retentionDays = 3;
    maxRecords = 1000;

    while (sqlite3_step (Stmt) == SQLITE_ROW)
    {
        const char* Key = (const char*)sqlite3_column_text (Stmt, 0);
        int Value = sqlite3_column_int (Stmt, 1);

        if (Key && string (Key) == "retentionDays")
        {
            retentionDays = Value;
        }
        else if (Key && string (Key) == "maxRecords")
        {
            maxRecords = Value;
        }
    }

    sqlite3_finalize (Stmt);
    return true;
}

// 保存开机自启设置到数据库
bool Storage::SaveAutoStartSetting (bool enabled)
{
    if (!g_db) return false;

    const char* UpsertSQL = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* Stmt = NULL;

    int Rc = sqlite3_prepare_v2 (g_db, UpsertSQL, -1, &Stmt, NULL);
    if (Rc) return false;

    sqlite3_reset (Stmt);
    sqlite3_clear_bindings (Stmt);
    sqlite3_bind_text (Stmt, 1, "autoStart", -1, SQLITE_STATIC);
    sqlite3_bind_int (Stmt, 2, enabled ? 1 : 0);
    sqlite3_step (Stmt);

    sqlite3_finalize (Stmt);
    return true;
}

// 从数据库加载开机自启设置
bool Storage::LoadAutoStartSetting (bool & enabled)
{
    if (!g_db)
    {
        enabled = false;
        return true;
    }

    const char* SelectSQL = "SELECT value FROM settings WHERE key = 'autoStart';";
    sqlite3_stmt* Stmt = NULL;

    int Rc = sqlite3_prepare_v2 (g_db, SelectSQL, -1, &Stmt, NULL);
    if (Rc)
    {
        enabled = false;
        return true;
    }

    enabled = false;
    if (sqlite3_step (Stmt) == SQLITE_ROW)
    {
        enabled = (sqlite3_column_int (Stmt, 0) == 1);
    }

    sqlite3_finalize (Stmt);
    return true;
}

// 设置开机自启（写入/删除注册表）
bool Storage::SetAutoStart (bool enabled)
{
    HKEY hKey;
    LPCWSTR RegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    LONG result = RegOpenKeyExW (HKEY_CURRENT_USER, RegPath, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    if (enabled)
    {
        // 获取exe绝对路径
        wchar_t ExePath[MAX_PATH];
        GetModuleFileNameW (NULL, ExePath, MAX_PATH);

        // 写入注册表
        result = RegSetValueExW (hKey, L"ClipboardHistoryManager", 0, REG_SZ,
            (const BYTE*)ExePath, (DWORD)(wcslen (ExePath) + 1) * sizeof (wchar_t));
    }
    else
    {
        // 删除注册表项
        result = RegDeleteValueW (hKey, L"ClipboardHistoryManager");
        // 如果值不存在也算成功
        if (result == ERROR_FILE_NOT_FOUND)
        {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey (hKey);
    return (result == ERROR_SUCCESS);
}

// 检查是否已设置开机自启
bool Storage::IsAutoStartEnabled ()
{
    HKEY hKey;
    LPCWSTR RegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    LONG result = RegOpenKeyExW (HKEY_CURRENT_USER, RegPath, 0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    wchar_t Value[MAX_PATH];
    DWORD ValueSize = sizeof (Value);
    DWORD ValueType = 0;

    result = RegQueryValueExW (hKey, L"ClipboardHistoryManager", NULL, &ValueType,
        (LPBYTE)Value, &ValueSize);

    RegCloseKey (hKey);
    return (result == ERROR_SUCCESS);
}

// 宽字符串转UTF-8
string wstring_to_utf8 (const wstring & wstr)
{
    if (wstr.empty ()) return string ();
    int Size = WideCharToMultiByte (CP_UTF8, 0, wstr.c_str (), (int)wstr.length (), NULL, 0, NULL, NULL);
    string Result (Size, 0);
    WideCharToMultiByte (CP_UTF8, 0, wstr.c_str (), (int)wstr.length (), &Result[0], Size, NULL, NULL);
    return Result;
}

// UTF-8转宽字符串
wstring utf8_to_wstring (const string & str)
{
    if (str.empty ()) return wstring ();
    int Size = MultiByteToWideChar (CP_UTF8, 0, str.c_str (), (int)str.length (), NULL, 0);
    wstring Result (Size, 0);
    MultiByteToWideChar (CP_UTF8, 0, str.c_str (), (int)str.length (), &Result[0], Size);
    return Result;
}

// 宽字符串转窄字符串（用于文件路径）
string wstring_to_string (const wstring & wstr)
{
    if (wstr.empty ()) return string ();
    int Size = WideCharToMultiByte (CP_ACP, 0, wstr.c_str (), (int)wstr.length (), NULL, 0, NULL, NULL);
    string Result (Size, 0);
    WideCharToMultiByte (CP_ACP, 0, wstr.c_str (), (int)wstr.length (), &Result[0], Size, NULL, NULL);
    return Result;
}
