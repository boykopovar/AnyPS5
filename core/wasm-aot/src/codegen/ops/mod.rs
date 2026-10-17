mod atomic;
mod bmi;
mod control;
mod data;
mod flagcalc;
mod integer;
mod operand;
mod string;
mod system;
pub use string::is_string;
mod vector;
mod x87;

use iced_x86::{Code, Instruction, Mnemonic, OpKind};

use crate::Result;
use crate::codegen::function::Emitter;
use integer::Binary;

pub fn is_trap(instruction: &Instruction) -> bool {
    instruction.code() == Code::INVALID
        || matches!(instruction.mnemonic(), Mnemonic::Ud2 | Mnemonic::Int3 | Mnemonic::Hlt | Mnemonic::Ud0 | Mnemonic::Ud1 | Mnemonic::Int1 | Mnemonic::Int)
}

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    if is_trap(instruction) {
        e.trap(instruction);
        return Ok(());
    }
    e.live &= instruction.rflags_modified();
    let locked = instruction.has_lock_prefix() && instruction.op0_kind() == OpKind::Memory;
    if string::is_string(instruction) {
        return string::translate(e, instruction);
    }
    if let Some(result) = vector::translate(e, instruction) {
        return result;
    }
    if let Some(result) = x87::translate(e, instruction) {
        return result;
    }
    let mnemonic = instruction.mnemonic();
    let binary = match mnemonic {
        Mnemonic::Add => Some(Binary::Add),
        Mnemonic::Adc => Some(Binary::Adc),
        Mnemonic::Sub => Some(Binary::Sub),
        Mnemonic::Sbb => Some(Binary::Sbb),
        Mnemonic::And => Some(Binary::And),
        Mnemonic::Or => Some(Binary::Or),
        Mnemonic::Xor => Some(Binary::Xor),
        Mnemonic::Cmp => Some(Binary::Cmp),
        Mnemonic::Test => Some(Binary::Test),
        _ => None,
    };
    if let Some(op) = binary {
        return if locked { atomic::binary(e, instruction, op) } else { integer::binary(e, instruction, op) };
    }
    if data::is_conditional_move(mnemonic) {
        return data::cmov(e, instruction);
    }
    if data::is_set(mnemonic) {
        return data::setcc(e, instruction);
    }
    match mnemonic {
        Mnemonic::Mov | Mnemonic::Movzx => data::mov(e, instruction),
        Mnemonic::Movsx | Mnemonic::Movsxd => data::movsx(e, instruction),
        Mnemonic::Lea => data::lea(e, instruction),
        Mnemonic::Xchg => data::xchg(e, instruction),
        Mnemonic::Push => data::push(e, instruction),
        Mnemonic::Pop => data::pop(e, instruction),
        Mnemonic::Leave => {
            data::leave(e);
            Ok(())
        }
        Mnemonic::Inc | Mnemonic::Dec | Mnemonic::Neg | Mnemonic::Not => {
            if locked {
                atomic::unary(e, instruction)
            } else {
                integer::unary(e, instruction)
            }
        }
        Mnemonic::Cbw | Mnemonic::Cwde | Mnemonic::Cdqe | Mnemonic::Cwd | Mnemonic::Cdq | Mnemonic::Cqo => integer::convert(e, instruction),
        Mnemonic::Imul if instruction.op_count() > 1 => integer::imul_truncating(e, instruction),
        Mnemonic::Imul | Mnemonic::Mul => integer::multiply_wide(e, instruction),
        Mnemonic::Div | Mnemonic::Idiv => integer::divide(e, instruction),
        Mnemonic::Shl | Mnemonic::Sal | Mnemonic::Shr | Mnemonic::Sar | Mnemonic::Rol | Mnemonic::Ror | Mnemonic::Rcl | Mnemonic::Rcr => {
            integer::shift(e, instruction)
        }
        Mnemonic::Shld | Mnemonic::Shrd => integer::double_shift(e, instruction),
        Mnemonic::Bt => integer::bit_test(e, instruction),
        Mnemonic::Bts | Mnemonic::Btr | Mnemonic::Btc => {
            if locked {
                atomic::bit_test(e, instruction)
            } else {
                integer::bit_test(e, instruction)
            }
        }
        Mnemonic::Bsf | Mnemonic::Bsr | Mnemonic::Tzcnt | Mnemonic::Lzcnt | Mnemonic::Popcnt => integer::bit_scan(e, instruction),
        Mnemonic::Bswap => integer::bswap(e, instruction),
        Mnemonic::Movbe => integer::movbe(e, instruction),
        Mnemonic::Cmpxchg16b => atomic::compare_exchange_16(e, instruction),
        Mnemonic::Cmpxchg => atomic::compare_exchange(e, instruction),
        Mnemonic::Xadd => atomic::exchange_add(e, instruction),
        Mnemonic::Andn
        | Mnemonic::Bextr
        | Mnemonic::Blsi
        | Mnemonic::Blsr
        | Mnemonic::Blsmsk
        | Mnemonic::Bzhi
        | Mnemonic::Sarx
        | Mnemonic::Shlx
        | Mnemonic::Shrx
        | Mnemonic::Rorx
        | Mnemonic::Mulx
        | Mnemonic::Pdep
        | Mnemonic::Pext => bmi::translate(e, instruction),
        Mnemonic::Ret => control::ret(e, instruction),
        Mnemonic::Jmp => control::jump(e, instruction),
        Mnemonic::Call => control::call(e, instruction),
        _ if instruction.flow_control() == iced_x86::FlowControl::ConditionalBranch => control::conditional(e, instruction),
        Mnemonic::Cpuid | Mnemonic::Rdtsc | Mnemonic::Rdtscp => system::runtime_call(e, instruction),
        Mnemonic::Xgetbv => {
            system::xgetbv(e);
            Ok(())
        }
        Mnemonic::Clc
        | Mnemonic::Stc
        | Mnemonic::Cmc
        | Mnemonic::Cld
        | Mnemonic::Std
        | Mnemonic::Lahf
        | Mnemonic::Sahf
        | Mnemonic::Pushfq
        | Mnemonic::Popfq => system::flag_instruction(e, instruction),
        Mnemonic::Mfence | Mnemonic::Lfence | Mnemonic::Sfence => {
            e.w().atomic_fence();
            Ok(())
        }
        Mnemonic::Nop
        | Mnemonic::Pause
        | Mnemonic::Endbr64
        | Mnemonic::Prefetch
        | Mnemonic::Prefetchnta
        | Mnemonic::Prefetcht0
        | Mnemonic::Prefetcht1
        | Mnemonic::Prefetcht2
        | Mnemonic::Prefetchw => Ok(()),
        _ => e.unsupported(instruction),
    }
}
