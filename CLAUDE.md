# 历史剪贴板管理器 - 项目指引

## 项目概述
一个运行在Windows平台上的历史剪贴板软件，自动记录用户的复制内容，支持查看、搜索、管理历史记录。

## 技术栈
- C++17标准
- MinGW-w64 (GCC) 编译器
- Win32原生API（不引入Qt等重型库）
- nlohmann/json（JSON解析库）

## 项目结构
```
Project1/
├── Main.cpp                 # 入口点，窗口创建和消息循环
├── ClipboardManager.h/cpp   # 剪贴板监听和内容捕获
├── Storage.h/cpp            # 文件存储和管理
├── UIManager.h/cpp          # 界面绘制和交互
├── Utils.h/cpp              # 工具函数
├── clips/                   # 存储目录（自动创建）
│   ├── history.json         # 历史记录索引
│   └── images/              # 图片存储
├── docs/                    # 项目文档
│   ├── requirements.md      # 需求文档
│   ├── technical-design.md  # 技术设计规范
│   └── execution-steps.md   # 执行步骤
├── devlogs/                 # 开发日志
│   └── YYYY-MM-DD-*.md      # 按日期记录的开发日志
└── output/                  # 编译输出
    └── ClipboardHistory.exe # 可执行文件
```

## 编译命令
```bash
g++ -std=c++17 -o output/ClipboardHistory.exe *.cpp -luser32 -lkernel32 -lgdi32
```

## 开发规范

### 代码风格
- 缩进：4个空格
- 大括号：Allman风格（左大括号单独换行）
- 注释：单行注释，简短说明功能
- 使用 `using namespace std;`

### 命名规范
- 类名：PascalCase（如 `ClipboardManager`）
- 函数名：PascalCase（如 `OnClipboardUpdate`）
- 变量名：PascalCase（如 `ClipRecord`）
- 常量：UPPER_SNAKE_CASE（如 `MAX_RECORDS`）
- 全局变量：G_前缀（如 `G_hWnd`）

### 编码要求
- 必须添加 `#define UNICODE` 和 `#define _UNICODE`
- 支持中文字符集
- main函数末尾必须写 `return 0;`

## 开发流程

### 阶段划分
1. **基础框架**：窗口创建和编译配置 ✅
2. **剪贴板监听**：文字和图片捕获
3. **存储系统**：JSON读写和文件管理
4. **界面交互**：列表、搜索、置顶、删除
5. **测试优化**：功能测试和打包

### 工作要求
- 每完成一个阶段，更新devlogs中的开发日志
- 每完成一个阶段，提交到Git仓库
- 按照docs/execution-steps.md中的步骤执行
- 遇到问题及时记录到开发日志

## 依赖库

### 必需
- Windows API（user32, kernel32, gdi32, shell32）
- nlohmann/json（单头文件，需下载到项目目录）

### 下载nlohmann/json
```bash
# 下载单头文件到项目根目录
curl -L https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -o json.hpp
```

## 功能需求
- 自动监听剪贴板变化
- 支持文字内容（最大10000字符）
- 支持图片内容（PNG格式保存）
- 历史记录最多1000条
- 存储期限：1天、3天、5天、7天、30天、永久
- 实时清理过期内容
- 时间降序排列
- 支持搜索、置顶、删除、再次粘贴

## 性能要求
- 启动时间：< 1秒
- 复制响应时间：< 100ms
- 搜索响应时间：< 200ms
- 内存占用：< 50MB

## 重要文件路径
- 需求文档：docs/requirements.md
- 技术设计：docs/technical-design.md
- 执行步骤：docs/execution-steps.md
- 开发日志：devlogs/YYYY-MM-DD-*.md
- 编译配置：.vscode/c_cpp_properties.json
