mod common;

use std::collections::BTreeMap;
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::path::PathBuf;

use anyps5_wasm_aot::{Error, discover::discover, load_images, program::Program};
use common::*;
use iced_x86::{Encoder, FlowControl, Instruction, InstructionInfoFactory, Mnemonic, OpAccess, OpKind, Register};

const SEEDS: u64 = 6;
const OPERAND_OFFSET: u64 = 0x400;
const SCRATCH: [Register; 6] = [Register::R15, Register::R14, Register::R13, Register::R12, Register::R11, Register::R10];

fn game_directory() -> Option<PathBuf> {
    std::env::var_os("ANYPS5_DREAMING_SARAH_DIR").map(PathBuf::from)
}

fn instruction_forms(directory: &std::path::Path) -> BTreeMap<(u32, u32), Instruction> {
    let main = std::fs::read(directory.join("app.elf")).unwrap();
    let mut resolve = |needed: &str| -> anyps5_wasm_aot::Result<Option<(String, Vec<u8>)>> {
        let path = directory.join(needed.trim_start_matches("$ORIGIN/"));
        let bytes = std::fs::read(&path).map_err(|error| Error::new(format!("{}: {error}", path.display())))?;
        Ok(Some((needed.to_string(), bytes)))
    };
    let program = Program::bind(load_images("app.elf", &main, &mut resolve).unwrap()).unwrap();
    let discovery = discover(&program).unwrap();
    let mut forms = BTreeMap::new();
    for function in &discovery.functions {
        for block in &function.blocks {
            for instruction in &block.instructions {
                forms.entry((instruction.code() as u32, shape(instruction))).or_insert(*instruction);
            }
        }
    }
    forms
}

fn memory_operand(instruction: &Instruction) -> bool {
    (0..instruction.op_count()).any(|operand| instruction.op_kind(operand) == OpKind::Memory)
}

fn shape(instruction: &Instruction) -> u32 {
    if !memory_operand(instruction) {
        return 0;
    }
    let base = instruction.memory_base();
    let index = instruction.memory_index();
    let displacement = instruction.memory_displacement64() as i64;
    let mut shape = 1;
    if base == Register::RIP {
        shape |= 2;
    } else if base != Register::None {
        shape |= 4;
    }
    if index != Register::None {
        shape |= 8 | (instruction.memory_index_scale() << 4);
    }
    if base != Register::None && base == index {
        shape |= 0x100;
    }
    if base != Register::RIP {
        if displacement < 0 {
            shape |= 0x200;
        } else if displacement > 0x7f {
            shape |= 0x400;
        } else if displacement > 0 {
            shape |= 0x800;
        }
    }
    if instruction.segment_prefix() != Register::None {
        shape |= 0x1000;
    }
    shape
}

fn writes_stack_pointer(instruction: &Instruction) -> bool {
    let mut factory = InstructionInfoFactory::new();
    factory.info(instruction).used_registers().iter().any(|used| {
        used.register().full_register() == Register::RSP && matches!(used.access(), OpAccess::Write | OpAccess::ReadWrite | OpAccess::CondWrite | OpAccess::ReadCondWrite)
    })
}

fn excluded(instruction: &Instruction) -> bool {
    if instruction.flow_control() != FlowControl::Next || writes_stack_pointer(instruction) {
        return true;
    }
    matches!(
        instruction.mnemonic(),
        Mnemonic::Cpuid
            | Mnemonic::Rdtsc
            | Mnemonic::Rdtscp
            | Mnemonic::Rdrand
            | Mnemonic::Rdseed
            | Mnemonic::Xgetbv
            | Mnemonic::Div
            | Mnemonic::Idiv
            | Mnemonic::Leave
            | Mnemonic::Enter
            | Mnemonic::Pause
            | Mnemonic::Hlt
            | Mnemonic::Fxsave
            | Mnemonic::Fxsave64
            | Mnemonic::Lfence
            | Mnemonic::Mfence
            | Mnemonic::Sfence
            | Mnemonic::Prefetchnta
            | Mnemonic::Prefetcht0
            | Mnemonic::Prefetcht1
            | Mnemonic::Prefetcht2
            | Mnemonic::Prefetchw
    ) || instruction.is_stack_instruction()
        || (0..instruction.op_count()).any(|operand| instruction.op_kind(operand) == OpKind::Memory && instruction.memory_index().is_vector_register())
}

fn scratch_register(instruction: &Instruction) -> Option<Register> {
    let mut factory = InstructionInfoFactory::new();
    let used: Vec<Register> = factory.info(instruction).used_registers().iter().map(|used| used.register().full_register()).collect();
    SCRATCH.into_iter().find(|candidate| !used.contains(candidate))
}

fn place_operand(rewritten: &mut Instruction, instruction: &Instruction) -> Option<Vec<(Register, u64)>> {
    let target = (DATA_BASE + OPERAND_OFFSET) as i64;
    let base = instruction.memory_base();
    let index = instruction.memory_index();
    let scale = instruction.memory_index_scale() as i64;
    let displacement = instruction.memory_displacement64() as i64;
    rewritten.set_segment_prefix(Register::None);
    if base == Register::RIP {
        rewritten.set_memory_displacement64(target as u64);
        return Some(Vec::new());
    }
    let wide = |register: Register| register == Register::None || register.is_gpr64();
    if !wide(base) || !wide(index) || base == Register::RSP || index == Register::RSP {
        let register = scratch_register(instruction)?;
        rewritten.set_memory_base(register);
        rewritten.set_memory_index(Register::None);
        rewritten.set_memory_index_scale(1);
        rewritten.set_memory_displacement64(OPERAND_OFFSET);
        rewritten.set_memory_displ_size(8);
        return Some(vec![(register, DATA_BASE)]);
    }
    let remaining = target - displacement;
    match (base, index) {
        (Register::None, Register::None) => None,
        (Register::None, index) => (remaining % scale == 0).then(|| vec![(index, (remaining / scale) as u64)]),
        (base, Register::None) => Some(vec![(base, remaining as u64)]),
        (base, index) if base == index => (remaining % (1 + scale) == 0).then(|| vec![(base, (remaining / (1 + scale)) as u64)]),
        (base, index) => Some(vec![(index, 3), (base, (remaining - 3 * scale) as u64)]),
    }
}

fn rewrite(instruction: &Instruction) -> Option<(Vec<u8>, Vec<(Register, u64)>)> {
    let mut rewritten = *instruction;
    let assignments = if memory_operand(instruction) { place_operand(&mut rewritten, instruction)? } else { Vec::new() };
    let mut encoder = Encoder::new(64);
    if let Err(error) = encoder.encode(&rewritten, CODE_BASE) {
        if std::env::var_os("APS5_TRACE_REWRITE").is_some() {
            eprintln!("{:?}: {error}", instruction.code());
        }
        return None;
    }
    let mut bytes = if is_x87(instruction) { vec![0xD9, 0xE8, 0xD9, 0xEE] } else { Vec::new() };
    if is_x87(instruction) {
        let mut placed = Encoder::new(64);
        placed.encode(&rewritten, CODE_BASE + 4).ok()?;
        bytes.extend(placed.take_buffer());
    } else {
        bytes.extend(encoder.take_buffer());
    }
    bytes.push(0xC3);
    Some((bytes, assignments))
}

fn is_x87(instruction: &Instruction) -> bool {
    format!("{:?}", instruction.code()).starts_with('F')
}

fn is_square_root(instruction: &Instruction) -> bool {
    matches!(instruction.mnemonic(), Mnemonic::Sqrtss | Mnemonic::Sqrtsd | Mnemonic::Sqrtps | Mnemonic::Sqrtpd | Mnemonic::Vsqrtss | Mnemonic::Vsqrtsd | Mnemonic::Vsqrtps | Mnemonic::Vsqrtpd)
}

fn oracle_flag_mask(instruction: &Instruction) -> u64 {
    const CARRY: u64 = 0x1;
    const ZERO: u64 = 0x40;
    match instruction.mnemonic() {
        Mnemonic::Cmpxchg => ZERO,
        Mnemonic::Ptest | Mnemonic::Vptest => ZERO | CARRY,
        _ => !0,
    }
}

fn clear_float_signs(state: &mut State) {
    for register in state.ymm.iter_mut() {
        for chunk in register.chunks_mut(4) {
            chunk[3] &= 0x7f;
        }
    }
    for chunk in state.data.chunks_mut(4) {
        chunk[3] &= 0x7f;
    }
}

fn uses_floats(instruction: &Instruction) -> bool {
    let name = format!("{:?}", instruction.code());
    ["ps", "pd", "ss", "sd", "_ph", "Fadd", "Fmul", "Fsub", "Fdiv", "Fld", "Fst", "Fcom", "Fucom", "Fsqrt", "Frndint"].iter().any(|marker| name.contains(marker))
        || format!("{:?}", instruction.mnemonic()).starts_with('F')
}

fn states(instruction: &Instruction, assignments: &[(Register, u64)]) -> Vec<State> {
    let floats = uses_floats(instruction);
    (0..SEEDS)
        .map(|seed| {
            let mut state = if floats && seed % 2 == 1 { State::random_floats(seed, seed % 4 == 1) } else { State::random(seed) };
            state.rflags &= !0x400;
            for &(register, value) in assignments {
                state.gpr[register.number()] = value;
            }
            if instruction.is_string_instruction() {
                state.gpr[RSI] = DATA_BASE + 0x100;
                state.gpr[RDI] = DATA_BASE + 0x800;
                state.gpr[RCX] = seed % 4 * 3;
                if seed % 3 == 0 {
                    let source = state.data[0x100..0x140].to_vec();
                    state.data[0x800..0x840].copy_from_slice(&source);
                }
            }
            if is_square_root(instruction) {
                clear_float_signs(&mut state);
            }
            if matches!(instruction.mnemonic(), Mnemonic::Ldmxcsr | Mnemonic::Vldmxcsr) {
                let control = 0x1F80u32 | if seed % 2 == 0 { 0x8040 } else { 0 };
                state.data[OPERAND_OFFSET as usize..OPERAND_OFFSET as usize + 4].copy_from_slice(&control.to_le_bytes());
            }
            state
        })
        .collect()
}

#[test]
fn every_instruction_form_of_the_game_matches_the_cpu() {
    let Some(directory) = game_directory() else {
        eprintln!("ANYPS5_DREAMING_SARAH_DIR is not set; skipped");
        return;
    };
    let forms = instruction_forms(&directory);
    let mut failures = Vec::new();
    let mut tested = 0;
    let mut skipped = Vec::new();
    let mut crashed = Vec::new();
    for instruction in forms.values() {
        if excluded(instruction) {
            continue;
        }
        let Some((code, assignments)) = rewrite(instruction) else {
            skipped.push(format!("{:?}", instruction.code()));
            continue;
        };
        let initial = states(instruction, &assignments);
        let mask = defined_flags(&code) & oracle_flag_mask(instruction);
        let cases: Vec<(&[u8], &State)> = initial.iter().map(|state| (code.as_slice(), state)).collect();
        let Ok(expected) = catch_unwind(AssertUnwindSafe(|| native::run(&cases))) else {
            crashed.push(format!("{:?} at {:#x}", instruction.code(), instruction.ip()));
            continue;
        };
        tested += 1;
        for (state, expected) in initial.iter().zip(&expected) {
            let outcome = catch_unwind(AssertUnwindSafe(|| {
                let actual = run_wasm(&code, state);
                let mut expected = expected.clone();
                if matches!(instruction.mnemonic(), Mnemonic::Fnstenv) {
                    let pointers = OPERAND_OFFSET as usize + 12..OPERAND_OFFSET as usize + 28;
                    expected.data[pointers.clone()].copy_from_slice(&actual.data[pointers]);
                }
                compare(&code, state, &expected, &actual, mask)
            }));
            if let Err(panic) = outcome {
                let message = panic.downcast_ref::<String>().cloned().or_else(|| panic.downcast_ref::<&str>().map(|text| text.to_string())).unwrap_or_default();
                failures.push(format!("{:?} at {:#x}: {}", instruction.code(), instruction.ip(), message.lines().take(4).collect::<Vec<_>>().join(" | ")));
                break;
            }
        }
    }
    eprintln!("tested {tested} instruction forms, skipped {} that could not be re-encoded: {skipped:?}", skipped.len());
    eprintln!("the CPU faulted on {} forms: {crashed:?}", crashed.len());
    assert!(failures.is_empty(), "{} instruction forms differ from the CPU:\n{}", failures.len(), failures.join("\n"));
}
