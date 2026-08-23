# 版本 1.8.0.0 需求文档

## 功能描述

### 当前问题

1. **GDI 对象频繁创建销毁**
   - 每次绘制都调用 CreateFont/DeleteObject 创建和销毁字体
   - 每次绘制都创建新的画笔、画刷
   - GDI 对象创建开销大，影响绘制性能

2. **界面绘制效率低**
   - GetFilteredRecords() 每次重绘都复制并排序全部记录
   - 无双缓冲机制，直接绘制到 HDC 导致闪烁
   - 全窗口重绘，未使用局部更新

3. **数据操作效率低**
   - vector 头部插入是 O(n) 操作
   - 搜索时 preview 和 content 重复匹配

### 解决方案

优化软件代码，提升响应速度，优化软件流畅性。

## 功能需求

### 1. GDI 对象缓存

**需求描述**：
缓存常用的 GDI 对象（字体、画笔、画刷），避免频繁创建销毁。

**功能要求**：
- 缓存常用字体（标题、正文、按钮等）
- 缓存常用画笔（边框、分割线等）
- 缓存常用画刷（背景色等）
- 程序退出时统一释放缓存对象

### 2. 双缓冲绘制

**需求描述**：
使用双缓冲机制消除界面闪烁。

**功能要求**：
- 创建内存 DC 和位图
- 先绘制到内存 DC
- 一次性拷贝到屏幕 DC
- 消除绘制闪烁

### 3. 记录缓存优化

**需求描述**：
优化 GetFilteredRecords 的调用频率和效率。

**功能要求**：
- 缓存筛选结果，仅在数据变化时重新计算
- 搜索文本变化时才重新筛选
- 记录增删时标记缓存失效

### 4. 数据结构优化

**需求描述**：
优化数据存储结构，提升操作效率。

**功能要求**：
- 使用 deque 替代 vector 实现头部插入
- 或使用 vector + 尾部插入 + 反向迭代
- 优化搜索逻辑，避免重复匹配

### 5. 局部重绘优化

**需求描述**：
减少不必要的全窗口重绘。

**功能要求**：
- 仅重绘变化的区域
- 滚动时使用 ScrollWindow 优化
- 减少不必要的 InvalidateRect 调用

## 技术要求

### 1. GDI 缓存结构

```cpp
// GDI 对象缓存管理器
struct GDICache {
    // 字体缓存
    HFONT fontTitle;        // 标题字体 (36px Bold)
    HFONT fontSection;      // 段落字体 (26px)
    HFONT fontContent;      // 内容字体 (24px)
    HFONT fontButton;       // 按钮字体 (18px)
    HFONT fontSmall;        // 小字体 (16px)

    // 画刷缓存
    HBRUSH brushWhite;      // 白色背景
    HBRUSH brushLightGray;  // 浅灰背景
    HBRUSH brushHover;      // 悬停背景

    // 画笔缓存
    HPEN penBorder;         // 边框画笔
    HPEN penDivider;        // 分割线画笔
    HPEN penHighlight;      // 高亮画笔

    void Initialize();
    void Cleanup();
};
```

### 2. 双缓冲实现

```cpp
// 双缓冲绘制
class DoubleBuffer {
    HDC m_memDC;
    HBITMAP m_memBitmap;
    HBITMAP m_oldBitmap;
    int m_width;
    int m_height;

public:
    void BeginDraw(HDC hdc, int width, int height);
    HDC GetDC();
    void EndDraw(HDC hdc);
    void Cleanup();
};
```

### 3. 记录缓存

```cpp
// 筛选结果缓存
struct RecordCache {
    vector<ClipRecord> filteredRecords;
    wstring lastSearchText;
    bool isValid;

    void Invalidate();
    const vector<ClipRecord>& GetFiltered(const wstring& searchText);
};
```

## 测试用例

### 1. 性能测试

1. 大量记录（500+）时的滚动流畅度
2. 搜索响应速度
3. 剪贴板捕获响应时间
4. 窗口拖动/缩放流畅度

### 2. 功能测试

1. 所有原有功能正常工作
2. GDI 对象无泄漏
3. 内存占用无异常增长
