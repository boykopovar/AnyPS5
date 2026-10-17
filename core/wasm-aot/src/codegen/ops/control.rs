use iced_x86::{FlowControl, Instruction, Mnemonic};

use crate::Result;
use crate::abi;
use crate::codegen::TYPE_CTX;
use crate::codegen::body::mem;
use crate::codegen::function::{CTX, Emitter, ZF, gpr};
use crate::discover::slot_operand;
use crate::program::Target;

pub fn ret(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let rsp = gpr(4);
    let pop = 8 + if instruction.op_count() == 1 { instruction.immediate16() as i64 } else { 0 };
    e.w().local_get(CTX).local_get(rsp).i64_load(mem(0)).i64_store(mem(abi::cpu::RIP));
    e.w().local_get(rsp).i64_const(pop).i64_add().local_set(rsp);
    e.flush();
    e.w().return_();
    Ok(())
}

pub fn conditional(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let rcx = gpr(1);
    match instruction.mnemonic() {
        Mnemonic::Jrcxz => {
            e.w().local_get(rcx).i64_eqz();
        }
        Mnemonic::Jecxz => {
            e.w().local_get(rcx).i64_const(0xffff_ffff).i64_and().i64_eqz();
        }
        Mnemonic::Loop | Mnemonic::Loope | Mnemonic::Loopne => {
            e.w().local_get(rcx).i64_const(1).i64_sub().local_tee(rcx).i64_eqz().i32_eqz();
            match instruction.mnemonic() {
                Mnemonic::Loope => {
                    e.w().local_get(ZF).i32_and();
                }
                Mnemonic::Loopne => {
                    e.w().local_get(ZF).i32_eqz().i32_and();
                }
                _ => {}
            }
        }
        _ => e.condition(instruction.condition_code()),
    }
    e.open_if();
    e.jump(instruction.near_branch_target())?;
    e.close();
    e.fallthrough(instruction.next_ip())
}

fn through_plt(e: &Emitter<'_>, vaddr: u64) -> Option<Target> {
    let discovery = e.context.discovery;
    let function = &discovery.functions[discovery.function_at(e.image(), vaddr)?];
    let first = function.blocks.first()?.instructions.first()?;
    if function.start != vaddr || first.flow_control() != FlowControl::IndirectBranch {
        return None;
    }
    slot_operand(e.context.program, e.image(), first)
}

pub fn jump(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    if instruction.flow_control() == FlowControl::UnconditionalBranch {
        let target = instruction.near_branch_target();
        if e.function.block_index(target).is_none()
            && let Some(bound) = through_plt(e, target)
        {
            return tail_to(e, bound);
        }
        return e.jump(target);
    }
    if let Some(bound) = slot_operand(e.context.program, e.image(), instruction) {
        return tail_to(e, bound);
    }
    e.read(instruction, 0)?;
    let target = e.save();
    if let Some(targets) = e.function.jump_tables.get(&instruction.ip()).cloned() {
        let offset = e.temp64();
        let base = e.context.layout.image_base_global(e.image());
        e.w().local_get(target).global_get(base).i64_sub().local_set(offset);
        let mut seen = Vec::new();
        for vaddr in targets {
            if seen.contains(&vaddr) || e.function.block_index(vaddr).is_none() {
                continue;
            }
            seen.push(vaddr);
            e.w().local_get(offset).i64_const(vaddr as i64).i64_eq();
            e.open_if();
            e.jump(vaddr)?;
            e.close();
        }
    }
    e.tail_call_indirect(target);
    Ok(())
}

fn tail_to(e: &mut Emitter<'_>, target: Target) -> Result<()> {
    match target {
        Target::Guest { image, vaddr } => e.tail_call(image, vaddr),
        Target::Import(index) => {
            e.tail_call_import(index);
            Ok(())
        }
    }
}

pub fn call(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    if instruction.flow_control() == FlowControl::Call {
        let target = instruction.near_branch_target();
        let bound = through_plt(e, target).unwrap_or(Target::Guest { image: e.image(), vaddr: target });
        return e.call_target(instruction, bound);
    }
    if let Some(bound) = slot_operand(e.context.program, e.image(), instruction) {
        return e.call_target(instruction, bound);
    }
    e.read(instruction, 0)?;
    let target = e.save();
    let lookup = e.context.layout.lookup;
    e.call_with(instruction, |emitter| {
        emitter.store_ctx_i64(abi::cpu::RIP, target);
        emitter.w().local_get(CTX).local_get(CTX).local_get(target).call(lookup).call_indirect(0, TYPE_CTX);
    })
}
