use serde::{Deserialize, Serialize};

// 剪贴板记录
#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ClipRecord {
    pub id: i64,
    #[serde(rename = "type")]
    pub record_type: String,
    pub content: String,
    pub preview: String,
    #[serde(rename = "filePath")]
    pub file_path: String,
    pub timestamp: String,
    #[serde(rename = "isPinned")]
    pub is_pinned: bool,
}

// 设置
#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Settings {
    #[serde(rename = "retentionDays")]
    pub retention_days: i64,
    #[serde(rename = "maxRecords")]
    pub max_records: i64,
    #[serde(rename = "autoStart")]
    pub auto_start: bool,
    #[serde(rename = "minimizeToTray")]
    pub minimize_to_tray: bool,
}

// 获取所有记录（通过C++ FFI）
#[tauri::command]
pub fn get_records() -> Result<Vec<ClipRecord>, String> {
    Ok(crate::ffi::get_records())
}

// 获取记录内容（用于复制）
#[tauri::command]
pub fn get_record_content(id: i64) -> Result<String, String> {
    let records = crate::ffi::get_records();
    for r in &records {
        if r.id == id {
            return Ok(r.content.clone());
        }
    }
    Err("记录不存在".to_string())
}

// 删除记录
#[tauri::command]
pub fn delete_record(id: i64) -> Result<(), String> {
    if crate::ffi::delete_record(id) {
        Ok(())
    } else {
        Err("删除失败".to_string())
    }
}

// 切换置顶
#[tauri::command]
pub fn toggle_pin(id: i64) -> Result<bool, String> {
    Ok(crate::ffi::toggle_pin(id))
}

// 批量删除
#[tauri::command]
pub fn batch_delete_records(ids: Vec<i64>) -> Result<(), String> {
    crate::ffi::batch_delete(&ids);
    Ok(())
}

// 获取设置
#[tauri::command]
pub fn get_settings() -> Result<Settings, String> {
    Ok(crate::ffi::get_settings())
}

// 保存设置
#[tauri::command]
pub fn save_settings(settings: Settings) -> Result<(), String> {
    crate::ffi::save_settings(&settings);
    Ok(())
}

// 清空所有记录
#[tauri::command]
pub fn clear_all_records() -> Result<(), String> {
    crate::ffi::clear_all();
    Ok(())
}
