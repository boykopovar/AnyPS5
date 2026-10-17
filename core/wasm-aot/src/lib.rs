pub mod abi;
mod codegen;
pub mod discover;
pub mod elf;
pub mod flags;
pub mod image;
pub mod lsda;
pub mod meta;
pub mod program;
pub mod report;
pub mod runtime_patch;

use std::fmt;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error(String);

impl Error {
    pub fn new(message: impl Into<String>) -> Self {
        Self(message.into())
    }
}

impl fmt::Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.0)
    }
}

impl std::error::Error for Error {}

pub type Result<T> = std::result::Result<T, Error>;

#[derive(Clone, Debug)]
pub struct TranslateOptions {
    pub shared_memory: bool,
    pub allow_unsupported: bool,
    pub flags_at_boundaries: bool,
}

impl Default for TranslateOptions {
    fn default() -> Self {
        Self { shared_memory: true, allow_unsupported: false, flags_at_boundaries: false }
    }
}

pub struct Output {
    pub wasm: Vec<u8>,
    pub report: report::Report,
}

pub type Resolver<'a> = dyn FnMut(&str) -> Result<Option<(String, Vec<u8>)>> + 'a;

pub fn load_images(name: &str, bytes: &[u8], resolve: &mut Resolver<'_>) -> Result<Vec<image::Image>> {
    let mut images = vec![elf::load(name, bytes)?];
    let mut loaded: Vec<String> = Vec::new();
    let mut next = 0;
    while next < images.len() {
        let needed = images[next].needed.clone();
        next += 1;
        for dependency in needed {
            if !dependency.starts_with("$ORIGIN/") || loaded.contains(&dependency) {
                continue;
            }
            loaded.push(dependency.clone());
            if let Some((module_name, module_bytes)) = resolve(&dependency)? {
                images.push(elf::load(&module_name, &module_bytes)?);
            }
        }
    }
    Ok(images)
}

pub fn translate_raw(code: &[u8], vaddr: u64, options: &TranslateOptions) -> Result<Output> {
    translate(program::Program::bind(vec![image::Image::raw(code, vaddr)])?, options)
}

pub fn translate(program: program::Program, options: &TranslateOptions) -> Result<Output> {
    let discovery = discover::discover(&program)?;
    let mut liveness = Vec::with_capacity(discovery.functions.len());
    for function in &discovery.functions {
        let (successors, exits) = flags::edges(function);
        liveness.push(flags::analyze(function, &successors, &exits, options.flags_at_boundaries)?);
    }
    let (wasm, unsupported) = codegen::emit_module(&program, &discovery, &liveness, options)?;
    validate(&wasm)?;
    let report = report::Report::new(&program, &discovery, unsupported, wasm.len());
    Ok(Output { wasm, report })
}

fn validate(wasm: &[u8]) -> Result<()> {
    let features = wasmparser::WasmFeatures::default()
        | wasmparser::WasmFeatures::MEMORY64
        | wasmparser::WasmFeatures::THREADS
        | wasmparser::WasmFeatures::TAIL_CALL
        | wasmparser::WasmFeatures::SIMD
        | wasmparser::WasmFeatures::RELAXED_SIMD;
    wasmparser::Validator::new_with_features(features).validate_all(wasm).map(|_| ()).map_err(|error| Error::new(format!("generated wasm is invalid: {error}")))
}
