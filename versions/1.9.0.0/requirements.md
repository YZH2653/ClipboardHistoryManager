# 版本 1.9.0.0 需求文档

## 功能描述

### 当前问题

1. **操作效率低**
   - 每次需要点击托盘图标或任务栏才能打开窗口
   - 没有快速粘贴的快捷方式
   - 频繁切换窗口影响工作效率

2. **缺少快捷键**
   - 没有全局快捷键支持
   - 无法通过键盘快速操作
   - 与其他软件的快捷键体验差距大

### 解决方案

新增全局快捷键支持，让用户可以通过键盘快速调出窗口和执行操作。

## 功能需求

### 1. 全局快捷键注册

**需求描述**：
注册系统级全局快捷键，即使窗口不在前台也能响应。

**功能要求**：
- 使用 RegisterHotKey 注册全局快捷键
- 支持组合键（Ctrl、Alt、Shift + 字母/数字/功能键）
- 程序退出时自动注销快捷键
- 快捷键冲突检测

### 2. 快速调出窗口

**需求描述**：
一键显示/隐藏主窗口。

**功能要求**：
- 默认快捷键：`Ctrl + Alt + V`（显示/隐藏窗口）
- 窗口在前台时按快捷键隐藏到托盘
- 窗口在后台时按快捷键调出到前台
- 窗口最小化时按快捷键恢复显示

### 3. 快速粘贴

**需求描述**：
快捷键触发粘贴操作。

**功能要求**：
- 默认快捷键：`Ctrl + Alt + C`（打开并复制最近一条记录）
- 支持选择历史记录后自动粘贴到当前窗口
- 粘贴后自动隐藏窗口

### 4. 自定义快捷键

**需求描述**：
支持用户自定义快捷键组合。

**功能要求**：
- 在设置页面添加快捷键配置
- 支持修改快捷键组合
- 支持恢复默认快捷键
- 快捷键配置持久化保存

## 技术要求

### 1. Windows API

```cpp
// 注册全局快捷键
BOOL RegisterHotKey(
    HWND hWnd,        // 窗口句柄
    int id,           // 快捷键标识
    UINT fsModifiers, // 修饰键（MOD_ALT, MOD_CONTROL, MOD_SHIFT, MOD_WIN）
    UINT vk           // 虚拟键码
);

// 注销全局快捷键
BOOL UnregisterHotKey(
    HWND hWnd, // 窗口句柄
    int id     // 快捷键标识
);

// 快捷键消息
#define WM_HOTKEY 0x0312
```

### 2. 快捷键配置结构

```cpp
// 快捷键配置
struct HotkeyConfig {
    int id;                // 快捷键标识
    wstring name;          // 功能名称
    UINT modifiers;        // 修饰键
    UINT vk;               // 虚拟键码
    bool enabled;          // 是否启用
};

// 快捷键管理器
class HotkeyManager {
private:
    vector<HotkeyConfig> configs;
    HWND hWnd;
public:
    bool Initialize(HWND hWnd);
    bool RegisterHotkey(const HotkeyConfig& config);
    bool UnregisterHotkey(int id);
    bool UnregisterAll();
    bool SaveConfig();
    bool LoadConfig();
    HotkeyConfig GetConfig(int id);
};
```

### 3. 存储要求

- 快捷键配置存储在数据库 settings 表
- 支持配置持久化
- 支持恢复默认配置

### 4. 界面要求

- 在设置页面添加快捷键配置入口
- 快捷键配置界面
- 快捷键录制控件（捕获用户按键）

## 测试用例

### 1. 基本功能测试

1. 注册全局快捷键
2. 测试快捷键响应
3. 测试窗口显示/隐藏
4. 测试快捷键注销

### 2. 组合键测试

1. Ctrl + Alt + 字母
2. Ctrl + Shift + 字母
3. Alt + Shift + 字母
4. 功能键（F1-F12）

### 3. 冲突测试

1. 与其他软件快捷键冲突
2. 重复注册同一快捷键
3. 快捷键修改后重新注册

### 4. 边界条件测试

1. 程序启动时注册
2. 程序退出时注销
3. 窗口最小化状态
4. 窗口最大化状态
