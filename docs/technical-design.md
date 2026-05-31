# 历史剪贴板管理器 - 技术设计规范

## 架构设计

### 模块划分
```
┌─────────────────────────────────────┐
│           Main.cpp                  │
│         (入口和消息循环)             │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│        ClipboardManager            │
│      (剪贴板监听和捕获)             │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│           Storage                  │
│       (文件存储和管理)              │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│         UIManager                  │
│        (界面绘制和交互)             │
└─────────────────────────────────────┘
```

## 数据结构

### 历史记录结构
```cpp
struct ClipRecord {
    int id;                    // 唯一标识
    int type;                  // 0=文字, 1=图片
    wstring content;           // 文字内容（或图片路径）
    wstring preview;           // 预览文本（前100字符）
    wstring filePath;          // 文件路径（图片）
    time_t timestamp;          // 创建时间
    bool isPinned;             // 是否置顶
};
```

### JSON存储格式
```json
{
  "version": "1.0",
  "settings": {
    "retentionDays": 3,
    "maxRecords": 1000
  },
  "records": [
    {
      "id": 1,
      "type": 0,
      "preview": "这是一段文字...",
      "filePath": "",
      "timestamp": 1706640000,
      "isPinned": false
    }
  ]
}
```

## 文件结构

```
Project1/
├── Main.cpp                 # 入口点，窗口创建
├── ClipboardManager.h       # 剪贴板管理头文件
├── ClipboardManager.cpp     # 剪贴板管理实现
├── Storage.h                # 存储管理头文件
├── Storage.cpp              # 存储管理实现
├── UIManager.h              # 界面管理头文件
├── UIManager.cpp            # 界面管理实现
├── Utils.h                  # 工具函数
├── Utils.cpp                # 工具函数实现
├── clips/                   # 存储目录
│   ├── history.json         # 历史记录索引
│   └── images/              # 图片存储
├── docs/                    # 项目文档
├── devlogs/                 # 开发日志
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
- 文字：`CF_UNICODETEXT` 格式获取
- 图片：`CF_DIB` 格式获取，转换为PNG保存

### 3. 界面绘制
- 使用GDI绘制控件
- 自定义按钮和列表样式
- 支持鼠标悬停和点击效果

### 4. 文件操作
- JSON读写使用nlohmann/json库
- 图片保存使用GDI+
- 文件路径使用相对路径确保便携性

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
- Windows API（user32, kernel32, gdi32, shell32）
- nlohmann/json（单头文件，JSON解析）

### 可选
- GDI+（图片处理，Windows自带）

## 编译配置

### 编译器选项
```bash
g++ -std=c++17 -o ClipboardHistory.exe *.cpp -luser32 -lkernel32 -lgdi32 -lshell32
```

### 链接库
- user32.dll：窗口和消息处理
- kernel32.dll：进程和文件操作
- gdi32.dll：图形设备接口
- shell32.dll：Shell API
