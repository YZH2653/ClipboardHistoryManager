import { Form, Select, Switch, Card, Divider, InputNumber } from "antd";

function SettingsPage() {
    return (
        <div>
            <h2>设置</h2>
            <Divider />

            <Card title="基本设置" style={{ marginBottom: 16 }}>
                <Form layout="vertical">
                    <Form.Item label="保存时间">
                        <Select
                            defaultValue="3"
                            options={[
                                { value: "3", label: "3天" },
                                { value: "5", label: "5天" },
                                { value: "7", label: "7天" },
                                { value: "30", label: "30天" },
                                { value: "-1", label: "永久" },
                            ]}
                        />
                    </Form.Item>

                    <Form.Item label="最大记录数">
                        <InputNumber
                            defaultValue={1000}
                            min={100}
                            max={10000}
                            style={{ width: "100%" }}
                        />
                    </Form.Item>
                </Form>
            </Card>

            <Card title="快捷键" style={{ marginBottom: 16 }}>
                <Form layout="vertical">
                    <Form.Item label="显示/隐藏窗口">
                        <Select
                            defaultValue="ctrl+alt+v"
                            options={[
                                { value: "ctrl+alt+v", label: "Ctrl + Alt + V" },
                                { value: "ctrl+shift+v", label: "Ctrl + Shift + V" },
                                { value: "alt+v", label: "Alt + V" },
                            ]}
                        />
                    </Form.Item>

                    <Form.Item label="快速复制">
                        <Select
                            defaultValue="ctrl+alt+c"
                            options={[
                                { value: "ctrl+alt+c", label: "Ctrl + Alt + C" },
                                { value: "ctrl+shift+c", label: "Ctrl + Shift + C" },
                                { value: "alt+c", label: "Alt + C" },
                            ]}
                        />
                    </Form.Item>
                </Form>
            </Card>

            <Card title="系统设置">
                <Form layout="vertical">
                    <Form.Item label="开机自启">
                        <Switch />
                    </Form.Item>

                    <Form.Item label="关闭时最小化到托盘">
                        <Switch defaultChecked />
                    </Form.Item>
                </Form>
            </Card>
        </div>
    );
}

export default SettingsPage;
