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
    char path[MAX_PATH];
    GetModuleFileNameA (NULL, path, MAX_PATH);
    string fullPath (path);
    size_t pos = fullPath.find_last_of ("\\");
    if (pos != string::npos)
    {
        return fullPath.substr (0, pos);
    }
    return fullPath;
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
void Storage::SetRootDir (const wstring& rootDir)
{
    m_rootDir = rootDir;
    g_exeDir = GetExeDirA ();
}

// 初始化存储系统
bool Storage::Initialize ()
{
    EnsureDirectories ();

    string dbPath = g_exeDir + "\\clips\\history.db";

    // 打开数据库（如果不存在则创建）
    int rc = sqlite3_open (dbPath.c_str (), &g_db);
    if (rc)
    {
        return false;
    }

    // 创建表（如果不存在）
    const char* createTableSQL =
        "CREATE TABLE IF NOT EXISTS records ("
        "id INTEGER PRIMARY KEY,"
        "type INTEGER,"
        "content TEXT,"
        "preview TEXT,"
        "filePath TEXT,"
        "timestamp INTEGER,"
        "isPinned INTEGER"
        ");";

    char* errMsg = NULL;
    rc = sqlite3_exec (g_db, createTableSQL, NULL, NULL, &errMsg);
    if (rc)
    {
        if (errMsg) sqlite3_free (errMsg);
        return false;
    }

    // 创建设置表
    const char* createSettingsSQL =
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY,"
        "value INTEGER"
        ");";

    rc = sqlite3_exec (g_db, createSettingsSQL, NULL, NULL, &errMsg);
    if (rc)
    {
        if (errMsg) sqlite3_free (errMsg);
        return false;
    }

    m_initialized = true;
    return true;
}

// 确保存储目录存在
void Storage::EnsureDirectories ()
{
    string clipsDir = g_exeDir + "\\clips";
    CreateDirectoryA (clipsDir.c_str (), NULL);
}

// 保存记录到数据库
bool Storage::SaveRecords (const vector<ClipRecord>& records)
{
    if (!g_db) return false;

    // 开始事务
    sqlite3_exec (g_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    // 清空旧记录
    sqlite3_exec (g_db, "DELETE FROM records;", NULL, NULL, NULL);

    // 插入新记录
    const char* insertSQL = "INSERT INTO records (id, type, content, preview, filePath, timestamp, isPinned) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (g_db, insertSQL, -1, &stmt, NULL);
    if (rc)
    {
        sqlite3_exec (g_db, "ROLLBACK;", NULL, NULL, NULL);
        return false;
    }

    for (const auto& record : records)
    {
        sqlite3_reset (stmt);
        sqlite3_clear_bindings (stmt);

        sqlite3_bind_int (stmt, 1, record.id);
        sqlite3_bind_int (stmt, 2, (int)record.type);

        // 将宽字符串转换为UTF-8
        string content_utf8 = wstring_to_utf8 (record.content);
        string preview_utf8 = wstring_to_utf8 (record.preview);
        string filePath_utf8 = wstring_to_utf8 (record.filePath);

        sqlite3_bind_text (stmt, 3, content_utf8.c_str (), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 4, preview_utf8.c_str (), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 5, filePath_utf8.c_str (), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64 (stmt, 6, (sqlite3_int64)record.timestamp);
        sqlite3_bind_int (stmt, 7, record.isPinned ? 1 : 0);

        sqlite3_step (stmt);
    }

    sqlite3_finalize (stmt);

    // 提交事务
    sqlite3_exec (g_db, "COMMIT;", NULL, NULL, NULL);

    return true;
}

// 从数据库加载记录
bool Storage::LoadRecords (vector<ClipRecord>& records)
{
    if (!g_db) return false;

    const char* selectSQL = "SELECT id, type, content, preview, filePath, timestamp, isPinned FROM records ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (g_db, selectSQL, -1, &stmt, NULL);
    if (rc)
    {
        return false;
    }

    records.clear ();

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        ClipRecord record;
        record.id = sqlite3_column_int (stmt, 0);
        record.type = (ClipType)sqlite3_column_int (stmt, 1);

        // 将UTF-8转换为宽字符串
        const char* content = (const char*)sqlite3_column_text (stmt, 2);
        const char* preview = (const char*)sqlite3_column_text (stmt, 3);
        const char* filePath = (const char*)sqlite3_column_text (stmt, 4);

        record.content = content ? utf8_to_wstring (content) : L"";
        record.preview = preview ? utf8_to_wstring (preview) : L"";
        record.filePath = filePath ? utf8_to_wstring (filePath) : L"";

        record.timestamp = (time_t)sqlite3_column_int64 (stmt, 5);
        record.isPinned = sqlite3_column_int (stmt, 6) == 1;

        records.push_back (record);
    }

    sqlite3_finalize (stmt);
    return true;
}

// 删除过期记录
int Storage::DeleteExpiredRecords (vector<ClipRecord>& records, int retentionDays)
{
    if (retentionDays <= 0)
    {
        return 0;
    }

    time_t now = time (NULL);
    time_t expireTime = now - (retentionDays * 24 * 60 * 60);
    int deletedCount = 0;

    auto it = records.begin ();
    while (it != records.end ())
    {
        if (!it->isPinned && it->timestamp < expireTime)
        {
            DeleteRecordFile (*it);
            it = records.erase (it);
            deletedCount++;
        }
        else
        {
            ++it;
        }
    }

    return deletedCount;
}

// 删除记录对应的图片文件
void Storage::DeleteRecordFile (const ClipRecord& record)
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

    const char* upsertSQL = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (g_db, upsertSQL, -1, &stmt, NULL);
    if (rc) return false;

    // 保存retentionDays
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, "retentionDays", -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, retentionDays);
    sqlite3_step (stmt);

    // 保存maxRecords
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, "maxRecords", -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, maxRecords);
    sqlite3_step (stmt);

    sqlite3_finalize (stmt);
    return true;
}

// 加载设置
bool Storage::LoadSettings (int& retentionDays, int& maxRecords)
{
    if (!g_db)
    {
        retentionDays = 3;
        maxRecords = 1000;
        return true;
    }

    const char* selectSQL = "SELECT key, value FROM settings;";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (g_db, selectSQL, -1, &stmt, NULL);
    if (rc)
    {
        retentionDays = 3;
        maxRecords = 1000;
        return true;
    }

    retentionDays = 3;
    maxRecords = 1000;

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        const char* key = (const char*)sqlite3_column_text (stmt, 0);
        int value = sqlite3_column_int (stmt, 1);

        if (key && string (key) == "retentionDays")
        {
            retentionDays = value;
        }
        else if (key && string (key) == "maxRecords")
        {
            maxRecords = value;
        }
    }

    sqlite3_finalize (stmt);
    return true;
}

// 保存开机自启设置到数据库
bool Storage::SaveAutoStartSetting (bool enabled)
{
    if (!g_db) return false;

    const char* upsertSQL = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (g_db, upsertSQL, -1, &stmt, NULL);
    if (rc) return false;

    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, "autoStart", -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, enabled ? 1 : 0);
    sqlite3_step (stmt);

    sqlite3_finalize (stmt);
    return true;
}

// 从数据库加载开机自启设置
bool Storage::LoadAutoStartSetting (bool& enabled)
{
    if (!g_db)
    {
        enabled = false;
        return true;
    }

    const char* selectSQL = "SELECT value FROM settings WHERE key = 'autoStart';";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (g_db, selectSQL, -1, &stmt, NULL);
    if (rc)
    {
        enabled = false;
        return true;
    }

    enabled = false;
    if (sqlite3_step (stmt) == SQLITE_ROW)
    {
        enabled = (sqlite3_column_int (stmt, 0) == 1);
    }

    sqlite3_finalize (stmt);
    return true;
}

// 保存关闭时最小化到托盘设置
bool Storage::SaveMinimizeToTraySetting (bool enabled)
{
    if (!g_db) return false;

    const char* upsertSQL = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (g_db, upsertSQL, -1, &stmt, NULL);
    if (rc) return false;

    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, "minimizeToTray", -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, enabled ? 1 : 0);
    sqlite3_step (stmt);

    sqlite3_finalize (stmt);
    return true;
}

// 加载关闭时最小化到托盘设置
bool Storage::LoadMinimizeToTraySetting (bool& enabled)
{
    if (!g_db)
    {
        enabled = true;
        return true;
    }

    const char* selectSQL = "SELECT value FROM settings WHERE key = 'minimizeToTray';";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (g_db, selectSQL, -1, &stmt, NULL);
    if (rc)
    {
        enabled = true;
        return true;
    }

    enabled = true;
    if (sqlite3_step (stmt) == SQLITE_ROW)
    {
        enabled = (sqlite3_column_int (stmt, 0) == 1);
    }

    sqlite3_finalize (stmt);
    return true;
}

// 设置开机自启（写入/删除注册表）
bool Storage::SetAutoStart (bool enabled)
{
    HKEY hKey;
    LPCWSTR regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    LONG result = RegOpenKeyExW (HKEY_CURRENT_USER, regPath, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    if (enabled)
    {
        // 获取exe绝对路径
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW (NULL, exePath, MAX_PATH);

        // 写入注册表
        result = RegSetValueExW (hKey, L"ClipboardHistoryManager", 0, REG_SZ,
            (const BYTE*)exePath, (DWORD)(wcslen (exePath) + 1) * sizeof (wchar_t));
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
    LPCWSTR regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    LONG result = RegOpenKeyExW (HKEY_CURRENT_USER, regPath, 0, KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    wchar_t value[MAX_PATH];
    DWORD valueSize = sizeof (value);
    DWORD valueType = 0;

    result = RegQueryValueExW (hKey, L"ClipboardHistoryManager", NULL, &valueType,
        (LPBYTE)value, &valueSize);

    RegCloseKey (hKey);
    return (result == ERROR_SUCCESS);
}

// 宽字符串转UTF-8
string wstring_to_utf8 (const wstring& wstr)
{
    if (wstr.empty ()) return string ();
    int size = WideCharToMultiByte (CP_UTF8, 0, wstr.c_str (), (int)wstr.length (), NULL, 0, NULL, NULL);
    string result (size, 0);
    WideCharToMultiByte (CP_UTF8, 0, wstr.c_str (), (int)wstr.length (), &result[0], size, NULL, NULL);
    return result;
}

// UTF-8转宽字符串
wstring utf8_to_wstring (const string& str)
{
    if (str.empty ()) return wstring ();
    int size = MultiByteToWideChar (CP_UTF8, 0, str.c_str (), (int)str.length (), NULL, 0);
    wstring result (size, 0);
    MultiByteToWideChar (CP_UTF8, 0, str.c_str (), (int)str.length (), &result[0], size);
    return result;
}

// 宽字符串转窄字符串（用于文件路径）
string wstring_to_string (const wstring& wstr)
{
    if (wstr.empty ()) return string ();
    int size = WideCharToMultiByte (CP_ACP, 0, wstr.c_str (), (int)wstr.length (), NULL, 0, NULL, NULL);
    string result (size, 0);
    WideCharToMultiByte (CP_ACP, 0, wstr.c_str (), (int)wstr.length (), &result[0], size, NULL, NULL);
    return result;
}
