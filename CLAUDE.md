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

### 后端（Rust）
- rusqlite（SQLite 数据库，bundled 模式）
- serde / serde_json（序列化）
- chrono（时间处理）
- tauri-plugin-shell（系统路径）

## 项目结构
```
ClipboardHistoryManager/
├── frontend/                # Tauri + React 应用
│   ├── src/                 # React 源码
│   │   ├── components/      # 通用组件
│   │   ├── pages/           # 页面组件
│   │   ├── styles/          # 样式文件
│   │   ├── types/           # TypeScript 类型
│   │   ├── utils/           # 工具函数
│   │   ├── App.tsx          # 主应用组件
│   │   └── main.tsx         # 入口文件
│   ├── src-tauri/           # Tauri Rust 后端
│   │   ├── src/
│   │   │   ├── commands.rs  # 所有业务逻辑（CRUD、设置）
│   │   │   ├── lib.rs       # Tauri 应用注册
│   │   │   └── main.rs      # 入口
│   │   ├── Cargo.toml       # Rust 依赖
│   │   └── tauri.conf.json  # Tauri 配置
│   ├── dist/                # 前端构建产物
│   └── package.json         # Node.js 依赖
├── clips/                   # 运行时数据（自动创建，gitignore）
│   ├── history.db           # SQLite 数据库
│   └── images/              # 图片存储
├── docs/                    # 项目文档
├── versions/                # 版本历史
├── devlogs/                 # 开发日志
├── Photo/                   # 图片资源
├── .claude/                 # Claude 配置
├── CLAUDE.md                # 本文件
└── README.md                # 项目说明
```

## 开发命令

### 前端开发
```bash
cd frontend
npm install          # 安装依赖
npm run dev          # 启动开发服务器（Vite HMR）
npm run build        # 构建前端
```

### Tauri 开发
```bash
cd frontend
npm run tauri dev    # 启动 Tauri 开发模式（前端 + Rust 后端热重载）
npm run tauri build  # 打包 Tauri 应用（产出 .exe 到 src-tauri/target/release/bundle/）
```

## 架构说明

前端 React 通过 Tauri 的 `invoke()` IPC 机制调用 Rust 后端命令：

```
React 组件 → invoke("command") → Rust commands.rs → rusqlite → SQLite
```

### 后端命令（commands.rs）
| 命令 | 功能 |
|------|------|
| `get_records` | 获取所有记录（时间倒序） |
| `get_record_content` | 获取记录内容（用于复制） |
| `delete_record` | 删除单条记录 |
| `toggle_pin` | 切换置顶状态 |
| `batch_delete_records` | 批量删除记录 |
| `get_settings` | 获取设置 |
| `save_settings` | 保存设置 |
| `clear_all_records` | 清空所有记录 |

## 开发规范

### 代码风格
- React 函数组件 + TypeScript
- Ant Design 组件库
- 4空格缩进
- 单行注释，简短说明功能

### 命名规范
- 组件：PascalCase（如 `HistoryPage`、`MainLayout`）
- 函数：camelCase（如 `handleCopy`、`loadRecords`）
- 类型：PascalCase（如 `ClipRecord`、`Settings`）

### Git 工作流
- 每个版本创建独立分支 `vX.Y.Z.W`
- 测试通过后合并到 main
- 打版本标签

## 系统要求
- Windows 7/8/10/11
- Node.js 18+ (LTS)
- Rust (rustup)
