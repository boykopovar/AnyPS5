use iced_x86::{Instruction, Mnemonic};

use super::{elements, is_vex};
use crate::Result;
use crate::codegen::function::Emitter;

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Option<Result<()>> {
    use Mnemonic::*;
    let imm = || instruction.immediate8();
    Some(match mnemonic {
        Pshufd | Vpshufd => {
            let control = imm();
            let order: Vec<u8> = (0..4).map(|lane| (control >> (2 * lane)) & 3).collect();
            e.unary_lanes(instruction, 1, &|e| {
                e.w().v128_const(0).i8x16_shuffle(elements(4, &order));
            })
        }
        Pshuflw | Vpshuflw | Pshufhw | Vpshufhw => {
            let control = imm();
            let high = matches!(mnemonic, Pshufhw | Vpshufhw);
            let mut order: Vec<u8> = (0..8).collect();
            for lane in 0..4 {
                let selected = (control >> (2 * lane)) & 3;
                if high {
                    order[4 + lane as usize] = 4 + selected;
                } else {
                    order[lane as usize] = selected;
                }
            }
            e.unary_lanes(instruction, 1, &|e| {
                e.w().v128_const(0).i8x16_shuffle(elements(2, &order));
            })
        }
        Pshufb | Vpshufb => e.binary_lanes(instruction, &|e| {
            e.w().v128_const(i128::from_le_bytes([0x8f; 16])).v128_and().i8x16_swizzle();
        }),
        Shufps | Vshufps => {
            let control = imm();
            let order: Vec<u8> = (0..4).map(|lane| ((control >> (2 * lane)) & 3) + if lane >= 2 { 4 } else { 0 }).collect();
            e.binary_lanes(instruction, &|e| {
                e.w().i8x16_shuffle(elements(4, &order));
            })
        }
        Shufpd | Vshufpd => {
            let control = imm();
            per_half(e, instruction, &|half| {
                let bits = control >> (2 * half);
                elements(8, &[bits & 1, 2 + ((bits >> 1) & 1)])
            })
        }
        Unpcklps | Vunpcklps | Punpckldq | Vpunpckldq => interleave(e, instruction, 4, false),
        Unpckhps | Vunpckhps | Punpckhdq | Vpunpckhdq => interleave(e, instruction, 4, true),
        Unpcklpd | Vunpcklpd | Punpcklqdq | Vpunpcklqdq => interleave(e, instruction, 8, false),
        Unpckhpd | Vunpckhpd | Punpckhqdq | Vpunpckhqdq => interleave(e, instruction, 8, true),
        Punpcklbw | Vpunpcklbw => interleave(e, instruction, 1, false),
        Punpckhbw | Vpunpckhbw => interleave(e, instruction, 1, true),
        Punpcklwd | Vpunpcklwd => interleave(e, instruction, 2, false),
        Punpckhwd | Vpunpckhwd => interleave(e, instruction, 2, true),
        Vpermilps if instruction.op_count() == 3 && instruction.op2_kind() == iced_x86::OpKind::Immediate8 => {
            let control = imm();
            let order: Vec<u8> = (0..4).map(|lane| (control >> (2 * lane)) & 3).collect();
            e.unary_lanes(instruction, 1, &|e| {
                e.w().v128_const(0).i8x16_shuffle(elements(4, &order));
            })
        }
        Vpermilpd if instruction.op2_kind() == iced_x86::OpKind::Immediate8 => {
            let control = imm();
            unary_per_half(e, instruction, 1, &|half| {
                let bits = control >> (2 * half);
                elements(8, &[bits & 1, (bits >> 1) & 1])
            })
        }
        Vpermilps => e.binary_lanes(instruction, &|e| {
            e.w().v128_const(i128::from_le_bytes([3, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0])).v128_and();
            e.w().v128_const(i128::from_le_bytes([4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4])).i32x4_mul();
            e.w().v128_const(i128::from_le_bytes([0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3])).i32x4_add().i8x16_swizzle();
        }),
        Vpermilpd => e.binary_lanes(instruction, &|e| {
            e.w().i32_const(1).i64x2_shr_u().v128_const(1i128 | (1i128 << 64)).v128_and();
            e.w().v128_const(i128::from_le_bytes([8; 16])).i64x2_mul();
            e.w().v128_const(i128::from_le_bytes([0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7])).i8x16_add().i8x16_swizzle();
        }),
        Vpermq | Vpermpd => {
            let control = imm();
            let order: Vec<u8> = (0..4).map(|lane| (control >> (2 * lane)) & 3).collect();
            cross_permute(e, instruction, &order)
        }
        Vpermd | Vpermps => permute_dwords(e, instruction),
        Vperm2f128 | Vperm2i128 => permute_halves(e, instruction),
        Blendps | Vblendps | Blendpd | Vblendpd | Vpblendd | Pblendw | Vpblendw => {
            let control = imm();
            let size: u8 = match mnemonic {
                Blendps | Vblendps | Vpblendd => 4,
                Blendpd | Vblendpd => 8,
                _ => 2,
            };
            let count = 16 / size;
            let same_for_both = size == 2;
            per_half(e, instruction, &|half| {
                let bits = if same_for_both { control } else { control >> (count * half) };
                let order: Vec<u8> = (0..count).map(|lane| lane + if bits & (1 << lane) != 0 { count } else { 0 }).collect();
                elements(size, &order)
            })
        }
        Blendvps | Vblendvps => variable_blend(e, instruction, 4),
        Blendvpd | Vblendvpd => variable_blend(e, instruction, 8),
        Pblendvb | Vpblendvb => variable_blend(e, instruction, 1),
        Palignr | Vpalignr => {
            let shift = imm();
            e.binary_lanes(instruction, &|e| {
                let (first, second) = (e.save128(), e.save128());
                let mut lanes = [0u8; 16];
                if shift < 16 {
                    e.w().local_get(first).local_get(second);
                    for (position, lane) in lanes.iter_mut().enumerate() {
                        *lane = shift + position as u8;
                    }
                } else {
                    e.w().local_get(second).v128_const(0);
                    for (position, lane) in lanes.iter_mut().enumerate() {
                        *lane = (shift - 16 + position as u8).min(16);
                    }
                }
                e.w().i8x16_shuffle(lanes);
            })
        }
        Pslldq | Vpslldq | Psrldq | Vpsrldq => {
            let shift = imm().min(16);
            let left = matches!(mnemonic, Pslldq | Vpslldq);
            let source = if is_vex(instruction) { 1 } else { 0 };
            e.unary_lanes(instruction, source, &|e| {
                let mut lanes = [0u8; 16];
                for (position, lane) in lanes.iter_mut().enumerate() {
                    let position = position as u8;
                    *lane = if left {
                        if position >= shift { position - shift } else { 16 }
                    } else if position + shift < 16 {
                        position + shift
                    } else {
                        16
                    };
                }
                e.w().v128_const(0).i8x16_shuffle(lanes);
            })
        }
        Pmovzxbw | Vpmovzxbw => widen(e, instruction, &[(1, false)]),
        Pmovzxbd | Vpmovzxbd => widen(e, instruction, &[(1, false), (2, false)]),
        Pmovzxbq | Vpmovzxbq => widen(e, instruction, &[(1, false), (2, false), (4, false)]),
        Pmovzxwd | Vpmovzxwd => widen(e, instruction, &[(2, false)]),
        Pmovzxwq | Vpmovzxwq => widen(e, instruction, &[(2, false), (4, false)]),
        Pmovzxdq | Vpmovzxdq => widen(e, instruction, &[(4, false)]),
        Pmovsxbw | Vpmovsxbw => widen(e, instruction, &[(1, true)]),
        Pmovsxbd | Vpmovsxbd => widen(e, instruction, &[(1, true), (2, true)]),
        Pmovsxbq | Vpmovsxbq => widen(e, instruction, &[(1, true), (2, true), (4, true)]),
        Pmovsxwd | Vpmovsxwd => widen(e, instruction, &[(2, true)]),
        Pmovsxwq | Vpmovsxwq => widen(e, instruction, &[(2, true), (4, true)]),
        Pmovsxdq | Vpmovsxdq => widen(e, instruction, &[(4, true)]),
        _ => return None,
    })
}

fn per_half(e: &mut Emitter<'_>, instruction: &Instruction, lanes: &dyn Fn(u8) -> [u8; 16]) -> Result<()> {
    let (a, b) = e.sources(instruction);
    e.vlow(instruction, a)?;
    e.vlow(instruction, b)?;
    e.w().i8x16_shuffle(lanes(0));
    let low = e.save128();
    let high = if e.wide(instruction) {
        e.vhigh(instruction, a)?;
        e.vhigh(instruction, b)?;
        e.w().i8x16_shuffle(lanes(1));
        Some(e.save128())
    } else {
        None
    };
    e.vwrite(instruction, 0, low, high)
}

fn unary_per_half(e: &mut Emitter<'_>, instruction: &Instruction, operand: u32, lanes: &dyn Fn(u8) -> [u8; 16]) -> Result<()> {
    e.vlow(instruction, operand)?;
    e.w().v128_const(0).i8x16_shuffle(lanes(0));
    let low = e.save128();
    let high = if e.wide(instruction) {
        e.vhigh(instruction, operand)?;
        e.w().v128_const(0).i8x16_shuffle(lanes(1));
        Some(e.save128())
    } else {
        None
    };
    e.vwrite(instruction, 0, low, high)
}

fn interleave(e: &mut Emitter<'_>, instruction: &Instruction, size: u8, high: bool) -> Result<()> {
    let count = 16 / size;
    let start = if high { count / 2 } else { 0 };
    let order: Vec<u8> = (0..count / 2).flat_map(|index| [start + index, count + start + index]).collect();
    let lanes = elements(size, &order);
    e.binary_lanes(instruction, &|e| {
        e.w().i8x16_shuffle(lanes);
    })
}

fn cross_permute(e: &mut Emitter<'_>, instruction: &Instruction, order: &[u8]) -> Result<()> {
    e.vlow(instruction, 1)?;
    let low_source = e.save128();
    e.vhigh(instruction, 1)?;
    let high_source = e.save128();
    let mut halves = Vec::new();
    for half in 0..2 {
        e.w().local_get(low_source).local_get(high_source).i8x16_shuffle(elements(8, &order[half * 2..half * 2 + 2]));
        halves.push(e.save128());
    }
    e.vwrite(instruction, 0, halves[0], Some(halves[1]))
}

fn permute_dwords(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    e.vlow(instruction, 2)?;
    let low_source = e.save128();
    e.vhigh(instruction, 2)?;
    let high_source = e.save128();
    let mut halves = Vec::new();
    for half in 0..2 {
        if half == 0 {
            e.vlow(instruction, 1)?
        } else {
            e.vhigh(instruction, 1)?
        }
        let indices = e.save128();
        let bytes = e.temp128();
        e.w().local_get(indices).v128_const(i128::from_le_bytes([3, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0])).v128_and();
        e.w().v128_const(i128::from_le_bytes([4; 16])).i32x4_mul();
        e.w().v128_const(i128::from_le_bytes([0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3])).i32x4_add().local_set(bytes);
        e.w().local_get(high_source).local_get(bytes).i8x16_swizzle();
        e.w().local_get(low_source).local_get(bytes).i8x16_swizzle();
        e.w().local_get(indices).i32_const(29).i32x4_shl().i32_const(31).i32x4_shr_s().v128_bitselect();
        halves.push(e.save128());
    }
    e.vwrite(instruction, 0, halves[0], Some(halves[1]))
}

fn permute_halves(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let control = instruction.immediate8();
    let mut sources = Vec::new();
    for (operand, high) in [(1, false), (1, true), (2, false), (2, true)] {
        if high {
            e.vhigh(instruction, operand)?;
        } else {
            e.vlow(instruction, operand)?;
        }
        sources.push(e.save128());
    }
    let mut halves = Vec::new();
    for half in 0..2 {
        let bits = control >> (4 * half);
        if bits & 8 != 0 {
            e.w().v128_const(0);
        } else {
            e.w().local_get(sources[(bits & 3) as usize]);
        }
        halves.push(e.save128());
    }
    e.vwrite(instruction, 0, halves[0], Some(halves[1]))
}

fn variable_blend(e: &mut Emitter<'_>, instruction: &Instruction, size: u32) -> Result<()> {
    let mask_operand = instruction.op_count() - 1;
    let (a, b) = e.sources(instruction);
    let mut halves = Vec::new();
    let wide = e.wide(instruction);
    for high in [false, true] {
        if high && !wide {
            break;
        }
        for operand in [b, a, mask_operand] {
            if high {
                e.vhigh(instruction, operand)?;
            } else {
                e.vlow(instruction, operand)?;
            }
        }
        match size {
            1 => e.w().i32_const(7).i8x16_shr_s(),
            4 => e.w().i32_const(31).i32x4_shr_s(),
            _ => e.w().i32_const(63).i64x2_shr_s(),
        };
        e.w().v128_bitselect();
        halves.push(e.save128());
    }
    let high = halves.get(1).copied();
    e.vwrite(instruction, 0, halves[0], high)
}

fn widen(e: &mut Emitter<'_>, instruction: &Instruction, steps: &[(u8, bool)]) -> Result<()> {
    e.vlow(instruction, 1)?;
    let source = e.save128();
    let extend = |e: &mut Emitter<'_>| {
        for &(size, signed) in steps {
            match (size, signed) {
                (1, false) => e.w().i16x8_extend_low_i8x16_u(),
                (1, true) => e.w().i16x8_extend_low_i8x16_s(),
                (2, false) => e.w().i32x4_extend_low_i16x8_u(),
                (2, true) => e.w().i32x4_extend_low_i16x8_s(),
                (4, false) => e.w().i64x2_extend_low_i32x4_u(),
                _ => e.w().i64x2_extend_low_i32x4_s(),
            };
        }
    };
    e.w().local_get(source);
    extend(e);
    let low = e.save128();
    let high = if e.wide(instruction) {
        let offset = 16u8 >> steps.len();
        let mut lanes = [16u8; 16];
        for (position, lane) in lanes.iter_mut().enumerate() {
            if (offset as usize + position) < 16 {
                *lane = offset + position as u8;
            }
        }
        e.w().local_get(source).v128_const(0).i8x16_shuffle(lanes);
        extend(e);
        Some(e.save128())
    } else {
        None
    };
    e.vwrite(instruction, 0, low, high)
}
