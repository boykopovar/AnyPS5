use iced_x86::{FlowControl, Instruction, Mnemonic, OpKind, RflagsBits};

use crate::discover::Function;
use crate::{Error, Result};

pub const STATUS: u32 = RflagsBits::OF | RflagsBits::SF | RflagsBits::ZF | RflagsBits::AF | RflagsBits::CF | RflagsBits::PF;
pub const ALL: u32 = STATUS | RflagsBits::DF;

pub fn effects(instruction: &Instruction) -> (u32, u32) {
    let read = instruction.rflags_read() & ALL;
    let mut defined = instruction.rflags_modified() & ALL;
    if is_counted_shift(instruction.mnemonic()) {
        let count_operand = instruction.op_count() - 1;
        match instruction.op_kind(count_operand) {
            OpKind::Register => return (read | defined, defined),
            OpKind::Immediate8 if instruction.immediate8() as u32 & count_mask(instruction) == 0 => defined = 0,
            _ => {}
        }
    }
    if is_repeated_compare(instruction) {
        return (read | defined, defined);
    }
    if matches!(instruction.flow_control(), FlowControl::Call | FlowControl::IndirectCall) {
        defined = ALL;
    }
    (read, defined)
}

pub fn is_repeated_compare(instruction: &Instruction) -> bool {
    (instruction.has_repe_prefix() || instruction.has_repne_prefix())
        && matches!(
            instruction.mnemonic(),
            Mnemonic::Cmpsb | Mnemonic::Cmpsw | Mnemonic::Cmpsd | Mnemonic::Cmpsq | Mnemonic::Scasb | Mnemonic::Scasw | Mnemonic::Scasd | Mnemonic::Scasq
        )
        && crate::codegen::is_string(instruction)
}

pub fn count_mask(instruction: &Instruction) -> u32 {
    let size = match instruction.op0_kind() {
        OpKind::Register => instruction.op0_register().size(),
        _ => instruction.memory_size().size(),
    };
    if size == 8 { 0x3f } else { 0x1f }
}

pub fn is_counted_shift(mnemonic: Mnemonic) -> bool {
    matches!(
        mnemonic,
        Mnemonic::Shl | Mnemonic::Shr | Mnemonic::Sar | Mnemonic::Rol | Mnemonic::Ror | Mnemonic::Rcl | Mnemonic::Rcr | Mnemonic::Shld | Mnemonic::Shrd
    )
}

pub struct Liveness {
    pub after: Vec<Vec<u32>>,
}

pub fn analyze(function: &Function, successors: &[Vec<usize>], exits: &[bool], flags_at_boundaries: bool) -> Result<Liveness> {
    let exit_live = if flags_at_boundaries { ALL } else { 0 };
    let count = function.blocks.len();
    let mut live_in = vec![0u32; count];
    let transfer = |block: usize, live_out: u32| -> u32 {
        let mut live = live_out;
        for instruction in function.blocks[block].instructions.iter().rev() {
            let (read, defined) = effects(instruction);
            live = (live & !defined) | read;
        }
        live
    };
    let live_out = |live_in: &[u32], block: usize| -> u32 {
        let mut live = if exits[block] { exit_live } else { 0 };
        for &successor in &successors[block] {
            live |= live_in[successor];
        }
        live
    };
    let mut changed = true;
    while changed {
        changed = false;
        for block in (0..count).rev() {
            let updated = transfer(block, live_out(&live_in, block));
            if updated != live_in[block] {
                live_in[block] = updated;
                changed = true;
            }
        }
    }
    if !flags_at_boundaries {
        for &entry in &function.entries {
            let block = function.block_index(entry).expect("entry is a block start");
            if live_in[block] & STATUS != 0 {
                return Err(Error::new(format!(
                    "function {:#x}: flags {:#x} are read at {entry:#x} without being set inside the function",
                    function.start,
                    live_in[block] & STATUS
                )));
            }
        }
    }
    let mut after = Vec::with_capacity(count);
    for block in 0..count {
        let mut live = live_out(&live_in, block);
        let instructions = &function.blocks[block].instructions;
        let mut masks = vec![0; instructions.len()];
        for (position, instruction) in instructions.iter().enumerate().rev() {
            masks[position] = live;
            let (read, defined) = effects(instruction);
            live = (live & !defined) | read;
        }
        after.push(masks);
    }
    Ok(Liveness { after })
}

pub fn edges(function: &Function) -> (Vec<Vec<usize>>, Vec<bool>) {
    let mut successors = vec![Vec::new(); function.blocks.len()];
    let mut exits = vec![false; function.blocks.len()];
    for (index, block) in function.blocks.iter().enumerate() {
        let last = block.instructions.last().unwrap();
        let mut targets: Vec<u64> = Vec::new();
        match last.flow_control() {
            _ if crate::codegen::is_trap(last) => {}
            FlowControl::Next | FlowControl::Call | FlowControl::IndirectCall => targets.push(block.end),
            FlowControl::ConditionalBranch => targets.extend([last.near_branch_target(), last.next_ip()]),
            FlowControl::UnconditionalBranch => targets.push(last.near_branch_target()),
            FlowControl::IndirectBranch => {
                targets.extend(function.jump_tables.get(&last.ip()).into_iter().flatten());
                exits[index] = true;
            }
            FlowControl::Return => exits[index] = true,
            FlowControl::Interrupt | FlowControl::Exception | FlowControl::XbeginXabortXend => {}
        }
        for target in targets {
            match function.block_index(target) {
                Some(successor) => successors[index].push(successor),
                None => exits[index] = true,
            }
        }
    }
    (successors, exits)
}
