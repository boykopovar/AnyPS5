use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::OnceLock;

use super::{DATA_SIZE, RSP, SENTINEL, STACK_SIZE, State};

fn oracle() -> &'static Path {
    static BINARY: OnceLock<PathBuf> = OnceLock::new();
    BINARY.get_or_init(|| {
        let source = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/oracle");
        let binary = Path::new(env!("CARGO_TARGET_TMPDIR")).join(format!("x86-oracle-{}", std::process::id()));
        let status = Command::new("clang")
            .args(["-arch", "x86_64", "-O1", "-o"])
            .arg(&binary)
            .arg(source.join("native.c"))
            .arg(source.join("stub.S"))
            .status()
            .expect("clang runs");
        assert!(status.success(), "building the native oracle failed");
        binary
    })
}

pub fn run(cases: &[(&[u8], &State)]) -> Vec<State> {
    let mut input = Vec::new();
    for (code, state) in cases {
        input.extend_from_slice(&(code.len() as u32).to_le_bytes());
        input.extend_from_slice(code);
        let mut gpr = state.gpr;
        gpr[RSP] -= 8;
        let mut stack = state.stack.clone();
        let offset = (gpr[RSP] - super::STACK_BASE) as usize;
        stack[offset..offset + 8].copy_from_slice(&SENTINEL.to_le_bytes());
        for value in gpr {
            input.extend_from_slice(&value.to_le_bytes());
        }
        input.extend_from_slice(&state.rflags.to_le_bytes());
        for register in &state.ymm {
            input.extend_from_slice(register);
        }
        input.extend_from_slice(&state.data);
        input.extend_from_slice(&stack);
    }
    let directory = Path::new(env!("CARGO_TARGET_TMPDIR"));
    let unique = format!("{}-{:?}", std::process::id(), std::thread::current().id()).replace(['(', ')'], "");
    let input_path = directory.join(format!("oracle-in-{unique}"));
    let output_path = directory.join(format!("oracle-out-{unique}"));
    std::fs::write(&input_path, &input).unwrap();
    let result = Command::new(oracle()).arg(&input_path).arg(&output_path).output().unwrap();
    assert!(result.status.success(), "native oracle failed: {:?} {}", result.status, String::from_utf8_lossy(&result.stderr));
    let output = std::fs::read(&output_path).unwrap();
    let _ = std::fs::remove_file(&input_path);
    let _ = std::fs::remove_file(&output_path);
    let record = 0x80 + 8 + 0x200 + DATA_SIZE + STACK_SIZE;
    assert_eq!(output.len(), record * cases.len(), "native oracle output size");
    output
        .chunks(record)
        .map(|chunk| {
            let mut state = State::default();
            for (index, value) in state.gpr.iter_mut().enumerate() {
                *value = u64::from_le_bytes(chunk[index * 8..index * 8 + 8].try_into().unwrap());
            }
            state.rflags = u64::from_le_bytes(chunk[0x80..0x88].try_into().unwrap());
            for (index, register) in state.ymm.iter_mut().enumerate() {
                register.copy_from_slice(&chunk[0x88 + index * 32..0x88 + index * 32 + 32]);
            }
            let data_start = 0x88 + 0x200;
            state.data = chunk[data_start..data_start + DATA_SIZE].to_vec();
            state.stack = chunk[data_start + DATA_SIZE..].to_vec();
            state
        })
        .collect()
}
