use iced_x86::{Instruction, Mnemonic, OpKind};

use crate::Result;
use crate::codegen::body::mem;
use crate::codegen::function::{Emitter, gpr};
use crate::codegen::ops::atomic;

pub fn mov(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    e.read(instruction, 1)?;
    let value = e.save();
    e.write(instruction, 0, value)
}

pub fn movsx(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    e.read_signed(instruction, 1)?;
    let value = e.save();
    e.write(instruction, 0, value)
}

pub fn lea(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    e.push_address(instruction)?;
    let value = e.save();
    e.write(instruction, 0, value)
}

pub fn xchg(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    if instruction.op0_kind() == OpKind::Memory || instruction.op1_kind() == OpKind::Memory {
        return atomic::exchange(e, instruction);
    }
    e.read(instruction, 0)?;
    let first = e.save();
    e.read(instruction, 1)?;
    let second = e.save();
    e.write(instruction, 0, second)?;
    e.write(instruction, 1, first)
}

pub fn push(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = (-instruction.stack_pointer_increment()) as i64;
    e.read(instruction, 0)?;
    let value = e.save();
    let rsp = gpr(4);
    e.w().local_get(rsp).i64_const(size).i64_sub().local_tee(rsp).local_get(value);
    e.store(size as u32)
}

pub fn pop(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = instruction.stack_pointer_increment() as i64;
    let rsp = gpr(4);
    e.w().local_get(rsp);
    e.load(size as u32)?;
    let value = e.save();
    e.w().local_get(rsp).i64_const(size).i64_add().local_set(rsp);
    e.write(instruction, 0, value)
}

pub fn leave(e: &mut Emitter<'_>) {
    let (rsp, rbp) = (gpr(4), gpr(5));
    e.w().local_get(rbp).i64_load(mem(0)).local_get(rbp).i64_const(8).i64_add().local_set(rsp).local_set(rbp);
}

pub fn cmov(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    e.read(instruction, 1)?;
    e.read(instruction, 0)?;
    e.condition(instruction.condition_code());
    e.w().select();
    let value = e.save();
    e.write(instruction, 0, value)
}

pub fn setcc(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    e.condition(instruction.condition_code());
    e.w().i64_extend_i32_u();
    let value = e.save();
    e.write(instruction, 0, value)
}

pub fn is_conditional_move(mnemonic: Mnemonic) -> bool {
    matches!(
        mnemonic,
        Mnemonic::Cmovo
            | Mnemonic::Cmovno
            | Mnemonic::Cmovb
            | Mnemonic::Cmovae
            | Mnemonic::Cmove
            | Mnemonic::Cmovne
            | Mnemonic::Cmovbe
            | Mnemonic::Cmova
            | Mnemonic::Cmovs
            | Mnemonic::Cmovns
            | Mnemonic::Cmovp
            | Mnemonic::Cmovnp
            | Mnemonic::Cmovl
            | Mnemonic::Cmovge
            | Mnemonic::Cmovle
            | Mnemonic::Cmovg
    )
}

pub fn is_set(mnemonic: Mnemonic) -> bool {
    matches!(
        mnemonic,
        Mnemonic::Seto
            | Mnemonic::Setno
            | Mnemonic::Setb
            | Mnemonic::Setae
            | Mnemonic::Sete
            | Mnemonic::Setne
            | Mnemonic::Setbe
            | Mnemonic::Seta
            | Mnemonic::Sets
            | Mnemonic::Setns
            | Mnemonic::Setp
            | Mnemonic::Setnp
            | Mnemonic::Setl
            | Mnemonic::Setge
            | Mnemonic::Setle
            | Mnemonic::Setg
    )
}
