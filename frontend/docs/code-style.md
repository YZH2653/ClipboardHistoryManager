# 前端代码风格规范

## 基础格式规范

### 缩进与空格
- 缩进：统一使用 **4个空格**
- 运算符前后必须加空格
- 逗号后必须加空格
- 冒号后必须加空格
- 分号后必须加空格

### 命名规范
- **组件名**：PascalCase（大驼峰），如 `MainLayout`、`HistoryPage`
- **函数名**：camelCase（小驼峰），如 `getRecords`、`formatTime`
- **变量名**：camelCase（小驼峰），如 `searchText`、`filteredRecords`
- **常量名**：UPPER_SNAKE_CASE（全大写下划线），如 `MAX_RECORDS`、`API_BASE_URL`
- **类型名**：PascalCase（大驼峰），如 `ClipRecord`、`Settings`
- **接口名**：PascalCase（大驼峰），如 `ClipRecord`、`Settings`

### 文件命名
- **组件文件**：PascalCase.tsx，如 `MainLayout.tsx`、`HistoryPage.tsx`
- **工具文件**：camelCase.ts，如 `api.ts`、`format.ts`
- **类型文件**：camelCase.ts，如 `index.ts`、`types.ts`
- **样式文件**：camelCase.css，如 `global.css`、`components.css`

## React 规范

### 组件定义
```typescript
// 使用函数组件 + TypeScript
function MainLayout() {
    // 状态定义
    const [selectedKey, setSelectedKey] = useState("history");

    // 事件处理
    const handleClick = (key: string) => {
        setSelectedKey(key);
    };

    // 渲染
    return (
        <Layout>
            {/* 组件内容 */}
        </Layout>
    );
}

export default MainLayout;
```

### Props 定义
```typescript
// 使用 interface 定义 Props
interface CardProps {
    title: string;
    content: string;
    onCopy?: () => void;
    onDelete?: () => void;
}

function Card({ title, content, onCopy, onDelete }: CardProps) {
    return (
        <div>
            <h3>{title}</h3>
            <p>{content}</p>
            {onCopy && <button onClick={onCopy}>复制</button>}
            {onDelete && <button onClick={onDelete}>删除</button>}
        </div>
    );
}
```

### 状态管理
```typescript
// 使用 useState 管理本地状态
const [count, setCount] = useState(0);

// 使用 useReducer 管理复杂状态
const [state, dispatch] = useReducer(reducer, initialState);

// 使用 useEffect 处理副作用
useEffect(() => {
    // 组件挂载时执行
    fetchData();

    // 组件卸载时清理
    return () => {
        cleanup();
    };
}, [dependency]); // 依赖数组
```

## TypeScript 规范

### 类型定义
```typescript
// 使用 interface 定义对象类型
interface ClipRecord {
    id: number;
    content: string;
    timestamp: string;
    isPinned: boolean;
}

// 使用 type 定义联合类型
type ClipType = "text" | "image";

// 使用 enum 定义枚举
enum PageState {
    HISTORY = "history",
    SETTINGS = "settings",
    ABOUT = "about",
}
```

### 泛型使用
```typescript
// 泛型函数
function useState<T>(initialValue: T): [T, (value: T) => void] {
    // 实现
}

// 泛型组件
interface ListProps<T> {
    items: T[];
    renderItem: (item: T) => React.ReactNode;
}

function List<T>({ items, renderItem }: ListProps<T>) {
    return (
        <div>
            {items.map((item, index) => (
                <div key={index}>{renderItem(item)}</div>
            ))}
        </div>
    );
}
```

## 样式规范

### CSS 类名
```css
/* 使用 kebab-case */
.card-container {
    padding: 16px;
    border-radius: 8px;
}

.card-title {
    font-size: 18px;
    font-weight: bold;
}

/* BEM 命名（可选） */
.card__title {
    font-size: 18px;
}

.card__title--active {
    color: #4A90D9;
}
```

### 样式顺序
```css
/* 按类别排序 */
.element {
    /* 1. 定位 */
    position: relative;
    top: 0;
    left: 0;

    /* 2. 盒模型 */
    display: flex;
    width: 100%;
    height: 100px;
    margin: 0;
    padding: 16px;

    /* 3. 文字 */
    font-size: 14px;
    line-height: 1.5;
    color: #333;
    text-align: center;

    /* 4. 视觉 */
    background: #fff;
    border: 1px solid #ddd;
    border-radius: 8px;

    /* 5. 其他 */
    cursor: pointer;
    transition: all 0.3s;
}
```

## 注释规范

### 文件头注释
```typescript
/**
 * 历史记录页面组件
 * 显示剪贴板历史记录列表，支持搜索、复制、置顶、删除操作
 */
```

### 函数注释
```typescript
/**
 * 格式化时间戳
 * @param timestamp ISO 格式的时间字符串
 * @returns 格式化后的本地时间字符串
 */
function formatTime(timestamp: string): string {
    const date = new Date(timestamp);
    return date.toLocaleString("zh-CN");
}
```

### 行内注释
```typescript
// 过滤搜索结果
const filteredRecords = records.filter(
    (record) => record.content.includes(searchText),
);
```

## 导入规范

### 导入顺序
```typescript
// 1. React 相关
import { useState, useEffect } from "react";

// 2. 第三方库
import { Button, Input, List } from "antd";
import { CopyOutlined, DeleteOutlined } from "@ant-design/icons";

// 3. 本地组件
import MainLayout from "./components/MainLayout";
import HistoryPage from "./pages/HistoryPage";

// 4. 工具函数
import { formatTime } from "./utils/format";

// 5. 类型定义
import type { ClipRecord, Settings } from "./types";

// 6. 样式
import "./styles/global.css";
```

## 错误处理

### 异步操作
```typescript
// 使用 try-catch 处理异步错误
async function fetchRecords() {
    try {
        const records = await getRecords();
        setRecords(records);
    } catch (error) {
        console.error("获取记录失败:", error);
        message.error("获取记录失败");
    }
}
```

### 边界处理
```typescript
// 使用条件渲染处理边界情况
function RecordList({ records }: RecordListProps) {
    if (records.length === 0) {
        return <Empty description="暂无记录" />;
    }

    return (
        <List
            dataSource={records}
            renderItem={(record) => <RecordCard record={record} />}
        />
    );
}
```

## 性能优化

### 避免不必要的渲染
```typescript
// 使用 React.memo 避免不必要的渲染
const RecordCard = React.memo(function RecordCard({ record }: RecordCardProps) {
    return (
        <Card>
            <p>{record.content}</p>
        </Card>
    );
});

// 使用 useMemo 缓存计算结果
const filteredRecords = useMemo(
    () => records.filter((r) => r.content.includes(searchText)),
    [records, searchText],
);

// 使用 useCallback 缓存函数
const handleCopy = useCallback(
    (id: number) => {
        copyRecord(id);
    },
    [],
);
```

## 最佳实践

1. **单一职责**：每个组件只负责一个功能
2. **可复用性**：提取通用组件，避免重复代码
3. **类型安全**：充分利用 TypeScript 类型检查
4. **可读性**：代码清晰易懂，注释简洁明了
5. **可维护性**：模块化设计，便于后续修改
