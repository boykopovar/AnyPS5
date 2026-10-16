mod common;

use std::path::PathBuf;

use anyps5_wasm_aot::{Error, TranslateOptions, load_images, program::Program, translate};
use common::host::{HEAP_BASE, Machine, NidTable};

const MEMCMP: &str = "DfivPArhucg#guest";
const STRCMP: &str = "Ovb2dSJOAuE#guest";
const STRLEN: &str = "j4ViWNHEgww#guest";

fn machine() -> Option<Machine> {
    let directory = std::env::var_os("ANYPS5_DREAMING_SARAH_DIR").map(PathBuf::from)?;
    let main = std::fs::read(directory.join("app.elf")).unwrap();
    let mut resolve = |needed: &str| -> anyps5_wasm_aot::Result<Option<(String, Vec<u8>)>> {
        let path = directory.join(needed.trim_start_matches("$ORIGIN/"));
        let bytes = std::fs::read(&path).map_err(|error| Error::new(format!("{}: {error}", path.display())))?;
        Ok(Some((needed.to_string(), bytes)))
    };
    let images = load_images("app.elf", &main, &mut resolve).unwrap();
    let options = TranslateOptions { shared_memory: false, ..TranslateOptions::default() };
    let output = translate(Program::bind(images).unwrap(), &options).unwrap();
    Some(Machine::new(&output.wasm, NidTable::default()))
}

fn place(machine: &mut Machine, offset: u64, bytes: &[u8]) -> u64 {
    let address = HEAP_BASE + offset;
    machine.write(address, bytes);
    address
}

fn sign(value: u64) -> i32 {
    (value as i32).signum()
}

#[test]
fn guest_string_functions_order_like_the_c_library() {
    let Some(mut machine) = machine() else {
        eprintln!("ANYPS5_DREAMING_SARAH_DIR is not set; skipped");
        return;
    };
    let words: [&[u8]; 8] = [b"inverse", b"waterbg", b"water", b"outline", b"warp", b"vignette", b"noise", b"blurhorizontal"];
    let mut failures = Vec::new();
    for (index, left) in words.iter().enumerate() {
        for (other, right) in words.iter().enumerate() {
            for (shift_left, shift_right) in [(0u64, 0u64), (1, 3), (5, 2), (7, 0)] {
                let mut left_bytes = left.to_vec();
                left_bytes.push(0);
                let mut right_bytes = right.to_vec();
                right_bytes.push(0);
                let a = place(&mut machine, 0x1000 * index as u64 + shift_left, &left_bytes);
                let b = place(&mut machine, 0x80000 + 0x1000 * other as u64 + shift_right, &right_bytes);
                let length = left.len().min(right.len()) as u64;
                let expected = left[..length as usize].cmp(&right[..length as usize]) as i32;
                let memcmp = sign(machine.call_export(1, MEMCMP, &[a, b, length]).unwrap());
                if memcmp != expected {
                    failures.push(format!("memcmp({:?}+{shift_left}, {:?}+{shift_right}, {length}) = {memcmp}, expected {expected}", String::from_utf8_lossy(left), String::from_utf8_lossy(right)));
                }
                let strcmp = sign(machine.call_export(1, STRCMP, &[a, b]).unwrap());
                let expected = left.cmp(right) as i32;
                if strcmp != expected {
                    failures.push(format!("strcmp({:?}+{shift_left}, {:?}+{shift_right}) = {strcmp}, expected {expected}", String::from_utf8_lossy(left), String::from_utf8_lossy(right)));
                }
            }
        }
        let a = place(&mut machine, 0x40000 + 3, &[left.to_vec(), vec![0]].concat());
        let length = machine.call_export(1, STRLEN, &[a]).unwrap();
        if length != left.len() as u64 {
            failures.push(format!("strlen({:?}) = {length}", String::from_utf8_lossy(left)));
        }
    }
    assert!(failures.is_empty(), "{}", failures.join("\n"));
}
