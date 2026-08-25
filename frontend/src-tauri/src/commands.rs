use chrono::DateTime;
use rusqlite::{Connection, params};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::sync::Mutex;
use tauri::State;

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
    #[serde(rename = "hotkeyToggle")]
    pub hotkey_toggle: String,
    #[serde(rename = "hotkeyCopy")]
    pub hotkey_copy: String,
}

// 数据库连接状态
pub struct DbState {
    pub conn: Mutex<Option<Connection>>,
}

impl DbState {
    pub fn new() -> Self {
        Self {
            conn: Mutex::new(None),
        }
    }
}

// 获取数据库路径
fn get_db_path() -> PathBuf {
    let exe_path = std::env::current_exe().unwrap_or_else(|_| PathBuf::from("."));
    let default_dir = PathBuf::from(".");
    let exe_dir = exe_path.parent().unwrap_or(default_dir.as_path());
    exe_dir.join("clips").join("history.db")
}

// 获取exe目录
fn get_exe_dir() -> PathBuf {
    let exe_path = std::env::current_exe().unwrap_or_else(|_| PathBuf::from("."));
    let default_dir = PathBuf::from(".");
    exe_path.parent().unwrap_or(default_dir.as_path()).to_path_buf()
}

// 初始化数据库连接
pub fn init_db(state: &State<DbState>) -> Result<(), String> {
    let db_path = get_db_path();

    // 确保目录存在
    if let Some(parent) = db_path.parent() {
        std::fs::create_dir_all(parent).map_err(|e| format!("创建目录失败: {}", e))?;
    }

    let conn = Connection::open(&db_path).map_err(|e| format!("打开数据库失败: {}", e))?;

    // 创建记录表
    conn.execute(
        "CREATE TABLE IF NOT EXISTS records (
            id INTEGER PRIMARY KEY,
            type INTEGER,
            content TEXT,
            preview TEXT,
            filePath TEXT,
            timestamp INTEGER,
            isPinned INTEGER
        )",
        [],
    )
    .map_err(|e| format!("创建记录表失败: {}", e))?;

    // 创建设置表
    conn.execute(
        "CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value INTEGER
        )",
        [],
    )
    .map_err(|e| format!("创建设置表失败: {}", e))?;

    let mut conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    *conn_guard = Some(conn);

    Ok(())
}

// 时间戳转ISO字符串
fn timestamp_to_iso(ts: i64) -> String {
    match DateTime::from_timestamp_millis(ts) {
        Some(dt) => dt.to_rfc3339(),
        None => chrono::Utc::now().to_rfc3339(),
    }
}

// 获取所有记录
#[tauri::command]
pub fn get_records(state: State<DbState>) -> Result<Vec<ClipRecord>, String> {
    let conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    let conn = conn_guard.as_ref().ok_or("数据库未初始化")?;

    let mut stmt = conn
        .prepare("SELECT id, type, content, preview, filePath, timestamp, isPinned FROM records ORDER BY timestamp DESC")
        .map_err(|e| format!("准备查询失败: {}", e))?;

    let records = stmt
        .query_map([], |row| {
            let type_int: i64 = row.get(1)?;
            let record_type = if type_int == 0 { "text" } else { "image" }.to_string();
            let ts: i64 = row.get(5)?;
            let is_pinned_int: i64 = row.get(6)?;

            Ok(ClipRecord {
                id: row.get(0)?,
                record_type,
                content: row.get(2)?,
                preview: row.get(3)?,
                file_path: row.get(4)?,
                timestamp: timestamp_to_iso(ts),
                is_pinned: is_pinned_int != 0,
            })
        })
        .map_err(|e| format!("查询记录失败: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    Ok(records)
}

// 获取记录内容（用于复制）
#[tauri::command]
pub fn get_record_content(state: State<DbState>, id: i64) -> Result<String, String> {
    let conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    let conn = conn_guard.as_ref().ok_or("数据库未初始化")?;

    let content: String = conn
        .query_row(
            "SELECT content FROM records WHERE id = ?",
            params![id],
            |row| row.get(0),
        )
        .map_err(|e| format!("查询记录失败: {}", e))?;

    Ok(content)
}

// 删除记录
#[tauri::command]
pub fn delete_record(state: State<DbState>, id: i64) -> Result<(), String> {
    let conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    let conn = conn_guard.as_ref().ok_or("数据库未初始化")?;

    // 先获取文件路径（如果有）
    let file_path: Option<String> = conn
        .query_row(
            "SELECT filePath FROM records WHERE id = ?",
            params![id],
            |row| row.get(0),
        )
        .ok();

    // 删除数据库记录
    conn.execute("DELETE FROM records WHERE id = ?", params![id])
        .map_err(|e| format!("删除记录失败: {}", e))?;

    // 删除关联的图片文件
    if let Some(path) = file_path {
        if !path.is_empty() {
            let full_path = get_exe_dir().join(&path);
            let _ = std::fs::remove_file(full_path);
        }
    }

    Ok(())
}

// 切换置顶状态
#[tauri::command]
pub fn toggle_pin(state: State<DbState>, id: i64) -> Result<bool, String> {
    let conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    let conn = conn_guard.as_ref().ok_or("数据库未初始化")?;

    let current: i64 = conn
        .query_row(
            "SELECT isPinned FROM records WHERE id = ?",
            params![id],
            |row| row.get(0),
        )
        .map_err(|e| format!("查询记录失败: {}", e))?;

    let new_value = if current == 0 { 1 } else { 0 };
    conn.execute(
        "UPDATE records SET isPinned = ? WHERE id = ?",
        params![new_value, id],
    )
    .map_err(|e| format!("更新置顶状态失败: {}", e))?;

    Ok(new_value != 0)
}

// 批量删除记录
#[tauri::command]
pub fn batch_delete_records(state: State<DbState>, ids: Vec<i64>) -> Result<(), String> {
    let conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    let conn = conn_guard.as_ref().ok_or("数据库未初始化")?;
    let exe_dir = get_exe_dir();

    for id in &ids {
        // 获取文件路径
        let file_path: Option<String> = conn
            .query_row(
                "SELECT filePath FROM records WHERE id = ?",
                params![id],
                |row| row.get(0),
            )
            .ok();

        conn.execute("DELETE FROM records WHERE id = ?", params![id])
            .map_err(|e| format!("删除记录 {} 失败: {}", id, e))?;

        if let Some(path) = file_path {
            if !path.is_empty() {
                let full_path = exe_dir.join(&path);
                let _ = std::fs::remove_file(full_path);
            }
        }
    }

    Ok(())
}

// 获取设置
#[tauri::command]
pub fn get_settings(state: State<DbState>) -> Result<Settings, String> {
    let conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    let conn = conn_guard.as_ref().ok_or("数据库未初始化")?;

    let get_int = |key: &str, default: i64| -> i64 {
        conn.query_row(
            "SELECT value FROM settings WHERE key = ?",
            params![key],
            |row| row.get(0),
        )
        .unwrap_or(default)
    };

    Ok(Settings {
        retention_days: get_int("retentionDays", 3),
        max_records: get_int("maxRecords", 1000),
        auto_start: get_int("autoStart", 0) != 0,
        minimize_to_tray: get_int("minimizeToTray", 1) != 0,
        hotkey_toggle: "ctrl+alt+v".to_string(),
        hotkey_copy: "ctrl+alt+c".to_string(),
    })
}

// 保存设置
#[tauri::command]
pub fn save_settings(state: State<DbState>, settings: Settings) -> Result<(), String> {
    let conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    let conn = conn_guard.as_ref().ok_or("数据库未初始化")?;

    let set_int = |key: &str, value: i64| -> Result<(), String> {
        conn.execute(
            "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)",
            params![key, value],
        )
        .map_err(|e| format!("保存设置 {} 失败: {}", key, e))?;
        Ok(())
    };

    set_int("retentionDays", settings.retention_days)?;
    set_int("maxRecords", settings.max_records)?;
    set_int("autoStart", if settings.auto_start { 1 } else { 0 })?;
    set_int("minimizeToTray", if settings.minimize_to_tray { 1 } else { 0 })?;

    Ok(())
}

// 清空所有记录
#[tauri::command]
pub fn clear_all_records(state: State<DbState>) -> Result<(), String> {
    let conn_guard = state.conn.lock().map_err(|e| format!("获取锁失败: {}", e))?;
    let conn = conn_guard.as_ref().ok_or("数据库未初始化")?;

    // 获取所有图片文件路径
    let mut stmt = conn
        .prepare("SELECT filePath FROM records WHERE filePath IS NOT NULL AND filePath != ''")
        .map_err(|e| format!("准备查询失败: {}", e))?;

    let paths: Vec<String> = stmt
        .query_map([], |row| row.get(0))
        .map_err(|e| format!("查询失败: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    // 删除图片文件
    let exe_dir = get_exe_dir();
    for path in &paths {
        let full_path = exe_dir.join(path);
        let _ = std::fs::remove_file(full_path);
    }

    // 清空记录表
    conn.execute("DELETE FROM records", [])
        .map_err(|e| format!("清空记录失败: {}", e))?;

    Ok(())
}
