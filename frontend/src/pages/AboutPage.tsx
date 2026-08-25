import { Card, Descriptions, Divider, Typography } from "antd";

const { Paragraph, Link } = Typography;

function AboutPage() {
    return (
        <div>
            <h2>关于</h2>
            <Divider />

            <Card>
                <Descriptions column={1} bordered>
                    <Descriptions.Item label="软件名称">
                        历史剪贴板管理器
                    </Descriptions.Item>
                    <Descriptions.Item label="版本号">
                        2.0.0
                    </Descriptions.Item>
                    <Descriptions.Item label="更新日期">
                        2026-08-25
                    </Descriptions.Item>
                    <Descriptions.Item label="作者">
                        YZH2653
                    </Descriptions.Item>
                    <Descriptions.Item label="邮箱">
                        yzh2653@163.com
                    </Descriptions.Item>
                    <Descriptions.Item label="GitHub">
                        <Link
                            href="https://github.com/YZH2653/ClipboardHistoryManager"
                            target="_blank"
                        >
                            ClipboardHistoryManager
                        </Link>
                    </Descriptions.Item>
                </Descriptions>
            </Card>

            <Card title="更新内容" style={{ marginTop: 16 }}>
                <Paragraph>
                    <ul>
                        <li>全新前端界面（React + TypeScript + Ant Design）</li>
                        <li>现代化 UI 设计</li>
                        <li>更好的用户体验</li>
                        <li>全局快捷键支持</li>
                    </ul>
                </Paragraph>
            </Card>

            <Card title="快捷键" style={{ marginTop: 16 }}>
                <Descriptions column={1} bordered>
                    <Descriptions.Item label="显示/隐藏窗口">
                        Ctrl + Alt + V
                    </Descriptions.Item>
                    <Descriptions.Item label="快速复制最近记录">
                        Ctrl + Alt + C
                    </Descriptions.Item>
                </Descriptions>
            </Card>
        </div>
    );
}

export default AboutPage;
