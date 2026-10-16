use iced_x86::{EncodingKind, FlowControl, Instruction, InstructionInfoFactory, Register, RflagsBits};
use wasm_encoder::{BlockType, Function as WasmFunction, InstructionSink, ValType};

use super::body::{Body, mem};
use super::{Context, TYPE_CTX, ops};
use crate::abi;
use crate::discover::Function;
use crate::flags::Liveness;
use crate::program::Target;
use crate::{Error, Result};

pub const CTX: u32 = 0;
const GPR_BASE: u32 = 1;
pub const CF: u32 = 17;
pub const PF: u32 = 18;
pub const AF: u32 = 19;
pub const ZF: u32 = 20;
pub const SF: u32 = 21;
pub const OF: u32 = 22;
pub const DF: u32 = 23;
const PC: u32 = 24;
const XLO_BASE: u32 = 25;
const XHI_BASE: u32 = 41;

pub fn gpr(index: u32) -> u32 {
    GPR_BASE + index
}

pub fn xlo(index: u32) -> u32 {
    XLO_BASE + index
}

pub fn xhi(index: u32) -> u32 {
    XHI_BASE + index
}

pub const FLAG_BITS: [(u32, u32, u32); 7] = [
    (CF, 0, RflagsBits::CF),
    (PF, 2, RflagsBits::PF),
    (AF, 4, RflagsBits::AF),
    (ZF, 6, RflagsBits::ZF),
    (SF, 7, RflagsBits::SF),
    (DF, 10, RflagsBits::DF),
    (OF, 11, RflagsBits::OF),
];

#[derive(Default)]
struct Usage {
    gprs: u16,
    low: u16,
    high: u16,
}

pub struct Emitter<'a> {
    pub context: &'a Context<'a>,
    pub function: &'a Function,
    pub body: Body,
    block: usize,
    pub nest: u32,
    pub live: u32,
    pub address: Option<u32>,
    guessed: bool,
    usage: Usage,
}

pub fn emit(context: &Context<'_>, function: &Function, liveness: &Liveness) -> Result<WasmFunction> {
    let mut fixed = vec![ValType::I64; 16];
    fixed.extend([ValType::I32; 8]);
    fixed.extend([ValType::V128; 32]);
    let mut emitter =
        Emitter { context, function, body: Body::new(1, &fixed), block: 0, nest: 0, live: 0, address: None, guessed: false, usage: usage(function) };
    emitter.reload();
    emitter.select_entry();

    let count = function.blocks.len() as u32;
    emitter.w().loop_(BlockType::Empty).block(BlockType::Empty);
    for _ in 0..count {
        emitter.w().block(BlockType::Empty);
    }
    emitter.w().local_get(PC).br_table(0..count, count).end();
    for (index, block) in function.blocks.iter().enumerate() {
        emitter.block = index;
        for (position, instruction) in block.instructions.iter().enumerate() {
            emitter.live = liveness.after[index][position];
            emitter.nest = 0;
            emitter.address = None;
            emitter.guessed = function.guessed.contains(&instruction.ip());
            ops::translate(&mut emitter, instruction)?;
            emitter.body.release_temps();
        }
        let last = block.instructions.last().unwrap();
        if last.flow_control() == FlowControl::Next && !ops::is_trap(last) {
            emitter.nest = 0;
            emitter.fallthrough(block.end)?;
        }
        emitter.w().end();
    }
    emitter.w().unreachable().end().unreachable().end();
    Ok(emitter.body.finish())
}

fn usage(function: &Function) -> Usage {
    let mut factory = InstructionInfoFactory::new();
    let mut usage = Usage::default();
    for instruction in function.blocks.iter().flat_map(|block| &block.instructions) {
        if matches!(instruction.mnemonic(), iced_x86::Mnemonic::Vzeroupper | iced_x86::Mnemonic::Vzeroall) {
            usage.low = 0xffff;
            usage.high = 0xffff;
        }
        if matches!(
            instruction.mnemonic(),
            iced_x86::Mnemonic::Fxsave | iced_x86::Mnemonic::Fxsave64 | iced_x86::Mnemonic::Fxrstor | iced_x86::Mnemonic::Fxrstor64
        ) {
            usage.low = 0xffff;
        }
        let vex = matches!(instruction.encoding(), EncodingKind::VEX | EncodingKind::EVEX);
        for used in factory.info(instruction).used_registers() {
            let register = used.register();
            if register.is_gpr() {
                usage.gprs |= 1 << (register.full_register() as u32 - Register::RAX as u32);
            } else if register.is_xmm() || register.is_ymm() || register.is_zmm() {
                let base = if register.is_xmm() {
                    Register::XMM0
                } else if register.is_ymm() {
                    Register::YMM0
                } else {
                    Register::ZMM0
                };
                let index = register as u32 - base as u32;
                if index < 16 {
                    usage.low |= 1 << index;
                    if vex || !register.is_xmm() {
                        usage.high |= 1 << index;
                    }
                }
            }
        }
    }
    usage
}

impl<'a> Emitter<'a> {
    pub fn w(&mut self) -> InstructionSink<'_> {
        self.body.sink()
    }

    pub fn temp64(&mut self) -> u32 {
        self.body.temp(ValType::I64)
    }

    pub fn temp32(&mut self) -> u32 {
        self.body.temp(ValType::I32)
    }

    pub fn temp128(&mut self) -> u32 {
        self.body.temp(ValType::V128)
    }

    pub fn options(&self) -> &crate::TranslateOptions {
        self.context.options
    }

    pub fn image(&self) -> u32 {
        self.function.image
    }

    pub fn runtime_address_in(&mut self, image: u32, vaddr: u64) {
        let global = self.context.layout.image_base_global(image);
        self.w().i64_const(vaddr as i64).global_get(global).i64_add();
    }

    pub fn runtime_address(&mut self, vaddr: u64) {
        self.runtime_address_in(self.function.image, vaddr);
    }

    pub fn want(&self, flag: u32) -> bool {
        self.live & flag != 0
    }

    pub fn open_if(&mut self) {
        self.w().if_(BlockType::Empty);
        self.nest += 1;
    }

    pub fn close(&mut self) {
        self.w().end();
        self.nest -= 1;
    }

    pub fn store_ctx_i64(&mut self, offset: u64, value: u32) {
        self.w().local_get(CTX).local_get(value).i64_store(mem(offset));
    }

    pub fn flush(&mut self) {
        for index in 0..16 {
            if self.usage.gprs & (1 << index) != 0 {
                self.w().local_get(CTX).local_get(gpr(index)).i64_store(mem(abi::cpu::gpr(index)));
            }
            if self.usage.low & (1 << index) != 0 {
                self.w().local_get(CTX).local_get(xlo(index)).v128_store(mem(abi::cpu::ymm(index)));
            }
            if self.usage.high & (1 << index) != 0 {
                self.w().local_get(CTX).local_get(xhi(index)).v128_store(mem(abi::cpu::ymm(index) + 16));
            }
        }
        if self.options().flags_at_boundaries {
            self.store_rflags();
        }
    }

    pub fn reload(&mut self) {
        for index in 0..16 {
            if self.usage.gprs & (1 << index) != 0 {
                self.w().local_get(CTX).i64_load(mem(abi::cpu::gpr(index))).local_set(gpr(index));
            }
            if self.usage.low & (1 << index) != 0 {
                self.w().local_get(CTX).v128_load(mem(abi::cpu::ymm(index))).local_set(xlo(index));
            }
            if self.usage.high & (1 << index) != 0 {
                self.w().local_get(CTX).v128_load(mem(abi::cpu::ymm(index) + 16)).local_set(xhi(index));
            }
        }
        if self.options().flags_at_boundaries {
            self.load_rflags();
        }
    }

    fn store_rflags(&mut self) {
        let mut keep = !0u64;
        for (_, bit, _) in FLAG_BITS {
            keep &= !(1u64 << bit);
        }
        self.w().local_get(CTX).local_get(CTX).i64_load(mem(abi::cpu::RFLAGS)).i64_const(keep as i64).i64_and();
        for (local, bit, _) in FLAG_BITS {
            self.w().local_get(local).i64_extend_i32_u().i64_const(bit as i64).i64_shl().i64_or();
        }
        self.w().i64_store(mem(abi::cpu::RFLAGS));
    }

    fn load_rflags(&mut self) {
        for (local, bit, _) in FLAG_BITS {
            self.w().local_get(CTX).i64_load(mem(abi::cpu::RFLAGS)).i64_const(bit as i64).i64_shr_u().i32_wrap_i64().i32_const(1).i32_and().local_set(local);
        }
    }

    fn select_entry(&mut self) {
        let function = self.function;
        if function.entries.len() == 1 {
            let block = function.block_index(function.start).unwrap() as i32;
            self.w().i32_const(block).local_set(PC);
            return;
        }
        let offset = self.temp64();
        let base = self.context.layout.image_base_global(function.image);
        self.w().local_get(CTX).i64_load(mem(abi::cpu::RIP)).global_get(base).i64_sub().local_set(offset);
        self.w().block(BlockType::Empty);
        for &entry in &function.entries {
            let block = function.block_index(entry).unwrap() as i32;
            self.w().local_get(offset).i64_const(entry as i64).i64_eq().if_(BlockType::Empty).i32_const(block).local_set(PC).br(1).end();
        }
        let unresolved = self.context.layout.unresolved;
        self.w().local_get(CTX).local_get(CTX).i64_load(mem(abi::cpu::RIP)).call(unresolved).unreachable().end();
        self.body.release_temps();
    }

    fn loop_depth(&self) -> u32 {
        self.function.blocks.len() as u32 - self.block as u32 + self.nest
    }

    pub fn jump(&mut self, vaddr: u64) -> Result<()> {
        match self.function.block_index(vaddr) {
            Some(block) => {
                let depth = self.loop_depth();
                self.w().i32_const(block as i32).local_set(PC).br(depth);
                Ok(())
            }
            None => self.tail_call(self.function.image, vaddr),
        }
    }

    pub fn fallthrough(&mut self, vaddr: u64) -> Result<()> {
        if self.nest == 0 && self.function.block_index(vaddr) == Some(self.block + 1) {
            return Ok(());
        }
        self.jump(vaddr)
    }

    fn guest_function(&self, image: u32, vaddr: u64) -> Result<u32> {
        let index = self.context.discovery.function_at(image, vaddr).ok_or_else(|| Error::new(format!("no translated code at {vaddr:#x} in image {image}")))?;
        if !self.context.discovery.functions[index].entries.contains(&vaddr) {
            return Err(Error::new(format!("{vaddr:#x} in image {image} is not an entry of its function")));
        }
        Ok(self.context.layout.guest_base + index as u32)
    }

    pub fn tail_call(&mut self, image: u32, vaddr: u64) -> Result<()> {
        let function = match self.guest_function(image, vaddr) {
            Ok(function) => function,
            Err(_) if self.guessed => {
                self.flush();
                let unresolved = self.context.layout.unresolved;
                self.w().local_get(CTX);
                self.runtime_address_in(image, vaddr);
                self.w().call(unresolved).unreachable();
                return Ok(());
            }
            Err(error) => return Err(error),
        };
        self.flush();
        self.w().local_get(CTX);
        self.runtime_address_in(image, vaddr);
        self.w().i64_store(mem(abi::cpu::RIP)).local_get(CTX).return_call(function);
        Ok(())
    }

    pub fn tail_call_indirect(&mut self, target: u32) {
        self.flush();
        self.store_ctx_i64(abi::cpu::RIP, target);
        let lookup = self.context.layout.lookup;
        self.w().local_get(CTX).local_get(CTX).local_get(target).call(lookup).return_call_indirect(0, TYPE_CTX);
    }

    pub fn push_value(&mut self, value: u32) {
        let rsp = gpr(4);
        self.w().local_get(rsp).i64_const(8).i64_sub().local_tee(rsp).local_get(value).i64_store(mem(0));
    }

    pub fn call_with(&mut self, instruction: &Instruction, enter: impl FnOnce(&mut Self)) -> Result<()> {
        let return_address = self.temp64();
        self.runtime_address(instruction.next_ip());
        self.w().local_set(return_address);
        self.push_value(return_address);
        self.flush();
        enter(self);
        self.w().local_get(CTX).i64_load(mem(abi::cpu::RIP)).local_get(return_address).i64_ne().if_(BlockType::Empty).return_().end();
        self.reload();
        self.fallthrough(instruction.next_ip())
    }

    pub fn call_target(&mut self, instruction: &Instruction, target: Target) -> Result<()> {
        match target {
            Target::Guest { image, vaddr } => {
                let function = self.guest_function(image, vaddr)?;
                self.call_with(instruction, |emitter| {
                    emitter.w().local_get(CTX);
                    emitter.runtime_address_in(image, vaddr);
                    emitter.w().i64_store(mem(abi::cpu::RIP)).local_get(CTX).call(function);
                })
            }
            Target::Import(index) => {
                let function = self.context.layout.nid_base + index;
                self.call_with(instruction, |emitter| {
                    emitter.w().local_get(CTX).call(function);
                })
            }
        }
    }

    pub fn tail_call_import(&mut self, index: u32) {
        self.flush();
        let function = self.context.layout.nid_base + index;
        self.w().local_get(CTX).call(function).return_();
    }

    pub fn trap(&mut self, instruction: &Instruction) {
        self.flush();
        let address = self.temp64();
        self.runtime_address(instruction.ip());
        self.w().local_set(address);
        let unsupported = self.context.layout.unsupported;
        self.w().local_get(CTX).local_get(address).call(unsupported).unreachable();
    }

    pub fn unsupported(&mut self, instruction: &Instruction) -> Result<()> {
        if self.options().allow_unsupported {
            self.context.unsupported.borrow_mut().push(crate::report::Unsupported {
                image: self.function.image,
                address: instruction.ip(),
                code: format!("{:?}", instruction.code()),
                text: format!("{instruction}"),
            });
            self.trap(instruction);
            return Ok(());
        }
        Err(Error::new(format!(
            "unsupported instruction at {:#x} in image {}: {instruction} ({:?})",
            instruction.ip(),
            self.function.image,
            instruction.code()
        )))
    }
}
