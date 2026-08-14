#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include "Storage.h"
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include "sqlite3.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

// 获取exe所在目录的绝对路径
static wstring GetExeDirW ()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW (NULL, path, MAX_PATH);
    wstring fullPath (path);
    size_t pos = fullPath.find_last_of (L"\\");
    if (pos != wstring::npos)
    {
        return fullPath.substr (0, pos);
    }
    return fullPath;
}

// 将宽字符串转换为 UTF-8 字符串
static string WstringToUtf8 (const wstring& wstr)
{
    if (wstr.empty ())
        return string ();

    int size_needed = WideCharToMultiByte (CP_UTF8, 0, &wstr[0], (int)wstr.size (), NULL, 0, NULL, NULL);
    string strTo (size_needed, 0);
    WideCharToMultiByte (CP_UTF8, 0, &wstr[0], (int)wstr.size (), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// 全局变量
static wstring G_WideExeDir = L"";  // 宽字符串形式
wstring G_ExeDir = L"";            // 宽字符串形式（外部可见）
static string G_ExeDirUtf8 = "";   // UTF-8字符串形式
static sqlite3* G_Db = NULL;

// 构造函数
Storage::Storage ()
    : m_initialized (false)
{
}

// 析构函数
Storage::~Storage ()
{
    if (G_Db)
    {
        sqlite3_close (G_Db);
        G_Db = NULL;
    }
}

// 设置程序根目录
void Storage::SetRootDir (const wstring& rootDir)
{
    m_rootDir = rootDir;
    G_WideExeDir = GetExeDirW ();
    G_ExeDir = G_WideExeDir;
    G_ExeDirUtf8 = WstringToUtf8 (G_WideExeDir);

    // 设置清理规则管理器的根目录
    m_cleanupRuleManager.SetRootDir (rootDir);
}

// 初始化存储系统
bool Storage::Initialize ()
{
    EnsureDirectories ();

    string dbPath = G_ExeDirUtf8 + "\\clips\\history.db";

    // 打开数据库（如果不存在则创建）
    int rc = sqlite3_open (dbPath.c_str (), &G_Db);
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
    rc = sqlite3_exec (G_Db, createTableSQL, NULL, NULL, &errMsg);
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

    rc = sqlite3_exec (G_Db, createSettingsSQL, NULL, NULL, &errMsg);
    if (rc)
    {
        if (errMsg) sqlite3_free (errMsg);
        return false;
    }

    // 加载清理规则
    m_cleanupRuleManager.LoadRules ();

    m_initialized = true;
    return true;
}

// 确保存储目录存在
void Storage::EnsureDirectories ()
{
    string clipsDir = G_ExeDirUtf8 + "\\clips";
    CreateDirectoryA (clipsDir.c_str (), NULL);
}

// 保存记录到数据库
bool Storage::SaveRecords (const vector<ClipRecord>& records)
{
    if (!G_Db) return false;

    // 开始事务
    sqlite3_exec (G_Db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    // 清空旧记录
    sqlite3_exec (G_Db, "DELETE FROM records;", NULL, NULL, NULL);

    // 插入新记录
    const char* insertSQL = "INSERT INTO records (id, type, content, preview, filePath, timestamp, isPinned) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (G_Db, insertSQL, -1, &stmt, NULL);
    if (rc)
    {
        sqlite3_exec (G_Db, "ROLLBACK;", NULL, NULL, NULL);
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
    sqlite3_exec (G_Db, "COMMIT;", NULL, NULL, NULL);

    return true;
}

// 从数据库加载记录
bool Storage::LoadRecords (vector<ClipRecord>& records)
{
    if (!G_Db) return false;

    const char* selectSQL = "SELECT id, type, content, preview, filePath, timestamp, isPinned FROM records ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (G_Db, selectSQL, -1, &stmt, NULL);
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
    if (!G_Db) return false;

    const char* upsertSQL = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (G_Db, upsertSQL, -1, &stmt, NULL);
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
    if (!G_Db)
    {
        retentionDays = 3;
        maxRecords = 1000;
        return true;
    }

    const char* selectSQL = "SELECT key, value FROM settings;";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (G_Db, selectSQL, -1, &stmt, NULL);
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
    if (!G_Db) return false;

    const char* upsertSQL = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (G_Db, upsertSQL, -1, &stmt, NULL);
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
    if (!G_Db)
    {
        enabled = false;
        return true;
    }

    const char* selectSQL = "SELECT value FROM settings WHERE key = 'autoStart';";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (G_Db, selectSQL, -1, &stmt, NULL);
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
    if (!G_Db) return false;

    const char* upsertSQL = "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (G_Db, upsertSQL, -1, &stmt, NULL);
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
    if (!G_Db)
    {
        enabled = true;
        return true;
    }

    const char* selectSQL = "SELECT value FROM settings WHERE key = 'minimizeToTray';";
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2 (G_Db, selectSQL, -1, &stmt, NULL);
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
        // 获取exe绝对路径并添加 --minimized 参数
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW (NULL, exePath, MAX_PATH);
        wstring cmdLine = wstring (exePath) + L" --minimized";

        // 写入注册表
        result = RegSetValueExW (hKey, L"ClipboardHistoryManager", 0, REG_SZ,
            (const BYTE*)cmdLine.c_str (), (DWORD)(cmdLine.length () + 1) * sizeof (wchar_t));
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

// CleanupRuleManager 实现

// 构造函数
CleanupRuleManager::CleanupRuleManager ()
    : m_nextId (1)
{
}

// 析构函数
CleanupRuleManager::~CleanupRuleManager ()
{
}

// 设置存储目录
void CleanupRuleManager::SetRootDir (const wstring& rootDir)
{
    m_rootDir = rootDir;
}

// 获取规则配置文件路径
wstring CleanupRuleManager::GetRulesPath ()
{
    return m_rootDir + L"\\clips\\cleanup_rules.json";
}

// 生成下一个规则ID
int CleanupRuleManager::GenerateRuleId ()
{
    return m_nextId++;
}

// 加载规则
bool CleanupRuleManager::LoadRules ()
{
    wstring path = GetRulesPath ();
    ifstream file (path.c_str ());
    if (!file.is_open ())
    {
        // 文件不存在，使用默认规则
        m_rules.clear ();
        m_nextId = 1;
        return true;
    }

    try
    {
        json j;
        file >> j;
        file.close ();

        m_rules.clear ();
        m_nextId = 1;

        if (j.contains ("rules") && j["rules"].is_array ())
        {
            for (const auto& ruleJson : j["rules"])
            {
                CleanupRule rule;
                rule.id = ruleJson.value ("id", 0);
                rule.name = utf8_to_wstring (ruleJson.value ("name", ""));
                rule.enabled = ruleJson.value ("enabled", true);
                rule.priority = ruleJson.value ("priority", 0);
                rule.type = (CleanupRuleType)ruleJson.value ("type", 1);
                rule.days = ruleJson.value ("days", 30);
                rule.maxRecords = ruleJson.value ("maxRecords", 1000);
                rule.maxSizeMB = ruleJson.value ("maxSizeMB", 100);
                rule.deleteImages = ruleJson.value ("deleteImages", true);

                m_rules.push_back (rule);

                if (rule.id >= m_nextId)
                {
                    m_nextId = rule.id + 1;
                }
            }
        }

        if (j.contains ("nextId"))
        {
            m_nextId = j["nextId"];
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}

// 保存规则
bool CleanupRuleManager::SaveRules ()
{
    wstring path = GetRulesPath ();

    // 确保目录存在
    wstring dir = m_rootDir + L"\\clips";
    CreateDirectoryW (dir.c_str (), NULL);

    json j;
    j["nextId"] = m_nextId;

    json rulesArray = json::array ();
    for (const auto& rule : m_rules)
    {
        json ruleJson;
        ruleJson["id"] = rule.id;
        ruleJson["name"] = wstring_to_utf8 (rule.name);
        ruleJson["enabled"] = rule.enabled;
        ruleJson["priority"] = rule.priority;
        ruleJson["type"] = (int)rule.type;
        ruleJson["days"] = rule.days;
        ruleJson["maxRecords"] = rule.maxRecords;
        ruleJson["maxSizeMB"] = rule.maxSizeMB;
        ruleJson["deleteImages"] = rule.deleteImages;
        rulesArray.push_back (ruleJson);
    }

    j["rules"] = rulesArray;

    ofstream file (path.c_str ());
    if (!file.is_open ())
    {
        return false;
    }

    file << j.dump (4);
    file.close ();

    return true;
}

// 添加规则
bool CleanupRuleManager::AddRule (CleanupRule rule)
{
    rule.id = GenerateRuleId ();
    rule.priority = (int)m_rules.size ();
    m_rules.push_back (rule);
    return SaveRules ();
}

// 删除规则
bool CleanupRuleManager::DeleteRule (int ruleId)
{
    for (auto it = m_rules.begin (); it != m_rules.end (); ++it)
    {
        if (it->id == ruleId)
        {
            m_rules.erase (it);
            // 重新调整优先级
            for (int i = 0; i < (int)m_rules.size (); i++)
            {
                m_rules[i].priority = i;
            }
            return SaveRules ();
        }
    }
    return false;
}

// 更新规则
bool CleanupRuleManager::UpdateRule (CleanupRule rule)
{
    for (auto& existingRule : m_rules)
    {
        if (existingRule.id == rule.id)
        {
            existingRule.name = rule.name;
            existingRule.enabled = rule.enabled;
            existingRule.type = rule.type;
            existingRule.days = rule.days;
            existingRule.maxRecords = rule.maxRecords;
            existingRule.maxSizeMB = rule.maxSizeMB;
            existingRule.deleteImages = rule.deleteImages;
            return SaveRules ();
        }
    }
    return false;
}

// 上移规则
bool CleanupRuleManager::MoveRuleUp (int ruleId)
{
    for (int i = 0; i < (int)m_rules.size (); i++)
    {
        if (m_rules[i].id == ruleId && i > 0)
        {
            swap (m_rules[i], m_rules[i - 1]);
            m_rules[i].priority = i;
            m_rules[i - 1].priority = i - 1;
            return SaveRules ();
        }
    }
    return false;
}

// 下移规则
bool CleanupRuleManager::MoveRuleDown (int ruleId)
{
    for (int i = 0; i < (int)m_rules.size (); i++)
    {
        if (m_rules[i].id == ruleId && i < (int)m_rules.size () - 1)
        {
            swap (m_rules[i], m_rules[i + 1]);
            m_rules[i].priority = i;
            m_rules[i + 1].priority = i + 1;
            return SaveRules ();
        }
    }
    return false;
}

// 获取所有规则
vector<CleanupRule> CleanupRuleManager::GetRules ()
{
    return m_rules;
}

// 预览清理效果
vector<ClipRecord> CleanupRuleManager::PreviewCleanup (const vector<ClipRecord>& records)
{
    vector<ClipRecord> toDelete;
    vector<ClipRecord> remaining = records;

    // 按优先级排序规则
    vector<CleanupRule> sortedRules = m_rules;
    sort (sortedRules.begin (), sortedRules.end (), [] (const CleanupRule& a, const CleanupRule& b)
    {
        return a.priority < b.priority;
    });

    // 应用每个规则
    for (const auto& rule : sortedRules)
    {
        if (!rule.enabled)
        {
            continue;
        }

        vector<ClipRecord> ruleRemaining;

        switch (rule.type)
        {
        case RULE_BY_TIME:
        {
            time_t now = time (NULL);
            time_t expireTime = now - (rule.days * 24 * 60 * 60);
            for (const auto& record : remaining)
            {
                if (!record.isPinned && record.timestamp < expireTime)
                {
                    toDelete.push_back (record);
                }
                else
                {
                    ruleRemaining.push_back (record);
                }
            }
            remaining = ruleRemaining;
            break;
        }
        case RULE_BY_COUNT:
        {
            if ((int)remaining.size () > rule.maxRecords)
            {
                // 按时间排序，保留最新的
                sort (remaining.begin (), remaining.end (), [] (const ClipRecord& a, const ClipRecord& b)
                {
                    return a.timestamp > b.timestamp;
                });

                // 删除超出数量的记录
                for (int i = rule.maxRecords; i < (int)remaining.size (); i++)
                {
                    if (!remaining[i].isPinned)
                    {
                        toDelete.push_back (remaining[i]);
                    }
                }

                // 保留前maxRecords条
                if ((int)remaining.size () > rule.maxRecords)
                {
                    remaining.resize (rule.maxRecords);
                }
            }
            break;
        }
        case RULE_BY_SIZE:
        {
            // 计算当前总大小
            long long totalSize = 0;
            for (const auto& record : remaining)
            {
                if (record.type == CLIP_IMAGE && !record.filePath.empty ())
                {
                    // 获取文件大小
                    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
                    if (GetFileAttributesExW (record.filePath.c_str (), GetFileExInfoStandard, &fileInfo))
                    {
                        totalSize += ((long long)fileInfo.nFileSizeHigh << 32) + fileInfo.nFileSizeLow;
                    }
                }
                else
                {
                    // 文字内容大小（估算）
                    totalSize += record.content.length () * sizeof (wchar_t);
                }
            }

            long long maxSizeBytes = (long long)rule.maxSizeMB * 1024 * 1024;
            if (totalSize > maxSizeBytes)
            {
                // 按时间排序，保留最新的
                sort (remaining.begin (), remaining.end (), [] (const ClipRecord& a, const ClipRecord& b)
                {
                    return a.timestamp > b.timestamp;
                });

                // 删除超出大小的记录
                long long currentSize = 0;
                vector<ClipRecord> newRemaining;
                for (const auto& record : remaining)
                {
                    long long recordSize = 0;
                    if (record.type == CLIP_IMAGE && !record.filePath.empty ())
                    {
                        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
                        if (GetFileAttributesExW (record.filePath.c_str (), GetFileExInfoStandard, &fileInfo))
                        {
                            recordSize = ((long long)fileInfo.nFileSizeHigh << 32) + fileInfo.nFileSizeLow;
                        }
                    }
                    else
                    {
                        recordSize = record.content.length () * sizeof (wchar_t);
                    }

                    if (currentSize + recordSize <= maxSizeBytes)
                    {
                        newRemaining.push_back (record);
                        currentSize += recordSize;
                    }
                    else if (!record.isPinned)
                    {
                        toDelete.push_back (record);
                    }
                }
                remaining = newRemaining;
            }
            break;
        }
        }
    }

    return toDelete;
}

// 执行清理
int CleanupRuleManager::ExecuteCleanup (vector<ClipRecord>& records)
{
    vector<ClipRecord> toDelete = PreviewCleanup (records);
    int deletedCount = 0;

    for (const auto& deleteRecord : toDelete)
    {
        for (auto it = records.begin (); it != records.end (); ++it)
        {
            if (it->id == deleteRecord.id)
            {
                // 删除图片文件
                if (it->type == CLIP_IMAGE && !it->filePath.empty ())
                {
                    DeleteFileW (it->filePath.c_str ());
                }
                records.erase (it);
                deletedCount++;
                break;
            }
        }
    }

    return deletedCount;
}

// Storage 类的清理规则管理器方法

// 获取清理规则管理器
CleanupRuleManager& Storage::GetCleanupRuleManager ()
{
    return m_cleanupRuleManager;
}

