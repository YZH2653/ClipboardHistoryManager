import { useState } from "react";
import { Layout, Menu } from "antd";
import {
    HistoryOutlined,
    SettingOutlined,
    InfoCircleOutlined,
} from "@ant-design/icons";
import HistoryPage from "../pages/HistoryPage";
import SettingsPage from "../pages/SettingsPage";
import AboutPage from "../pages/AboutPage";

const { Content, Sider } = Layout;

// 菜单项
const menuItems = [
    {
        key: "history",
        icon: <HistoryOutlined />,
        label: "历史记录",
    },
    {
        key: "settings",
        icon: <SettingOutlined />,
        label: "设置",
    },
    {
        key: "about",
        icon: <InfoCircleOutlined />,
        label: "关于",
    },
];

// 页面组件映射
const pageComponents: Record<string, React.ComponentType> = {
    history: HistoryPage,
    settings: SettingsPage,
    about: AboutPage,
};

function MainLayout() {
    const [selectedKey, setSelectedKey] = useState("history");

    const PageComponent = pageComponents[selectedKey] || HistoryPage;

    return (
        <Layout style={{ minHeight: "100vh" }}>
            <Sider
                breakpoint="lg"
                collapsedWidth="0"
                style={{ background: "#fff" }}
            >
                <div
                    style={{
                        height: 64,
                        display: "flex",
                        alignItems: "center",
                        justifyContent: "center",
                        borderBottom: "1px solid #f0f0f0",
                    }}
                >
                    <h2 style={{ margin: 0, color: "#4A90D9" }}>📋 剪贴板</h2>
                </div>
                <Menu
                    mode="inline"
                    selectedKeys={[selectedKey]}
                    items={menuItems}
                    onClick={({ key }) => setSelectedKey(key)}
                    style={{ borderRight: 0 }}
                />
            </Sider>
            <Layout style={{ overflow: "hidden" }}>
                <Content
                    style={{
                        margin: 24,
                        padding: 24,
                        background: "#fff",
                        borderRadius: 8,
                        minHeight: 280,
                        overflow: "auto",
                    }}
                >
                    <PageComponent />
                </Content>
            </Layout>
        </Layout>
    );
}

export default MainLayout;
