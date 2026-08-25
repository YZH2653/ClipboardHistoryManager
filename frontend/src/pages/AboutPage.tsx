import {
    Card,
    Descriptions,
    Typography,
    Tag,
    Space,
    Timeline,
    Button,
} from "antd";
import {
    GithubOutlined,
    MailOutlined,
    CopyOutlined,
    HistoryOutlined,
    ThunderboltOutlined,
    SafetyCertificateOutlined,
} from "@ant-design/icons";

const { Paragraph, Link, Title } = Typography;

function AboutPage() {
    // 复制邮箱
    const copyEmail = () => {
        navigator.clipboard.writeText("yzh2653@163.com");
    };

    return (
        <div>
            <Title level={2} style={{ marginBottom: 24 }}>
                关于
            </Title>

            {/* 基本信息 */}
            <Card style={{ marginBottom: 16 }}>
                <Descriptions column={1} bordered>
                    <Descriptions.Item label="软件名称">
                        历史剪贴板管理器
                    </Descriptions.Item>
                    <Descriptions.Item label="版本号">
                        <Tag color="blue">v2.0.0</Tag>
                    </Descriptions.Item>
                    <Descriptions.Item label="更新日期">
                        2026-08-25
                    </Descriptions.Item>
                    <Descriptions.Item label="技术栈">
                        <Space wrap>
                            <Tag color="green">React</Tag>
                            <Tag color="blue">TypeScript</Tag>
                            <Tag color="cyan">Ant Design</Tag>
                            <Tag color="orange">Tauri</Tag>
                            <Tag color="red">Rust</Tag>
                            <Tag color="purple">Vite</Tag>
                        </Space>
                    </Descriptions.Item>
                    <Descriptions.Item label="作者">
                        YZH2653
                    </Descriptions.Item>
                    <Descriptions.Item label="联系方式">
                        <Space>
                            <MailOutlined />
                            <span>yzh2653@163.com</span>
                            <Button
                                type="text"
                                icon={<CopyOutlined />}
                                size="small"
                                onClick={copyEmail}
                            />
                        </Space>
                    </Descriptions.Item>
                    <Descriptions.Item label="GitHub">
                        <Link
                            href="https://github.com/YZH2653/ClipboardHistoryManager"
                            target="_blank"
                        >
                            <Space>
                                <GithubOutlined />
                                ClipboardHistoryManager
                            </Space>
                        </Link>
                    </Descriptions.Item>
                </Descriptions>
            </Card>

            {/* 功能特性 */}
            <Card title="功能特性" style={{ marginBottom: 16 }}>
                <Space direction="vertical" size="middle" style={{ width: "100%" }}>
                    <div style={{ display: "flex", alignItems: "flex-start", gap: 12 }}>
                        <HistoryOutlined style={{ fontSize: 24, color: "#1890ff", marginTop: 4 }} />
                        <div>
                            <div style={{ fontWeight: "bold", marginBottom: 4 }}>
                                历史记录管理
                            </div>
                            <div style={{ color: "#666" }}>
                                自动记录剪贴板内容，支持搜索、置顶、删除、批量操作
                            </div>
                        </div>
                    </div>
                    <div style={{ display: "flex", alignItems: "flex-start", gap: 12 }}>
                        <ThunderboltOutlined style={{ fontSize: 24, color: "#fa8c16", marginTop: 4 }} />
                        <div>
                            <div style={{ fontWeight: "bold", marginBottom: 4 }}>
                                全局快捷键
                            </div>
                            <div style={{ color: "#666" }}>
                                Ctrl+Alt+V 显示/隐藏窗口，Ctrl+Alt+C 快速复制最近记录
                            </div>
                        </div>
                    </div>
                    <div style={{ display: "flex", alignItems: "flex-start", gap: 12 }}>
                        <SafetyCertificateOutlined style={{ fontSize: 24, color: "#52c41a", marginTop: 4 }} />
                        <div>
                            <div style={{ fontWeight: "bold", marginBottom: 4 }}>
                                数据安全
                            </div>
                            <div style={{ color: "#666" }}>
                                本地存储，支持自动清理过期记录，保护隐私安全
                            </div>
                        </div>
                    </div>
                </Space>
            </Card>

            {/* 快捷键说明 */}
            <Card title="快捷键" style={{ marginBottom: 16 }}>
                <Descriptions column={1} bordered>
                    <Descriptions.Item label="显示/隐藏窗口">
                        <Space wrap>
                            <Tag>Ctrl</Tag> + <Tag>Alt</Tag> + <Tag>V</Tag>
                        </Space>
                    </Descriptions.Item>
                    <Descriptions.Item label="快速复制最近记录">
                        <Space wrap>
                            <Tag>Ctrl</Tag> + <Tag>Alt</Tag> + <Tag>C</Tag>
                        </Space>
                    </Descriptions.Item>
                </Descriptions>
            </Card>

            {/* 更新日志 */}
            <Card title="更新日志">
                <Timeline
                    items={[
                        {
                            color: "blue",
                            children: (
                                <div>
                                    <Tag color="blue">v2.0.0</Tag>
                                    <span style={{ color: "#999", marginLeft: 8 }}>
                                        2026-08-25
                                    </span>
                                    <Paragraph style={{ marginTop: 8 }}>
                                        <ul>
                                            <li>全新前端界面（React + TypeScript + Ant Design）</li>
                                            <li>现代化 UI 设计</li>
                                            <li>更好的用户体验</li>
                                            <li>全局快捷键支持</li>
                                        </ul>
                                    </Paragraph>
                                </div>
                            ),
                        },
                        {
                            color: "green",
                            children: (
                                <div>
                                    <Tag color="green">v1.9.0</Tag>
                                    <span style={{ color: "#999", marginLeft: 8 }}>
                                        2026-08-23
                                    </span>
                                    <Paragraph style={{ marginTop: 8 }}>
                                        <ul>
                                            <li>全局快捷键支持</li>
                                            <li>防重复打开功能</li>
                                        </ul>
                                    </Paragraph>
                                </div>
                            ),
                        },
                        {
                            color: "gray",
                            children: (
                                <div>
                                    <Tag>v1.8.0</Tag>
                                    <span style={{ color: "#999", marginLeft: 8 }}>
                                        2026-08-23
                                    </span>
                                    <Paragraph style={{ marginTop: 8 }}>
                                        <ul>
                                            <li>性能优化</li>
                                            <li>GDI 对象缓存</li>
                                            <li>双缓冲绘制</li>
                                        </ul>
                                    </Paragraph>
                                </div>
                            ),
                        },
                    ]}
                />
            </Card>
        </div>
    );
}

export default AboutPage;
