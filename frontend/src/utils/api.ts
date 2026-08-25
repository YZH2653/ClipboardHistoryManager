import { invoke } from "@tauri-apps/api/core";
import type { ClipRecord, Settings } from "../types";

// 获取所有记录
export async function getRecords(): Promise<ClipRecord[]> {
    return invoke("get_records");
}

// 复制记录到剪贴板
export async function copyRecord(id: number): Promise<void> {
    return invoke("copy_record", { id });
}

// 删除记录
export async function deleteRecord(id: number): Promise<void> {
    return invoke("delete_record", { id });
}

// 切换置顶状态
export async function togglePin(id: number): Promise<void> {
    return invoke("toggle_pin", { id });
}

// 获取设置
export async function getSettings(): Promise<Settings> {
    return invoke("get_settings");
}

// 保存设置
export async function saveSettings(settings: Settings): Promise<void> {
    return invoke("save_settings", { settings });
}
