use std::path::PathBuf;

use anyps5_wasm_aot::{Error, TranslateOptions, load_images, program::Program, translate};

#[test]
fn dreaming_sarah_translates_without_unsupported_instructions() {
    let Some(directory) = std::env::var_os("ANYPS5_DREAMING_SARAH_DIR").map(PathBuf::from) else {
        eprintln!("ANYPS5_DREAMING_SARAH_DIR is not set; skipped");
        return;
    };
    let main = std::fs::read(directory.join("app.elf")).unwrap();
    let mut resolve = |needed: &str| -> anyps5_wasm_aot::Result<Option<(String, Vec<u8>)>> {
        let path = directory.join(needed.trim_start_matches("$ORIGIN/"));
        let bytes = std::fs::read(&path).map_err(|error| Error::new(format!("{}: {error}", path.display())))?;
        Ok(Some((needed.to_string(), bytes)))
    };
    let images = load_images("app.elf", &main, &mut resolve).unwrap();
    assert_eq!(images.len(), 2, "app.elf and the converted libc.prx");
    let output = translate(Program::bind(images).unwrap(), &TranslateOptions::default()).unwrap();
    assert!(output.report.unsupported.is_empty());

    let mut config = wasmtime::Config::new();
    config.wasm_memory64(true).wasm_threads(true).shared_memory(true).wasm_tail_call(true).wasm_relaxed_simd(true);
    let engine = wasmtime::Engine::new(&config).unwrap();
    wasmtime::Module::new(&engine, &output.wasm).unwrap();
}
