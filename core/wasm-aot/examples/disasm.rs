use iced_x86::{Decoder, DecoderOptions, Formatter, IntelFormatter};

fn parse(text: &str) -> u64 {
    u64::from_str_radix(text.trim_start_matches("0x"), 16).expect("hex address")
}

fn main() {
    let arguments: Vec<String> = std::env::args().collect();
    let bytes = std::fs::read(&arguments[1]).expect("readable file");
    let image = anyps5_wasm_aot::elf::load("input", &bytes).expect("ELF file");
    let (start, end) = (parse(&arguments[2]), parse(&arguments[3]));
    let code = image.bytes_at(start).expect("address inside a segment");
    let mut decoder = Decoder::with_ip(64, code, start, DecoderOptions::NONE);
    let mut formatter = IntelFormatter::new();
    while decoder.can_decode() {
        let instruction = decoder.decode();
        if instruction.ip() >= end {
            break;
        }
        let mut text = String::new();
        formatter.format(&instruction, &mut text);
        println!("{:#010x}  {text}", instruction.ip());
    }
}
