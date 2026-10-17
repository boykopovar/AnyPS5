mod convert;
mod float;
mod integer;
mod moves;
mod shuffle;
mod strings;

use iced_x86::{EncodingKind, Instruction, OpKind, Register};

use crate::codegen::body::mem;
use crate::codegen::function::{Emitter, xhi, xlo};
use crate::codegen::ops::operand::Loc;
use crate::{Error, Result};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum VecLoc {
    Xmm(u32),
    Ymm(u32),
    Mem(u32),
    Gpr(Loc),
    Imm(u64),
}

pub fn is_vex(instruction: &Instruction) -> bool {
    matches!(instruction.encoding(), EncodingKind::VEX | EncodingKind::EVEX)
}

pub fn elements(size: u8, indices: &[u8]) -> [u8; 16] {
    let mut bytes = [0u8; 16];
    for (position, &index) in indices.iter().enumerate() {
        for byte in 0..size {
            bytes[position * size as usize + byte as usize] = index * size + byte;
        }
    }
    bytes
}

impl Emitter<'_> {
    pub fn vloc(&self, instruction: &Instruction, operand: u32) -> Result<VecLoc> {
        Ok(match instruction.op_kind(operand) {
            OpKind::Register => {
                let register = instruction.op_register(operand);
                if register.is_xmm() {
                    VecLoc::Xmm(register as u32 - Register::XMM0 as u32)
                } else if register.is_ymm() {
                    VecLoc::Ymm(register as u32 - Register::YMM0 as u32)
                } else {
                    VecLoc::Gpr(self.loc(instruction, operand)?)
                }
            }
            OpKind::Memory => VecLoc::Mem(instruction.memory_size().size() as u32),
            OpKind::Immediate8 => VecLoc::Imm(instruction.immediate8() as u64),
            kind => {
                return Err(Error::new(format!("{:#x}: vector operand kind {kind:?}", instruction.ip())));
            }
        })
    }

    pub fn wide(&self, instruction: &Instruction) -> bool {
        match instruction.op0_kind() {
            OpKind::Register => instruction.op0_register().is_ymm(),
            OpKind::Memory => instruction.memory_size().size() == 32,
            _ => false,
        }
    }

    pub fn vlow(&mut self, instruction: &Instruction, operand: u32) -> Result<()> {
        match self.vloc(instruction, operand)? {
            VecLoc::Xmm(index) | VecLoc::Ymm(index) => {
                self.w().local_get(xlo(index));
            }
            VecLoc::Mem(size) => {
                let address = self.address(instruction)?;
                self.w().local_get(address);
                match size {
                    16 | 32 => self.w().v128_load(mem(0)),
                    8 => self.w().v128_load64_zero(mem(0)),
                    4 => self.w().v128_load32_zero(mem(0)),
                    2 => self.w().v128_const(0).v128_load16_lane(mem(0), 0),
                    1 => self.w().v128_const(0).v128_load8_lane(mem(0), 0),
                    _ => {
                        return Err(Error::new(format!("{:#x}: vector load of {size} bytes", instruction.ip())));
                    }
                };
            }
            other => {
                return Err(Error::new(format!("{:#x}: operand {operand} ({other:?}) is not a vector", instruction.ip())));
            }
        }
        Ok(())
    }

    pub fn vhigh(&mut self, instruction: &Instruction, operand: u32) -> Result<()> {
        match self.vloc(instruction, operand)? {
            VecLoc::Ymm(index) => {
                self.w().local_get(xhi(index));
            }
            VecLoc::Mem(32) => {
                let address = self.address(instruction)?;
                self.w().local_get(address).v128_load(mem(16));
            }
            other => {
                return Err(Error::new(format!("{:#x}: operand {operand} ({other:?}) has no high half", instruction.ip())));
            }
        }
        Ok(())
    }

    pub fn save128(&mut self) -> u32 {
        let local = self.temp128();
        self.w().local_set(local);
        local
    }

    pub fn vwrite(&mut self, instruction: &Instruction, operand: u32, low: u32, high: Option<u32>) -> Result<()> {
        match self.vloc(instruction, operand)? {
            VecLoc::Xmm(index) => {
                self.w().local_get(low).local_set(xlo(index));
                if is_vex(instruction) {
                    self.w().v128_const(0).local_set(xhi(index));
                }
            }
            VecLoc::Ymm(index) => {
                let high = high.ok_or_else(|| Error::new(format!("{:#x}: 256-bit result without a high half", instruction.ip())))?;
                self.w().local_get(low).local_set(xlo(index)).local_get(high).local_set(xhi(index));
            }
            VecLoc::Mem(size) => {
                let address = self.address(instruction)?;
                self.w().local_get(address).local_get(low);
                match size {
                    16 => {
                        self.w().v128_store(mem(0));
                    }
                    32 => {
                        let high = high.ok_or_else(|| Error::new(format!("{:#x}: 256-bit store without a high half", instruction.ip())))?;
                        self.w().v128_store(mem(0));
                        self.w().local_get(address).local_get(high).v128_store(mem(16));
                    }
                    8 => {
                        self.w().v128_store64_lane(mem(0), 0);
                    }
                    4 => {
                        self.w().v128_store32_lane(mem(0), 0);
                    }
                    2 => {
                        self.w().v128_store16_lane(mem(0), 0);
                    }
                    1 => {
                        self.w().v128_store8_lane(mem(0), 0);
                    }
                    _ => {
                        return Err(Error::new(format!("{:#x}: vector store of {size} bytes", instruction.ip())));
                    }
                }
            }
            other => {
                return Err(Error::new(format!("{:#x}: vector result into {other:?}", instruction.ip())));
            }
        }
        Ok(())
    }

    pub fn sources(&self, instruction: &Instruction) -> (u32, u32) {
        if is_vex(instruction) { (1, 2) } else { (0, 1) }
    }

    pub fn binary_lanes(&mut self, instruction: &Instruction, op: &dyn Fn(&mut Emitter<'_>)) -> Result<()> {
        let (a, b) = self.sources(instruction);
        self.vlow(instruction, a)?;
        self.vlow(instruction, b)?;
        op(self);
        let low = self.save128();
        let high = if self.wide(instruction) {
            self.vhigh(instruction, a)?;
            self.vhigh(instruction, b)?;
            op(self);
            Some(self.save128())
        } else {
            None
        };
        self.vwrite(instruction, 0, low, high)
    }

    pub fn unary_lanes(&mut self, instruction: &Instruction, operand: u32, op: &dyn Fn(&mut Emitter<'_>)) -> Result<()> {
        self.vlow(instruction, operand)?;
        op(self);
        let low = self.save128();
        let high = if self.wide(instruction) {
            self.vhigh(instruction, operand)?;
            op(self);
            Some(self.save128())
        } else {
            None
        };
        self.vwrite(instruction, 0, low, high)
    }
}

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction) -> Option<Result<()>> {
    let mnemonic = instruction.mnemonic();
    moves::translate(e, instruction, mnemonic)
        .or_else(|| shuffle::translate(e, instruction, mnemonic))
        .or_else(|| integer::translate(e, instruction, mnemonic))
        .or_else(|| float::translate(e, instruction, mnemonic))
        .or_else(|| convert::translate(e, instruction, mnemonic))
        .or_else(|| strings::translate(e, instruction, mnemonic))
}
