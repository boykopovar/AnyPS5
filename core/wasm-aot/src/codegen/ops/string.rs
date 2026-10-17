use iced_x86::{Instruction, Mnemonic, OpKind, Register, RflagsBits};
use wasm_encoder::BlockType;

use crate::codegen::function::{DF, Emitter, ZF, gpr};
use crate::{Error, Result};

#[derive(Clone, Copy, PartialEq, Eq)]
enum Kind {
    Movs,
    Stos,
    Lods,
    Scas,
    Cmps,
}

fn kind(mnemonic: Mnemonic) -> Option<Kind> {
    Some(match mnemonic {
        Mnemonic::Movsb | Mnemonic::Movsw | Mnemonic::Movsd | Mnemonic::Movsq => Kind::Movs,
        Mnemonic::Stosb | Mnemonic::Stosw | Mnemonic::Stosd | Mnemonic::Stosq => Kind::Stos,
        Mnemonic::Lodsb | Mnemonic::Lodsw | Mnemonic::Lodsd | Mnemonic::Lodsq => Kind::Lods,
        Mnemonic::Scasb | Mnemonic::Scasw | Mnemonic::Scasd | Mnemonic::Scasq => Kind::Scas,
        Mnemonic::Cmpsb | Mnemonic::Cmpsw | Mnemonic::Cmpsd | Mnemonic::Cmpsq => Kind::Cmps,
        _ => return None,
    })
}

fn string_operands(instruction: &Instruction) -> impl Iterator<Item = OpKind> + '_ {
    (0..instruction.op_count()).map(|operand| instruction.op_kind(operand)).filter(|kind| {
        matches!(
            kind,
            OpKind::MemorySegSI
                | OpKind::MemorySegESI
                | OpKind::MemorySegRSI
                | OpKind::MemorySegDI
                | OpKind::MemorySegEDI
                | OpKind::MemorySegRDI
                | OpKind::MemoryESDI
                | OpKind::MemoryESEDI
                | OpKind::MemoryESRDI
        )
    })
}

pub fn is_string(instruction: &Instruction) -> bool {
    kind(instruction.mnemonic()).is_some() && string_operands(instruction).next().is_some()
}

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let kind = kind(instruction.mnemonic()).unwrap();
    let wide = string_operands(instruction).all(|kind| matches!(kind, OpKind::MemorySegRSI | OpKind::MemorySegRDI | OpKind::MemoryESRDI));
    if instruction.memory_segment() == Register::FS || instruction.memory_segment() == Register::GS || !wide {
        return Err(Error::new(format!("{:#x}: string instruction with a segment or address size override", instruction.ip())));
    }
    let size = instruction.memory_size().size() as u32;
    let step = e.temp64();
    e.w().i64_const(size as i64).i64_const(-(size as i64)).local_get(DF).i32_eqz().select().local_set(step);
    let repeat = instruction.has_rep_prefix() || instruction.has_repne_prefix();
    if !repeat {
        return element(e, kind, size, step);
    }
    let rcx = gpr(1);
    if matches!(kind, Kind::Scas | Kind::Cmps) {
        e.live |= RflagsBits::ZF;
    }
    e.w().block(BlockType::Empty);
    e.nest += 1;
    if kind == Kind::Movs || (kind == Kind::Stos && size == 1) {
        fast_path(e, kind, size)?;
    }
    e.w().loop_(BlockType::Empty);
    e.nest += 1;
    e.w().local_get(rcx).i64_eqz().br_if(1);
    element(e, kind, size, step)?;
    e.w().local_get(rcx).i64_const(1).i64_sub().local_set(rcx);
    if matches!(kind, Kind::Scas | Kind::Cmps) {
        e.w().local_get(ZF);
        if instruction.has_rep_prefix() {
            e.w().i32_eqz();
        }
        e.w().br_if(1);
    }
    e.w().br(0).end();
    e.nest -= 1;
    e.w().end();
    e.nest -= 1;
    Ok(())
}

fn fast_path(e: &mut Emitter<'_>, kind: Kind, size: u32) -> Result<()> {
    let (rcx, rsi, rdi) = (gpr(1), gpr(6), gpr(7));
    let total = e.temp64();
    e.w().local_get(rcx).i64_const(size as i64).i64_mul().local_set(total);
    e.w().local_get(DF).i32_eqz();
    if kind == Kind::Movs {
        e.w().local_get(rdi).local_get(rsi).i64_sub().local_get(total).i64_ge_u().i32_and();
    }
    e.w().if_(BlockType::Empty);
    if kind == Kind::Movs {
        e.w().local_get(rdi).local_get(rsi).local_get(total).memory_copy(0, 0);
        e.w().local_get(rsi).local_get(total).i64_add().local_set(rsi);
    } else {
        e.w().local_get(rdi).local_get(gpr(0)).i32_wrap_i64().local_get(total).memory_fill(0);
    }
    e.w().local_get(rdi).local_get(total).i64_add().local_set(rdi);
    e.w().i64_const(0).local_set(rcx).br(1).end();
    Ok(())
}

fn element(e: &mut Emitter<'_>, kind: Kind, size: u32, step: u32) -> Result<()> {
    let (rsi, rdi) = (gpr(6), gpr(7));
    match kind {
        Kind::Movs => {
            e.w().local_get(rdi).local_get(rsi);
            e.load(size)?;
            e.store(size)?;
        }
        Kind::Stos => {
            e.w().local_get(rdi).local_get(gpr(0));
            e.store(size)?;
        }
        Kind::Lods => {
            e.w().local_get(rsi);
            e.load(size)?;
            let value = e.save();
            e.write_gpr(0, size, false, value);
        }
        Kind::Scas | Kind::Cmps => {
            let a = e.temp64();
            let b = e.temp64();
            if kind == Kind::Scas {
                e.w().local_get(gpr(0)).local_set(a);
            } else {
                e.w().local_get(rsi);
                e.load(size)?;
                e.w().local_set(a);
            }
            e.w().local_get(rdi);
            e.load(size)?;
            e.w().local_set(b);
            let result = e.temp64();
            e.w().local_get(a).local_get(b).i64_sub().local_set(result);
            e.flags_sub(a, b, result, size);
        }
    }
    if matches!(kind, Kind::Movs | Kind::Lods | Kind::Cmps) {
        e.w().local_get(rsi).local_get(step).i64_add().local_set(rsi);
    }
    if matches!(kind, Kind::Movs | Kind::Stos | Kind::Scas | Kind::Cmps) {
        e.w().local_get(rdi).local_get(step).i64_add().local_set(rdi);
    }
    Ok(())
}
