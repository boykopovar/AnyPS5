use std::process::ExitCode;

use anyps5_wasm_aot::runtime_patch::route_guest_calls;

const USAGE: &str = "usage: anyps5-wasm-runtime-patch <runtime.wasm> -o <patched.wasm>";

fn run() -> Result<(), String> {
    let arguments: Vec<String> = std::env::args().skip(1).collect();
    let [input, flag, output] = arguments.as_slice() else { return Err(USAGE.into()) };
    if flag != "-o" {
        return Err(USAGE.into());
    }
    let bytes = std::fs::read(input).map_err(|error| format!("{input}: {error}"))?;
    let patched = route_guest_calls(&bytes).map_err(|error| format!("{input}: {error}"))?;
    std::fs::write(output, patched).map_err(|error| format!("{output}: {error}"))
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("anyps5-wasm-runtime-patch: {error}");
            ExitCode::FAILURE
        }
    }
}
