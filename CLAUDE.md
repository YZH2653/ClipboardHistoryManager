# 历史剪贴板管理器 - 项目指引

## 项目概述
一个运行在Windows平台上的历史剪贴板软件，自动记录用户的复制内容，支持查看、搜索、管理历史记录。

## 技术栈

### 前端（Tauri + React）
- Tauri 2.x（桌面应用框架）
- React 18.x（UI 框架）
- TypeScript 5.x（类型安全）
- Ant Design 5.x（UI 组件库）
- Vite 5.x（构建工具）

### 后端（C++ FFI）
- C++17 标准
- MinGW-w64 (GCC) 编译器
- Win32 API（剪贴板监听 `AddClipboardFormatListener`）
- SQLite3（数据存储）
- nlohmann/json（JSON 解析）
- GDI+（图片捕获）

### 桥接层（Rust FFI）
- Rust 通过 `extern "C"` 调用 C++ 后端
- `cc` crate 自动编译 C++ 源文件
- `CRITICAL_SECTION` 保证线程安全

## 项目结构
```
ClipboardHistoryManager/
├── frontend/                # Tauri + React 应用
│   ├── src/                 # React 源码
│   │   ├── components/      # 通用组件（MainLayout）
│   │   ├── pages/           # 页面（HistoryPage、SettingsPage、AboutPage）
│   │   ├── styles/          # 全局样式
│   │   ├── types/           # TypeScript 类型
│   │   └── utils/           # API 封装
│   ├── src-tauri/           # Rust 桥接层
│   │   ├── src/
│   │   │   ├── commands.rs  # Tauri 命令（调用 FFI）
│   │   │   ├── ffi.rs       # FFI 绑定（调用 C++）
│   │   │   ├── lib.rs       # 应用注册 + 托盘
│   │   │   └── main.rs      # 入口
│   │   ├── build.rs         # 自动编译 C++（cc crate）
│   │   └── tauri.conf.json  # Tauri 配置
│   └── package.json         # Node.js 依赖
├── ffi_bridge.cpp           # FFI 桥接层（线程安全）
├── ClipboardManager.h/cpp   # 剪贴板监听和内容捕获
├── Storage.h/cpp            # SQLite 存储管理
├── sqlite3.h/c              # SQLite 引擎
├── json.hpp                 # nlohmann JSON
├── clips/                   # 运行时数据（gitignore）
│   ├── history.db           # SQLite 数据库
│   └── images/              # 图片存储
├── docs/                    # 项目文档
├── versions/                # 版本历史
└── devlogs/                 # 开发日志
```

## 架构说明

```
React 前端 → Tauri invoke → Rust commands.rs → ffi.rs → C++ ffi_bridge.cpp → ClipboardManager/Storage
```

### 数据流
1. 剪贴板变化 → `WM_CLIPBOARDUPDATE` → C++ 监听线程捕获 → 保存到 SQLite
2. 前端每 2 秒轮询 → Rust FFI → 读取记录 → 更新 UI
3. 用户操作（删除/置顶）→ Rust FFI → C++ 修改 → 保存到 SQLite

### 线程安全
- C++ 使用 `CRITICAL_SECTION` 保护 `G_Records`
- 剪贴板监听线程和主线程互斥访问共享数据
- 监听回调先检查去重再捕获，防止复制按钮触发重复记录

## 开发命令

### 构建
```bash
cd frontend
npm run tauri build    # 打包应用（自动编译 C++ + Rust + React）
```

### 开发
```bash
cd frontend
npm run tauri dev      # 开发模式（热重载）
```

## 开发规范

### C++ 代码风格
- 缩进：4 个空格
- 大括号：Allman 风格（左大括号单独换行）
- 命名：PascalCase（函数、变量）
- 全局变量：G_ 前缀
- 注释：单行，≤30 字符

### TypeScript/React 代码风格
- 函数组件 + TypeScript
- Ant Design 组件库
- camelCase 命名

### Git 工作流
- 每个版本创建独立分支 `vX.Y.Z.W`
- 测试通过后合并到 main
- 打版本标签
