// FFI 绑定：Rust 调用 C++ 后端
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

// C 兼容记录结构
#[repr(C)]
pub struct FFIRecord {
    pub id: i64,
    pub record_type: i32,
    pub content: [u8; 10001],
    pub preview: [u8; 101],
    pub file_path: [u8; 512],
    pub timestamp: i64,
    pub is_pinned: bool,
}

// C 兼容设置结构
#[repr(C)]
pub struct FFISettings {
    pub retention_days: i32,
    pub max_records: i32,
    pub auto_start: bool,
    pub minimize_to_tray: bool,
}

// 外部 C 函数声明
extern "C" {
    fn FfiInitialize(root_dir_utf8: *const c_char) -> bool;
    fn FfiShutdown();
    fn FfiGetRecords(out_records: *mut FFIRecord, max_count: i32) -> i32;
    fn FfiCopyToClipboard(text_utf8: *const c_char) -> bool;
    fn FfiCopyRecord(id: i64) -> bool;
    fn FfiDeleteRecord(id: i64) -> bool;
    fn FfiTogglePin(id: i64) -> bool;
    fn FfiBatchDelete(ids: *const i64, count: i32) -> i32;
    fn FfiClearAll();
    fn FfiGetSettings(settings: *mut FFISettings);
    fn FfiSaveSettings(settings: *const FFISettings);
}

// 初始化
pub fn initialize(root_dir: &str) -> bool {
    let c_str = CString::new(root_dir).unwrap_or_default();
    unsafe { FfiInitialize(c_str.as_ptr()) }
}

// 关闭
pub fn shutdown() {
    unsafe { FfiShutdown(); }
}

// 获取所有记录
pub fn get_records() -> Vec<crate::commands::ClipRecord> {
    let mut buffer = vec![FFIRecord {
        id: 0,
        record_type: 0,
        content: [0u8; 10001],
        preview: [0u8; 101],
        file_path: [0u8; 512],
        timestamp: 0,
        is_pinned: false,
    }; 2000];

    let count = unsafe { FfiGetRecords(buffer.as_mut_ptr(), 2000) };

    let mut records = Vec::new();
    for i in 0..count as usize {
        let r = &buffer[i];
        let content = CStr::from_bytes_until_nul(&r.content)
            .unwrap_or_default()
            .to_string_lossy()
            .to_string();
        let preview = CStr::from_bytes_until_nul(&r.preview)
            .unwrap_or_default()
            .to_string_lossy()
            .to_string();
        let file_path = CStr::from_bytes_until_nul(&r.file_path)
            .unwrap_or_default()
            .to_string_lossy()
            .to_string();

        records.push(crate::commands::ClipRecord {
            id: r.id,
            record_type: if r.record_type == 0 {
                "text"
            } else {
                "image"
            }
            .to_string(),
            content,
            preview,
            file_path,
            timestamp: chrono::DateTime::from_timestamp_millis(r.timestamp)
                .unwrap_or_else(|| chrono::Utc::now())
                .to_rfc3339(),
            is_pinned: r.is_pinned,
        });
    }
    records
}

// 复制到剪贴板
pub fn copy_to_clipboard(text: &str) -> bool {
    let c_str = CString::new(text).unwrap_or_default();
    unsafe { FfiCopyToClipboard(c_str.as_ptr()) }
}

// 复制记录
pub fn copy_record(id: i64) -> bool {
    unsafe { FfiCopyRecord(id) }
}

// 删除记录
pub fn delete_record(id: i64) -> bool {
    unsafe { FfiDeleteRecord(id) }
}

// 切换置顶
pub fn toggle_pin(id: i64) -> bool {
    unsafe { FfiTogglePin(id) }
}

// 批量删除
pub fn batch_delete(ids: &[i64]) -> i32 {
    unsafe { FfiBatchDelete(ids.as_ptr(), ids.len() as i32) }
}

// 清空所有
pub fn clear_all() {
    unsafe { FfiClearAll(); }
}

// 获取设置
pub fn get_settings() -> crate::commands::Settings {
    let mut ffi_settings = FFISettings {
        retention_days: 3,
        max_records: 1000,
        auto_start: false,
        minimize_to_tray: true,
    };
    unsafe { FfiGetSettings(&mut ffi_settings); }
    crate::commands::Settings {
        retention_days: ffi_settings.retention_days as i64,
        max_records: ffi_settings.max_records as i64,
        auto_start: ffi_settings.auto_start,
        minimize_to_tray: ffi_settings.minimize_to_tray,
        hotkey_toggle: "ctrl+alt+v".to_string(),
        hotkey_copy: "ctrl+alt+c".to_string(),
    }
}

// 保存设置
pub fn save_settings(settings: &crate::commands::Settings) {
    let ffi_settings = FFISettings {
        retention_days: settings.retention_days as i32,
        max_records: settings.max_records as i32,
        auto_start: settings.auto_start,
        minimize_to_tray: settings.minimize_to_tray,
    };
    unsafe { FfiSaveSettings(&ffi_settings); }
}
