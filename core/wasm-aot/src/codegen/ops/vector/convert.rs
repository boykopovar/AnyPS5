use iced_x86::{Instruction, Mnemonic, OpKind};
use wasm_encoder::ValType;

use super::{VecLoc, elements};
use crate::abi;
use crate::codegen::body::mem;
use crate::codegen::function::{CTX, Emitter, xlo};
use crate::{Error, Result};

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Option<Result<()>> {
    use Mnemonic::*;
    Some(match mnemonic {
        Cvtsi2ss | Vcvtsi2ss => from_integer(e, instruction, false),
        Cvtsi2sd | Vcvtsi2sd => from_integer(e, instruction, true),
        Cvttss2si | Vcvttss2si => to_integer(e, instruction, false, true),
        Cvtss2si | Vcvtss2si => to_integer(e, instruction, false, false),
        Cvttsd2si | Vcvttsd2si => to_integer(e, instruction, true, true),
        Cvtsd2si | Vcvtsd2si => to_integer(e, instruction, true, false),
        Cvtss2sd | Vcvtss2sd => scalar_width(e, instruction, true),
        Cvtsd2ss | Vcvtsd2ss => scalar_width(e, instruction, false),
        Cvtdq2ps | Vcvtdq2ps => e.unary_lanes(instruction, 1, &|e| {
            e.w().f32x4_convert_i32x4_s();
        }),
        Cvtps2dq | Vcvtps2dq | Cvttps2dq | Vcvttps2dq => {
            let truncate = matches!(mnemonic, Cvttps2dq | Vcvttps2dq);
            e.unary_lanes(instruction, 1, &|e| {
                let value = e.save128();
                e.w().local_get(value);
                if !truncate {
                    e.w().f32x4_nearest();
                }
                e.w().i32x4_trunc_sat_f32x4_s();
                e.w().v128_const(splat32(0x8000_0000));
                in_range_f32(e, value);
                e.w().v128_bitselect();
            })
        }
        Cvtps2pd | Vcvtps2pd => widen(e, instruction, &|e| {
            e.w().f64x2_promote_low_f32x4();
        }),
        Cvtdq2pd | Vcvtdq2pd => widen(e, instruction, &|e| {
            e.w().f64x2_convert_low_i32x4_s();
        }),
        Cvtpd2ps | Vcvtpd2ps => narrow(e, instruction, &|e| {
            e.w().f32x4_demote_f64x2_zero();
        }),
        Cvttpd2dq | Vcvttpd2dq | Cvtpd2dq | Vcvtpd2dq => {
            let truncate = matches!(mnemonic, Cvttpd2dq | Vcvttpd2dq);
            narrow(e, instruction, &|e| {
                let value = e.save128();
                if !truncate {
                    e.w().local_get(value).f64x2_nearest().local_set(value);
                }
                e.w().local_get(value).i32x4_trunc_sat_f64x2_s_zero();
                e.w().v128_const(splat32(0x8000_0000) & 0xffff_ffff_ffff_ffff);
                e.w().local_get(value).v128_const(splat64((-2147483649.0f64).to_bits())).f64x2_gt();
                e.w().local_get(value).v128_const(splat64(2147483648.0f64.to_bits())).f64x2_lt().v128_and();
                e.w().v128_const(0).i8x16_shuffle(elements(4, &[0, 2, 4, 4]));
                e.w().v128_bitselect();
            })
        }
        Stmxcsr | Vstmxcsr => (|| {
            let address = e.address(instruction)?;
            e.w().local_get(address).local_get(CTX).i32_load(mem(abi::cpu::MXCSR)).i32_store(mem(0));
            Ok(())
        })(),
        Ldmxcsr | Vldmxcsr => (|| {
            let address = e.address(instruction)?;
            let value = e.temp32();
            e.w().local_get(address).i32_load(mem(0)).local_tee(value).i32_const(0x6000).i32_and();
            e.open_if();
            e.trap(instruction);
            e.close();
            e.w().local_get(CTX).local_get(value).i32_store(mem(abi::cpu::MXCSR));
            Ok(())
        })(),
        _ => return None,
    })
}

fn splat32(value: u32) -> i128 {
    let mut result = 0i128;
    for lane in 0..4 {
        result |= (value as i128) << (32 * lane);
    }
    result
}

fn splat64(value: u64) -> i128 {
    (value as i128) | ((value as i128) << 64)
}

fn in_range_f32(e: &mut Emitter<'_>, value: u32) {
    e.w().local_get(value).v128_const(splat32((-2147483648.0f32).to_bits())).f32x4_ge();
    e.w().local_get(value).v128_const(splat32(2147483648.0f32.to_bits())).f32x4_lt().v128_and();
}

fn from_integer(e: &mut Emitter<'_>, instruction: &Instruction, double: bool) -> Result<()> {
    let source = instruction.op_count() - 1;
    let size = match e.vloc(instruction, source)? {
        VecLoc::Gpr(loc) => loc.size(),
        VecLoc::Mem(size) => size,
        other => {
            return Err(Error::new(format!("{:#x}: cvtsi2s* source {other:?}", instruction.ip())));
        }
    };
    e.read(instruction, source)?;
    let integer = e.save();
    let (vector, _) = e.sources(instruction);
    e.vlow(instruction, vector)?;
    e.w().local_get(integer);
    match (double, size) {
        (false, 4) => e.w().i32_wrap_i64().f32_convert_i32_s().f32x4_replace_lane(0),
        (false, _) => e.w().f32_convert_i64_s().f32x4_replace_lane(0),
        (true, 4) => e.w().i32_wrap_i64().f64_convert_i32_s().f64x2_replace_lane(0),
        (true, _) => e.w().f64_convert_i64_s().f64x2_replace_lane(0),
    };
    let result = e.save128();
    e.vwrite(instruction, 0, result, None)
}

fn to_integer(e: &mut Emitter<'_>, instruction: &Instruction, double: bool, truncate: bool) -> Result<()> {
    let size = instruction.op0_register().size();
    let value = e.body.temp(ValType::F64);
    match e.vloc(instruction, 1)? {
        VecLoc::Mem(_) => {
            let address = e.address(instruction)?;
            e.w().local_get(address);
            if double {
                e.w().f64_load(mem(0));
            } else {
                e.w().f32_load(mem(0)).f64_promote_f32();
            }
        }
        VecLoc::Xmm(index) => {
            e.w().local_get(xlo(index));
            if double {
                e.w().f64x2_extract_lane(0);
            } else {
                e.w().f32x4_extract_lane(0).f64_promote_f32();
            }
        }
        other => {
            return Err(Error::new(format!("{:#x}: cvt*2si source {other:?}", instruction.ip())));
        }
    }
    if !truncate {
        e.w().f64_nearest();
    }
    e.w().local_set(value);
    let (low, high, indefinite) =
        if size == 4 { (-2147483649.0f64, 2147483648.0f64, 0x8000_0000u64 as i64) } else { (-9223372036854777856.0f64, 9223372036854775808.0f64, i64::MIN) };
    if size == 4 {
        e.w().local_get(value).i32_trunc_sat_f64_s().i64_extend_i32_u();
    } else {
        e.w().local_get(value).i64_trunc_sat_f64_s();
    }
    e.w().i64_const(indefinite);
    e.w().local_get(value).f64_const(low.into()).f64_gt().local_get(value).f64_const(high.into()).f64_lt().i32_and().select();
    let result = e.save();
    e.write(instruction, 0, result)
}

fn scalar_width(e: &mut Emitter<'_>, instruction: &Instruction, to_double: bool) -> Result<()> {
    let (a, b) = e.sources(instruction);
    let source = e.body.temp(if to_double { ValType::F32 } else { ValType::F64 });
    match e.vloc(instruction, b)? {
        VecLoc::Mem(_) => {
            let address = e.address(instruction)?;
            e.w().local_get(address);
            if to_double {
                e.w().f32_load(mem(0));
            } else {
                e.w().f64_load(mem(0));
            }
        }
        VecLoc::Xmm(index) => {
            e.w().local_get(xlo(index));
            if to_double {
                e.w().f32x4_extract_lane(0);
            } else {
                e.w().f64x2_extract_lane(0);
            }
        }
        other => {
            return Err(Error::new(format!("{:#x}: cvt width source {other:?}", instruction.ip())));
        }
    }
    e.w().local_set(source);
    e.vlow(instruction, a)?;
    e.w().local_get(source);
    if to_double {
        e.w().f64_promote_f32().f64x2_replace_lane(0);
    } else {
        e.w().f32_demote_f64().f32x4_replace_lane(0);
    }
    let vector = e.save128();
    e.vwrite(instruction, 0, vector, None)
}

fn widen(e: &mut Emitter<'_>, instruction: &Instruction, convert: &dyn Fn(&mut Emitter<'_>)) -> Result<()> {
    e.vlow(instruction, 1)?;
    let source = e.save128();
    e.w().local_get(source);
    convert(e);
    let low = e.save128();
    let high = if e.wide(instruction) {
        e.w().local_get(source).v128_const(0).i8x16_shuffle(elements(4, &[2, 3, 4, 4]));
        convert(e);
        Some(e.save128())
    } else {
        None
    };
    e.vwrite(instruction, 0, low, high)
}

fn narrow(e: &mut Emitter<'_>, instruction: &Instruction, convert: &dyn Fn(&mut Emitter<'_>)) -> Result<()> {
    let wide_source = match instruction.op1_kind() {
        OpKind::Register => instruction.op1_register().is_ymm(),
        _ => instruction.memory_size().size() == 32,
    };
    e.vlow(instruction, 1)?;
    convert(e);
    let mut result = e.save128();
    if wide_source {
        e.vhigh(instruction, 1)?;
        convert(e);
        let high = e.save128();
        e.w().local_get(result).local_get(high).i8x16_shuffle(elements(4, &[0, 1, 4, 5]));
        result = e.save128();
    }
    e.vwrite(instruction, 0, result, None)
}
