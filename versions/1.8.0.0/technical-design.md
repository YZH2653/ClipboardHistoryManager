# 版本 1.8.0.0 技术设计

## 设计概述

通过 GDI 对象缓存、双缓冲绘制、记录缓存和数据结构优化，提升软件响应速度和流畅性。

## 架构设计

### 模块划分

```
┌─────────────────────────────────────────┐
│              Main.cpp                    │
│  ┌──────────┐  ┌──────────┐  ┌────────┐ │
│  │ GDICache │  │DoubleBuf │  │RecCache│ │
│  │ GDI缓存  │  │ 双缓冲   │  │记录缓存│ │
│  └──────────┘  └──────────┘  └────────┘ │
│         │            │           │       │
│         ▼            ▼           ▼       │
│  ┌──────────────────────────────────────┐│
│  │         绘制函数 (DrawXxx)           ││
│  └──────────────────────────────────────┘│
│         │                                │
│         ▼                                │
│  ┌──────────────────────────────────────┐│
│  │      ClipboardManager / Storage      ││
│  └──────────────────────────────────────┘│
└─────────────────────────────────────────┘
```

### 数据流

1. **绘制流程**：WM_PAINT → DoubleBuffer.BeginDraw → 绘制到内存DC → DoubleBuffer.EndDraw → 拷贝到屏幕
2. **记录流程**：剪贴板变化 → 更新记录 → 标记缓存失效 → 触发重绘
3. **搜索流程**：输入搜索文本 → 检查缓存 → 缓存失效则重新筛选 → 返回结果

## 详细设计

### 1. GDI 对象缓存

**实现位置**：Main.cpp 顶部

**初始化时机**：程序启动时（WinMain 或 main 函数中）

**释放时机**：程序退出时（WM_DESTROY 中）

**缓存对象**：
- 5 种常用字体（标题、段落、内容、按钮、小字）
- 3 种常用画刷（白色、浅灰、悬停）
- 3 种常用画笔（边框、分割线、高亮）

**使用方式**：
```cpp
// 之前
HFONT font = CreateFont(24, ...);
SelectObject(hdc, font);
// ... 绘制 ...
DeleteObject(font);

// 之后
SelectObject(hdc, G_GDICache.fontContent);
// ... 绘制 ...
// 不需要 DeleteObject
```

### 2. 双缓冲绘制

**实现位置**：Main.cpp 中新增 DoubleBuffer 类

**使用时机**：WM_PAINT 消息处理

**实现步骤**：
1. 创建与窗口客户区同大小的内存 DC 和位图
2. 在内存 DC 上执行所有绘制操作
3. 使用 BitBlt 将内存 DC 内容拷贝到屏幕 DC
4. 释放内存 DC 和位图

**注意事项**：
- 内存 DC 和位图在每次 WM_PAINT 时创建/释放
- 或者缓存起来，仅在窗口大小改变时重建

### 3. 记录缓存

**实现位置**：Main.cpp 中的 GetFilteredRecords 函数

**缓存策略**：
- 缓存上一次的搜索文本和筛选结果
- 搜索文本未变时直接返回缓存结果
- 记录增删/置顶操作时标记缓存失效

**失效条件**：
- 搜索文本变化
- 记录添加/删除
- 记录置顶状态变化
- 窗口获得焦点时（可能有外部剪贴板变化）

### 4. 数据结构优化

**当前问题**：
```cpp
// ClipboardManager::AddRecord
m_records.insert(m_records.begin(), record);  // O(n)
```

**优化方案**：
```cpp
// 方案1：使用 deque
deque<ClipRecord> m_records;
m_records.push_front(record);  // O(1)

// 方案2：保持 vector，尾部插入
m_records.push_back(record);  // O(1)
// 显示时使用反向迭代器或 rbegin/rend
```

**推荐方案2**：保持 vector 兼容性，尾部插入，显示时反向遍历。

### 5. 搜索优化

**当前问题**：
```cpp
// 同时搜索 preview 和 content
bool textMatch = (record.preview.find(G_SearchText) != wstring::npos ||
                  record.content.find(G_SearchText) != wstring::npos);
```

**优化方案**：
- preview 是 content 的子串，只需搜索 content
- 使用缓存避免重复搜索

## 技术风险

| 风险 | 影响 | 应对方案 |
|------|------|----------|
| GDI 对象泄漏 | 程序运行时间长后内存增长 | 严格的初始化/释放配对 |
| 双缓冲内存占用 | 高分辨率下位图较大 | 仅在需要时创建，窗口大小变化时重建 |
| 缓存一致性 | 显示过期数据 | 严格管理失效条件 |

## 依赖关系

- 无新增外部依赖
- 保持现有 SQLite3 和 nlohmann/json 依赖
- 保持 Win32 API 兼容性
