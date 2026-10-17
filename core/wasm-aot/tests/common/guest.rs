use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::{Mutex, OnceLock};

fn rust_lld() -> PathBuf {
    let sysroot = Command::new("rustc").args(["--print", "sysroot"]).output().expect("rustc runs");
    let sysroot = PathBuf::from(String::from_utf8(sysroot.stdout).unwrap().trim());
    let host = Command::new("rustc").arg("-vV").output().expect("rustc runs");
    let host = String::from_utf8(host.stdout).unwrap();
    let triple = host.lines().find_map(|line| line.strip_prefix("host: ")).expect("host triple").to_string();
    sysroot.join("lib/rustlib").join(triple).join("bin/rust-lld")
}

pub fn build(name: &str, sources: &[(&str, &[&str])]) -> Vec<u8> {
    build_linked(name, sources, &[])
}

pub fn build_linked(name: &str, sources: &[(&str, &[&str])], link_flags: &[&str]) -> Vec<u8> {
    static BUILT: OnceLock<Mutex<HashMap<String, Vec<u8>>>> = OnceLock::new();
    let mut built = BUILT.get_or_init(Default::default).lock().unwrap();
    if let Some(bytes) = built.get(name) {
        return bytes.clone();
    }
    let bytes = compile(name, sources, link_flags);
    built.insert(name.to_string(), bytes.clone());
    bytes
}

fn compile(name: &str, sources: &[(&str, &[&str])], link_flags: &[&str]) -> Vec<u8> {
    let source_directory = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/elf");
    let directory = Path::new(env!("CARGO_TARGET_TMPDIR"));
    let unique = format!("{name}-{}", std::process::id());
    let mut objects = Vec::new();
    for (index, (file, flags)) in sources.iter().enumerate() {
        let source = source_directory.join(file);
        let object = directory.join(format!("{unique}-{index}.o"));
        let compiler = if file.ends_with(".cpp") { "clang++" } else { "clang" };
        let status = Command::new(compiler)
            .args(["--target=x86_64-unknown-linux-gnu", "-O2", "-fPIC", "-fno-stack-protector", "-fno-builtin", "-c"])
            .args(*flags)
            .arg(&source)
            .arg("-o")
            .arg(&object)
            .status()
            .expect("clang runs");
        assert!(status.success(), "compiling {} failed", source.display());
        objects.push(object);
    }
    let library = directory.join(format!("{unique}.so"));
    let status = Command::new(rust_lld())
        .args(["-flavor", "gnu", "-shared", "--hash-style=both", "-z", "now", "--eh-frame-hdr"])
        .args(link_flags)
        .arg("-o")
        .arg(&library)
        .args(&objects)
        .status()
        .expect("rust-lld runs");
    assert!(status.success(), "linking {name}.so failed");
    let bytes = std::fs::read(&library).unwrap();
    for object in objects {
        let _ = std::fs::remove_file(object);
    }
    let _ = std::fs::remove_file(&library);
    bytes
}
