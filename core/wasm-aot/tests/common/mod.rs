#![allow(dead_code)]

pub mod guest;
pub mod host;
pub mod native;
pub mod oracle;
pub mod unwind;

use anyps5_wasm_aot::{TranslateOptions, translate_raw};
use iced_x86::code_asm::CodeAssembler;

pub const REGION: u64 = 0x4_0000_0000;
pub const CODE_BASE: u64 = REGION + 0x10000;
pub const DATA_BASE: u64 = REGION + 0x40000;
pub const DATA_SIZE: usize = 0x1000;
pub const STACK_BASE: u64 = REGION + 0x80000;
pub const STACK_SIZE: usize = 0x1000;
pub const INITIAL_RSP: u64 = STACK_BASE + STACK_SIZE as u64 - 0x200;
pub const SENTINEL: u64 = REGION + 0xF000;

pub const RAX: usize = 0;
pub const RCX: usize = 1;
pub const RDX: usize = 2;
pub const RBX: usize = 3;
pub const RSP: usize = 4;
pub const RBP: usize = 5;
pub const RSI: usize = 6;
pub const RDI: usize = 7;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct State {
    pub gpr: [u64; 16],
    pub rflags: u64,
    pub ymm: [[u8; 32]; 16],
    pub data: Vec<u8>,
    pub stack: Vec<u8>,
}

impl Default for State {
    fn default() -> Self {
        let mut gpr = [0; 16];
        gpr[RSP] = INITIAL_RSP;
        Self { gpr, rflags: 0x2, ymm: [[0; 32]; 16], data: vec![0; DATA_SIZE], stack: vec![0; STACK_SIZE] }
    }
}

impl State {
    pub fn random(seed: u64) -> Self {
        let mut rng = SplitMix(seed);
        let mut state = Self::default();
        for (index, value) in state.gpr.iter_mut().enumerate() {
            if index != RSP {
                *value = rng.next();
            }
        }
        state.rflags = 0x2 | (rng.next() & 0x8d5);
        for register in state.ymm.iter_mut() {
            for chunk in register.chunks_mut(8) {
                chunk.copy_from_slice(&rng.next().to_le_bytes());
            }
        }
        for chunk in state.data.chunks_mut(8) {
            chunk.copy_from_slice(&rng.next().to_le_bytes());
        }
        state
    }
}

impl State {
    pub fn random_floats(seed: u64, double: bool) -> Self {
        let mut state = Self::random(seed);
        let mut rng = SplitMix(seed ^ 0x5eed);
        let mut float_bytes = |bytes: &mut [u8]| {
            if double {
                for chunk in bytes.chunks_mut(8) {
                    let value = (rng.next() as i64 as f64) / (1u64 << 52) as f64;
                    chunk.copy_from_slice(&value.to_le_bytes());
                }
            } else {
                for chunk in bytes.chunks_mut(4) {
                    let value = (rng.next() as i32 as f32) / (1u32 << 20) as f32;
                    chunk.copy_from_slice(&value.to_le_bytes());
                }
            }
        };
        for register in state.ymm.iter_mut() {
            float_bytes(register);
        }
        float_bytes(&mut state.data);
        state
    }
}

pub struct SplitMix(pub u64);

impl SplitMix {
    pub fn next(&mut self) -> u64 {
        self.0 = self.0.wrapping_add(0x9e37_79b9_7f4a_7c15);
        let mut z = self.0;
        z = (z ^ (z >> 30)).wrapping_mul(0xbf58_476d_1ce4_e5b9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94d0_49bb_1331_11eb);
        z ^ (z >> 31)
    }
}

pub fn assemble(build: impl FnOnce(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) -> Vec<u8> {
    let mut assembler = CodeAssembler::new(64).unwrap();
    build(&mut assembler).unwrap();
    assembler.assemble(CODE_BASE).unwrap()
}

pub fn snippet_options() -> TranslateOptions {
    TranslateOptions { shared_memory: false, flags_at_boundaries: true, ..TranslateOptions::default() }
}

pub fn run_wasm(code: &[u8], initial: &State) -> State {
    let output = translate_raw(code, CODE_BASE, &snippet_options()).unwrap();
    let mut machine = host::Machine::new(&output.wasm, host::NidTable::default());
    machine.write(DATA_BASE, &initial.data);
    machine.write(STACK_BASE, &initial.stack);
    let mut gpr = initial.gpr;
    gpr[RSP] -= 8;
    machine.write(gpr[RSP], &SENTINEL.to_le_bytes());
    machine.set_state(&gpr, initial.rflags, &initial.ymm);
    machine.run(CODE_BASE, SENTINEL).unwrap();
    let (gpr, rflags, ymm) = machine.state();
    State { gpr, rflags, ymm, data: machine.read(DATA_BASE, DATA_SIZE), stack: machine.read(STACK_BASE, STACK_SIZE) }
}

pub fn assert_matches_oracle(code: &[u8], initial: &State, flags_mask: u64) {
    compare(code, initial, &oracle::run(code, initial), &run_wasm(code, initial), flags_mask);
}

pub fn compare(code: &[u8], initial: &State, expected: &State, actual: &State, flags_mask: u64) {
    let listing = disassemble(code);
    for index in 0..16 {
        assert_eq!(actual.gpr[index], expected.gpr[index], "gpr {index} differs (wasm left, oracle right) in\n{listing}initial gpr: {:x?}", initial.gpr);
    }
    assert_eq!(
        actual.rflags & flags_mask,
        expected.rflags & flags_mask,
        "rflags differ (mask {flags_mask:#x}, wasm left, oracle right) in\n{listing}initial gpr: {:x?} rflags {:#x}",
        initial.gpr,
        initial.rflags
    );
    for index in 0..16 {
        assert_eq!(actual.ymm[index], expected.ymm[index], "ymm{index} differs (wasm left, oracle right) in\n{listing}");
    }
    assert!(
        actual.data == expected.data,
        "data differs in\n{listing}first difference at {:#x}",
        first_difference(&actual.data, &expected.data) + DATA_BASE as usize
    );
    assert!(
        actual.stack == expected.stack,
        "stack differs in\n{listing}first difference at {:#x}",
        first_difference(&actual.stack, &expected.stack) + STACK_BASE as usize
    );
}

fn first_difference(left: &[u8], right: &[u8]) -> usize {
    left.iter().zip(right).position(|(a, b)| a != b).unwrap_or(0)
}

pub fn disassemble(code: &[u8]) -> String {
    use iced_x86::{Decoder, DecoderOptions, Formatter, IntelFormatter};
    let mut decoder = Decoder::with_ip(64, code, CODE_BASE, DecoderOptions::NONE);
    let mut formatter = IntelFormatter::new();
    let mut listing = String::new();
    while decoder.can_decode() {
        let instruction = decoder.decode();
        listing.push_str("    ");
        formatter.format(&instruction, &mut listing);
        listing.push('\n');
    }
    listing
}

pub fn defined_flags(code: &[u8]) -> u64 {
    use iced_x86::{Decoder, DecoderOptions, RflagsBits};
    const BITS: [(u32, u64); 6] = [
        (RflagsBits::CF, 1 << 0),
        (RflagsBits::PF, 1 << 2),
        (RflagsBits::AF, 1 << 4),
        (RflagsBits::ZF, 1 << 6),
        (RflagsBits::SF, 1 << 7),
        (RflagsBits::OF, 1 << 11),
    ];
    let mut undefined = 0u32;
    let mut decoder = Decoder::with_ip(64, code, CODE_BASE, DecoderOptions::NONE);
    while decoder.can_decode() {
        let instruction = decoder.decode();
        let defined = instruction.rflags_written() | instruction.rflags_set() | instruction.rflags_cleared();
        undefined = (undefined & !defined) | instruction.rflags_undefined();
    }
    let mut mask = 0x400u64;
    for (bit, x86) in BITS {
        if undefined & bit == 0 {
            mask |= x86;
        }
    }
    mask
}

pub fn check(build: impl Fn(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) {
    check_except(0, build);
}

pub fn check_native(build: impl Fn(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) {
    check_native_with(State::random, build);
}

pub fn check_native_except(excluded: u64, build: impl Fn(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) {
    check_native_masked(State::random, excluded, build);
}

pub fn check_f32(build: impl Fn(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) {
    check_native_with(|seed| State::random_floats(seed, false), build);
}

pub fn check_f64(build: impl Fn(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) {
    check_native_with(|seed| State::random_floats(seed, true), build);
}

pub fn check_native_with(initial: impl Fn(u64) -> State, build: impl Fn(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) {
    check_native_masked(initial, 0, build);
}

fn check_native_masked(initial: impl Fn(u64) -> State, excluded: u64, build: impl Fn(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) {
    let code = assemble(|a| {
        build(a)?;
        a.ret()
    });
    let mask = defined_flags(&code) & !excluded;
    let states: Vec<State> = (0..24).map(initial).collect();
    let cases: Vec<(&[u8], &State)> = states.iter().map(|state| (code.as_slice(), state)).collect();
    let expected = native::run(&cases);
    for (initial, expected) in states.iter().zip(expected) {
        compare(&code, initial, &expected, &run_wasm(&code, initial), mask);
    }
}

pub fn check_except(excluded: u64, build: impl Fn(&mut CodeAssembler) -> Result<(), iced_x86::IcedError>) {
    let code = assemble(|a| {
        build(a)?;
        a.ret()
    });
    let mask = defined_flags(&code) & !excluded;
    for seed in 0..24 {
        assert_matches_oracle(&code, &State::random(seed), mask);
    }
}
