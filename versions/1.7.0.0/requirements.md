# 版本 1.7.0.0 需求文档

## 功能描述

### 当前问题

1. **清理规则单一**
   - 当前只有固定的保存时间选项（3天、5天、7天、30天、永久）
   - 无法根据记录数量自动清理
   - 无法根据存储大小自动清理
   - 无法设置多个清理条件组合

2. **用户体验不足**
   - 用户无法预览清理效果
   - 无法自定义清理规则
   - 缺少规则管理功能

### 解决方案

新增自动清理规则功能，提供更灵活的自动清理设置。

## 功能需求

### 1. 自定义清理规则

**需求描述**：
支持按时间、数量、大小等多种条件自动清理历史记录。

**功能要求**：
- 支持按时间清理（X天前的记录）
- 支持按数量清理（保留最近X条记录）
- 支持按大小清理（保留最近XMB的记录）
- 支持混合条件（同时满足多个条件才清理）

### 2. 规则优先级

**需求描述**：
支持设置多个清理规则的优先级。

**功能要求**：
- 支持添加多个清理规则
- 支持调整规则优先级（上移、下移）
- 按优先级顺序执行规则
- 支持启用/禁用单个规则

### 3. 规则预览

**需求描述**：
在应用规则前预览将要清理的内容。

**功能要求**：
- 显示将要清理的记录数量
- 显示将要释放的存储空间
- 显示清理后的记录数量
- 支持确认后执行清理

### 4. 规则导入导出

**需求描述**：
支持清理规则的导入和导出。

**功能要求**：
- 支持导出规则为JSON文件
- 支持从JSON文件导入规则
- 支持规则备份和恢复

## 技术要求

### 1. 数据结构

```cpp
// 清理规则结构
struct CleanupRule {
    int id;                    // 规则ID
    wstring name;              // 规则名称
    bool enabled;              // 是否启用
    int priority;              // 优先级（数字越小优先级越高）
    int type;                  // 规则类型：1=按时间，2=按数量，3=按大小
    int days;                  // 保留天数（按时间清理）
    int maxRecords;            // 最大记录数（按数量清理）
    int maxSizeMB;             // 最大大小MB（按大小清理）
    bool deleteImages;         // 是否同时删除图片
};

// 清理规则管理器
class CleanupRuleManager {
private:
    vector<CleanupRule> rules;
    wstring configPath;
public:
    bool LoadRules();
    bool SaveRules();
    bool AddRule(CleanupRule rule);
    bool DeleteRule(int ruleId);
    bool UpdateRule(CleanupRule rule);
    bool MoveRuleUp(int ruleId);
    bool MoveRuleDown(int ruleId);
    vector<CleanupRule> GetRules();
    vector<ClipRecord> PreviewCleanup(const vector<ClipRecord>& records);
    int ExecuteCleanup(vector<ClipRecord>& records);
};
```

### 2. 存储要求

- 规则配置存储在 `clips/cleanup_rules.json`
- 支持规则的持久化存储
- 支持规则的版本兼容

### 3. 界面要求

- 在设置页面添加"清理规则"入口
- 清理规则管理页面
- 规则编辑对话框
- 规则预览界面

## 测试用例

### 1. 基本功能测试

1. 添加按时间清理规则
2. 添加按数量清理规则
3. 添加按大小清理规则
4. 测试规则优先级
5. 测试规则启用/禁用

### 2. 规则预览测试

1. 预览按时间清理效果
2. 预览按数量清理效果
3. 预览按大小清理效果
4. 预览混合条件清理效果

### 3. 导入导出测试

1. 导出规则到JSON文件
2. 从JSON文件导入规则
3. 导入后规则正确性验证

### 4. 边界条件测试

1. 空规则列表
2. 规则优先级调整
3. 大量规则性能测试
4. 规则冲突处理
