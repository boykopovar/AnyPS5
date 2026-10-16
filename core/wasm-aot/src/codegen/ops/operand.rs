use iced_x86::{Instruction, OpKind, Register};

use crate::abi;
use crate::codegen::body::mem;
use crate::codegen::function::{CTX, Emitter, gpr};
use crate::{Error, Result};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Loc {
    Gpr { index: u32, size: u32, high: bool },
    Mem { size: u32 },
    Imm { value: u64, size: u32 },
}

impl Loc {
    pub fn size(self) -> u32 {
        match self {
            Loc::Gpr { size, .. } | Loc::Mem { size } | Loc::Imm { size, .. } => size,
        }
    }
}

pub fn gpr_index(register: Register) -> u32 {
    register.full_register() as u32 - Register::RAX as u32
}

pub fn is_high_byte(register: Register) -> bool {
    matches!(register, Register::AH | Register::CH | Register::DH | Register::BH)
}

pub fn mask(size: u32) -> u64 {
    if size >= 8 { u64::MAX } else { (1u64 << (size * 8)) - 1 }
}

impl Emitter<'_> {
    pub fn loc(&self, instruction: &Instruction, operand: u32) -> Result<Loc> {
        Ok(match instruction.op_kind(operand) {
            OpKind::Register => {
                let register = instruction.op_register(operand);
                if !register.is_gpr() {
                    return Err(Error::new(format!("{:#x}: operand {operand} is not a general register", instruction.ip())));
                }
                Loc::Gpr { index: gpr_index(register), size: register.size() as u32, high: is_high_byte(register) }
            }
            OpKind::Memory => Loc::Mem { size: instruction.memory_size().size() as u32 },
            OpKind::Immediate8 => Loc::Imm { value: instruction.immediate8() as u64, size: 1 },
            OpKind::Immediate16 => Loc::Imm { value: instruction.immediate16() as u64, size: 2 },
            OpKind::Immediate32 => Loc::Imm { value: instruction.immediate32() as u64, size: 4 },
            OpKind::Immediate64 => Loc::Imm { value: instruction.immediate64(), size: 8 },
            OpKind::Immediate8to16 => Loc::Imm { value: instruction.immediate8to16() as u16 as u64, size: 2 },
            OpKind::Immediate8to32 => Loc::Imm { value: instruction.immediate8to32() as u32 as u64, size: 4 },
            OpKind::Immediate8to64 => Loc::Imm { value: instruction.immediate8to64() as u64, size: 8 },
            OpKind::Immediate32to64 => Loc::Imm { value: instruction.immediate32to64() as u64, size: 8 },
            kind => {
                return Err(Error::new(format!("{:#x}: operand kind {kind:?} is not handled", instruction.ip())));
            }
        })
    }

    pub fn push_address(&mut self, instruction: &Instruction) -> Result<()> {
        if instruction.is_ip_rel_memory_operand() {
            self.runtime_address(instruction.ip_rel_memory_address());
            return self.add_segment(instruction);
        }
        let base = instruction.memory_base();
        let index = instruction.memory_index();
        let narrow = [base, index].iter().any(|register| *register != Register::None && register.size() == 4);
        if [base, index].iter().any(|register| *register != Register::None && !register.is_gpr()) {
            return Err(Error::new(format!("{:#x}: unsupported address registers in {instruction}", instruction.ip())));
        }
        self.w().i64_const(instruction.memory_displacement64() as i64);
        if base != Register::None {
            self.w().local_get(gpr(gpr_index(base))).i64_add();
        }
        if index != Register::None {
            self.w().local_get(gpr(gpr_index(index)));
            let shift = instruction.memory_index_scale().trailing_zeros();
            if shift != 0 {
                self.w().i64_const(shift as i64).i64_shl();
            }
            self.w().i64_add();
        }
        if narrow {
            self.w().i64_const(0xffff_ffff).i64_and();
        }
        self.add_segment(instruction)
    }

    fn add_segment(&mut self, instruction: &Instruction) -> Result<()> {
        match instruction.memory_segment() {
            Register::FS => {
                self.w().local_get(CTX).i64_load(mem(abi::cpu::FS_BASE)).i64_add();
            }
            Register::GS => {
                self.w().local_get(CTX).i64_load(mem(abi::cpu::GS_BASE)).i64_add();
            }
            _ => {}
        }
        Ok(())
    }

    pub fn address(&mut self, instruction: &Instruction) -> Result<u32> {
        if let Some(local) = self.address {
            return Ok(local);
        }
        self.push_address(instruction)?;
        let local = self.temp64();
        self.w().local_set(local);
        self.address = Some(local);
        Ok(local)
    }

    pub fn read(&mut self, instruction: &Instruction, operand: u32) -> Result<Loc> {
        let loc = self.loc(instruction, operand)?;
        match loc {
            Loc::Gpr { index, size, high } => {
                self.w().local_get(gpr(index));
                if high {
                    self.w().i64_const(8).i64_shr_u();
                }
                if size < 8 {
                    self.w().i64_const(mask(size) as i64).i64_and();
                }
            }
            Loc::Mem { size } => {
                let address = self.address(instruction)?;
                self.w().local_get(address);
                self.load(size)?;
            }
            Loc::Imm { value, size } => {
                self.w().i64_const((value & mask(size)) as i64);
            }
        }
        Ok(loc)
    }

    pub fn read_signed(&mut self, instruction: &Instruction, operand: u32) -> Result<Loc> {
        let loc = self.read(instruction, operand)?;
        self.sign_extend(loc.size());
        Ok(loc)
    }

    pub fn sign_extend(&mut self, size: u32) {
        match size {
            1 => {
                self.w().i64_extend8_s();
            }
            2 => {
                self.w().i64_extend16_s();
            }
            4 => {
                self.w().i64_extend32_s();
            }
            _ => {}
        }
    }

    pub fn load(&mut self, size: u32) -> Result<()> {
        match size {
            1 => self.w().i64_load8_u(mem(0)),
            2 => self.w().i64_load16_u(mem(0)),
            4 => self.w().i64_load32_u(mem(0)),
            8 => self.w().i64_load(mem(0)),
            _ => return Err(Error::new(format!("integer load of {size} bytes"))),
        };
        Ok(())
    }

    pub fn store(&mut self, size: u32) -> Result<()> {
        match size {
            1 => self.w().i64_store8(mem(0)),
            2 => self.w().i64_store16(mem(0)),
            4 => self.w().i64_store32(mem(0)),
            8 => self.w().i64_store(mem(0)),
            _ => return Err(Error::new(format!("integer store of {size} bytes"))),
        };
        Ok(())
    }

    pub fn write(&mut self, instruction: &Instruction, operand: u32, value: u32) -> Result<()> {
        let loc = self.loc(instruction, operand)?;
        match loc {
            Loc::Gpr { index, size, high } => self.write_gpr(index, size, high, value),
            Loc::Mem { size } => {
                let address = self.address(instruction)?;
                self.w().local_get(address).local_get(value);
                self.store(size)?;
            }
            Loc::Imm { .. } => {
                return Err(Error::new(format!("{:#x}: write to an immediate", instruction.ip())));
            }
        }
        Ok(())
    }

    pub fn write_gpr(&mut self, index: u32, size: u32, high: bool, value: u32) {
        let register = gpr(index);
        match (size, high) {
            (8, _) => {
                self.w().local_get(value).local_set(register);
            }
            (4, _) => {
                self.w().local_get(value).i64_const(0xffff_ffff).i64_and().local_set(register);
            }
            (_, false) => {
                let bits = mask(size) as i64;
                self.w().local_get(register).i64_const(!bits).i64_and().local_get(value).i64_const(bits).i64_and().i64_or().local_set(register);
            }
            (_, true) => {
                self.w().local_get(register).i64_const(!0xff00).i64_and();
                self.w().local_get(value).i64_const(0xff).i64_and().i64_const(8).i64_shl().i64_or().local_set(register);
            }
        }
    }

    pub fn save(&mut self) -> u32 {
        let local = self.temp64();
        self.w().local_set(local);
        local
    }
}
