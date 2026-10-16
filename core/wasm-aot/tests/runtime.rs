mod common;

use std::path::PathBuf;
use std::process::Command;

use anyps5_wasm_aot::{TranslateOptions, load_images, program::Program, translate};
use common::guest;

fn runtime() -> Option<PathBuf> {
    std::env::var_os("ANYPS5_WASM_RUNTIME_TEST").map(PathBuf::from)
}

fn program_wasm() -> PathBuf {
    let bytes = guest::build("program", &[("program.cpp", &[]), ("tls_initial_exec.cpp", &["-ftls-model=initial-exec"]), ("jump.S", &[])]);
    let images = load_images("program.so", &bytes, &mut |_| Ok(None)).unwrap();
    let output = translate(Program::bind(images).unwrap(), &TranslateOptions::default()).unwrap();
    let path = PathBuf::from(env!("CARGO_TARGET_TMPDIR")).join(format!("runtime-program-{}.wasm", std::process::id()));
    std::fs::write(&path, &output.wasm).unwrap();
    path
}

fn run(calls: &[&[&str]]) -> Vec<String> {
    let runtime = runtime().expect("ANYPS5_WASM_RUNTIME_TEST names the runtime test build");
    let wasm = program_wasm();
    let mut command = Command::new("node");
    command.arg(&runtime).arg(&wasm);
    for call in calls {
        command.arg("--call").args(*call);
    }
    let output = command.output().expect("node runs");
    let _ = std::fs::remove_file(&wasm);
    let stdout = String::from_utf8_lossy(&output.stdout).into_owned();
    assert!(output.status.success(), "runtime failed: {}\n{}", String::from_utf8_lossy(&output.stderr), stdout);
    stdout.lines().map(str::to_string).collect()
}

fn classify(value: i64) -> i64 {
    match value {
        0 => 3,
        1 => 14,
        2 => 15,
        3 => 92,
        4 => 65,
        5 => 35,
        6 => 89,
        7 => 79,
        _ => -1,
    }
}

#[test]
fn guest_programs_run_on_the_wasm_runtime() {
    if runtime().is_none() {
        eprintln!("skipped: set ANYPS5_WASM_RUNTIME_TEST to the anyps5-runtime-test.js build");
        return;
    }
    let switch = (-1..9).fold(0i64, |sum, value| sum * 3 + classify(value));
    let lines = run(&[
        &["run_initialized"],
        &["run_switch"],
        &["run_imports", "4"],
        &["run_callback", "5"],
        &["run_callback", "4"],
        &["run_vtable"],
        &["run_tls"],
        &["run_tls"],
        &["run_nonlocal"],
        &["run_guest_longjmp"],
    ]);
    let expected = [
        "run_initialized = 1000".to_string(),
        format!("run_switch = {switch}"),
        "print 4".to_string(),
        "run_imports = 15".to_string(),
        "run_callback = 15".to_string(),
        "run_callback = 8".to_string(),
        "run_vtable = 31".to_string(),
        "run_tls = 609".to_string(),
        "run_tls = 711".to_string(),
        "run_nonlocal = 77".to_string(),
        "run_guest_longjmp = 332".to_string(),
    ];
    assert_eq!(lines, expected);
}
