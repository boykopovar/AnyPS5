use iced_x86::{Instruction, Mnemonic, RflagsBits};
use wasm_encoder::ValType;

use super::VecLoc;
use crate::Result;
use crate::codegen::function::{AF, CF, Emitter, OF, PF, SF, ZF};

#[derive(Clone, Copy, PartialEq, Eq)]
enum Shift {
    Left,
    Logical,
    Arithmetic,
}

pub fn swap(e: &mut Emitter<'_>) {
    let top = e.save128();
    let below = e.save128();
    e.w().local_get(top).local_get(below);
}

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Option<Result<()>> {
    use Mnemonic::*;
    macro_rules! lanes {
        ($method:ident) => {
            e.binary_lanes(instruction, &|e| {
                e.w().$method();
            })
        };
    }
    Some(match mnemonic {
        Pand | Vpand | Andps | Vandps | Andpd | Vandpd => lanes!(v128_and),
        Por | Vpor | Orps | Vorps | Orpd | Vorpd => lanes!(v128_or),
        Pxor | Vpxor | Xorps | Vxorps | Xorpd | Vxorpd => lanes!(v128_xor),
        Pandn | Vpandn | Andnps | Vandnps | Andnpd | Vandnpd => e.binary_lanes(instruction, &|e| {
            swap(e);
            e.w().v128_andnot();
        }),
        Paddb | Vpaddb => lanes!(i8x16_add),
        Paddw | Vpaddw => lanes!(i16x8_add),
        Paddd | Vpaddd => lanes!(i32x4_add),
        Paddq | Vpaddq => lanes!(i64x2_add),
        Psubb | Vpsubb => lanes!(i8x16_sub),
        Psubw | Vpsubw => lanes!(i16x8_sub),
        Psubd | Vpsubd => lanes!(i32x4_sub),
        Psubq | Vpsubq => lanes!(i64x2_sub),
        Paddsb | Vpaddsb => lanes!(i8x16_add_sat_s),
        Paddsw | Vpaddsw => lanes!(i16x8_add_sat_s),
        Paddusb | Vpaddusb => lanes!(i8x16_add_sat_u),
        Paddusw | Vpaddusw => lanes!(i16x8_add_sat_u),
        Psubsb | Vpsubsb => lanes!(i8x16_sub_sat_s),
        Psubsw | Vpsubsw => lanes!(i16x8_sub_sat_s),
        Psubusb | Vpsubusb => lanes!(i8x16_sub_sat_u),
        Psubusw | Vpsubusw => lanes!(i16x8_sub_sat_u),
        Pcmpeqb | Vpcmpeqb => lanes!(i8x16_eq),
        Pcmpeqw | Vpcmpeqw => lanes!(i16x8_eq),
        Pcmpeqd | Vpcmpeqd => lanes!(i32x4_eq),
        Pcmpeqq | Vpcmpeqq => lanes!(i64x2_eq),
        Pcmpgtb | Vpcmpgtb => lanes!(i8x16_gt_s),
        Pcmpgtw | Vpcmpgtw => lanes!(i16x8_gt_s),
        Pcmpgtd | Vpcmpgtd => lanes!(i32x4_gt_s),
        Pcmpgtq | Vpcmpgtq => lanes!(i64x2_gt_s),
        Pmaxsb | Vpmaxsb => lanes!(i8x16_max_s),
        Pmaxub | Vpmaxub => lanes!(i8x16_max_u),
        Pmaxsw | Vpmaxsw => lanes!(i16x8_max_s),
        Pmaxuw | Vpmaxuw => lanes!(i16x8_max_u),
        Pmaxsd | Vpmaxsd => lanes!(i32x4_max_s),
        Pmaxud | Vpmaxud => lanes!(i32x4_max_u),
        Pminsb | Vpminsb => lanes!(i8x16_min_s),
        Pminub | Vpminub => lanes!(i8x16_min_u),
        Pminsw | Vpminsw => lanes!(i16x8_min_s),
        Pminuw | Vpminuw => lanes!(i16x8_min_u),
        Pminsd | Vpminsd => lanes!(i32x4_min_s),
        Pminud | Vpminud => lanes!(i32x4_min_u),
        Pavgb | Vpavgb => lanes!(i8x16_avgr_u),
        Pavgw | Vpavgw => lanes!(i16x8_avgr_u),
        Pmullw | Vpmullw => lanes!(i16x8_mul),
        Pmulld | Vpmulld => lanes!(i32x4_mul),
        Pmaddwd | Vpmaddwd => lanes!(i32x4_dot_i16x8_s),
        Pmuludq | Vpmuludq => e.binary_lanes(instruction, &|e| {
            let low = 0xffff_ffffi128 | (0xffff_ffffi128 << 64);
            e.w().v128_const(low).v128_and();
            swap(e);
            e.w().v128_const(low).v128_and().i64x2_mul();
        }),
        Pmuldq | Vpmuldq => e.binary_lanes(instruction, &|e| {
            e.w().i32_const(32).i64x2_shl().i32_const(32).i64x2_shr_s();
            swap(e);
            e.w().i32_const(32).i64x2_shl().i32_const(32).i64x2_shr_s().i64x2_mul();
        }),
        Pmulhw | Vpmulhw | Pmulhuw | Vpmulhuw => {
            let signed = matches!(mnemonic, Pmulhw | Vpmulhw);
            e.binary_lanes(instruction, &|e| {
                let b = e.save128();
                let a = e.save128();
                for high in [false, true] {
                    e.w().local_get(a).local_get(b);
                    let mut w = e.w();
                    match (signed, high) {
                        (true, false) => w.i32x4_extmul_low_i16x8_s().i32_const(16).i32x4_shr_s(),
                        (true, true) => w.i32x4_extmul_high_i16x8_s().i32_const(16).i32x4_shr_s(),
                        (false, false) => w.i32x4_extmul_low_i16x8_u().i32_const(16).i32x4_shr_u(),
                        (false, true) => w.i32x4_extmul_high_i16x8_u().i32_const(16).i32x4_shr_u(),
                    };
                }
                if signed {
                    e.w().i16x8_narrow_i32x4_s();
                } else {
                    e.w().i16x8_narrow_i32x4_u();
                }
            })
        }
        Pabsb | Vpabsb => e.unary_lanes(instruction, 1, &|e| {
            e.w().i8x16_abs();
        }),
        Pabsw | Vpabsw => e.unary_lanes(instruction, 1, &|e| {
            e.w().i16x8_abs();
        }),
        Pabsd | Vpabsd => e.unary_lanes(instruction, 1, &|e| {
            e.w().i32x4_abs();
        }),
        Packsswb | Vpacksswb => lanes!(i8x16_narrow_i16x8_s),
        Packuswb | Vpackuswb => lanes!(i8x16_narrow_i16x8_u),
        Packssdw | Vpackssdw => lanes!(i16x8_narrow_i32x4_s),
        Packusdw | Vpackusdw => lanes!(i16x8_narrow_i32x4_u),
        Psllw | Vpsllw => shift(e, instruction, 2, Shift::Left),
        Pslld | Vpslld => shift(e, instruction, 4, Shift::Left),
        Psllq | Vpsllq => shift(e, instruction, 8, Shift::Left),
        Psrlw | Vpsrlw => shift(e, instruction, 2, Shift::Logical),
        Psrld | Vpsrld => shift(e, instruction, 4, Shift::Logical),
        Psrlq | Vpsrlq => shift(e, instruction, 8, Shift::Logical),
        Psraw | Vpsraw => shift(e, instruction, 2, Shift::Arithmetic),
        Psrad | Vpsrad => shift(e, instruction, 4, Shift::Arithmetic),
        Vpsllvd => variable_shift(e, instruction, 4, Shift::Left),
        Vpsllvq => variable_shift(e, instruction, 8, Shift::Left),
        Vpsrlvd => variable_shift(e, instruction, 4, Shift::Logical),
        Vpsrlvq => variable_shift(e, instruction, 8, Shift::Logical),
        Vpsravd => variable_shift(e, instruction, 4, Shift::Arithmetic),
        Ptest | Vptest => ptest(e, instruction),
        _ => return None,
    })
}

fn apply_shift(e: &mut Emitter<'_>, size: u32, kind: Shift) {
    let mut w = e.w();
    match (size, kind) {
        (2, Shift::Left) => w.i16x8_shl(),
        (2, Shift::Logical) => w.i16x8_shr_u(),
        (2, Shift::Arithmetic) => w.i16x8_shr_s(),
        (4, Shift::Left) => w.i32x4_shl(),
        (4, Shift::Logical) => w.i32x4_shr_u(),
        (4, Shift::Arithmetic) => w.i32x4_shr_s(),
        (_, Shift::Left) => w.i64x2_shl(),
        (_, Shift::Logical) => w.i64x2_shr_u(),
        (_, Shift::Arithmetic) => w.i64x2_shr_s(),
    };
}

fn shift(e: &mut Emitter<'_>, instruction: &Instruction, size: u32, kind: Shift) -> Result<()> {
    let bits = size as u64 * 8;
    let (source, count_operand) = if super::is_vex(instruction) { (1, 2) } else { (0, 1) };
    if let VecLoc::Imm(count) = e.vloc(instruction, count_operand)? {
        return e.unary_lanes(instruction, source, &|e| {
            if count >= bits && kind != Shift::Arithmetic {
                e.w().drop().v128_const(0);
            } else {
                e.w().i32_const(count.min(bits - 1) as i32);
                apply_shift(e, size, kind);
            }
        });
    }
    e.vlow(instruction, count_operand)?;
    let count = e.temp64();
    e.w().i64x2_extract_lane(0).local_set(count);
    e.unary_lanes(instruction, source, &|e| {
        if kind == Shift::Arithmetic {
            e.w().local_get(count).i64_const(bits as i64 - 1).local_get(count).i64_const(bits as i64 - 1).i64_lt_u().select().i32_wrap_i64();
            apply_shift(e, size, kind);
        } else {
            e.w().local_get(count).i32_wrap_i64();
            apply_shift(e, size, kind);
            e.w().v128_const(0).local_get(count).i64_const(bits as i64).i64_lt_u().select();
        }
    })
}

fn variable_shift(e: &mut Emitter<'_>, instruction: &Instruction, size: u32, kind: Shift) -> Result<()> {
    let bits = size as i64 * 8;
    let scalar = if size == 4 { ValType::I32 } else { ValType::I64 };
    e.binary_lanes(instruction, &|e| {
        let counts = e.save128();
        let values = e.save128();
        let value = e.body.temp(scalar);
        let count = e.body.temp(scalar);
        e.w().local_get(values);
        for lane in 0..(16 / size) as u8 {
            if size == 4 {
                e.w().local_get(values).i32x4_extract_lane(lane).local_set(value);
                e.w().local_get(counts).i32x4_extract_lane(lane).local_set(count);
                match kind {
                    Shift::Arithmetic => {
                        e.w().local_get(value).local_get(count).i32_const(31).local_get(count).i32_const(31).i32_lt_u().select().i32_shr_s();
                    }
                    _ => {
                        e.w().local_get(value).local_get(count);
                        if kind == Shift::Left {
                            e.w().i32_shl();
                        } else {
                            e.w().i32_shr_u();
                        }
                        e.w().i32_const(0).local_get(count).i32_const(bits as i32).i32_lt_u().select();
                    }
                }
                e.w().i32x4_replace_lane(lane);
            } else {
                e.w().local_get(values).i64x2_extract_lane(lane).local_set(value);
                e.w().local_get(counts).i64x2_extract_lane(lane).local_set(count);
                match kind {
                    Shift::Arithmetic => {
                        e.w().local_get(value).local_get(count).i64_const(63).local_get(count).i64_const(63).i64_lt_u().select().i64_shr_s();
                    }
                    _ => {
                        e.w().local_get(value).local_get(count);
                        if kind == Shift::Left {
                            e.w().i64_shl();
                        } else {
                            e.w().i64_shr_u();
                        }
                        e.w().i64_const(0).local_get(count).i64_const(bits).i64_lt_u().select();
                    }
                }
                e.w().i64x2_replace_lane(lane);
            }
        }
    })
}

fn ptest(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let wide = instruction.op0_register().is_ymm();
    let mut halves = vec![false];
    if wide {
        halves.push(true);
    }
    let zero = e.temp32();
    let carry = e.temp32();
    e.w().i32_const(0).local_set(zero).i32_const(0).local_set(carry);
    for high in halves {
        for operand in [0, 1] {
            if high {
                e.vhigh(instruction, operand)?;
            } else {
                e.vlow(instruction, operand)?;
            }
        }
        let b = e.save128();
        let a = e.save128();
        e.w().local_get(a).local_get(b).v128_and().v128_any_true().local_get(zero).i32_or().local_set(zero);
        e.w().local_get(b).local_get(a).v128_andnot().v128_any_true().local_get(carry).i32_or().local_set(carry);
    }
    if e.want(RflagsBits::ZF) {
        e.w().local_get(zero).i32_eqz().local_set(ZF);
    }
    if e.want(RflagsBits::CF) {
        e.w().local_get(carry).i32_eqz().local_set(CF);
    }
    for (flag, local) in [(RflagsBits::OF, OF), (RflagsBits::SF, SF), (RflagsBits::AF, AF), (RflagsBits::PF, PF)] {
        e.set_flag(flag, local, false);
    }
    Ok(())
}
