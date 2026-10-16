use iced_x86::{Instruction, Mnemonic};

use crate::Result;
use crate::abi;
use crate::codegen::body::mem;
use crate::codegen::function::{AF, CF, CTX, DF, Emitter, FLAG_BITS, PF, SF, ZF, gpr};

pub fn runtime_call(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let function = match instruction.mnemonic() {
        Mnemonic::Cpuid => e.context.layout.cpuid,
        _ => e.context.layout.rdtsc,
    };
    e.flush();
    e.w().local_get(CTX).call(function);
    e.reload();
    if instruction.mnemonic() == Mnemonic::Rdtscp {
        e.w().i64_const(0).local_set(gpr(1));
    }
    Ok(())
}

pub fn xgetbv(e: &mut Emitter<'_>) {
    e.w().i64_const(0x7).local_set(gpr(0)).i64_const(0).local_set(gpr(2));
}

pub fn flag_instruction(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    match instruction.mnemonic() {
        Mnemonic::Clc => e.w().i32_const(0).local_set(CF),
        Mnemonic::Stc => e.w().i32_const(1).local_set(CF),
        Mnemonic::Cmc => e.w().local_get(CF).i32_eqz().local_set(CF),
        Mnemonic::Cld => e.w().i32_const(0).local_set(DF),
        Mnemonic::Std => e.w().i32_const(1).local_set(DF),
        Mnemonic::Lahf => {
            let value = e.temp64();
            e.w().i64_const(0x2);
            for (local, bit) in [(CF, 0), (PF, 2), (AF, 4), (ZF, 6), (SF, 7)] {
                e.w().local_get(local).i64_extend_i32_u().i64_const(bit).i64_shl().i64_or();
            }
            e.w().local_set(value);
            e.write_gpr(0, 1, true, value);
            return Ok(());
        }
        Mnemonic::Sahf => {
            for (local, bit) in [(CF, 0), (PF, 2), (AF, 4), (ZF, 6), (SF, 7)] {
                e.w().local_get(gpr(0)).i64_const(8 + bit).i64_shr_u().i32_wrap_i64().i32_const(1).i32_and().local_set(local);
            }
            return Ok(());
        }
        Mnemonic::Pushfq => {
            let value = rflags_value(e);
            e.push_value(value);
            return Ok(());
        }
        _ => {
            let rsp = gpr(4);
            let value = e.temp64();
            e.w().local_get(rsp).i64_load(mem(0)).local_set(value).local_get(rsp).i64_const(8).i64_add().local_set(rsp);
            for (local, bit, _) in FLAG_BITS {
                e.w().local_get(value).i64_const(bit as i64).i64_shr_u().i32_wrap_i64().i32_const(1).i32_and().local_set(local);
            }
            return Ok(());
        }
    };
    Ok(())
}

fn rflags_value(e: &mut Emitter<'_>) -> u32 {
    let mut keep = !0u64;
    for (_, bit, _) in FLAG_BITS {
        keep &= !(1u64 << bit);
    }
    let value = e.temp64();
    e.w().local_get(CTX).i64_load(mem(abi::cpu::RFLAGS)).i64_const(keep as i64).i64_and();
    for (local, bit, _) in FLAG_BITS {
        e.w().local_get(local).i64_extend_i32_u().i64_const(bit as i64).i64_shl().i64_or();
    }
    e.w().local_set(value);
    value
}
