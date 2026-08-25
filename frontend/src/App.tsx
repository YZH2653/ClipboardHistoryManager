import { useState } from "react";
import { ConfigProvider, theme } from "antd";
import zhCN from "antd/locale/zh_CN";
import MainLayout from "./components/MainLayout";

function App() {
    const [isDarkMode] = useState(false);

    return (
        <ConfigProvider
            locale={zhCN}
            theme={{
                algorithm: isDarkMode ? theme.darkAlgorithm : theme.defaultAlgorithm,
                token: {
                    colorPrimary: "#4A90D9",
                    borderRadius: 8,
                },
            }}
        >
            <MainLayout />
        </ConfigProvider>
    );
}

export default App;
