fn main() {
    // 链接C++静态库（项目根目录）
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let root_dir = std::path::Path::new(&manifest_dir).parent().unwrap().parent().unwrap();
    println!("cargo:rustc-link-search=native={}", root_dir.display());
    println!("cargo:rustc-link-lib=static=clipboard");

    // 链接C++标准库
    println!("cargo:rustc-link-lib=stdc++");

    // 链接Windows系统库
    println!("cargo:rustc-link-lib=user32");
    println!("cargo:rustc-link-lib=kernel32");
    println!("cargo:rustc-link-lib=gdi32");
    println!("cargo:rustc-link-lib=gdiplus");
    println!("cargo:rustc-link-lib=shell32");
    println!("cargo:rustc-link-lib=advapi32");

    tauri_build::build()
}
