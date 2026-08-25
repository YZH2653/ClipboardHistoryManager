// 剪贴板记录类型
export interface ClipRecord {
    id: number;
    type: "text" | "image";
    content: string;
    preview: string;
    filePath: string;
    timestamp: string;
    isPinned: boolean;
}

// 设置类型
export interface Settings {
    retentionDays: number;
    maxRecords: number;
    autoStart: boolean;
    minimizeToTray: boolean;
    hotkeyToggle: string;
    hotkeyCopy: string;
}

// 快捷键配置类型
export interface HotkeyConfig {
    id: string;
    name: string;
    modifiers: string[];
    key: string;
    enabled: boolean;
}
