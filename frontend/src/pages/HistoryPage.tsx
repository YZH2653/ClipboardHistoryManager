import { useState, useEffect, useMemo, useCallback } from "react";
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
    Spin,
} from "antd";
import {
    SearchOutlined,
    CopyOutlined,
    PushpinOutlined,
    DeleteOutlined,
    SelectOutlined,
    CloseCircleOutlined,
    ReloadOutlined,
} from "@ant-design/icons";
import type { ClipRecord } from "../types";
import {
    getRecords,
    getRecordContent,
    deleteRecord,
    togglePin,
    batchDeleteRecords,
} from "../utils/api";

function HistoryPage() {
    const [searchText, setSearchText] = useState("");
    const [records, setRecords] = useState<ClipRecord[]>([]);
    const [selectedIds, setSelectedIds] = useState<number[]>([]);
    const [isSelectMode, setIsSelectMode] = useState(false);
    const [loading, setLoading] = useState(true);

    // 加载记录
    const loadRecords = useCallback(async () => {
        try {
            setLoading(true);
            const data = await getRecords();
            setRecords(data);
        } catch (error) {
            console.error("加载记录失败:", error);
            message.error("加载记录失败");
        } finally {
            setLoading(false);
        }
    }, []);

    useEffect(() => {
        loadRecords();
        // 每2秒自动刷新，检测新剪贴板记录
        const timer = setInterval(loadRecords, 2000);
        return () => clearInterval(timer);
    }, [loadRecords]);

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
        } else if (diff < 604800000) {
            return `${Math.floor(diff / 86400000)} 天前`;
        } else {
            return date.toLocaleDateString("zh-CN");
        }
    };

    // 复制记录
    const handleCopy = async (record: ClipRecord) => {
        try {
            const content = await getRecordContent(record.id);
            await navigator.clipboard.writeText(content);
            message.success("已复制到剪贴板");
        } catch (error) {
            // 降级：直接使用记录中的内容
            try {
                await navigator.clipboard.writeText(record.content);
                message.success("已复制到剪贴板");
            } catch {
                message.error("复制失败");
            }
        }
    };

    // 切换置顶
    const handleTogglePin = async (record: ClipRecord) => {
        try {
            await togglePin(record.id);
            await loadRecords();
            message.success(record.isPinned ? "已取消置顶" : "已置顶");
        } catch (error) {
            message.error("操作失败");
        }
    };

    // 删除记录
    const handleDelete = async (record: ClipRecord) => {
        try {
            await deleteRecord(record.id);
            setRecords((prev) => prev.filter((r) => r.id !== record.id));
            message.success("已删除");
        } catch (error) {
            message.error("删除失败");
        }
    };

    // 批量删除
    const handleBatchDelete = async () => {
        try {
            await batchDeleteRecords(selectedIds);
            setRecords((prev) => prev.filter((r) => !selectedIds.includes(r.id)));
            message.success(`已删除 ${selectedIds.length} 条记录`);
            setSelectedIds([]);
            setIsSelectMode(false);
        } catch (error) {
            message.error("批量删除失败");
        }
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

    return (
        <div>
            {/* 搜索和操作栏 */}
            <div
                style={{
                    display: "flex",
                    flexWrap: "wrap",
                    justifyContent: "space-between",
                    alignItems: "center",
                    marginBottom: 16,
                    gap: 8,
                }}
            >
                <Input
                    placeholder="搜索历史记录..."
                    prefix={<SearchOutlined />}
                    value={searchText}
                    onChange={(e) => setSearchText(e.target.value)}
                    style={{ flex: 1, minWidth: 180, maxWidth: 400 }}
                    allowClear
                />

                <Space wrap>
                    <Button
                        icon={<ReloadOutlined />}
                        onClick={loadRecords}
                        loading={loading}
                    >
                        刷新
                    </Button>
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
            <Spin spinning={loading}>
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
                                                {record.type === "image" ? (
                                                    <Space>
                                                        <Tag color="purple">图片</Tag>
                                                        <span style={{ color: "#999" }}>
                                                            {record.preview || "图片记录"}
                                                        </span>
                                                    </Space>
                                                ) : (
                                                    <>
                                                        {record.content.length > 150
                                                            ? record.content.substring(0, 150) + "..."
                                                            : record.content}
                                                    </>
                                                )}
                                            </div>
                                            <Space>
                                                <Tag color="blue">
                                                    {formatTime(record.timestamp)}
                                                </Tag>
                                                {record.isPinned && (
                                                    <Tag color="orange">置顶</Tag>
                                                )}
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
                                                <Tooltip title="删除">
                                                    <Button
                                                        type="text"
                                                        danger
                                                        icon={<DeleteOutlined />}
                                                        onClick={() => handleDelete(record)}
                                                    />
                                                </Tooltip>
                                            </Space>
                                        )}
                                    </div>
                                </Card>
                            </List.Item>
                        )}
                    />
                ) : (
                    !loading && (
                        <Empty
                            description={
                                searchText ? "未找到匹配的记录" : "暂无历史记录"
                            }
                        />
                    )
                )}
            </Spin>
        </div>
    );
}

export default HistoryPage;
