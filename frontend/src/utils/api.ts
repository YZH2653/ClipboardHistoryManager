import { invoke } from "@tauri-apps/api/core";
import type { ClipRecord, Settings } from "../types";

// 获取所有记录
export async function getRecords(): Promise<ClipRecord[]> {
    return invoke("get_records");
}

// 获取记录内容（用于复制）
export async function getRecordContent(id: number): Promise<string> {
    return invoke("get_record_content", { id });
}

// 删除记录
export async function deleteRecord(id: number): Promise<void> {
    return invoke("delete_record", { id });
}

// 切换置顶状态
export async function togglePin(id: number): Promise<boolean> {
    return invoke("toggle_pin", { id });
}

// 批量删除记录
export async function batchDeleteRecords(ids: number[]): Promise<void> {
    return invoke("batch_delete_records", { ids });
}

// 获取设置
export async function getSettings(): Promise<Settings> {
    return invoke("get_settings");
}

// 保存设置
export async function saveSettings(settings: Settings): Promise<void> {
    return invoke("save_settings", { settings });
}

// 清空所有记录
export async function clearAllRecords(): Promise<void> {
    return invoke("clear_all_records");
}
