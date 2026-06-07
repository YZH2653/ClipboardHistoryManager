# 版本 1.4.0.0 执行步骤

## 阶段1：基础准备

### 步骤1.1：创建分支 ✅
- [x] 从 main 分支创建 `1.4.0.0` 分支

### 步骤1.2：创建版本文档 ✅
- [x] 创建 versions/1.4.0.0/ 目录结构
- [x] 编写 README.md、requirements.md、execution-steps.md、changelog.md
- [x] 创建 devlogs 目录和开发日志

---

## 阶段2：实现系统托盘功能

### 步骤2.1：添加托盘图标 ✅
- [x] 在 Main.cpp 中定义 `WM_TRAYICON` 自定义消息
- [x] 定义 `NOTIFYICONDATA` 全局变量
- [x] 编写 `AddTrayIcon()` 函数：创建托盘图标
- [x] 编写 `RemoveTrayIcon()` 函数：删除托盘图标
- [x] 在 `main()` 中调用 `AddTrayIcon()`
- [x] 在 `WM_DESTROY` 中调用 `RemoveTrayIcon()`
- [x] 提交代码

### 步骤2.2：实现最小化到托盘 ✅
- [x] 在 `WM_SIZE` 中检测 `SIZE_MINIMIZED`
- [x] 最小化时调用 `ShowWindow(hWnd, SW_HIDE)` 隐藏窗口
- [x] 测试最小化行为
- [x] 提交代码

### 步骤2.3：实现托盘右键菜单 ✅
- [x] 在 `WindowProc` 中处理 `WM_TRAYICON` 消息
- [x] 处理 `WM_RBUTTONDOWN`：创建并显示弹出菜单
- [x] 实现"显示窗口"菜单项：恢复窗口
- [x] 实现"退出"菜单项：发送 `WM_CLOSE`
- [x] 提交代码

### 步骤2.4：实现双击恢复 ✅
- [x] 处理 `WM_LBUTTONDBLCLK`：双击托盘图标
- [x] 恢复窗口显示（`ShowWindow` + `SetForegroundWindow`）
- [x] 提交代码

---

## 阶段3：自启最小化集成

### 步骤3.1：自启时最小化到托盘 ✅
- [x] 在 `main()` 中判断自启设置
- [x] 自启时使用 `ShowWindow(hWnd, SW_HIDE)` 启动
- [x] 自启时不在任务栏显示
- [x] 测试自启和手动启动的行为差异
- [x] 更新版本号到 1.4.0.0
- [x] 提交代码

---

## 阶段4：测试和发布

### 步骤4.1：编译测试 ⬜
- [ ] 编译程序（添加 -lshell32 链接）
- [ ] 测试托盘图标显示
- [ ] 测试最小化到托盘
- [ ] 测试右键菜单功能
- [ ] 测试双击恢复
- [ ] 测试自启最小化
- [ ] 测试窗口关闭和退出
- [ ] 提交代码

### 步骤4.2：更新文档 ⬜
- [ ] 更新版本开发日志
- [ ] 更新 changelog.md
- [ ] 更新根 README.md
- [ ] 更新 versions/README.md
- [ ] 提交文档

### 步骤4.3：合并主干 ⬜
- [ ] 将 1.4.0.0 分支合并到 main 分支
