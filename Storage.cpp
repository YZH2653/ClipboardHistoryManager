#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include "Storage.h"
#include <iostream>
#include <fstream>
#include <windows.h>
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

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

// 全局变量：exe所在目录
static string g_exeDir = "";

// 构造函数
Storage::Storage ()
    : m_initialized (false)
{
}

// 析构函数
Storage::~Storage ()
{
}

// 设置程序根目录
void Storage::SetRootDir (const wstring& rootDir)
{
    m_rootDir = rootDir;
    // 同时设置全局exe目录（ANSI编码）
    g_exeDir = GetExeDirA ();
}

// 初始化存储系统
bool Storage::Initialize ()
{
    EnsureDirectories ();
    m_initialized = true;
    return true;
}

// 确保存储目录存在
void Storage::EnsureDirectories ()
{
    // 使用绝对路径创建clips目录
    string clipsDir = g_exeDir + "\\clips";
    CreateDirectoryA (clipsDir.c_str (), NULL);

    // 使用绝对路径创建images目录
    string imagesDir = g_exeDir + "\\clips\\images";
    CreateDirectoryA (imagesDir.c_str (), NULL);
}

// 获取索引文件路径（绝对路径）
string Storage::GetIndexPath ()
{
    return g_exeDir + "\\clips\\history.json";
}

// 获取设置文件路径（绝对路径）
string Storage::GetSettingsPath ()
{
    return g_exeDir + "\\clips\\settings.json";
}

// 保存记录到文件
bool Storage::SaveRecords (const vector<ClipRecord>& records)
{
    try
    {
        json j;
        j["version"] = "1.0";
        j["records"] = json::array ();

        for (const auto& record : records)
        {
            json item;
            item["id"] = record.id;
            item["type"] = (int)record.type;
            item["content"] = wstring_to_utf8 (record.content);
            item["preview"] = wstring_to_utf8 (record.preview);
            item["filePath"] = wstring_to_utf8 (record.filePath);
            item["timestamp"] = record.timestamp;
            item["isPinned"] = record.isPinned;
            j["records"].push_back (item);
        }

        // 转换为UTF-8字符串
        string jsonStr = j.dump (4);
        string indexPath = GetIndexPath ();

        // 写入文件（覆盖模式，清空旧数据）
        ofstream file (indexPath, ios::out | ios::trunc);
        if (!file.is_open ())
        {
            return false;
        }

        file << jsonStr;
        file.flush ();
        file.close ();

        return true;
    }
    catch (const exception& e)
    {
        return false;
    }
}

// 从文件加载记录
bool Storage::LoadRecords (vector<ClipRecord>& records)
{
    try
    {
        string indexPath = GetIndexPath ();

        // 检查文件是否存在，不存在则创建空文件
        ifstream checkFile (indexPath);
        if (!checkFile.is_open ())
        {
            // 创建空的JSON文件
            ofstream newFile (indexPath, ios::out | ios::trunc);
            if (newFile.is_open ())
            {
                newFile << "{\"version\":\"1.0\",\"records\":[]}";
                newFile.flush ();
                newFile.close ();
            }
            return true;
        }
        checkFile.close ();

        // 读取文件内容
        ifstream file (indexPath);
        if (!file.is_open ())
        {
            return true;
        }

        string jsonStr;
        getline (file, jsonStr);
        file.close ();

        if (jsonStr.empty ())
        {
            return true;
        }

        json j = json::parse (jsonStr);
        records.clear ();

        if (j.contains ("records"))
        {
            for (const auto& item : j["records"])
            {
                ClipRecord record;
                record.id = item["id"];
                record.type = (ClipType)(int)item["type"];
                record.content = utf8_to_wstring (item["content"].get<string> ());
                record.preview = utf8_to_wstring (item["preview"].get<string> ());
                record.filePath = utf8_to_wstring (item["filePath"].get<string> ());
                record.timestamp = item["timestamp"];
                record.isPinned = item["isPinned"];
                records.push_back (record);
            }
        }

        return true;
    }
    catch (const exception& e)
    {
        return false;
    }
}

// 删除过期记录
int Storage::DeleteExpiredRecords (vector<ClipRecord>& records, int retentionDays)
{
    if (retentionDays <= 0)
    {
        return 0;  // 永不过期
    }

    time_t now = time (NULL);
    time_t expireTime = now - (retentionDays * 24 * 60 * 60);
    int deletedCount = 0;

    auto it = records.begin ();
    while (it != records.end ())
    {
        if (!it->isPinned && it->timestamp < expireTime)
        {
            // 删除图片文件
            DeleteRecordFile (*it);

            // 删除记录
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
    try
    {
        json j;
        j["retentionDays"] = retentionDays;
        j["maxRecords"] = maxRecords;

        string jsonStr = j.dump (4);
        string settingsPath = GetSettingsPath ();

        ofstream file (settingsPath, ios::out | ios::trunc);
        if (!file.is_open ())
        {
            return false;
        }

        file << jsonStr;
        file.flush ();
        file.close ();

        return true;
    }
    catch (const exception& e)
    {
        return false;
    }
}

// 加载设置
bool Storage::LoadSettings (int& retentionDays, int& maxRecords)
{
    try
    {
        string settingsPath = GetSettingsPath ();
        ifstream file (settingsPath);
        if (!file.is_open ())
        {
            retentionDays = 3;
            maxRecords = 1000;
            return true;
        }

        string jsonStr;
        getline (file, jsonStr);
        file.close ();

        if (jsonStr.empty ())
        {
            retentionDays = 3;
            maxRecords = 1000;
            return true;
        }

        json j = json::parse (jsonStr);
        retentionDays = j.value ("retentionDays", 3);
        maxRecords = j.value ("maxRecords", 1000);

        return true;
    }
    catch (const exception& e)
    {
        retentionDays = 3;
        maxRecords = 1000;
        return false;
    }
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
