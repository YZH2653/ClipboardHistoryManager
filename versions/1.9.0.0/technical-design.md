# 版本 1.9.0.0 技术设计

## 设计概述

新增开发事项与待办事项管理功能，集成到现有剪贴板管理器中。

## 架构设计

### 模块划分

```
┌─────────────────────────────────────────┐
│              Main.cpp                    │
│  ┌──────────────────────────────────┐   │
│  │         TodoManager              │   │
│  │  ┌────────┐  ┌────────┐         │   │
│  │  │ Load   │  │ Save   │         │   │
│  │  └────────┘  └────────┘         │   │
│  │  ┌────────┐  ┌────────┐         │   │
│  │  │ Add    │  │ Delete │         │   │
│  │  └────────┘  └────────┘         │   │
│  └──────────────────────────────────┘   │
│         │                               │
│         ▼                               │
│  ┌──────────────────────────────────────┐│
│  │         Storage (SQLite)             ││
│  └──────────────────────────────────────┘│
└─────────────────────────────────────────┘
```

### 数据流

1. **添加流程**：用户输入 → 创建 TodoItem → 保存到数据库 → 刷新界面
2. **查询流程**：界面请求 → 从数据库加载 → 筛选/排序 → 显示
3. **更新流程**：用户编辑 → 更新 TodoItem → 保存到数据库 → 刷新界面

## 详细设计

### 1. 数据库设计

**新增表**：

```sql
-- 待办事项表
CREATE TABLE IF NOT EXISTS todos (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    description TEXT,
    category TEXT,
    tags TEXT,           -- JSON数组格式
    priority INTEGER DEFAULT 2,  -- 1=高，2=中，3=低
    status INTEGER DEFAULT 0,    -- 0=待办，1=进行中，2=已完成，3=已取消
    create_time INTEGER,
    update_time INTEGER,
    due_date INTEGER,
    is_completed INTEGER DEFAULT 0
);
```

### 2. 接口设计

```cpp
// TodoManager 类接口
class TodoManager {
public:
    // 初始化
    bool Initialize(sqlite3* db);

    // CRUD 操作
    bool AddItem(TodoItem& item);
    bool UpdateItem(const TodoItem& item);
    bool DeleteItem(int itemId);
    TodoItem GetItem(int itemId);

    // 查询
    vector<TodoItem> GetAllItems();
    vector<TodoItem> GetItemsByStatus(int status);
    vector<TodoItem> GetItemsByCategory(const wstring& category);
    vector<TodoItem> GetItemsByPriority(int priority);

    // 状态管理
    bool ToggleComplete(int itemId);
    bool SetStatus(int itemId, int status);

    // 统计
    int GetTotalCount();
    int GetCompletedCount();
    int GetPendingCount();
};
```

### 3. 界面设计

**主界面**：
- 在现有标签栏添加"待办"标签
- 待办事项列表（卡片式）
- 添加按钮（右下角）

**待办卡片**：
- 标题
- 截止日期
- 优先级标记（颜色）
- 状态标记
- 完成复选框

**编辑对话框**：
- 标题输入框
- 描述输入框
- 分类下拉框
- 标签输入
- 优先级选择
- 截止日期选择
- 保存/取消按钮

## 技术风险

| 风险 | 影响 | 应对方案 |
|------|------|----------|
| 数据库迁移 | 现有数据兼容 | 使用 CREATE TABLE IF NOT EXISTS |
| 界面复杂度 | 开发周期 | 分阶段实现，先核心后完善 |
| 性能问题 | 大量数据 | 添加索引，分页加载 |

## 依赖关系

- 依赖现有 SQLite 数据库
- 依赖现有 Storage 类
- 依赖现有界面框架
