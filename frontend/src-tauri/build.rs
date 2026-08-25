fn main() {
    // 编译C++源文件
    cc::Build::new()
        .cpp(true)
        .file("../../ffi_bridge.cpp")
        .file("../../ClipboardManager.cpp")
        .file("../../Storage.cpp")
        .define("UNICODE", None)
        .define("_UNICODE", None)
        .warnings(false)
        .compile("clipboard_cpp");

    // 编译C源文件（sqlite3）
    cc::Build::new()
        .cpp(false)
        .file("../../sqlite3.c")
        .warnings(false)
        .compile("clipboard_c");

    // 链接Windows系统库
    println!("cargo:rustc-link-lib=user32");
    println!("cargo:rustc-link-lib=kernel32");
    println!("cargo:rustc-link-lib=gdi32");
    println!("cargo:rustc-link-lib=gdiplus");
    println!("cargo:rustc-link-lib=shell32");
    println!("cargo:rustc-link-lib=advapi32");

    tauri_build::build()
}
