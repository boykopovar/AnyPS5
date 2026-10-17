use std::path::{Path, PathBuf};
use std::process::ExitCode;

use anyps5_wasm_aot::{Error, Result, TranslateOptions, abi, load_images, program::Program, translate};

const USAGE: &str =
    "usage: anyps5-wasm-aot <app.elf> -o <game.wasm> [--report <report.json>] [--header <aps5_wasm_abi.h>] [--allow-unsupported] [--unshared-memory]";

struct Arguments {
    input: PathBuf,
    output: PathBuf,
    report: Option<PathBuf>,
    header: Option<PathBuf>,
    options: TranslateOptions,
}

fn parse(mut arguments: impl Iterator<Item = String>) -> Result<Arguments> {
    let mut input = None;
    let mut output = None;
    let mut report = None;
    let mut header = None;
    let mut options = TranslateOptions::default();
    while let Some(argument) = arguments.next() {
        let mut value = |name: &str| arguments.next().map(PathBuf::from).ok_or_else(|| Error::new(format!("{name} needs a value")));
        match argument.as_str() {
            "-o" => output = Some(value("-o")?),
            "--report" => report = Some(value("--report")?),
            "--header" => header = Some(value("--header")?),
            "--allow-unsupported" => options.allow_unsupported = true,
            "--unshared-memory" => options.shared_memory = false,
            "-h" | "--help" => return Err(Error::new(USAGE)),
            _ if argument.starts_with('-') => {
                return Err(Error::new(format!("unknown option {argument}\n{USAGE}")));
            }
            _ if input.is_none() => input = Some(PathBuf::from(argument)),
            _ => {
                return Err(Error::new(format!("unexpected argument {argument}\n{USAGE}")));
            }
        }
    }
    Ok(Arguments { input: input.ok_or_else(|| Error::new(USAGE))?, output: output.ok_or_else(|| Error::new(USAGE))?, report, header, options })
}

fn read(path: &Path) -> Result<Vec<u8>> {
    std::fs::read(path).map_err(|error| Error::new(format!("{}: {error}", path.display())))
}

fn write(path: &Path, bytes: &[u8]) -> Result<()> {
    std::fs::write(path, bytes).map_err(|error| Error::new(format!("{}: {error}", path.display())))
}

fn run() -> Result<()> {
    let arguments = parse(std::env::args().skip(1))?;
    let origin = arguments.input.parent().map(Path::to_path_buf).unwrap_or_default();
    let name = arguments.input.file_name().map(|name| name.to_string_lossy().into_owned()).unwrap_or_default();
    let mut resolve = |needed: &str| -> Result<Option<(String, Vec<u8>)>> {
        let path = origin.join(needed.trim_start_matches("$ORIGIN/"));
        if !path.is_file() {
            return Err(Error::new(format!("guest module {needed} is missing at {}", path.display())));
        }
        Ok(Some((needed.trim_start_matches("$ORIGIN/").to_string(), read(&path)?)))
    };
    let images = load_images(&name, &read(&arguments.input)?, &mut resolve)?;
    let output = translate(Program::bind(images)?, &arguments.options)?;
    write(&arguments.output, &output.wasm)?;
    if let Some(path) = &arguments.report {
        write(path, output.report.to_json().as_bytes())?;
    }
    if let Some(path) = &arguments.header {
        write(path, abi::c_header().as_bytes())?;
    }
    let report = &output.report;
    eprintln!(
        "{}: {} functions, {} instructions, {} imports, {} bytes of wasm",
        arguments.output.display(),
        report.functions,
        report.instructions,
        report.imports,
        report.wasm_bytes
    );
    if !report.unsupported.is_empty() {
        eprintln!("{} unsupported instructions became traps:", report.unsupported.len());
        for (code, count, example) in report.unsupported_by_code().iter().take(40) {
            eprintln!("  {count:6}  {code:40} {example}");
        }
    }
    Ok(())
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("anyps5-wasm-aot: {error}");
            ExitCode::FAILURE
        }
    }
}
