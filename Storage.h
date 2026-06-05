#pragma once
#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <string>
#include <vector>
#include "ClipboardManager.h"
using namespace std;

// 存储管理器类
class Storage
{
public:
    Storage ();
    ~Storage ();

    // 初始化存储系统
    bool Initialize ();

    // 设置程序根目录
    void SetRootDir (const wstring& rootDir);

    // 保存记录到文件
    bool SaveRecords (const vector<ClipRecord>& records);

    // 从文件加载记录
    bool LoadRecords (vector<ClipRecord>& records);

    // 删除过期记录
    int DeleteExpiredRecords (vector<ClipRecord>& records, int retentionDays);

    // 删除记录对应的图片文件
    void DeleteRecordFile (const ClipRecord& record);

    // 保存设置
    bool SaveSettings (int retentionDays, int maxRecords);

    // 加载设置
    bool LoadSettings (int& retentionDays, int& maxRecords);

    // 保存开机自启设置
    bool SaveAutoStartSetting (bool enabled);

    // 加载开机自启设置
    bool LoadAutoStartSetting (bool& enabled);

    // 设置开机自启（写入/删除注册表）
    static bool SetAutoStart (bool enabled);

    // 检查是否已设置开机自启
    static bool IsAutoStartEnabled ();

private:
    // 确保存储目录存在
    void EnsureDirectories ();

    // 获取索引文件路径（绝对路径）
    string GetIndexPath ();

    // 获取设置文件路径（绝对路径）
    string GetSettingsPath ();

    wstring m_rootDir;         // 程序根目录
    bool m_initialized;        // 是否已初始化
};

// 宽字符串转UTF-8
string wstring_to_utf8 (const wstring& wstr);

// UTF-8转宽字符串
wstring utf8_to_wstring (const string& str);

// 宽字符串转窄字符串（用于文件操作）
string wstring_to_string (const wstring& wstr);
