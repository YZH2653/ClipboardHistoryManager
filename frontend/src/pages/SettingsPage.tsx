import { useState, useEffect } from "react";
import {
    Form,
    Select,
    Switch,
    Card,
    InputNumber,
    Button,
    Space,
    message,
    Modal,
    Tag,
    Spin,
} from "antd";
import {
    SaveOutlined,
    ReloadOutlined,
    ExportOutlined,
    ImportOutlined,
    DeleteOutlined,
} from "@ant-design/icons";
import { getSettings, saveSettings, clearAllRecords } from "../utils/api";
import type { Settings } from "../types";

function SettingsPage() {
    const [form] = Form.useForm();
    const [loading, setLoading] = useState(false);
    const [initialLoading, setInitialLoading] = useState(true);

    // 加载设置
    useEffect(() => {
        loadSettings();
    }, []);

    const loadSettings = async () => {
        try {
            setInitialLoading(true);
            const settings = await getSettings();
            form.setFieldsValue(settings);
        } catch (error) {
            console.error("加载设置失败:", error);
            message.error("加载设置失败");
        } finally {
            setInitialLoading(false);
        }
    };

    // 保存设置
    const handleSave = async () => {
        try {
            setLoading(true);
            const values = await form.validateFields();
            await saveSettings(values as Settings);
            message.success("设置已保存");
        } catch (error) {
            console.error("保存失败:", error);
            message.error("保存失败");
        } finally {
            setLoading(false);
        }
    };

    // 恢复默认设置
    const handleReset = () => {
        Modal.confirm({
            title: "恢复默认设置",
            content: "确定要恢复所有设置到默认值吗？",
            onOk: () => {
                form.setFieldsValue({
                    retentionDays: 3,
                    maxRecords: 1000,
                    autoStart: false,
                    minimizeToTray: true,
                    hotkeyToggle: "ctrl+alt+v",
                    hotkeyCopy: "ctrl+alt+c",
                });
                message.success("已恢复默认设置");
            },
        });
    };

    // 导出设置
    const handleExport = () => {
        const values = form.getFieldsValue();
        const blob = new Blob([JSON.stringify(values, null, 2)], {
            type: "application/json",
        });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = "clipboard-settings.json";
        a.click();
        URL.revokeObjectURL(url);
        message.success("设置已导出");
    };

    // 导入设置
    const handleImport = () => {
        const input = document.createElement("input");
        input.type = "file";
        input.accept = ".json";
        input.onchange = (e) => {
            const file = (e.target as HTMLInputElement).files?.[0];
            if (file) {
                const reader = new FileReader();
                reader.onload = (event) => {
                    try {
                        const settings = JSON.parse(
                            event.target?.result as string,
                        );
                        form.setFieldsValue(settings);
                        message.success("设置已导入");
                    } catch {
                        message.error("导入失败，文件格式错误");
                    }
                };
                reader.readAsText(file);
            }
        };
        input.click();
    };

    // 清空所有记录
    const handleClearAll = () => {
        Modal.confirm({
            title: "清空所有记录",
            content: "确定要清空所有历史记录吗？此操作不可恢复。",
            okText: "确定",
            cancelText: "取消",
            okButtonProps: { danger: true },
            onOk: async () => {
                try {
                    await clearAllRecords();
                    message.success("已清空所有记录");
                } catch (error) {
                    message.error("清空失败");
                }
            },
        });
    };

    return (
        <Spin spinning={initialLoading}>
            <div>
                <div
                    style={{
                        display: "flex",
                        flexWrap: "wrap",
                        justifyContent: "space-between",
                        alignItems: "center",
                        marginBottom: 24,
                        gap: 8,
                    }}
                >
                    <h2 style={{ margin: 0 }}>设置</h2>
                    <Space wrap>
                        <Button icon={<ImportOutlined />} onClick={handleImport}>
                            导入
                        </Button>
                        <Button icon={<ExportOutlined />} onClick={handleExport}>
                            导出
                        </Button>
                        <Button icon={<ReloadOutlined />} onClick={handleReset}>
                            恢复默认
                        </Button>
                        <Button
                            type="primary"
                            icon={<SaveOutlined />}
                            loading={loading}
                            onClick={handleSave}
                        >
                            保存
                        </Button>
                    </Space>
                </div>

                <Form
                    form={form}
                    layout="vertical"
                    initialValues={{
                        retentionDays: 3,
                        maxRecords: 1000,
                        autoStart: false,
                        minimizeToTray: true,
                    }}
                >
                    {/* 基本设置 */}
                    <Card title="基本设置" style={{ marginBottom: 16 }}>
                        <Form.Item
                            label="保存时间"
                            name="retentionDays"
                            tooltip="超过保存时间的记录将被自动清理"
                        >
                            <Select
                                options={[
                                    { value: 1, label: "1天" },
                                    { value: 3, label: "3天" },
                                    { value: 5, label: "5天" },
                                    { value: 7, label: "7天" },
                                    { value: 30, label: "30天" },
                                    { value: -1, label: "永久保存" },
                                ]}
                            />
                        </Form.Item>

                        <Form.Item
                            label="最大记录数"
                            name="maxRecords"
                            tooltip="最多保存的记录数量"
                        >
                            <InputNumber
                                min={100}
                                max={10000}
                                step={100}
                                style={{ width: "100%" }}
                            />
                        </Form.Item>
                    </Card>

                    {/* 快捷键设置 */}
                    <Card title="快捷键设置" style={{ marginBottom: 16 }}>
                        <Form.Item label="显示/隐藏窗口">
                            <Space>
                                <Tag>Ctrl</Tag> + <Tag>Alt</Tag> + <Tag>V</Tag>
                            </Space>
                        </Form.Item>
                    </Card>

                    {/* 系统设置 */}
                    <Card title="系统设置">
                        <Form.Item
                            label="开机自启"
                            name="autoStart"
                            valuePropName="checked"
                            tooltip="Windows 启动时自动运行程序"
                        >
                            <Switch />
                        </Form.Item>

                        <Form.Item
                            label="关闭时最小化到托盘"
                            name="minimizeToTray"
                            valuePropName="checked"
                            tooltip="点击关闭按钮时最小化到系统托盘而不是退出"
                        >
                            <Switch />
                        </Form.Item>
                    </Card>
                </Form>

                {/* 危险操作 */}
                <Card
                    title="数据管理"
                    style={{ marginTop: 16, borderColor: "#ff4d4f" }}
                >
                    <Space>
                        <Button
                            danger
                            icon={<DeleteOutlined />}
                            onClick={handleClearAll}
                        >
                            清空所有记录
                        </Button>
                    </Space>
                </Card>
            </div>
        </Spin>
    );
}

export default SettingsPage;
