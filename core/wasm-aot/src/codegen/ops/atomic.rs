use iced_x86::{Instruction, Mnemonic, OpKind, RflagsBits};

use crate::codegen::body::aligned;
use crate::codegen::function::{CF, Emitter, gpr};
use crate::codegen::ops::integer::Binary;
use crate::{Error, Result};

#[derive(Clone, Copy)]
enum Rmw {
    Add,
    Sub,
    And,
    Or,
    Xor,
    Xchg,
}

fn rmw(e: &mut Emitter<'_>, op: Rmw, size: u32) -> Result<()> {
    let log2 = size.trailing_zeros();
    let memarg = aligned(0, log2);
    let mut w = e.w();
    match (op, size) {
        (Rmw::Add, 1) => w.i64_atomic_rmw8_add_u(memarg),
        (Rmw::Add, 2) => w.i64_atomic_rmw16_add_u(memarg),
        (Rmw::Add, 4) => w.i64_atomic_rmw32_add_u(memarg),
        (Rmw::Add, 8) => w.i64_atomic_rmw_add(memarg),
        (Rmw::Sub, 1) => w.i64_atomic_rmw8_sub_u(memarg),
        (Rmw::Sub, 2) => w.i64_atomic_rmw16_sub_u(memarg),
        (Rmw::Sub, 4) => w.i64_atomic_rmw32_sub_u(memarg),
        (Rmw::Sub, 8) => w.i64_atomic_rmw_sub(memarg),
        (Rmw::And, 1) => w.i64_atomic_rmw8_and_u(memarg),
        (Rmw::And, 2) => w.i64_atomic_rmw16_and_u(memarg),
        (Rmw::And, 4) => w.i64_atomic_rmw32_and_u(memarg),
        (Rmw::And, 8) => w.i64_atomic_rmw_and(memarg),
        (Rmw::Or, 1) => w.i64_atomic_rmw8_or_u(memarg),
        (Rmw::Or, 2) => w.i64_atomic_rmw16_or_u(memarg),
        (Rmw::Or, 4) => w.i64_atomic_rmw32_or_u(memarg),
        (Rmw::Or, 8) => w.i64_atomic_rmw_or(memarg),
        (Rmw::Xor, 1) => w.i64_atomic_rmw8_xor_u(memarg),
        (Rmw::Xor, 2) => w.i64_atomic_rmw16_xor_u(memarg),
        (Rmw::Xor, 4) => w.i64_atomic_rmw32_xor_u(memarg),
        (Rmw::Xor, 8) => w.i64_atomic_rmw_xor(memarg),
        (Rmw::Xchg, 1) => w.i64_atomic_rmw8_xchg_u(memarg),
        (Rmw::Xchg, 2) => w.i64_atomic_rmw16_xchg_u(memarg),
        (Rmw::Xchg, 4) => w.i64_atomic_rmw32_xchg_u(memarg),
        (Rmw::Xchg, 8) => w.i64_atomic_rmw_xchg(memarg),
        _ => return Err(Error::new(format!("atomic operation on {size} bytes"))),
    };
    Ok(())
}

fn cmpxchg_op(e: &mut Emitter<'_>, size: u32) -> Result<()> {
    let memarg = aligned(0, size.trailing_zeros());
    let mut w = e.w();
    match size {
        1 => w.i64_atomic_rmw8_cmpxchg_u(memarg),
        2 => w.i64_atomic_rmw16_cmpxchg_u(memarg),
        4 => w.i64_atomic_rmw32_cmpxchg_u(memarg),
        8 => w.i64_atomic_rmw_cmpxchg(memarg),
        _ => {
            return Err(Error::new(format!("atomic compare exchange on {size} bytes")));
        }
    };
    Ok(())
}

pub fn binary(e: &mut Emitter<'_>, instruction: &Instruction, op: Binary) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    let address = e.address(instruction)?;
    e.read(instruction, 1)?;
    let b = e.save();
    let kind = match op {
        Binary::Add => Rmw::Add,
        Binary::Sub => Rmw::Sub,
        Binary::And => Rmw::And,
        Binary::Or => Rmw::Or,
        Binary::Xor => Rmw::Xor,
        _ => return compare_exchange_loop(e, op, size, address, b),
    };
    e.w().local_get(address).local_get(b);
    rmw(e, kind, size)?;
    let old = e.save();
    let result = e.temp64();
    e.w().local_get(old).local_get(b);
    match kind {
        Rmw::Add => e.w().i64_add(),
        Rmw::Sub => e.w().i64_sub(),
        Rmw::And => e.w().i64_and(),
        Rmw::Or => e.w().i64_or(),
        _ => e.w().i64_xor(),
    };
    e.w().local_set(result);
    match kind {
        Rmw::Add => e.flags_add(old, b, result, size),
        Rmw::Sub => e.flags_sub(old, b, result, size),
        _ => e.flags_logic(result, size),
    }
    Ok(())
}

fn compare_exchange_loop(e: &mut Emitter<'_>, op: Binary, size: u32, address: u32, b: u32) -> Result<()> {
    let old = e.temp64();
    let result = e.temp64();
    e.w().loop_(wasm_encoder::BlockType::Empty);
    e.w().local_get(address);
    e.load(size)?;
    e.w().local_tee(old).local_get(b);
    match op {
        Binary::Adc => e.w().i64_add().local_get(CF).i64_extend_i32_u().i64_add(),
        _ => e.w().i64_sub().local_get(CF).i64_extend_i32_u().i64_sub(),
    };
    e.w().local_set(result).local_get(address).local_get(old).local_get(result);
    cmpxchg_op(e, size)?;
    e.w().local_get(old).i64_ne().br_if(0).end();
    match op {
        Binary::Adc => e.flags_add(old, b, result, size),
        _ => e.flags_sub(old, b, result, size),
    }
    Ok(())
}

pub fn unary(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    let address = e.address(instruction)?;
    let one = e.temp64();
    e.w().i64_const(1).local_set(one);
    match instruction.mnemonic() {
        Mnemonic::Inc | Mnemonic::Dec => {
            let add = instruction.mnemonic() == Mnemonic::Inc;
            e.w().local_get(address).local_get(one);
            rmw(e, if add { Rmw::Add } else { Rmw::Sub }, size)?;
            let old = e.save();
            let result = e.temp64();
            e.w().local_get(old).local_get(one);
            if add {
                e.w().i64_add().local_set(result);
                e.flags_add(old, one, result, size);
            } else {
                e.w().i64_sub().local_set(result);
                e.flags_sub(old, one, result, size);
            }
        }
        Mnemonic::Not => {
            e.w().local_get(address).i64_const(-1);
            rmw(e, Rmw::Xor, size)?;
            e.w().drop();
        }
        _ => {
            let old = e.temp64();
            let result = e.temp64();
            let zero = e.temp64();
            e.w().i64_const(0).local_set(zero);
            e.w().loop_(wasm_encoder::BlockType::Empty);
            e.w().local_get(address);
            e.load(size)?;
            e.w().local_set(old).i64_const(0).local_get(old).i64_sub().local_set(result);
            e.w().local_get(address).local_get(old).local_get(result);
            cmpxchg_op(e, size)?;
            e.w().local_get(old).i64_ne().br_if(0).end();
            e.flags_sub(zero, old, result, size);
        }
    }
    Ok(())
}

pub fn exchange(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let register = if instruction.op0_kind() == OpKind::Memory { 1 } else { 0 };
    let size = e.loc(instruction, register)?.size();
    let address = e.address(instruction)?;
    e.w().local_get(address);
    e.read(instruction, register)?;
    rmw(e, Rmw::Xchg, size)?;
    let old = e.save();
    e.write(instruction, register, old)
}

pub fn compare_exchange(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    e.read(instruction, 1)?;
    let source = e.save();
    let expected = e.temp64();
    e.w().local_get(gpr(0)).i64_const(crate::codegen::ops::operand::mask(size) as i64).i64_and().local_set(expected);
    let old = e.temp64();
    if instruction.op0_kind() == OpKind::Memory && instruction.has_lock_prefix() {
        let address = e.address(instruction)?;
        e.w().local_get(address).local_get(expected).local_get(source);
        cmpxchg_op(e, size)?;
        e.w().local_set(old);
    } else {
        e.read(instruction, 0)?;
        e.w().local_set(old);
        let value = e.temp64();
        e.w().local_get(source).local_get(old).local_get(old).local_get(expected).i64_eq().select().local_set(value);
        e.write(instruction, 0, value)?;
    }
    let result = e.temp64();
    e.w().local_get(expected).local_get(old).i64_sub().local_set(result);
    e.flags_sub(expected, old, result, size);
    e.w().local_get(old).local_get(expected).i64_ne();
    e.open_if();
    e.write_gpr(0, size, false, old);
    e.close();
    Ok(())
}

pub fn exchange_add(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    e.read(instruction, 1)?;
    let source = e.save();
    let old = e.temp64();
    let result = e.temp64();
    if instruction.op0_kind() == OpKind::Memory && instruction.has_lock_prefix() {
        let address = e.address(instruction)?;
        e.w().local_get(address).local_get(source);
        rmw(e, Rmw::Add, size)?;
        e.w().local_set(old).local_get(old).local_get(source).i64_add().local_set(result);
    } else {
        e.read(instruction, 0)?;
        e.w().local_set(old).local_get(old).local_get(source).i64_add().local_set(result);
        e.write(instruction, 1, old)?;
        e.write(instruction, 0, result)?;
        e.flags_add(old, source, result, size);
        return Ok(());
    }
    e.flags_add(old, source, result, size);
    e.write(instruction, 1, old)
}

pub fn bit_test(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    let bits = (size * 8) as i64;
    let index = e.temp64();
    if instruction.op1_kind() == OpKind::Register {
        e.read_signed(instruction, 1)?;
        let offset = e.save();
        e.push_address(instruction)?;
        e.w().local_get(offset).i64_const(bits.trailing_zeros() as i64).i64_shr_s().i64_const(size as i64).i64_mul().i64_add();
        let address = e.save();
        e.address = Some(address);
        e.w().local_get(offset).i64_const(bits - 1).i64_and().local_set(index);
    } else {
        e.read(instruction, 1)?;
        e.w().i64_const(bits - 1).i64_and().local_set(index);
    }
    let address = e.address(instruction)?;
    let bit = e.temp64();
    e.w().i64_const(1).local_get(index).i64_shl().local_set(bit);
    e.w().local_get(address);
    let op = match instruction.mnemonic() {
        Mnemonic::Bts => {
            e.w().local_get(bit);
            Rmw::Or
        }
        Mnemonic::Btr => {
            e.w().local_get(bit).i64_const(-1).i64_xor();
            Rmw::And
        }
        _ => {
            e.w().local_get(bit);
            Rmw::Xor
        }
    };
    rmw(e, op, size)?;
    let old = e.save();
    if e.want(RflagsBits::CF) {
        e.w().local_get(old).local_get(bit).i64_and().i64_const(0).i64_ne().local_set(CF);
    }
    Ok(())
}

pub fn compare_exchange_16(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let address = e.address(instruction)?;
    e.flush();
    let function = e.context.layout.cmpxchg16b;
    e.w().local_get(crate::codegen::function::CTX).local_get(address).call(function);
    e.reload();
    if e.want(RflagsBits::ZF) {
        e.w().local_get(crate::codegen::function::CTX).i64_load(crate::codegen::body::mem(crate::abi::cpu::RFLAGS)).i64_const(6).i64_shr_u();
        e.w().i32_wrap_i64().i32_const(1).i32_and().local_set(crate::codegen::function::ZF);
    }
    Ok(())
}
