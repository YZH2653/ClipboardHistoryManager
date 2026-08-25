import { useState, useMemo } from "react";
import {
    Input,
    List,
    Card,
    Tag,
    Empty,
    Button,
    Space,
    Tooltip,
    message,
    Checkbox,
    Dropdown,
} from "antd";
import {
    SearchOutlined,
    CopyOutlined,
    PushpinOutlined,
    DeleteOutlined,
    MoreOutlined,
    SelectOutlined,
    CloseCircleOutlined,
} from "@ant-design/icons";
import type { ClipRecord } from "../types";

// 模拟数据
const mockRecords: ClipRecord[] = [
    {
        id: 1,
        type: "text",
        content: "这是一段复制的文本内容，用于测试显示效果",
        preview: "这是一段复制的文本内容，用于测试显示效果",
        filePath: "",
        timestamp: new Date().toISOString(),
        isPinned: true,
    },
    {
        id: 2,
        type: "text",
        content: "Hello World - 测试英文内容",
        preview: "Hello World - 测试英文内容",
        filePath: "",
        timestamp: new Date(Date.now() - 3600000).toISOString(),
        isPinned: false,
    },
    {
        id: 3,
        type: "text",
        content: "剪贴板管理器测试内容，这是一段比较长的文本，用来测试截断显示效果",
        preview: "剪贴板管理器测试内容，这是一段比较长的文本，用来测试截断显示效果",
        filePath: "",
        timestamp: new Date(Date.now() - 7200000).toISOString(),
        isPinned: false,
    },
    {
        id: 4,
        type: "text",
        content: "React + TypeScript + Ant Design",
        preview: "React + TypeScript + Ant Design",
        filePath: "",
        timestamp: new Date(Date.now() - 10800000).toISOString(),
        isPinned: false,
    },
];

function HistoryPage() {
    const [searchText, setSearchText] = useState("");
    const [records, setRecords] = useState<ClipRecord[]>(mockRecords);
    const [selectedIds, setSelectedIds] = useState<number[]>([]);
    const [isSelectMode, setIsSelectMode] = useState(false);

    // 过滤和排序记录
    const filteredRecords = useMemo(() => {
        let result = records;

        // 搜索过滤
        if (searchText) {
            result = result.filter((record) =>
                record.content.toLowerCase().includes(searchText.toLowerCase()),
            );
        }

        // 排序：置顶优先，然后按时间倒序
        return [...result].sort((a, b) => {
            if (a.isPinned !== b.isPinned) {
                return a.isPinned ? -1 : 1;
            }
            return new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime();
        });
    }, [records, searchText]);

    // 格式化时间
    const formatTime = (timestamp: string) => {
        const date = new Date(timestamp);
        const now = new Date();
        const diff = now.getTime() - date.getTime();

        if (diff < 60000) {
            return "刚刚";
        } else if (diff < 3600000) {
            return `${Math.floor(diff / 60000)} 分钟前`;
        } else if (diff < 86400000) {
            return `${Math.floor(diff / 3600000)} 小时前`;
        } else {
            return date.toLocaleString("zh-CN");
        }
    };

    // 复制记录
    const handleCopy = (record: ClipRecord) => {
        navigator.clipboard.writeText(record.content);
        message.success("已复制到剪贴板");
    };

    // 切换置顶
    const handleTogglePin = (record: ClipRecord) => {
        setRecords((prev) =>
            prev.map((r) =>
                r.id === record.id ? { ...r, isPinned: !r.isPinned } : r,
            ),
        );
        message.success(record.isPinned ? "已取消置顶" : "已置顶");
    };

    // 删除记录
    const handleDelete = (record: ClipRecord) => {
        setRecords((prev) => prev.filter((r) => r.id !== record.id));
        message.success("已删除");
    };

    // 批量删除
    const handleBatchDelete = () => {
        setRecords((prev) => prev.filter((r) => !selectedIds.includes(r.id)));
        setSelectedIds([]);
        setIsSelectMode(false);
        message.success(`已删除 ${selectedIds.length} 条记录`);
    };

    // 切换选择
    const handleToggleSelect = (id: number) => {
        setSelectedIds((prev) =>
            prev.includes(id) ? prev.filter((i) => i !== id) : [...prev, id],
        );
    };

    // 全选/取消全选
    const handleSelectAll = () => {
        if (selectedIds.length === filteredRecords.length) {
            setSelectedIds([]);
        } else {
            setSelectedIds(filteredRecords.map((r) => r.id));
        }
    };

    // 更多操作菜单
    const getMoreMenu = (record: ClipRecord) => ({
        items: [
            {
                key: "copy",
                icon: <CopyOutlined />,
                label: "复制",
                onClick: () => handleCopy(record),
            },
            {
                key: "pin",
                icon: <PushpinOutlined />,
                label: record.isPinned ? "取消置顶" : "置顶",
                onClick: () => handleTogglePin(record),
            },
            {
                type: "divider" as const,
            },
            {
                key: "delete",
                icon: <DeleteOutlined />,
                label: "删除",
                danger: true,
                onClick: () => handleDelete(record),
            },
        ],
    });

    return (
        <div>
            {/* 搜索和操作栏 */}
            <div
                style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    marginBottom: 16,
                }}
            >
                <Input
                    placeholder="搜索历史记录..."
                    prefix={<SearchOutlined />}
                    value={searchText}
                    onChange={(e) => setSearchText(e.target.value)}
                    style={{ width: "100%", maxWidth: 400 }}
                    allowClear
                />

                <Space>
                    {isSelectMode ? (
                        <>
                            <Button
                                icon={<SelectOutlined />}
                                onClick={handleSelectAll}
                            >
                                {selectedIds.length === filteredRecords.length
                                    ? "取消全选"
                                    : "全选"}
                            </Button>
                            <Button
                                type="primary"
                                danger
                                icon={<DeleteOutlined />}
                                disabled={selectedIds.length === 0}
                                onClick={handleBatchDelete}
                            >
                                删除 ({selectedIds.length})
                            </Button>
                            <Button
                                icon={<CloseCircleOutlined />}
                                onClick={() => {
                                    setIsSelectMode(false);
                                    setSelectedIds([]);
                                }}
                            >
                                取消
                            </Button>
                        </>
                    ) : (
                        <Button
                            icon={<SelectOutlined />}
                            onClick={() => setIsSelectMode(true)}
                        >
                            选择
                        </Button>
                    )}
                </Space>
            </div>

            {/* 记录统计 */}
            <div style={{ marginBottom: 16, color: "#999", fontSize: 14 }}>
                共 {filteredRecords.length} 条记录
                {searchText && ` (搜索: "${searchText}")`}
            </div>

            {/* 记录列表 */}
            {filteredRecords.length > 0 ? (
                <List
                    dataSource={filteredRecords}
                    renderItem={(record) => (
                        <List.Item style={{ padding: "8px 0" }}>
                            <Card
                                size="small"
                                style={{
                                    width: "100%",
                                    borderLeft: record.isPinned
                                        ? "3px solid #fa8c16"
                                        : "3px solid #1890ff",
                                }}
                                hoverable
                            >
                                <div
                                    style={{
                                        display: "flex",
                                        justifyContent: "space-between",
                                        alignItems: "flex-start",
                                    }}
                                >
                                    {/* 选择框 */}
                                    {isSelectMode && (
                                        <Checkbox
                                            checked={selectedIds.includes(record.id)}
                                            onChange={() => handleToggleSelect(record.id)}
                                            style={{ marginRight: 12 }}
                                        />
                                    )}

                                    {/* 内容区域 */}
                                    <div style={{ flex: 1 }}>
                                        <div
                                            style={{
                                                marginBottom: 8,
                                                wordBreak: "break-all",
                                                fontSize: 14,
                                                lineHeight: 1.6,
                                            }}
                                        >
                                            {record.content.length > 150
                                                ? record.content.substring(0, 150) + "..."
                                                : record.content}
                                        </div>
                                        <Space>
                                            <Tag color="blue">
                                                {formatTime(record.timestamp)}
                                            </Tag>
                                            {record.isPinned && (
                                                <Tag color="orange">置顶</Tag>
                                            )}
                                            <Tag color="default">
                                                {record.content.length} 字符
                                            </Tag>
                                        </Space>
                                    </div>

                                    {/* 操作按钮 */}
                                    {!isSelectMode && (
                                        <Space>
                                            <Tooltip title="复制">
                                                <Button
                                                    type="text"
                                                    icon={<CopyOutlined />}
                                                    onClick={() => handleCopy(record)}
                                                />
                                            </Tooltip>
                                            <Tooltip title={record.isPinned ? "取消置顶" : "置顶"}>
                                                <Button
                                                    type="text"
                                                    icon={<PushpinOutlined />}
                                                    onClick={() => handleTogglePin(record)}
                                                    style={{
                                                        color: record.isPinned
                                                            ? "#fa8c16"
                                                            : undefined,
                                                    }}
                                                />
                                            </Tooltip>
                                            <Dropdown menu={getMoreMenu(record)} trigger={["click"]}>
                                                <Button
                                                    type="text"
                                                    icon={<MoreOutlined />}
                                                />
                                            </Dropdown>
                                        </Space>
                                    )}
                                </div>
                            </Card>
                        </List.Item>
                    )}
                />
            ) : (
                <Empty
                    description={
                        searchText ? "未找到匹配的记录" : "暂无历史记录"
                    }
                />
            )}
        </div>
    );
}

export default HistoryPage;
