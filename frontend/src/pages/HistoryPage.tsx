import { useState } from "react";
import { Input, List, Card, Tag, Empty, Button, Space } from "antd";
import {
    SearchOutlined,
    CopyOutlined,
    PushpinOutlined,
    DeleteOutlined,
} from "@ant-design/icons";

// 模拟数据
const mockRecords = [
    {
        id: 1,
        content: "这是一段复制的文本内容",
        timestamp: new Date().toISOString(),
        isPinned: true,
    },
    {
        id: 2,
        content: "Hello World",
        timestamp: new Date(Date.now() - 3600000).toISOString(),
        isPinned: false,
    },
    {
        id: 3,
        content: "剪贴板管理器测试内容",
        timestamp: new Date(Date.now() - 7200000).toISOString(),
        isPinned: false,
    },
];

function HistoryPage() {
    const [searchText, setSearchText] = useState("");

    // 过滤记录
    const filteredRecords = mockRecords.filter(
        (record) =>
            record.content.toLowerCase().includes(searchText.toLowerCase()),
    );

    // 格式化时间
    const formatTime = (timestamp: string) => {
        const date = new Date(timestamp);
        return date.toLocaleString("zh-CN");
    };

    return (
        <div>
            {/* 搜索框 */}
            <Input
                placeholder="搜索历史记录..."
                prefix={<SearchOutlined />}
                value={searchText}
                onChange={(e) => setSearchText(e.target.value)}
                style={{ marginBottom: 16 }}
                allowClear
            />

            {/* 记录列表 */}
            {filteredRecords.length > 0 ? (
                <List
                    dataSource={filteredRecords}
                    renderItem={(record) => (
                        <List.Item>
                            <Card
                                size="small"
                                style={{ width: "100%" }}
                                hoverable
                            >
                                <div
                                    style={{
                                        display: "flex",
                                        justifyContent: "space-between",
                                        alignItems: "flex-start",
                                    }}
                                >
                                    <div style={{ flex: 1 }}>
                                        <div
                                            style={{
                                                marginBottom: 8,
                                                wordBreak: "break-all",
                                            }}
                                        >
                                            {record.content.length > 100
                                                ? record.content.substring(
                                                      0,
                                                      100,
                                                  ) + "..."
                                                : record.content}
                                        </div>
                                        <Space>
                                            <Tag color="blue">
                                                {formatTime(record.timestamp)}
                                            </Tag>
                                            {record.isPinned && (
                                                <Tag color="orange">
                                                    置顶
                                                </Tag>
                                            )}
                                        </Space>
                                    </div>
                                    <Space>
                                        <Button
                                            type="text"
                                            icon={<CopyOutlined />}
                                            title="复制"
                                        />
                                        <Button
                                            type="text"
                                            icon={<PushpinOutlined />}
                                            title="置顶"
                                        />
                                        <Button
                                            type="text"
                                            danger
                                            icon={<DeleteOutlined />}
                                            title="删除"
                                        />
                                    </Space>
                                </div>
                            </Card>
                        </List.Item>
                    )}
                />
            ) : (
                <Empty description="暂无历史记录" />
            )}
        </div>
    );
}

export default HistoryPage;
