use iced_x86::{Instruction, Mnemonic, RflagsBits};
use wasm_encoder::ValType;

use super::VecLoc;
use super::integer::swap;
use crate::codegen::body::mem;
use crate::codegen::function::{AF, CF, Emitter, OF, PF, SF, ZF, xlo};
use crate::{Error, Result};

#[derive(Clone, Copy, PartialEq, Eq)]
enum Arith {
    Add,
    Sub,
    Mul,
    Div,
    Min,
    Max,
}

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Option<Result<()>> {
    use Mnemonic::*;
    Some(match mnemonic {
        Addps | Vaddps => packed(e, instruction, false, Arith::Add),
        Subps | Vsubps => packed(e, instruction, false, Arith::Sub),
        Mulps | Vmulps => packed(e, instruction, false, Arith::Mul),
        Divps | Vdivps => packed(e, instruction, false, Arith::Div),
        Minps | Vminps => packed(e, instruction, false, Arith::Min),
        Maxps | Vmaxps => packed(e, instruction, false, Arith::Max),
        Addpd | Vaddpd => packed(e, instruction, true, Arith::Add),
        Subpd | Vsubpd => packed(e, instruction, true, Arith::Sub),
        Mulpd | Vmulpd => packed(e, instruction, true, Arith::Mul),
        Divpd | Vdivpd => packed(e, instruction, true, Arith::Div),
        Minpd | Vminpd => packed(e, instruction, true, Arith::Min),
        Maxpd | Vmaxpd => packed(e, instruction, true, Arith::Max),
        Addss | Vaddss => scalar_arith(e, instruction, false, Arith::Add),
        Subss | Vsubss => scalar_arith(e, instruction, false, Arith::Sub),
        Mulss | Vmulss => scalar_arith(e, instruction, false, Arith::Mul),
        Divss | Vdivss => scalar_arith(e, instruction, false, Arith::Div),
        Minss | Vminss => scalar_arith(e, instruction, false, Arith::Min),
        Maxss | Vmaxss => scalar_arith(e, instruction, false, Arith::Max),
        Addsd | Vaddsd => scalar_arith(e, instruction, true, Arith::Add),
        Subsd | Vsubsd => scalar_arith(e, instruction, true, Arith::Sub),
        Mulsd | Vmulsd => scalar_arith(e, instruction, true, Arith::Mul),
        Divsd | Vdivsd => scalar_arith(e, instruction, true, Arith::Div),
        Minsd | Vminsd => scalar_arith(e, instruction, true, Arith::Min),
        Maxsd | Vmaxsd => scalar_arith(e, instruction, true, Arith::Max),
        Sqrtps | Vsqrtps => e.unary_lanes(instruction, 1, &|e| {
            e.w().f32x4_sqrt();
        }),
        Sqrtpd | Vsqrtpd => e.unary_lanes(instruction, 1, &|e| {
            e.w().f64x2_sqrt();
        }),
        Sqrtss | Vsqrtss => scalar(e, instruction, false, &|e, _, b| {
            e.w().local_get(b).f32_sqrt();
        }),
        Sqrtsd | Vsqrtsd => scalar(e, instruction, true, &|e, _, b| {
            e.w().local_get(b).f64_sqrt();
        }),
        Addsubps | Vaddsubps | Addsubpd | Vaddsubpd => {
            let double = matches!(mnemonic, Addsubpd | Vaddsubpd);
            e.binary_lanes(instruction, &|e| {
                let b = e.save128();
                let a = e.save128();
                e.w().local_get(a).local_get(b);
                if double {
                    e.w().f64x2_sub();
                } else {
                    e.w().f32x4_sub();
                }
                e.w().local_get(a).local_get(b);
                if double {
                    e.w().f64x2_add();
                } else {
                    e.w().f32x4_add();
                }
                let lanes = if double { super::elements(8, &[0, 3]) } else { super::elements(4, &[0, 5, 2, 7]) };
                e.w().i8x16_shuffle(lanes);
            })
        }
        Haddps | Vhaddps | Haddpd | Vhaddpd | Hsubps | Vhsubps | Hsubpd | Vhsubpd => {
            let double = matches!(mnemonic, Haddpd | Vhaddpd | Hsubpd | Vhsubpd);
            let subtract = matches!(mnemonic, Hsubps | Vhsubps | Hsubpd | Vhsubpd);
            e.binary_lanes(instruction, &|e| {
                let b = e.save128();
                let a = e.save128();
                let (even, odd) = if double {
                    (super::elements(8, &[0, 2]), super::elements(8, &[1, 3]))
                } else {
                    (super::elements(4, &[0, 2, 4, 6]), super::elements(4, &[1, 3, 5, 7]))
                };
                e.w().local_get(a).local_get(b).i8x16_shuffle(even).local_get(a).local_get(b).i8x16_shuffle(odd);
                match (double, subtract) {
                    (true, false) => e.w().f64x2_add(),
                    (true, true) => e.w().f64x2_sub(),
                    (false, false) => e.w().f32x4_add(),
                    (false, true) => e.w().f32x4_sub(),
                };
            })
        }
        Cmpps | Vcmpps | Cmppd | Vcmppd => {
            let double = matches!(mnemonic, Cmppd | Vcmppd);
            let predicate = instruction.immediate8();
            e.binary_lanes(instruction, &|e| compare(e, double, predicate))
        }
        Cmpss | Vcmpss | Cmpsd | Vcmpsd => scalar_compare(e, instruction, matches!(mnemonic, Cmpsd | Vcmpsd)),
        Ucomiss | Vucomiss | Comiss | Vcomiss => ordered_compare(e, instruction, false),
        Ucomisd | Vucomisd | Comisd | Vcomisd => ordered_compare(e, instruction, true),
        Roundps | Vroundps | Roundpd | Vroundpd => {
            let double = matches!(mnemonic, Roundpd | Vroundpd);
            let mode = rounding(instruction.immediate8());
            e.unary_lanes(instruction, 1, &|e| {
                let mut w = e.w();
                match (double, mode) {
                    (false, 0) => w.f32x4_nearest(),
                    (false, 1) => w.f32x4_floor(),
                    (false, 2) => w.f32x4_ceil(),
                    (false, _) => w.f32x4_trunc(),
                    (true, 0) => w.f64x2_nearest(),
                    (true, 1) => w.f64x2_floor(),
                    (true, 2) => w.f64x2_ceil(),
                    (true, _) => w.f64x2_trunc(),
                };
            })
        }
        Roundss | Vroundss | Roundsd | Vroundsd => {
            let double = matches!(mnemonic, Roundsd | Vroundsd);
            let mode = rounding(instruction.immediate8());
            scalar(e, instruction, double, &|e, _, b| {
                let mut w = e.w();
                w.local_get(b);
                match (double, mode) {
                    (false, 0) => w.f32_nearest(),
                    (false, 1) => w.f32_floor(),
                    (false, 2) => w.f32_ceil(),
                    (false, _) => w.f32_trunc(),
                    (true, 0) => w.f64_nearest(),
                    (true, 1) => w.f64_floor(),
                    (true, 2) => w.f64_ceil(),
                    (true, _) => w.f64_trunc(),
                };
            })
        }
        _ => return fma(e, instruction, mnemonic),
    })
}

fn rounding(control: u8) -> u8 {
    if control & 4 != 0 { 0 } else { control & 3 }
}

fn packed(e: &mut Emitter<'_>, instruction: &Instruction, double: bool, op: Arith) -> Result<()> {
    e.binary_lanes(instruction, &|e| {
        if matches!(op, Arith::Min | Arith::Max) {
            swap(e);
        }
        let mut w = e.w();
        match (double, op) {
            (false, Arith::Add) => w.f32x4_add(),
            (false, Arith::Sub) => w.f32x4_sub(),
            (false, Arith::Mul) => w.f32x4_mul(),
            (false, Arith::Div) => w.f32x4_div(),
            (false, Arith::Min) => w.f32x4_pmin(),
            (false, Arith::Max) => w.f32x4_pmax(),
            (true, Arith::Add) => w.f64x2_add(),
            (true, Arith::Sub) => w.f64x2_sub(),
            (true, Arith::Mul) => w.f64x2_mul(),
            (true, Arith::Div) => w.f64x2_div(),
            (true, Arith::Min) => w.f64x2_pmin(),
            (true, Arith::Max) => w.f64x2_pmax(),
        };
    })
}

fn scalar_arith(e: &mut Emitter<'_>, instruction: &Instruction, double: bool, op: Arith) -> Result<()> {
    scalar(e, instruction, double, &|e, a, b| {
        let mut w = e.w();
        match op {
            Arith::Min => {
                w.local_get(a).local_get(b).local_get(a).local_get(b);
                if double {
                    w.f64_lt()
                } else {
                    w.f32_lt()
                };
                w.select();
            }
            Arith::Max => {
                w.local_get(a).local_get(b).local_get(a).local_get(b);
                if double {
                    w.f64_gt()
                } else {
                    w.f32_gt()
                };
                w.select();
            }
            _ => {
                w.local_get(a).local_get(b);
                match (double, op) {
                    (false, Arith::Add) => w.f32_add(),
                    (false, Arith::Sub) => w.f32_sub(),
                    (false, Arith::Mul) => w.f32_mul(),
                    (false, _) => w.f32_div(),
                    (true, Arith::Add) => w.f64_add(),
                    (true, Arith::Sub) => w.f64_sub(),
                    (true, Arith::Mul) => w.f64_mul(),
                    (true, _) => w.f64_div(),
                };
            }
        }
    })
}

fn scalar_operand(e: &mut Emitter<'_>, instruction: &Instruction, operand: u32, double: bool) -> Result<u32> {
    let local = e.body.temp(if double { ValType::F64 } else { ValType::F32 });
    match e.vloc(instruction, operand)? {
        VecLoc::Mem(_) => {
            let address = e.address(instruction)?;
            e.w().local_get(address);
            if double {
                e.w().f64_load(mem(0));
            } else {
                e.w().f32_load(mem(0));
            }
        }
        VecLoc::Xmm(index) | VecLoc::Ymm(index) => {
            e.w().local_get(xlo(index));
            if double {
                e.w().f64x2_extract_lane(0);
            } else {
                e.w().f32x4_extract_lane(0);
            }
        }
        other => {
            return Err(Error::new(format!("{:#x}: scalar operand {other:?}", instruction.ip())));
        }
    }
    e.w().local_set(local);
    Ok(local)
}

pub fn scalar(e: &mut Emitter<'_>, instruction: &Instruction, double: bool, compute: &dyn Fn(&mut Emitter<'_>, u32, u32)) -> Result<()> {
    let (a_operand, b_operand) = e.sources(instruction);
    let b = scalar_operand(e, instruction, b_operand, double)?;
    let a = scalar_operand(e, instruction, a_operand, double)?;
    e.vlow(instruction, a_operand)?;
    compute(e, a, b);
    if double {
        e.w().f64x2_replace_lane(0);
    } else {
        e.w().f32x4_replace_lane(0);
    }
    let vector = e.save128();
    e.vwrite(instruction, 0, vector, None)
}

fn compare(e: &mut Emitter<'_>, double: bool, predicate: u8) {
    let b = e.save128();
    let a = e.save128();
    let unordered = |e: &mut Emitter<'_>| {
        e.w().local_get(a).local_get(a);
        if double {
            e.w().f64x2_ne();
        } else {
            e.w().f32x4_ne();
        }
        e.w().local_get(b).local_get(b);
        if double {
            e.w().f64x2_ne();
        } else {
            e.w().f32x4_ne();
        }
        e.w().v128_or();
    };
    let relation = |e: &mut Emitter<'_>, which: u8| {
        e.w().local_get(a).local_get(b);
        let mut w = e.w();
        match (double, which) {
            (false, 0) => w.f32x4_eq(),
            (false, 1) => w.f32x4_lt(),
            (false, 2) => w.f32x4_le(),
            (false, 3) => w.f32x4_ne(),
            (false, 4) => w.f32x4_ge(),
            (false, _) => w.f32x4_gt(),
            (true, 0) => w.f64x2_eq(),
            (true, 1) => w.f64x2_lt(),
            (true, 2) => w.f64x2_le(),
            (true, 3) => w.f64x2_ne(),
            (true, 4) => w.f64x2_ge(),
            (true, _) => w.f64x2_gt(),
        };
    };
    match predicate & 0xf {
        0 => relation(e, 0),
        1 => relation(e, 1),
        2 => relation(e, 2),
        3 => unordered(e),
        4 => relation(e, 3),
        5 => {
            relation(e, 1);
            e.w().v128_not();
        }
        6 => {
            relation(e, 2);
            e.w().v128_not();
        }
        7 => {
            unordered(e);
            e.w().v128_not();
        }
        8 => {
            relation(e, 0);
            unordered(e);
            e.w().v128_or();
        }
        9 => {
            relation(e, 4);
            e.w().v128_not();
        }
        10 => {
            relation(e, 5);
            e.w().v128_not();
        }
        11 => {
            e.w().v128_const(0);
        }
        12 => {
            relation(e, 3);
            unordered(e);
            e.w().v128_not().v128_and();
        }
        13 => relation(e, 4),
        14 => relation(e, 5),
        _ => {
            e.w().v128_const(-1);
        }
    }
}

fn scalar_compare(e: &mut Emitter<'_>, instruction: &Instruction, double: bool) -> Result<()> {
    let predicate = instruction.immediate8();
    let (a, b) = e.sources(instruction);
    e.vlow(instruction, a)?;
    let first = e.save128();
    e.w().local_get(first);
    e.vlow(instruction, b)?;
    compare(e, double, predicate);
    let size = if double { 8 } else { 4 };
    let mut lanes = [0u8; 16];
    for (position, lane) in lanes.iter_mut().enumerate() {
        *lane = if position < size { 16 + position as u8 } else { position as u8 };
    }
    let mask = e.save128();
    e.w().local_get(first).local_get(mask).i8x16_shuffle(lanes);
    let vector = e.save128();
    e.vwrite(instruction, 0, vector, None)
}

fn ordered_compare(e: &mut Emitter<'_>, instruction: &Instruction, double: bool) -> Result<()> {
    let a = scalar_operand(e, instruction, 0, double)?;
    let b = scalar_operand(e, instruction, 1, double)?;
    let unordered = e.temp32();
    e.w().local_get(a).local_get(a);
    if double {
        e.w().f64_ne();
    } else {
        e.w().f32_ne();
    }
    e.w().local_get(b).local_get(b);
    if double {
        e.w().f64_ne();
    } else {
        e.w().f32_ne();
    }
    e.w().i32_or().local_set(unordered);
    if e.want(RflagsBits::ZF) {
        e.w().local_get(a).local_get(b);
        if double {
            e.w().f64_eq();
        } else {
            e.w().f32_eq();
        }
        e.w().local_get(unordered).i32_or().local_set(ZF);
    }
    if e.want(RflagsBits::CF) {
        e.w().local_get(a).local_get(b);
        if double {
            e.w().f64_lt();
        } else {
            e.w().f32_lt();
        }
        e.w().local_get(unordered).i32_or().local_set(CF);
    }
    if e.want(RflagsBits::PF) {
        e.w().local_get(unordered).local_set(PF);
    }
    for (flag, local) in [(RflagsBits::OF, OF), (RflagsBits::SF, SF), (RflagsBits::AF, AF)] {
        e.set_flag(flag, local, false);
    }
    Ok(())
}

fn fma(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Option<Result<()>> {
    let name = format!("{mnemonic:?}");
    let rest = name.strip_prefix("Vf")?;
    let (negate, subtract, rest) = if let Some(rest) = rest.strip_prefix("nmadd") {
        (true, false, rest)
    } else if let Some(rest) = rest.strip_prefix("nmsub") {
        (true, true, rest)
    } else if let Some(rest) = rest.strip_prefix("madd") {
        (false, false, rest)
    } else if let Some(rest) = rest.strip_prefix("msub") {
        (false, true, rest)
    } else {
        return None;
    };
    if rest.len() != 5 {
        return None;
    }
    let order = &rest[..3];
    let (kind, double) = match &rest[3..] {
        "ps" => (false, false),
        "pd" => (false, true),
        "ss" => (true, false),
        "sd" => (true, true),
        _ => return None,
    };
    let (x, y, z) = match order {
        "132" => (0, 2, 1),
        "213" => (1, 0, 2),
        "231" => (1, 2, 0),
        _ => return None,
    };
    let multiply_add = move |e: &mut Emitter<'_>, product_x: u32, product_y: u32, addend: u32| {
        e.w().local_get(product_x).local_get(product_y).local_get(addend);
        if subtract {
            if double {
                e.w().f64x2_neg();
            } else {
                e.w().f32x4_neg();
            }
        }
        match (double, negate) {
            (false, false) => e.w().f32x4_relaxed_madd(),
            (false, true) => e.w().f32x4_relaxed_nmadd(),
            (true, false) => e.w().f64x2_relaxed_madd(),
            (true, true) => e.w().f64x2_relaxed_nmadd(),
        };
    };
    let result = (|| -> Result<()> {
        let wide = e.wide(instruction);
        let mut halves = Vec::new();
        for high in [false, true] {
            if high && (!wide || kind) {
                break;
            }
            let mut locals = [0; 3];
            for (slot, operand) in locals.iter_mut().zip([x, y, z]) {
                if high {
                    e.vhigh(instruction, operand)?;
                } else {
                    e.vlow(instruction, operand)?;
                }
                *slot = e.save128();
            }
            multiply_add(e, locals[0], locals[1], locals[2]);
            if kind {
                let product = e.save128();
                e.vlow(instruction, 0)?;
                e.w().local_get(product);
                if double {
                    e.w().f64x2_extract_lane(0).f64x2_replace_lane(0);
                } else {
                    e.w().f32x4_extract_lane(0).f32x4_replace_lane(0);
                }
            }
            halves.push(e.save128());
        }
        let high = halves.get(1).copied();
        e.vwrite(instruction, 0, halves[0], high)
    })();
    Some(result)
}
