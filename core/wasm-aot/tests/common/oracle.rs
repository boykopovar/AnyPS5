use unicorn_engine::{Arch, Mode, Prot, RegisterX86, Unicorn, X86CpuModel};

use super::{CODE_BASE, DATA_BASE, DATA_SIZE, RSP, SENTINEL, STACK_BASE, STACK_SIZE, State};

const GPRS: [RegisterX86; 16] = [
    RegisterX86::RAX,
    RegisterX86::RCX,
    RegisterX86::RDX,
    RegisterX86::RBX,
    RegisterX86::RSP,
    RegisterX86::RBP,
    RegisterX86::RSI,
    RegisterX86::RDI,
    RegisterX86::R8,
    RegisterX86::R9,
    RegisterX86::R10,
    RegisterX86::R11,
    RegisterX86::R12,
    RegisterX86::R13,
    RegisterX86::R14,
    RegisterX86::R15,
];

fn ymm(index: usize) -> i32 {
    RegisterX86::YMM0 as i32 + index as i32
}

pub fn run(code: &[u8], initial: &State) -> State {
    let mut emulator = Unicorn::new(Arch::X86, Mode::MODE_64).unwrap();
    emulator.ctl_set_cpu_model(X86CpuModel::EPYC_ROME as i32).unwrap();
    emulator.mem_map(SENTINEL & !0xfff, 0x1000, Prot::ALL).unwrap();
    emulator.mem_map(CODE_BASE, 0x10000, Prot::ALL).unwrap();
    emulator.mem_map(DATA_BASE, DATA_SIZE as u64, Prot::ALL).unwrap();
    emulator.mem_map(STACK_BASE, STACK_SIZE as u64, Prot::ALL).unwrap();
    emulator.mem_write(CODE_BASE, code).unwrap();
    emulator.mem_write(DATA_BASE, &initial.data).unwrap();
    emulator.mem_write(STACK_BASE, &initial.stack).unwrap();
    let mut gpr = initial.gpr;
    gpr[RSP] -= 8;
    emulator.mem_write(gpr[RSP], &SENTINEL.to_le_bytes()).unwrap();
    for (register, value) in GPRS.iter().zip(gpr) {
        emulator.reg_write(*register, value).unwrap();
    }
    emulator.reg_write(RegisterX86::RFLAGS, initial.rflags).unwrap();
    for (index, value) in initial.ymm.iter().enumerate() {
        emulator.reg_write_long(ymm(index), value).unwrap();
    }
    emulator.emu_start(CODE_BASE, SENTINEL, 0, 100_000).unwrap();
    assert_eq!(emulator.reg_read(RegisterX86::RIP).unwrap(), SENTINEL, "oracle did not reach the sentinel");

    let mut state = State::default();
    for (value, register) in state.gpr.iter_mut().zip(GPRS) {
        *value = emulator.reg_read(register).unwrap();
    }
    state.rflags = emulator.reg_read(RegisterX86::RFLAGS).unwrap();
    for (index, value) in state.ymm.iter_mut().enumerate() {
        value.copy_from_slice(&emulator.reg_read_long(ymm(index)).unwrap());
    }
    state.data = emulator.mem_read_as_vec(DATA_BASE, DATA_SIZE).unwrap();
    state.stack = emulator.mem_read_as_vec(STACK_BASE, STACK_SIZE).unwrap();
    state
}
