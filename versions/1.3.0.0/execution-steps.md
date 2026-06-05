# 版本 1.3.0.0 执行步骤

## 阶段1：基础准备

### 步骤1.1：创建分支 ✅
- [x] 从 main 分支创建 `1.3.0.0` 分支

### 步骤1.2：创建版本文档 ✅
- [x] 创建 versions/1.3.0.0/ 目录结构
- [x] 编写 README.md、requirements.md、execution-steps.md、changelog.md
- [x] 创建 devlogs 目录

---

## 阶段2：实现开机自启功能

### 步骤2.1：添加注册表操作函数 ✅
- [x] 在 Storage.h 中声明 SetAutoStart/IsAutoStartEnabled 函数
- [x] 在 Storage.cpp 中实现注册表读写
- [x] 在 settings 表中添加 autoStart 字段支持
- [x] 提交代码

### 步骤2.2：在设置页面添加开关 ✅
- [x] 添加 G_AutoStart 全局变量
- [x] 在 DrawSettingsPage 中绘制开关按钮
- [x] 提交代码

### 步骤2.3：实现开关交互 ✅
- [x] 在 WM_LBUTTONDOWN 中处理开关点击
- [x] 点击时切换状态、更新注册表、保存设置
- [x] 提交代码

### 步骤2.4：启动时加载设置 ✅
- [x] 在 main() 中加载自启设置
- [x] 同步注册表状态
- [x] 更新版本号到 1.3.0.0
- [x] 提交代码

---

## 阶段3：测试和发布

### 步骤3.1：编译测试 ✅
- [x] 编译程序（添加 -ladvapi32 链接）
- [ ] 测试开机自启开关功能
- [ ] 测试注册表写入和删除
- [ ] 测试设置持久化
- [ ] 提交代码

### 步骤3.2：更新文档 ✅
- [x] 更新版本开发日志
- [x] 更新 changelog.md
- [ ] 更新根 README.md
- [ ] 更新 versions/README.md
- [x] 提交文档

### 步骤3.3：合并主干
- [ ] 将 1.3.0.0 分支合并到 main 分支
