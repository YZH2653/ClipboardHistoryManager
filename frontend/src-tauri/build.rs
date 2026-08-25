fn main() {
    // 链接C++静态库
    println!("cargo:rustc-link-search=native=../../");
    println!("cargo:rustc-link-lib=static=clipboard");

    // 链接Windows系统库
    println!("cargo:rustc-link-lib=user32");
    println!("cargo:rustc-link-lib=kernel32");
    println!("cargo:rustc-link-lib=gdi32");
    println!("cargo:rustc-link-lib=gdiplus");
    println!("cargo:rustc-link-lib=shell32");
    println!("cargo:rustc-link-lib=advapi32");

    tauri_build::build()
}
