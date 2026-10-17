use iced_x86::{Instruction, Mnemonic, OpKind, RflagsBits};
use wasm_encoder::BlockType;

use super::is_vex;
use crate::codegen::body::mem;
use crate::codegen::function::{AF, CF, Emitter, OF, PF, SF, ZF, gpr, xhi, xlo};
use crate::{Error, Result};

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Option<Result<()>> {
    use Mnemonic::*;
    Some(match mnemonic {
        Pcmpistri | Vpcmpistri => compare(e, instruction, false, true),
        Pcmpestri | Vpcmpestri => compare(e, instruction, true, true),
        Pcmpistrm | Vpcmpistrm => compare(e, instruction, false, false),
        Pcmpestrm | Vpcmpestrm => compare(e, instruction, true, false),
        Vpmaskmovd | Vmaskmovps => masked_move(e, instruction, 4),
        Vpmaskmovq | Vmaskmovpd => masked_move(e, instruction, 8),
        _ => return None,
    })
}

fn length(e: &mut Emitter<'_>, vector: u32, words: bool, explicit_register: Option<u32>) {
    let count = if words { 8 } else { 16 };
    match explicit_register {
        Some(register) => {
            let value = e.temp32();
            e.w().local_get(gpr(register)).i32_wrap_i64().local_set(value);
            e.w().i32_const(0).local_get(value).i32_sub().local_get(value).local_get(value).i32_const(0).i32_lt_s().select().local_set(value);
            e.w().i32_const(count).local_get(value).local_get(value).i32_const(count).i32_gt_u().select();
        }
        None => {
            e.w().local_get(vector).v128_const(0);
            if words {
                e.w().i16x8_eq().i16x8_bitmask();
            } else {
                e.w().i8x16_eq().i8x16_bitmask();
            }
            e.w().i32_const(1 << count).i32_or().i32_ctz();
        }
    }
}

fn compare(e: &mut Emitter<'_>, instruction: &Instruction, explicit: bool, index: bool) -> Result<()> {
    let control = instruction.immediate8() as i32;
    let words = control & 1 != 0;
    let count = if words { 8 } else { 16 };
    e.vlow(instruction, 0)?;
    let a = e.save128();
    e.vlow(instruction, 1)?;
    let b = e.save128();
    let length_a = e.temp32();
    let length_b = e.temp32();
    length(e, a, words, explicit.then_some(0));
    e.w().local_set(length_a);
    length(e, b, words, explicit.then_some(2));
    e.w().local_set(length_b);
    let result = e.temp32();
    let helper = e.context.layout.pcmpstr;
    e.w().local_get(a).local_get(b).local_get(length_a).local_get(length_b).i32_const(control).call(helper).local_set(result);
    if index {
        let position = e.temp64();
        e.w().i32_const(count);
        if control & 0x40 != 0 {
            e.w().i32_const(31).local_get(result).i32_clz().i32_sub();
        } else {
            e.w().local_get(result).i32_ctz();
        }
        e.w().local_get(result).i32_eqz().select().i64_extend_i32_u().local_set(position);
        e.write_gpr(1, 4, false, position);
    } else {
        if control & 0x40 != 0 {
            if words {
                e.w().local_get(result).i16x8_splat();
                let bits = i128::from_le_bytes([1, 0, 2, 0, 4, 0, 8, 0, 16, 0, 32, 0, 64, 0, 128, 0]);
                e.w().v128_const(bits).v128_and().v128_const(bits).i16x8_eq();
            } else {
                e.w().local_get(result).i16x8_splat().v128_const(0).i8x16_shuffle([0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]);
                let bits = i128::from_le_bytes([1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128]);
                e.w().v128_const(bits).v128_and().v128_const(bits).i8x16_eq();
            }
        } else {
            e.w().v128_const(0).local_get(result).i32x4_replace_lane(0);
        }
        e.w().local_set(xlo(0));
        if is_vex(instruction) {
            e.w().v128_const(0).local_set(xhi(0));
        }
    }
    if e.want(RflagsBits::CF) {
        e.w().local_get(result).i32_eqz().i32_eqz().local_set(CF);
    }
    if e.want(RflagsBits::ZF) {
        e.w().local_get(length_b).i32_const(count).i32_lt_u().local_set(ZF);
    }
    if e.want(RflagsBits::SF) {
        e.w().local_get(length_a).i32_const(count).i32_lt_u().local_set(SF);
    }
    if e.want(RflagsBits::OF) {
        e.w().local_get(result).i32_const(1).i32_and().local_set(OF);
    }
    e.set_flag(RflagsBits::AF, AF, false);
    e.set_flag(RflagsBits::PF, PF, false);
    Ok(())
}

fn masked_move(e: &mut Emitter<'_>, instruction: &Instruction, size: u32) -> Result<()> {
    let store = instruction.op0_kind() == OpKind::Memory;
    let wide = if store { instruction.op2_register().is_ymm() } else { instruction.op0_register().is_ymm() };
    let address = e.address(instruction)?;
    let lanes = 16 / size;
    let mut halves = Vec::new();
    for half in 0..(if wide { 2 } else { 1 }) {
        if half == 0 {
            e.vlow(instruction, 1)?
        } else {
            e.vhigh(instruction, 1)?
        }
        let mask = e.save128();
        let value = e.temp128();
        if store {
            if half == 0 {
                e.vlow(instruction, 2)?
            } else {
                e.vhigh(instruction, 2)?
            }
            e.w().local_set(value);
        } else {
            e.w().v128_const(0).local_set(value);
        }
        for lane in 0..lanes as u8 {
            let offset = (half * 16 + lane as u32 * size) as u64;
            if size == 4 {
                e.w().local_get(mask).i32x4_extract_lane(lane).i32_const(0).i32_lt_s();
            } else {
                e.w().local_get(mask).i64x2_extract_lane(lane).i64_const(0).i64_lt_s();
            }
            e.w().if_(BlockType::Empty);
            match (store, size) {
                (true, 4) => e.w().local_get(address).local_get(value).i32x4_extract_lane(lane).i32_store(mem(offset)),
                (true, _) => e.w().local_get(address).local_get(value).i64x2_extract_lane(lane).i64_store(mem(offset)),
                (false, 4) => e.w().local_get(value).local_get(address).i32_load(mem(offset)).i32x4_replace_lane(lane).local_set(value),
                (false, _) => e.w().local_get(value).local_get(address).i64_load(mem(offset)).i64x2_replace_lane(lane).local_set(value),
            };
            e.w().end();
        }
        halves.push(value);
    }
    if store {
        return Ok(());
    }
    let high = if wide { Some(halves[1]) } else { None };
    if !wide && !is_vex(instruction) {
        return Err(Error::new("legacy masked move"));
    }
    e.vwrite(instruction, 0, halves[0], high)
}
