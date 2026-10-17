use iced_x86::{Instruction, Mnemonic, OpKind, RflagsBits};
use wasm_encoder::BlockType;

use crate::codegen::function::{AF, CF, Emitter, OF, PF, SF, ZF, gpr};
use crate::codegen::ops::operand::{Loc, mask};
use crate::{Error, Result};

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Binary {
    Add,
    Adc,
    Sub,
    Sbb,
    And,
    Or,
    Xor,
    Cmp,
    Test,
}

pub fn binary(e: &mut Emitter<'_>, instruction: &Instruction, op: Binary) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    e.read(instruction, 0)?;
    let a = e.save();
    e.read(instruction, 1)?;
    let b = e.save();
    let result = e.temp64();
    e.w().local_get(a).local_get(b);
    match op {
        Binary::Add => e.w().i64_add(),
        Binary::Adc => e.w().i64_add().local_get(CF).i64_extend_i32_u().i64_add(),
        Binary::Sub | Binary::Cmp => e.w().i64_sub(),
        Binary::Sbb => e.w().i64_sub().local_get(CF).i64_extend_i32_u().i64_sub(),
        Binary::And | Binary::Test => e.w().i64_and(),
        Binary::Or => e.w().i64_or(),
        Binary::Xor => e.w().i64_xor(),
    };
    e.w().local_set(result);
    match op {
        Binary::Add | Binary::Adc => e.flags_add(a, b, result, size),
        Binary::Sub | Binary::Sbb | Binary::Cmp => e.flags_sub(a, b, result, size),
        _ => e.flags_logic(result, size),
    }
    if !matches!(op, Binary::Cmp | Binary::Test) {
        e.write(instruction, 0, result)?;
    }
    Ok(())
}

pub fn unary(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    e.read(instruction, 0)?;
    let a = e.save();
    let result = e.temp64();
    let other = e.temp64();
    match instruction.mnemonic() {
        Mnemonic::Inc => {
            e.w().i64_const(1).local_set(other).local_get(a).i64_const(1).i64_add().local_set(result);
            e.flags_add(a, other, result, size);
        }
        Mnemonic::Dec => {
            e.w().i64_const(1).local_set(other).local_get(a).i64_const(1).i64_sub().local_set(result);
            e.flags_sub(a, other, result, size);
        }
        Mnemonic::Neg => {
            e.w().i64_const(0).local_set(other).i64_const(0).local_get(a).i64_sub().local_set(result);
            e.flags_sub(other, a, result, size);
        }
        _ => {
            e.w().local_get(a).i64_const(-1).i64_xor().local_set(result);
        }
    }
    e.write(instruction, 0, result)
}

pub fn convert(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let (rax, rdx) = (gpr(0), gpr(2));
    let value = e.temp64();
    match instruction.mnemonic() {
        Mnemonic::Cbw => {
            e.w().local_get(rax).i64_extend8_s().local_set(value);
            e.write_gpr(0, 2, false, value);
        }
        Mnemonic::Cwde => {
            e.w().local_get(rax).i64_extend16_s().local_set(value);
            e.write_gpr(0, 4, false, value);
        }
        Mnemonic::Cdqe => {
            e.w().local_get(rax).i64_extend32_s().local_set(rax);
        }
        Mnemonic::Cwd => {
            e.w().local_get(rax).i64_extend16_s().i64_const(15).i64_shr_s().local_set(value);
            e.write_gpr(2, 2, false, value);
        }
        Mnemonic::Cdq => {
            e.w().local_get(rax).i64_extend32_s().i64_const(31).i64_shr_s().local_set(value);
            e.write_gpr(2, 4, false, value);
        }
        _ => {
            e.w().local_get(rax).i64_const(63).i64_shr_s().local_set(rdx);
        }
    }
    Ok(())
}

pub fn imul_truncating(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    let (first, second) = if instruction.op_count() == 3 { (1, 2) } else { (0, 1) };
    e.read_signed(instruction, first)?;
    let a = e.save();
    e.read_signed(instruction, second)?;
    let b = e.save();
    let product = e.temp64();
    e.w().local_get(a).local_get(b).i64_mul().local_set(product);
    if e.want(RflagsBits::CF) || e.want(RflagsBits::OF) {
        let overflow = e.temp32();
        if size == 8 {
            let smulh = e.context.layout.smulh;
            e.w().local_get(a).local_get(b).call(smulh).local_get(product).i64_const(63).i64_shr_s().i64_ne().local_set(overflow);
        } else {
            e.w().local_get(product);
            e.sign_extend(size);
            e.w().local_get(product).i64_ne().local_set(overflow);
        }
        e.w().local_get(overflow).local_set(CF).local_get(overflow).local_set(OF);
    }
    e.flags_result(product, size);
    e.write(instruction, 0, product)
}

pub fn multiply_wide(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let signed = instruction.mnemonic() == Mnemonic::Imul;
    let size = e.loc(instruction, 0)?.size();
    if signed {
        e.read_signed(instruction, 0)?;
    } else {
        e.read(instruction, 0)?;
    }
    let source = e.save();
    let a = e.temp64();
    e.w().local_get(gpr(0));
    if size < 8 {
        e.w().i64_const(mask(size) as i64).i64_and();
        if signed {
            e.sign_extend(size);
        }
    }
    e.w().local_set(a);
    let low = e.temp64();
    let high = e.temp64();
    e.w().local_get(a).local_get(source).i64_mul().local_set(low);
    let overflow = e.temp32();
    if size == 8 {
        let helper = if signed { e.context.layout.smulh } else { e.context.layout.umulh };
        e.w().local_get(a).local_get(source).call(helper).local_set(high);
        if signed {
            e.w().local_get(high).local_get(low).i64_const(63).i64_shr_s().i64_ne().local_set(overflow);
        } else {
            e.w().local_get(high).i64_const(0).i64_ne().local_set(overflow);
        }
        e.w().local_get(low).local_set(gpr(0)).local_get(high).local_set(gpr(2));
    } else {
        let bits = (size * 8) as i64;
        e.w().local_get(low).i64_const(bits).i64_shr_u().local_set(high);
        if signed {
            e.w().local_get(low);
            e.sign_extend(size);
            e.w().local_get(low);
            if size < 4 {
                e.w().i64_const(mask(size * 2) as i64).i64_and();
                e.sign_extend(size * 2);
            }
            e.w().i64_ne().local_set(overflow);
        } else {
            e.w().local_get(high).i64_const(mask(size) as i64).i64_and().i64_const(0).i64_ne().local_set(overflow);
        }
        if size == 1 {
            e.write_gpr(0, 2, false, low);
        } else {
            e.write_gpr(0, size, false, low);
            e.write_gpr(2, size, false, high);
        }
    }
    if e.want(RflagsBits::CF) || e.want(RflagsBits::OF) {
        e.w().local_get(overflow).local_set(CF).local_get(overflow).local_set(OF);
    }
    e.flags_result(low, size);
    Ok(())
}

pub fn divide(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let signed = instruction.mnemonic() == Mnemonic::Idiv;
    let size = e.loc(instruction, 0)?.size();
    if signed {
        e.read_signed(instruction, 0)?;
    } else {
        e.read(instruction, 0)?;
    }
    let divisor = e.save();
    let quotient = e.temp64();
    let remainder = e.temp64();
    if size == 8 {
        let helper = if signed { e.context.layout.sdiv128 } else { e.context.layout.udiv128 };
        e.w().local_get(gpr(2)).local_get(gpr(0)).local_get(divisor).call(helper).local_set(remainder).local_set(quotient);
        e.w().local_get(quotient).local_set(gpr(0)).local_get(remainder).local_set(gpr(2));
        return Ok(());
    }
    let bits = (size * 8) as i64;
    let dividend = e.temp64();
    if size == 1 {
        e.w().local_get(gpr(0)).i64_const(0xffff).i64_and();
        if signed {
            e.w().i64_extend16_s();
        }
    } else {
        e.w().local_get(gpr(2)).i64_const(mask(size) as i64).i64_and().i64_const(bits).i64_shl();
        e.w().local_get(gpr(0)).i64_const(mask(size) as i64).i64_and().i64_or();
        if signed && size < 4 {
            e.sign_extend(size * 2);
        }
    }
    e.w().local_set(dividend);
    e.w().local_get(divisor).i64_eqz().if_(BlockType::Empty).unreachable().end();
    if signed {
        e.w().local_get(dividend).local_get(divisor).i64_div_s().local_set(quotient);
        e.w().local_get(dividend).local_get(divisor).i64_rem_s().local_set(remainder);
        e.w().local_get(quotient);
        e.sign_extend(size);
        e.w().local_get(quotient).i64_ne().if_(BlockType::Empty).unreachable().end();
    } else {
        e.w().local_get(dividend).local_get(divisor).i64_div_u().local_set(quotient);
        e.w().local_get(dividend).local_get(divisor).i64_rem_u().local_set(remainder);
        e.w().local_get(quotient).i64_const(mask(size) as i64).i64_gt_u().if_(BlockType::Empty).unreachable().end();
    }
    if size == 1 {
        let packed = e.temp64();
        e.w().local_get(remainder).i64_const(0xff).i64_and().i64_const(8).i64_shl();
        e.w().local_get(quotient).i64_const(0xff).i64_and().i64_or().local_set(packed);
        e.write_gpr(0, 2, false, packed);
    } else {
        e.write_gpr(0, size, false, quotient);
        e.write_gpr(2, size, false, remainder);
    }
    Ok(())
}

fn shift_count(e: &mut Emitter<'_>, instruction: &Instruction, operand: u32, size: u32) -> Result<(u32, Option<u64>)> {
    let count_mask: u64 = if size == 8 { 0x3f } else { 0x1f };
    let count = e.temp64();
    match instruction.op_kind(operand) {
        OpKind::Register => {
            e.read(instruction, operand)?;
            e.w().i64_const(count_mask as i64).i64_and().local_set(count);
            Ok((count, None))
        }
        _ => {
            let value = match e.loc(instruction, operand)? {
                Loc::Imm { value, .. } => value & count_mask,
                _ => {
                    return Err(Error::new(format!("{:#x}: shift count operand", instruction.ip())));
                }
            };
            e.w().i64_const(value as i64).local_set(count);
            Ok((count, Some(value)))
        }
    }
}

pub fn shift(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let mnemonic = instruction.mnemonic();
    let size = e.loc(instruction, 0)?.size();
    let bits = size * 8;
    let top = bits - 1;
    let (count, constant) = shift_count(e, instruction, 1, size)?;
    e.read(instruction, 0)?;
    let a = e.save();
    let result = e.temp64();
    match mnemonic {
        Mnemonic::Shl | Mnemonic::Sal => {
            e.w().local_get(a).local_get(count).i64_shl().local_set(result);
        }
        Mnemonic::Shr => {
            e.w().local_get(a).local_get(count).i64_shr_u().local_set(result);
        }
        Mnemonic::Sar => {
            e.w().local_get(a);
            e.sign_extend(size);
            e.w().local_get(count).i64_shr_s().local_set(result);
        }
        Mnemonic::Rol | Mnemonic::Ror => {
            let amount = e.temp64();
            e.w().local_get(count).i64_const(bits as i64 - 1).i64_and().local_set(amount);
            if mnemonic == Mnemonic::Ror {
                e.w().i64_const(bits as i64).local_get(amount).i64_sub().i64_const(bits as i64 - 1).i64_and().local_set(amount);
            }
            if size == 8 {
                e.w().local_get(a).local_get(amount).i64_rotl().local_set(result);
            } else {
                e.w().local_get(a).local_get(amount).i64_shl().local_get(a).i64_const(bits as i64).local_get(amount).i64_sub().i64_shr_u().i64_or();
                e.w().i64_const(mask(size) as i64).i64_and().local_set(result);
            }
        }
        _ => return rotate_through_carry(e, instruction, count, a, size),
    }
    let rotate = matches!(mnemonic, Mnemonic::Rol | Mnemonic::Ror);
    if constant == Some(0) {
        return e.write(instruction, 0, result);
    }
    if constant.is_none() {
        e.w().local_get(count).i64_eqz().i32_eqz();
        e.open_if();
    }
    if e.want(RflagsBits::CF) {
        match mnemonic {
            Mnemonic::Shl | Mnemonic::Sal => {
                let shifted = e.temp64();
                e.w().local_get(a).i64_const(bits as i64).local_get(count).i64_sub().i64_shr_u().local_set(shifted);
                e.w().local_get(count).i64_const(bits as i64).i64_le_u().if_(BlockType::Result(wasm_encoder::ValType::I32));
                e.bit_of(shifted, 0);
                e.w().else_().i32_const(0).end().local_set(CF);
            }
            Mnemonic::Shr | Mnemonic::Sar => {
                let shifted = e.temp64();
                e.w().local_get(a);
                if mnemonic == Mnemonic::Sar {
                    e.sign_extend(size);
                    e.w().local_get(count).i64_const(1).i64_sub().i64_shr_s().local_set(shifted);
                } else {
                    e.w().local_get(count).i64_const(1).i64_sub().i64_shr_u().local_set(shifted);
                }
                e.bit_of(shifted, 0);
                e.w().local_set(CF);
            }
            Mnemonic::Rol => {
                e.bit_of(result, 0);
                e.w().local_set(CF);
            }
            _ => {
                e.bit_of(result, top);
                e.w().local_set(CF);
            }
        }
    }
    if e.want(RflagsBits::OF) {
        match mnemonic {
            Mnemonic::Shl | Mnemonic::Sal | Mnemonic::Rol => {
                e.bit_of(result, top);
                e.w().local_get(CF).i32_xor().local_set(OF);
            }
            Mnemonic::Shr => {
                e.bit_of(a, top);
                e.w().local_set(OF);
            }
            Mnemonic::Sar => {
                e.w().i32_const(0).local_set(OF);
            }
            _ => {
                e.bit_of(result, top);
                e.bit_of(result, top - 1);
                e.w().i32_xor().local_set(OF);
            }
        }
    }
    if !rotate {
        e.flags_result(result, size);
    }
    if constant.is_none() {
        e.close();
    }
    e.write(instruction, 0, result)
}

fn rotate_through_carry(e: &mut Emitter<'_>, instruction: &Instruction, count: u32, a: u32, size: u32) -> Result<()> {
    let left = instruction.mnemonic() == Mnemonic::Rcl;
    let top = size * 8 - 1;
    let steps = e.temp64();
    let value = e.temp64();
    let carry = e.temp32();
    let next = e.temp32();
    e.w().local_get(count);
    if size < 4 {
        e.w().i64_const(size as i64 * 8 + 1).i64_rem_u();
    }
    e.w().local_set(steps).local_get(a).local_set(value).local_get(CF).local_set(carry);
    e.w().block(BlockType::Empty).loop_(BlockType::Empty);
    e.w().local_get(steps).i64_eqz().br_if(1);
    if left {
        e.bit_of(value, top);
        e.w().local_set(next).local_get(value).i64_const(1).i64_shl().local_get(carry).i64_extend_i32_u().i64_or().local_set(value);
    } else {
        e.bit_of(value, 0);
        e.w().local_set(next).local_get(value).i64_const(mask(size) as i64).i64_and().i64_const(1).i64_shr_u();
        e.w().local_get(carry).i64_extend_i32_u().i64_const(top as i64).i64_shl().i64_or().local_set(value);
    }
    e.w().local_get(next).local_set(carry).local_get(steps).i64_const(1).i64_sub().local_set(steps).br(0);
    e.w().end().end();
    e.w().local_get(count).i64_eqz().i32_eqz();
    e.open_if();
    if e.want(RflagsBits::OF) {
        if left {
            e.bit_of(value, top);
            e.w().local_get(carry).i32_xor().local_set(OF);
        } else {
            e.bit_of(value, top);
            e.bit_of(value, top - 1);
            e.w().i32_xor().local_set(OF);
        }
    }
    if e.want(RflagsBits::CF) {
        e.w().local_get(carry).local_set(CF);
    }
    e.close();
    e.write(instruction, 0, value)
}

pub fn double_shift(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let left = instruction.mnemonic() == Mnemonic::Shld;
    let size = e.loc(instruction, 0)?.size();
    let bits = (size * 8) as i64;
    let (count, constant) = shift_count(e, instruction, 2, size)?;
    e.read(instruction, 0)?;
    let a = e.save();
    if constant == Some(0) {
        return e.write(instruction, 0, a);
    }
    e.read(instruction, 1)?;
    let b = e.save();
    let result = e.temp64();
    let out = e.temp64();
    if left {
        if size == 8 {
            e.w().local_get(a).local_get(count).i64_shl().local_get(b).i64_const(64).local_get(count).i64_sub().i64_shr_u().i64_or().local_set(result);
        } else {
            e.w().local_get(a).i64_const(bits).i64_shl().local_get(b).i64_or().local_get(count).i64_shl().i64_const(bits).i64_shr_u();
            e.w().i64_const(mask(size) as i64).i64_and().local_set(result);
        }
        e.w().local_get(a).i64_const(bits).local_get(count).i64_sub().i64_shr_u().local_set(out);
    } else {
        if size == 8 {
            e.w().local_get(a).local_get(count).i64_shr_u().local_get(b).i64_const(64).local_get(count).i64_sub().i64_shl().i64_or().local_set(result);
        } else {
            e.w().local_get(b).i64_const(bits).i64_shl().local_get(a).i64_or().local_get(count).i64_shr_u();
            e.w().i64_const(mask(size) as i64).i64_and().local_set(result);
        }
        e.w().local_get(a).local_get(count).i64_const(1).i64_sub().i64_shr_u().local_set(out);
    }
    if constant.is_none() {
        e.w().local_get(count).i64_eqz().i32_eqz();
        e.open_if();
    }
    if e.want(RflagsBits::CF) {
        e.bit_of(out, 0);
        e.w().local_set(CF);
    }
    if e.want(RflagsBits::OF) {
        e.bit_of(result, size * 8 - 1);
        e.bit_of(a, size * 8 - 1);
        e.w().i32_xor().local_set(OF);
    }
    e.flags_result(result, size);
    if constant.is_none() {
        e.close();
    }
    e.write(instruction, 0, result)
}

pub fn bit_test(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    let bits = (size * 8) as i64;
    let index = e.temp64();
    let register_index = instruction.op1_kind() == OpKind::Register;
    if register_index && instruction.op0_kind() == OpKind::Memory {
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
    e.read(instruction, 0)?;
    let value = e.save();
    let bit = e.temp64();
    e.w().i64_const(1).local_get(index).i64_shl().local_set(bit);
    if e.want(RflagsBits::CF) {
        e.w().local_get(value).local_get(bit).i64_and().i64_const(0).i64_ne().local_set(CF);
    }
    let result = e.temp64();
    match instruction.mnemonic() {
        Mnemonic::Bt => return Ok(()),
        Mnemonic::Bts => e.w().local_get(value).local_get(bit).i64_or(),
        Mnemonic::Btr => e.w().local_get(value).local_get(bit).i64_const(-1).i64_xor().i64_and(),
        _ => e.w().local_get(value).local_get(bit).i64_xor(),
    };
    e.w().local_set(result);
    e.write(instruction, 0, result)
}

pub fn bit_scan(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    let bits = (size * 8) as i64;
    e.read(instruction, 1)?;
    let source = e.save();
    let result = e.temp64();
    match instruction.mnemonic() {
        Mnemonic::Bsf | Mnemonic::Bsr => {
            if e.want(RflagsBits::ZF) {
                e.w().local_get(source).i64_eqz().local_set(ZF);
            }
            e.w().local_get(source).i64_eqz().i32_eqz();
            e.open_if();
            if instruction.mnemonic() == Mnemonic::Bsf {
                e.w().local_get(source).i64_ctz().local_set(result);
            } else {
                e.w().i64_const(63).local_get(source).i64_clz().i64_sub().local_set(result);
            }
            e.write(instruction, 0, result)?;
            e.close();
            return Ok(());
        }
        Mnemonic::Tzcnt => {
            e.w().i64_const(bits).local_get(source).i64_ctz().local_get(source).i64_eqz().select().local_set(result);
            if e.want(RflagsBits::CF) {
                e.w().local_get(source).i64_eqz().local_set(CF);
            }
        }
        Mnemonic::Lzcnt => {
            e.w().local_get(source).i64_clz().i64_const(64 - bits).i64_sub().local_set(result);
            if e.want(RflagsBits::CF) {
                e.w().local_get(source).i64_eqz().local_set(CF);
            }
        }
        _ => {
            e.w().local_get(source).i64_popcnt().local_set(result);
            for (flag, local) in [(RflagsBits::CF, CF), (RflagsBits::OF, OF), (RflagsBits::SF, SF), (RflagsBits::AF, AF), (RflagsBits::PF, PF)] {
                e.set_flag(flag, local, false);
            }
        }
    }
    if e.want(RflagsBits::ZF) {
        e.w().local_get(result).i64_eqz().local_set(ZF);
    }
    e.write(instruction, 0, result)
}

pub fn bswap(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    e.read(instruction, 0)?;
    let value = e.save();
    let result = swap_bytes(e, value, size);
    e.write(instruction, 0, result)
}

pub fn movbe(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    e.read(instruction, 1)?;
    let value = e.save();
    let result = swap_bytes(e, value, size);
    e.write(instruction, 0, result)
}

fn swap_bytes(e: &mut Emitter<'_>, value: u32, size: u32) -> u32 {
    let result = e.temp64();
    e.w().i64_const(0).local_set(result);
    for byte in 0..size as i64 {
        let shift = 8 * byte;
        let target = 8 * (size as i64 - 1 - byte);
        e.w().local_get(result).local_get(value).i64_const(shift).i64_shr_u().i64_const(0xff).i64_and().i64_const(target).i64_shl().i64_or().local_set(result);
    }
    result
}
