# 历史剪贴板管理器 - 技术设计规范

## 架构设计

### 模块划分
```
┌─────────────────────────────────────┐
│           Main.cpp                  │
│   (入口、消息循环、UI绘制)          │
└──────────────┬──────────────────────┘
               │
       ┌───────┴───────┐
       ▼               ▼
┌──────────────┐ ┌──────────────┐
│ClipboardManager│ │   Storage    │
│(剪贴板监听)  │ │(SQLite存储)  │
└──────────────┘ └──────────────┘
```

- **Main.cpp**：程序入口、窗口创建、消息循环、所有UI绘制逻辑
- **ClipboardManager**：剪贴板监听、内容捕获（文字/图片）、记录管理
- **Storage**：SQLite数据库操作、记录保存/加载、设置保存/加载、过期清理

## 数据结构

### 历史记录结构
```cpp
enum ClipType
{
    CLIP_TEXT = 0,   // 文字
    CLIP_IMAGE = 1   // 图片
};

struct ClipRecord
{
    int id;                  // 唯一标识
    ClipType type;           // 内容类型
    wstring content;         // 文字内容
    wstring preview;         // 预览文本（前100字符）
    wstring filePath;        // 文件路径（图片）
    time_t timestamp;        // 创建时间
    bool isPinned;           // 是否置顶
};
```

### SQLite 数据库结构
```sql
-- 历史记录表
CREATE TABLE records (
    id INTEGER PRIMARY KEY,
    type INTEGER,
    content TEXT,
    preview TEXT,
    filePath TEXT,
    timestamp INTEGER,
    isPinned INTEGER
);

-- 设置表
CREATE TABLE settings (
    key TEXT PRIMARY KEY,
    value INTEGER
);
```

## 文件结构

```
ClipboardHistoryManager/
├── Main.cpp                 # 入口点，窗口创建，UI绘制
├── ClipboardManager.h       # 剪贴板管理头文件
├── ClipboardManager.cpp     # 剪贴板管理实现
├── Storage.h                # 存储管理头文件
├── Storage.cpp              # 存储管理实现（含工具函数）
├── sqlite3.h                # SQLite 数据库引擎头文件
├── sqlite3.c                # SQLite 数据库引擎实现
├── clips/                   # 存储目录（运行时创建）
│   ├── history.db           # SQLite 数据库文件
│   └── images/              # 图片存储
├── docs/                    # 项目文档
├── devlogs/                 # 软件初始开发日志
├── versions/                # 版本开发目录
└── output/                  # 编译输出
```

## 关键技术点

### 1. 剪贴板监听
```cpp
// 注册剪贴板监听
AddClipboardFormatListener(hWnd);

// 处理剪贴板更新消息
case WM_CLIPBOARDUPDATE:
    OnClipboardUpdate();
    break;
```

### 2. 内容捕获
- 文字：`CF_UNICODETEXT` 格式获取，最大10000字符
- 图片：`CF_DIB` 格式获取，使用GDI+转换为PNG保存

### 3. 截图去重
- 问题：系统截图工具（Win+Shift+S）会多次触发剪贴板更新
- 方案：取DIB数据前1024字节计算快速哈希
- 规则：哈希相同 + 间隔<10秒 → 跳过保存

### 4. 界面绘制
- 使用GDI API直接绘制（CreateFont/CreateSolidBrush/TextOut等）
- 页面切换：PAGE_MAIN / PAGE_SETTINGS / PAGE_VERSION / PAGE_FEEDBACK
- 卡片列表：计算滚动偏移量，只绘制可见区域
- 下拉菜单：最后绘制，确保在最上层

### 5. 数据存储
- 使用 SQLite3 数据库替代 JSON 文件
- 通过 GetModuleFileNameA 获取 exe 绝对路径
- 事务处理：使用 BEGIN/COMMIT 确保数据一致性
- 字符编码：存储使用 UTF-8，内部使用 wstring

## 编码规范

### 命名规范
- 类名：PascalCase（如 `ClipboardManager`）
- 函数名：PascalCase（如 `OnClipboardUpdate`）
- 变量名：PascalCase（如 `ClipRecord`）
- 常量：UPPER_SNAKE_CASE（如 `MAX_RECORDS`）
- 全局变量：G_前缀（如 `G_hWnd`）

### 代码风格
- 缩进：4个空格
- 大括号：Allman风格（左大括号单独换行）
- 注释：单行注释，简短说明功能
- 使用 `using namespace std;`

## 依赖库

### 必需
- Windows API（user32, kernel32, gdi32, gdiplus）
- SQLite3（v3.45.0，单文件编译 sqlite3.c）

### 已弃用
- nlohmann/json（已改用SQLite，保留json.hpp兼容）

## 编译配置

### 编译器选项
```bash
g++ -std=c++17 -static -mwindows -o output/ClipboardHistory.exe Main.cpp ClipboardManager.cpp Storage.cpp sqlite3.c -lgdiplus -lgdi32 -luser32 -lole32
```

### 链接库
- user32.dll：窗口和消息处理
- kernel32.dll：进程和文件操作
- gdi32.dll：图形设备接口
- gdiplus.dll：图片处理（PNG编码）
- ole32.dll：COM对象（GDI+编码器查找）
