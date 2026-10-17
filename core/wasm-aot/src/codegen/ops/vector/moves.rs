use iced_x86::{Instruction, Mnemonic};

use super::{VecLoc, elements, is_vex};
use crate::codegen::function::{Emitter, xhi, xlo};
use crate::{Error, Result};

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Option<Result<()>> {
    use Mnemonic::*;
    Some(match mnemonic {
        Movups | Movaps | Movupd | Movapd | Movdqu | Movdqa | Movntdq | Movntps | Movntpd | Movntdqa | Lddqu | Vmovups | Vmovaps | Vmovupd | Vmovapd
        | Vmovdqu | Vmovdqa | Vmovntdq | Vmovntps | Vmovntpd | Vmovntdqa | Vlddqu => e.unary_lanes(instruction, 1, &|_| {}),
        Movd | Vmovd => move_integer(e, instruction, 4),
        Movq | Vmovq => move_integer(e, instruction, 8),
        Movss | Vmovss => move_scalar(e, instruction, 4),
        Movsd | Vmovsd => move_scalar(e, instruction, 8),
        Movhps | Movhpd | Vmovhps | Vmovhpd => move_half(e, instruction, true),
        Movlps | Movlpd | Vmovlps | Vmovlpd => move_half(e, instruction, false),
        Movlhps | Vmovlhps => combine(e, instruction, elements(8, &[0, 2])),
        Movhlps | Vmovhlps => {
            let (a, b) = e.sources(instruction);
            combine_operands(e, instruction, b, a, elements(8, &[1, 3]))
        }
        Movddup | Vmovddup => e.unary_lanes(instruction, 1, &|e| {
            e.w().v128_const(0).i8x16_shuffle(elements(8, &[0, 0]));
        }),
        Movshdup | Vmovshdup => e.unary_lanes(instruction, 1, &|e| {
            e.w().v128_const(0).i8x16_shuffle(elements(4, &[1, 1, 3, 3]));
        }),
        Movsldup | Vmovsldup => e.unary_lanes(instruction, 1, &|e| {
            e.w().v128_const(0).i8x16_shuffle(elements(4, &[0, 0, 2, 2]));
        }),
        Vzeroupper | Vzeroall => {
            for index in 0..16 {
                e.w().v128_const(0).local_set(xhi(index));
                if mnemonic == Vzeroall {
                    e.w().v128_const(0).local_set(xlo(index));
                }
            }
            Ok(())
        }
        Vpbroadcastb | Vpbroadcastw | Vpbroadcastd | Vpbroadcastq | Vbroadcastss | Vbroadcastsd => broadcast(e, instruction, mnemonic),
        Vbroadcastf128 | Vbroadcasti128 => (|| {
            e.vlow(instruction, 1)?;
            let value = e.save128();
            e.vwrite(instruction, 0, value, Some(value))
        })(),
        Vextracti128 | Vextractf128 => (|| {
            let VecLoc::Ymm(source) = e.vloc(instruction, 1)? else {
                return Err(Error::new("vextract from a non-ymm register"));
            };
            let local = if instruction.immediate8() & 1 == 0 { xlo(source) } else { xhi(source) };
            e.vwrite(instruction, 0, local, None)
        })(),
        Vinserti128 | Vinsertf128 => (|| {
            e.vlow(instruction, 2)?;
            let inserted = e.save128();
            if instruction.immediate8() & 1 == 0 {
                e.vhigh(instruction, 1)?;
                let high = e.save128();
                e.vwrite(instruction, 0, inserted, Some(high))
            } else {
                e.vlow(instruction, 1)?;
                let low = e.save128();
                e.vwrite(instruction, 0, low, Some(inserted))
            }
        })(),
        Pextrb | Vpextrb => extract(e, instruction, 1),
        Pextrw | Vpextrw => extract(e, instruction, 2),
        Pextrd | Vpextrd | Extractps | Vextractps => extract(e, instruction, 4),
        Pextrq | Vpextrq => extract(e, instruction, 8),
        Pinsrb | Vpinsrb => insert(e, instruction, 1),
        Pinsrw | Vpinsrw => insert(e, instruction, 2),
        Pinsrd | Vpinsrd => insert(e, instruction, 4),
        Pinsrq | Vpinsrq => insert(e, instruction, 8),
        Insertps | Vinsertps => insertps(e, instruction),
        Movmskps | Vmovmskps => mask(e, instruction, 4),
        Movmskpd | Vmovmskpd => mask(e, instruction, 8),
        Pmovmskb | Vpmovmskb => mask(e, instruction, 1),
        _ => return None,
    })
}

fn move_integer(e: &mut Emitter<'_>, instruction: &Instruction, size: u32) -> Result<()> {
    let destination = e.vloc(instruction, 0)?;
    let source = e.vloc(instruction, 1)?;
    match (destination, source) {
        (VecLoc::Xmm(_), VecLoc::Gpr(_)) => {
            e.read(instruction, 1)?;
            let value = e.save();
            e.w().v128_const(0).local_get(value);
            if size == 4 {
                e.w().i32_wrap_i64().i32x4_replace_lane(0);
            } else {
                e.w().i64x2_replace_lane(0);
            }
            let vector = e.save128();
            e.vwrite(instruction, 0, vector, None)
        }
        (VecLoc::Xmm(_), VecLoc::Mem(_)) => {
            e.vlow(instruction, 1)?;
            let vector = e.save128();
            e.vwrite(instruction, 0, vector, None)
        }
        (VecLoc::Xmm(_), VecLoc::Xmm(_)) => {
            e.vlow(instruction, 1)?;
            e.w().v128_const(0).i8x16_shuffle(elements(8, &[0, 2]));
            let vector = e.save128();
            e.vwrite(instruction, 0, vector, None)
        }
        (VecLoc::Gpr(_), VecLoc::Xmm(index)) => {
            e.w().local_get(xlo(index));
            if size == 4 {
                e.w().i32x4_extract_lane(0).i64_extend_i32_u();
            } else {
                e.w().i64x2_extract_lane(0);
            }
            let value = e.save();
            e.write(instruction, 0, value)
        }
        (VecLoc::Mem(_), VecLoc::Xmm(index)) => e.vwrite(instruction, 0, xlo(index), None),
        _ => Err(Error::new(format!("{:#x}: movd/movq operands", instruction.ip()))),
    }
}

fn move_scalar(e: &mut Emitter<'_>, instruction: &Instruction, size: u8) -> Result<()> {
    if instruction.op_count() == 3 {
        let (a, b) = (1, 2);
        let mut lanes = [0u8; 16];
        for (position, lane) in lanes.iter_mut().enumerate() {
            *lane = if position < size as usize { 16 + position as u8 } else { position as u8 };
        }
        return combine_operands(e, instruction, a, b, lanes);
    }
    match (e.vloc(instruction, 0)?, e.vloc(instruction, 1)?) {
        (VecLoc::Xmm(_), VecLoc::Xmm(_)) => {
            let mut lanes = [0u8; 16];
            for (position, lane) in lanes.iter_mut().enumerate() {
                *lane = if position < size as usize { 16 + position as u8 } else { position as u8 };
            }
            combine_operands(e, instruction, 0, 1, lanes)
        }
        _ => {
            e.vlow(instruction, 1)?;
            let vector = e.save128();
            e.vwrite(instruction, 0, vector, None)
        }
    }
}

fn combine(e: &mut Emitter<'_>, instruction: &Instruction, lanes: [u8; 16]) -> Result<()> {
    let (a, b) = e.sources(instruction);
    combine_operands(e, instruction, a, b, lanes)
}

fn combine_operands(e: &mut Emitter<'_>, instruction: &Instruction, a: u32, b: u32, lanes: [u8; 16]) -> Result<()> {
    e.vlow(instruction, a)?;
    e.vlow(instruction, b)?;
    e.w().i8x16_shuffle(lanes);
    let vector = e.save128();
    e.vwrite(instruction, 0, vector, None)
}

fn move_half(e: &mut Emitter<'_>, instruction: &Instruction, high: bool) -> Result<()> {
    if let VecLoc::Mem(_) = e.vloc(instruction, 0)? {
        let VecLoc::Xmm(source) = e.vloc(instruction, 1)? else {
            return Err(Error::new("movhps store source"));
        };
        let address = e.address(instruction)?;
        e.w().local_get(address).local_get(xlo(source)).v128_store64_lane(crate::codegen::body::mem(0), high as u8);
        return Ok(());
    }
    let (a, b) = e.sources(instruction);
    let lanes = if high { elements(8, &[0, 2]) } else { elements(8, &[2, 1]) };
    combine_operands(e, instruction, a, b, lanes)
}

fn broadcast(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Result<()> {
    e.vlow(instruction, 1)?;
    let mut w = e.w();
    match mnemonic {
        Mnemonic::Vpbroadcastb => w.i8x16_extract_lane_u(0).i8x16_splat(),
        Mnemonic::Vpbroadcastw => w.i16x8_extract_lane_u(0).i16x8_splat(),
        Mnemonic::Vpbroadcastd | Mnemonic::Vbroadcastss => w.i32x4_extract_lane(0).i32x4_splat(),
        _ => w.i64x2_extract_lane(0).i64x2_splat(),
    };
    let vector = e.save128();
    let high = e.wide(instruction).then_some(vector);
    e.vwrite(instruction, 0, vector, high)
}

fn extract(e: &mut Emitter<'_>, instruction: &Instruction, size: u32) -> Result<()> {
    let VecLoc::Xmm(source) = e.vloc(instruction, 1)? else {
        return Err(Error::new("pextr source"));
    };
    let lanes = 16 / size as u8;
    let lane = instruction.immediate8() & (lanes - 1);
    e.w().local_get(xlo(source));
    match size {
        1 => e.w().i8x16_extract_lane_u(lane).i64_extend_i32_u(),
        2 => e.w().i16x8_extract_lane_u(lane).i64_extend_i32_u(),
        4 => e.w().i32x4_extract_lane(lane).i64_extend_i32_u(),
        _ => e.w().i64x2_extract_lane(lane),
    };
    let value = e.save();
    match e.vloc(instruction, 0)? {
        VecLoc::Mem(_) => {
            let address = e.address(instruction)?;
            e.w().local_get(address).local_get(value);
            e.store(size)
        }
        _ => e.write(instruction, 0, value),
    }
}

fn insert(e: &mut Emitter<'_>, instruction: &Instruction, size: u32) -> Result<()> {
    let (vector_operand, value_operand) = if is_vex(instruction) { (1, 2) } else { (0, 1) };
    let lanes = 16 / size as u8;
    let lane = instruction.immediate8() & (lanes - 1);
    match e.vloc(instruction, value_operand)? {
        VecLoc::Mem(_) => {
            let address = e.address(instruction)?;
            e.w().local_get(address);
            e.load(size)?;
        }
        _ => {
            e.read(instruction, value_operand)?;
        }
    }
    let value = e.save();
    e.vlow(instruction, vector_operand)?;
    e.w().local_get(value);
    match size {
        1 => e.w().i32_wrap_i64().i8x16_replace_lane(lane),
        2 => e.w().i32_wrap_i64().i16x8_replace_lane(lane),
        4 => e.w().i32_wrap_i64().i32x4_replace_lane(lane),
        _ => e.w().i64x2_replace_lane(lane),
    };
    let vector = e.save128();
    e.vwrite(instruction, 0, vector, None)
}

fn insertps(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let (vector_operand, value_operand) = e.sources(instruction);
    let control = instruction.immediate8();
    let element = e.temp32();
    match e.vloc(instruction, value_operand)? {
        VecLoc::Mem(_) => {
            let address = e.address(instruction)?;
            e.w().local_get(address).i32_load(crate::codegen::body::mem(0)).local_set(element);
        }
        VecLoc::Xmm(index) => {
            e.w().local_get(xlo(index)).i32x4_extract_lane(control >> 6).local_set(element);
        }
        other => return Err(Error::new(format!("insertps source {other:?}"))),
    }
    e.vlow(instruction, vector_operand)?;
    e.w().local_get(element).i32x4_replace_lane((control >> 4) & 3);
    let zero = control & 0xf;
    if zero != 0 {
        let mut keep: i128 = 0;
        for lane in 0..4 {
            if zero & (1 << lane) == 0 {
                keep |= 0xffff_ffffi128 << (32 * lane);
            }
        }
        e.w().v128_const(keep).v128_and();
    }
    let vector = e.save128();
    e.vwrite(instruction, 0, vector, None)
}

fn mask(e: &mut Emitter<'_>, instruction: &Instruction, element: u32) -> Result<()> {
    let source = match e.vloc(instruction, 1)? {
        VecLoc::Xmm(index) | VecLoc::Ymm(index) => index,
        other => return Err(Error::new(format!("mask source {other:?}"))),
    };
    let wide = instruction.op1_register().is_ymm();
    let bitmask = |e: &mut Emitter<'_>, local: u32| {
        e.w().local_get(local);
        match element {
            1 => e.w().i8x16_bitmask(),
            4 => e.w().i32x4_bitmask(),
            _ => e.w().i64x2_bitmask(),
        };
    };
    bitmask(e, xlo(source));
    if wide {
        bitmask(e, xhi(source));
        e.w().i32_const((16 / element) as i32).i32_shl().i32_or();
    }
    e.w().i64_extend_i32_u();
    let value = e.save();
    e.write(instruction, 0, value)
}
