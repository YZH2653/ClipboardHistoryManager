# 前端技术设计文档

## 架构设计

```
┌─────────────────────────────────────────┐
│              Tauri 桌面应用              │
│  ┌───────────────────────────────────┐  │
│  │    React + TypeScript + Ant Design │  │
│  │           (前端 UI)                │  │
│  └─────────────────┬─────────────────┘  │
│                    │ Tauri invoke        │
│  ┌─────────────────▼─────────────────┐  │
│  │           Rust 后端                │  │
│  │    (桥接前端和 C++ 后端)           │  │
│  └─────────────────┬─────────────────┘  │
│                    │ FFI / 命令行        │
│  ┌─────────────────▼─────────────────┐  │
│  │         C++ 后端 (已有代码)        │  │
│  │  - 剪贴板监听                      │  │
│  │  - 数据存储 (SQLite)               │  │
│  │  - 快捷键注册                      │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

## 目录结构

```
frontend/
├── src/
│   ├── components/      # 通用组件
│   │   └── MainLayout.tsx
│   ├── pages/           # 页面组件
│   │   ├── HistoryPage.tsx
│   │   ├── SettingsPage.tsx
│   │   └── AboutPage.tsx
│   ├── styles/          # 样式文件
│   │   └── global.css
│   ├── types/           # TypeScript 类型
│   │   └── index.ts
│   ├── utils/           # 工具函数
│   │   └── api.ts
│   ├── App.tsx          # 主应用组件
│   └── main.tsx         # 入口文件
├── src-tauri/           # Tauri Rust 代码
│   ├── src/
│   │   ├── lib.rs
│   │   └── main.rs
│   ├── Cargo.toml
│   ├── tauri.conf.json
│   └── build.rs
├── docs/                # 文档
├── index.html           # HTML 入口
├── package.json         # 依赖配置
├── tsconfig.json        # TypeScript 配置
└── vite.config.ts       # Vite 配置
```

## 组件设计

### MainLayout

主布局组件，包含侧边栏和内容区。

### HistoryPage

历史记录页面，显示剪贴板记录列表。

### SettingsPage

设置页面，配置应用参数。

### AboutPage

关于页面，显示版本和作者信息。

## 数据流

```
用户操作 → React 组件 → API 调用 → Tauri invoke → Rust → C++ → 返回数据 → 更新 UI
```
