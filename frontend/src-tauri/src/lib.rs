mod commands;

use commands::DbState;
use tauri::Manager;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .manage(DbState::new())
        .setup(|app| {
            let state = app.state::<DbState>();
            if let Err(e) = commands::init_db(&state) {
                eprintln!("初始化数据库失败: {}", e);
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::get_records,
            commands::get_record_content,
            commands::delete_record,
            commands::toggle_pin,
            commands::batch_delete_records,
            commands::get_settings,
            commands::save_settings,
            commands::clear_all_records,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
