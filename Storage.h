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

// 清理规则类型枚举
enum CleanupRuleType
{
    RULE_BY_TIME = 1,      // 按时间清理
    RULE_BY_COUNT = 2,     // 按数量清理
    RULE_BY_SIZE = 3       // 按大小清理
};

// 清理规则结构
struct CleanupRule
{
    int id;                    // 规则ID
    wstring name;              // 规则名称
    bool enabled;              // 是否启用
    int priority;              // 优先级（数字越小优先级越高）
    CleanupRuleType type;      // 规则类型
    int days;                  // 保留天数（按时间清理）
    int maxRecords;            // 最大记录数（按数量清理）
    int maxSizeMB;             // 最大大小MB（按大小清理）
    bool deleteImages;         // 是否同时删除图片
};

// 清理规则管理器类
class CleanupRuleManager
{
public:
    CleanupRuleManager ();
    ~CleanupRuleManager ();

    // 设置存储目录
    void SetRootDir (const wstring& rootDir);

    // 加载规则
    bool LoadRules ();

    // 保存规则
    bool SaveRules ();

    // 添加规则
    bool AddRule (CleanupRule rule);

    // 删除规则
    bool DeleteRule (int ruleId);

    // 更新规则
    bool UpdateRule (CleanupRule rule);

    // 上移规则
    bool MoveRuleUp (int ruleId);

    // 下移规则
    bool MoveRuleDown (int ruleId);

    // 获取所有规则
    vector<CleanupRule> GetRules ();

    // 预览清理效果
    vector<ClipRecord> PreviewCleanup (const vector<ClipRecord>& records);

    // 执行清理
    int ExecuteCleanup (vector<ClipRecord>& records);

    // 导出规则到JSON文件
    bool ExportRules (const wstring& filePath);

    // 从JSON文件导入规则
    bool ImportRules (const wstring& filePath);

private:
    // 获取规则配置文件路径
    wstring GetRulesPath ();

    // 生成下一个规则ID
    int GenerateRuleId ();

    vector<CleanupRule> m_rules;    // 清理规则列表
    wstring m_rootDir;              // 程序根目录
    int m_nextId;                   // 下一个规则ID
};

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

    // 保存关闭时最小化到托盘设置
    bool SaveMinimizeToTraySetting (bool enabled);

    // 加载关闭时最小化到托盘设置
    bool LoadMinimizeToTraySetting (bool& enabled);

    // 设置开机自启（写入/删除注册表）
    static bool SetAutoStart (bool enabled);

    // 检查是否已设置开机自启
    static bool IsAutoStartEnabled ();

    // 获取清理规则管理器
    CleanupRuleManager& GetCleanupRuleManager ();

private:
    // 确保存储目录存在
    void EnsureDirectories ();

    // 获取索引文件路径（绝对路径）
    string GetIndexPath ();

    // 获取设置文件路径（绝对路径）
    string GetSettingsPath ();

    wstring m_rootDir;         // 程序根目录
    bool m_initialized;        // 是否已初始化
    CleanupRuleManager m_cleanupRuleManager;  // 清理规则管理器
};

// 宽字符串转UTF-8
string wstring_to_utf8 (const wstring& wstr);

// UTF-8转宽字符串
wstring utf8_to_wstring (const string& str);

// 宽字符串转窄字符串（用于文件操作）
string wstring_to_string (const wstring& wstr);
