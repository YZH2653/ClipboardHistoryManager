#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include "Storage.h"
#include <iostream>
#include <fstream>
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

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
}

// 初始化存储系统
bool Storage::Initialize ()
{
    EnsureDirectories ();
    m_initialized = true;
    wcout << L"存储系统初始化完成" << endl;
    return true;
}

// 确保存储目录存在
void Storage::EnsureDirectories ()
{
    // 创建clips目录
    wstring clipsDir = m_rootDir + L"\\clips";
    CreateDirectoryW (clipsDir.c_str (), NULL);

    // 创建images目录
    wstring imagesDir = m_rootDir + L"\\clips\\images";
    CreateDirectoryW (imagesDir.c_str (), NULL);
}

// 获取索引文件路径（绝对路径）
string Storage::GetIndexPath ()
{
    wstring fullPath = m_rootDir + L"\\clips\\history.json";
    return wstring_to_string (fullPath);
}

// 获取设置文件路径（绝对路径）
string Storage::GetSettingsPath ()
{
    wstring fullPath = m_rootDir + L"\\clips\\settings.json";
    return wstring_to_string (fullPath);
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

        // 写入文件
        ofstream file (GetIndexPath ());
        if (!file.is_open ())
        {
            wcout << L"无法打开索引文件进行写入" << endl;
            return false;
        }

        file << jsonStr;
        file.close ();

        wcout << L"保存记录成功，共 " << records.size () << " 条" << endl;
        return true;
    }
    catch (const exception& e)
    {
        wcout << L"保存记录失败: " << e.what () << endl;
        return false;
    }
}

// 从文件加载记录
bool Storage::LoadRecords (vector<ClipRecord>& records)
{
    try
    {
        ifstream file (GetIndexPath ());
        if (!file.is_open ())
        {
            wcout << L"索引文件不存在，将创建新文件" << endl;
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

        wcout << L"加载记录成功，共 " << records.size () << " 条" << endl;
        return true;
    }
    catch (const exception& e)
    {
        wcout << L"加载记录失败: " << e.what () << endl;
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

    if (deletedCount > 0)
    {
        wcout << L"删除过期记录: " << deletedCount << " 条" << endl;
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

        ofstream file (GetSettingsPath ());
        if (!file.is_open ())
        {
            return false;
        }

        file << jsonStr;
        file.close ();

        wcout << L"保存设置成功" << endl;
        return true;
    }
    catch (const exception& e)
    {
        wcout << L"保存设置失败: " << e.what () << endl;
        return false;
    }
}

// 加载设置
bool Storage::LoadSettings (int& retentionDays, int& maxRecords)
{
    try
    {
        ifstream file (GetSettingsPath ());
        if (!file.is_open ())
        {
            // 使用默认设置
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

        wcout << L"加载设置成功" << endl;
        return true;
    }
    catch (const exception& e)
    {
        wcout << L"加载设置失败: " << e.what () << endl;
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

// 宽字符串转窄字符串
string wstring_to_string (const wstring& wstr)
{
    if (wstr.empty ()) return string ();
    int size = WideCharToMultiByte (CP_ACP, 0, wstr.c_str (), (int)wstr.length (), NULL, 0, NULL, NULL);
    string result (size, 0);
    WideCharToMultiByte (CP_ACP, 0, wstr.c_str (), (int)wstr.length (), &result[0], size, NULL, NULL);
    return result;
}
