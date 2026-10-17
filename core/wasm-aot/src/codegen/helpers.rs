use wasm_encoder::{BlockType, Function, ValType};

use super::body::Body;

pub fn umulh() -> Function {
    let mut body = Body::new(2, &[ValType::I64; 5]);
    let mut w = body.sink();
    let low = 0xffff_ffffi64;
    w.local_get(0).i64_const(low).i64_and().local_get(1).i64_const(low).i64_and().i64_mul().local_set(2);
    w.local_get(0).i64_const(low).i64_and().local_get(1).i64_const(32).i64_shr_u().i64_mul().local_set(3);
    w.local_get(0).i64_const(32).i64_shr_u().local_get(1).i64_const(low).i64_and().i64_mul().local_set(4);
    w.local_get(0).i64_const(32).i64_shr_u().local_get(1).i64_const(32).i64_shr_u().i64_mul().local_set(5);
    w.local_get(2).i64_const(32).i64_shr_u().local_get(3).i64_const(low).i64_and().i64_add().local_get(4).i64_const(low).i64_and().i64_add().local_set(6);
    w.local_get(5).local_get(3).i64_const(32).i64_shr_u().i64_add().local_get(4).i64_const(32).i64_shr_u().i64_add();
    w.local_get(6).i64_const(32).i64_shr_u().i64_add().end();
    body.finish()
}

pub fn smulh(umulh: u32) -> Function {
    let mut body = Body::new(2, &[]);
    let mut w = body.sink();
    w.local_get(0).local_get(1).call(umulh);
    w.local_get(1).local_get(0).i64_const(63).i64_shr_s().i64_and().i64_sub();
    w.local_get(0).local_get(1).i64_const(63).i64_shr_s().i64_and().i64_sub().end();
    body.finish()
}

pub fn udiv128() -> Function {
    let mut body = Body::new(3, &[ValType::I64, ValType::I64, ValType::I64]);
    let mut w = body.sink();
    w.local_get(2).i64_eqz().local_get(0).local_get(2).i64_ge_u().i32_or().if_(BlockType::Empty).unreachable().end();
    w.local_get(0).i64_eqz().if_(BlockType::Empty);
    w.local_get(1).local_get(2).i64_div_u().local_get(1).local_get(2).i64_rem_u().return_();
    w.end();
    w.i64_const(64).local_set(4);
    w.loop_(BlockType::Empty);
    w.local_get(0).i64_const(63).i64_shr_u().local_set(5);
    w.local_get(0).i64_const(1).i64_shl().local_get(1).i64_const(63).i64_shr_u().i64_or().local_set(0);
    w.local_get(1).i64_const(1).i64_shl().local_set(1);
    w.local_get(3).i64_const(1).i64_shl().local_set(3);
    w.local_get(5).i64_eqz().i32_eqz().local_get(0).local_get(2).i64_ge_u().i32_or().if_(BlockType::Empty);
    w.local_get(0).local_get(2).i64_sub().local_set(0).local_get(3).i64_const(1).i64_or().local_set(3);
    w.end();
    w.local_get(4).i64_const(1).i64_sub().local_tee(4).i64_eqz().i32_eqz().br_if(0);
    w.end();
    w.local_get(3).local_get(0).end();
    body.finish()
}

pub fn sdiv128(udiv128: u32) -> Function {
    let mut body = Body::new(3, &[ValType::I32, ValType::I32, ValType::I64, ValType::I64]);
    let mut w = body.sink();
    w.local_get(0).i64_const(0).i64_lt_s().local_set(3);
    w.local_get(2).i64_const(0).i64_lt_s().local_set(4);
    w.local_get(3).if_(BlockType::Empty);
    w.local_get(0).i64_const(-1).i64_xor().local_get(1).i64_eqz().i64_extend_i32_u().i64_add().local_set(0);
    w.i64_const(0).local_get(1).i64_sub().local_set(1);
    w.end();
    w.local_get(4).if_(BlockType::Empty).i64_const(0).local_get(2).i64_sub().local_set(2).end();
    w.local_get(0).local_get(1).local_get(2).call(udiv128).local_set(6).local_set(5);
    w.local_get(3).local_get(4).i32_ne().if_(BlockType::Empty);
    w.local_get(5).i64_const(i64::MIN).i64_gt_u().if_(BlockType::Empty).unreachable().end();
    w.i64_const(0).local_get(5).i64_sub().local_set(5);
    w.else_();
    w.local_get(5).i64_const(0).i64_lt_s().if_(BlockType::Empty).unreachable().end();
    w.end();
    w.local_get(3).if_(BlockType::Empty).i64_const(0).local_get(6).i64_sub().local_set(6).end();
    w.local_get(5).local_get(6).end();
    body.finish()
}

pub fn x80_load(scalbn: u32) -> Function {
    let mut body = Body::new(1, &[ValType::I64, ValType::I32, ValType::I32, ValType::I64]);
    let mut w = body.sink();
    w.local_get(0).i64_load(super::body::mem(0)).local_set(1);
    w.local_get(0).i32_load16_u(super::body::mem(8)).local_set(2);
    w.local_get(2).i32_const(0x7fff).i32_and().local_set(3);
    w.local_get(3).i32_const(0x7fff).i32_eq().if_(BlockType::Empty);
    w.local_get(2).i64_extend_i32_u().i64_const(15).i64_shr_u().i64_const(63).i64_shl();
    w.i64_const(0x7ff0_0000_0000_0000).i64_or();
    w.local_get(1).i64_const(1).i64_shl().i64_const(12).i64_shr_u().i64_or();
    w.local_get(1).i64_const(1).i64_shl().i64_eqz().if_(BlockType::Result(ValType::I64)).i64_const(0).else_().i64_const(1 << 51).end().i64_or();
    w.f64_reinterpret_i64().return_();
    w.end();
    w.local_get(1).i64_eqz().if_(BlockType::Empty);
    w.local_get(2).i64_extend_i32_u().i64_const(15).i64_shr_u().i64_const(63).i64_shl().f64_reinterpret_i64().return_();
    w.end();
    w.local_get(1).f64_convert_i64_u();
    w.local_get(3).i32_const(1).local_get(3).select().i32_const(16383 + 63).i32_sub().call(scalbn);
    w.local_get(2)
        .i32_const(0x8000)
        .i32_and()
        .if_(BlockType::Result(ValType::F64))
        .f64_const((-1.0f64).into())
        .else_()
        .f64_const(1.0f64.into())
        .end()
        .f64_mul();
    w.end();
    body.finish()
}

pub fn x80_store() -> Function {
    let mut body = Body::new(2, &[ValType::I64, ValType::I64, ValType::I64, ValType::I64, ValType::I64]);
    let mut w = body.sink();
    let fraction_mask = (1i64 << 52) - 1;
    w.local_get(1).i64_reinterpret_f64().local_tee(2).i64_const(63).i64_shr_u().i64_const(15).i64_shl().local_set(3);
    w.local_get(2).i64_const(52).i64_shr_u().i64_const(0x7ff).i64_and().local_set(4);
    w.local_get(2).i64_const(fraction_mask).i64_and().local_set(5);
    w.local_get(4).i64_const(0x7ff).i64_eq().if_(BlockType::Empty);
    w.local_get(0).local_get(5).i64_const(11).i64_shl().i64_const(i64::MIN).i64_or().i64_store(super::body::mem(0));
    w.local_get(0).local_get(3).i64_const(0x7fff).i64_or().i64_store16(super::body::mem(8)).return_();
    w.end();
    w.local_get(4).i64_eqz().if_(BlockType::Empty);
    w.local_get(5).i64_eqz().if_(BlockType::Empty);
    w.local_get(0).i64_const(0).i64_store(super::body::mem(0)).local_get(0).local_get(3).i64_store16(super::body::mem(8)).return_();
    w.end();
    w.local_get(5).i64_clz().local_set(6);
    w.local_get(0).local_get(5).local_get(6).i64_shl().i64_store(super::body::mem(0));
    w.local_get(0).local_get(3).i64_const(15372).local_get(6).i64_sub().i64_or().i64_store16(super::body::mem(8)).return_();
    w.end();
    w.local_get(0).local_get(5).i64_const(11).i64_shl().i64_const(i64::MIN).i64_or().i64_store(super::body::mem(0));
    w.local_get(0).local_get(3).local_get(4).i64_const(15360).i64_add().i64_or().i64_store16(super::body::mem(8));
    w.end();
    body.finish()
}

pub fn scalbn() -> Function {
    let mut body = Body::new(2, &[]);
    let mut w = body.sink();
    w.block(BlockType::Empty).loop_(BlockType::Empty);
    w.local_get(1).i32_const(1023).i32_le_s().br_if(1);
    w.local_get(0).f64_const(f64::from_bits(0x7fe0_0000_0000_0000).into()).f64_mul().local_set(0);
    w.local_get(1).i32_const(1023).i32_sub().local_set(1).br(0);
    w.end().end();
    w.block(BlockType::Empty).loop_(BlockType::Empty);
    w.local_get(1).i32_const(-1022).i32_ge_s().br_if(1);
    w.local_get(0).f64_const(f64::from_bits(0x0010_0000_0000_0000).into()).f64_mul().local_set(0);
    w.local_get(1).i32_const(1022).i32_add().local_set(1).br(0);
    w.end().end();
    w.local_get(0).local_get(1).i64_extend_i32_s().i64_const(1023).i64_add().i64_const(52).i64_shl().f64_reinterpret_i64().f64_mul();
    w.end();
    body.finish()
}

pub fn pcmpstr() -> Function {
    use wasm_encoder::InstructionSink;
    let mut body = Body::new(
        5,
        &[
            ValType::I32,
            ValType::I32,
            ValType::I32,
            ValType::I32,
            ValType::I32,
            ValType::I32,
            ValType::I32,
            ValType::V128,
            ValType::I32,
            ValType::I32,
            ValType::I32,
            ValType::V128,
        ],
    );
    let mut w = body.sink();
    let compare = |w: &mut InstructionSink<'_>, left: u32, right: u32, words: fn(&mut InstructionSink<'_>), bytes: fn(&mut InstructionSink<'_>)| {
        w.local_get(left).local_get(right);
        words(w);
        w.local_get(left).local_get(right);
        bytes(w);
        w.local_get(5).select();
    };
    let bitmask = |w: &mut InstructionSink<'_>| {
        w.local_tee(16).i16x8_bitmask().local_get(16).i8x16_bitmask().local_get(5).select();
    };
    w.local_get(4).i32_const(1).i32_and().local_set(5);
    w.i32_const(8).i32_const(16).local_get(5).select().local_set(6);
    w.i32_const(1).local_get(2).i32_shl().i32_const(1).i32_sub().local_set(9);
    w.i32_const(1).local_get(3).i32_shl().i32_const(1).i32_sub().local_set(10);
    w.i32_const(1).local_get(6).i32_shl().i32_const(1).i32_sub().local_set(11);
    w.local_get(4).i32_const(2).i32_shr_u().i32_const(3).i32_and().local_set(14);
    w.local_get(14).i32_const(2).i32_eq().if_(BlockType::Empty);
    compare(
        &mut w,
        0,
        1,
        |w| {
            w.i16x8_eq();
        },
        |w| {
            w.i8x16_eq();
        },
    );
    bitmask(&mut w);
    w.local_get(9).i32_and().local_get(10).i32_and();
    w.local_get(9).local_get(10).i32_or().i32_const(-1).i32_xor().local_get(11).i32_and().i32_or().local_set(7);
    w.else_();
    w.i32_const(0).local_set(8);
    w.block(BlockType::Empty).loop_(BlockType::Empty);
    w.local_get(8).local_get(6).i32_ge_u().br_if(1);
    w.local_get(14).i32_const(3).i32_eq().if_(BlockType::Empty);
    w.local_get(1);
    w.v128_const(i128::from_le_bytes([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]));
    w.local_get(8).local_get(5).i32_const(1).i32_add().i32_mul().i8x16_splat().i8x16_add().i8x16_swizzle().local_set(12);
    compare(
        &mut w,
        0,
        12,
        |w| {
            w.i16x8_eq();
        },
        |w| {
            w.i8x16_eq();
        },
    );
    bitmask(&mut w);
    w.local_get(10).local_get(8).i32_shr_u().i32_and().local_get(9).i32_const(-1).i32_xor().i32_or();
    w.i32_const(1).local_get(6).local_get(8).i32_sub().i32_shl().i32_const(1).i32_sub().local_tee(13).i32_and().local_get(13).i32_eq();
    w.local_get(8).i32_shl().local_get(7).i32_or().local_set(7);
    w.else_();
    w.local_get(8).local_get(3).i32_lt_u().if_(BlockType::Empty);
    w.local_get(1);
    w.local_get(8).i32_const(2).i32_mul().local_get(8).i32_const(2).i32_mul().i32_const(1).i32_add().i32_const(8).i32_shl().i32_or().i16x8_splat();
    w.local_get(8).i8x16_splat().local_get(5).select();
    w.i8x16_swizzle().local_set(12);
    w.local_get(14).i32_eqz().if_(BlockType::Empty);
    compare(
        &mut w,
        0,
        12,
        |w| {
            w.i16x8_eq();
        },
        |w| {
            w.i8x16_eq();
        },
    );
    bitmask(&mut w);
    w.local_get(9).i32_and().local_set(13);
    w.else_();
    w.local_get(4).i32_const(2).i32_and().if_(BlockType::Empty);
    compare(
        &mut w,
        0,
        12,
        |w| {
            w.i16x8_le_s();
        },
        |w| {
            w.i8x16_le_s();
        },
    );
    bitmask(&mut w);
    w.local_set(13);
    compare(
        &mut w,
        0,
        12,
        |w| {
            w.i16x8_ge_s();
        },
        |w| {
            w.i8x16_ge_s();
        },
    );
    bitmask(&mut w);
    w.local_set(15);
    w.else_();
    compare(
        &mut w,
        0,
        12,
        |w| {
            w.i16x8_le_u();
        },
        |w| {
            w.i8x16_le_u();
        },
    );
    bitmask(&mut w);
    w.local_set(13);
    compare(
        &mut w,
        0,
        12,
        |w| {
            w.i16x8_ge_u();
        },
        |w| {
            w.i8x16_ge_u();
        },
    );
    bitmask(&mut w);
    w.local_set(15);
    w.end();
    w.local_get(13).local_get(15).i32_const(1).i32_shr_u().i32_and().i32_const(0x5555).i32_and().local_get(9).i32_const(1).i32_shr_u().i32_and().local_set(13);
    w.end();
    w.local_get(13).i32_eqz().i32_eqz().local_get(8).i32_shl().local_get(7).i32_or().local_set(7);
    w.end();
    w.end();
    w.local_get(8).i32_const(1).i32_add().local_set(8).br(0);
    w.end().end();
    w.end();
    w.local_get(4).i32_const(4).i32_shr_u().i32_const(3).i32_and().local_tee(13).i32_const(1).i32_eq().if_(BlockType::Empty);
    w.local_get(7).local_get(11).i32_xor().local_set(7).end();
    w.local_get(13).i32_const(3).i32_eq().if_(BlockType::Empty);
    w.local_get(7).local_get(10).i32_xor().local_set(7).end();
    w.local_get(7).local_get(11).i32_and().end();
    body.finish()
}
