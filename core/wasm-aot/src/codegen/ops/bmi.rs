use iced_x86::{Instruction, Mnemonic, RflagsBits};
use wasm_encoder::BlockType;

use crate::Result;
use crate::codegen::function::{CF, Emitter, OF, ZF, gpr};
use crate::codegen::ops::operand::mask;

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction) -> Result<()> {
    let size = e.loc(instruction, 0)?.size();
    let bits = (size * 8) as i64;
    let full = mask(size) as i64;
    let result = e.temp64();
    match instruction.mnemonic() {
        Mnemonic::Andn => {
            e.read(instruction, 1)?;
            e.w().i64_const(-1).i64_xor();
            e.read(instruction, 2)?;
            e.w().i64_and().local_set(result);
            e.flags_logic(result, size);
        }
        Mnemonic::Bextr => {
            e.read(instruction, 1)?;
            let source = e.save();
            e.read(instruction, 2)?;
            let control = e.save();
            let start = e.temp64();
            let length = e.temp64();
            e.w().local_get(control).i64_const(0xff).i64_and().local_set(start);
            e.w().local_get(control).i64_const(8).i64_shr_u().i64_const(0xff).i64_and().local_set(length);
            e.w().local_get(source).local_get(start).i64_shr_u().i64_const(0).local_get(start).i64_const(bits).i64_lt_u().select();
            e.w().i64_const(1).local_get(length).i64_shl().i64_const(1).i64_sub().i64_const(-1).local_get(length).i64_const(bits).i64_lt_u().select();
            e.w().i64_and().local_set(result);
            e.flags_logic(result, size);
        }
        Mnemonic::Blsi | Mnemonic::Blsr | Mnemonic::Blsmsk => {
            e.read(instruction, 1)?;
            let source = e.save();
            match instruction.mnemonic() {
                Mnemonic::Blsi => e.w().i64_const(0).local_get(source).i64_sub().local_get(source).i64_and(),
                Mnemonic::Blsr => e.w().local_get(source).i64_const(1).i64_sub().local_get(source).i64_and(),
                _ => e.w().local_get(source).i64_const(1).i64_sub().local_get(source).i64_xor(),
            };
            e.w().i64_const(full).i64_and().local_set(result);
            e.flags_result(result, size);
            if e.want(RflagsBits::CF) {
                e.w().local_get(source).i64_eqz();
                if instruction.mnemonic() == Mnemonic::Blsi {
                    e.w().i32_eqz();
                }
                e.w().local_set(CF);
            }
            e.set_flag(RflagsBits::OF, OF, false);
            if instruction.mnemonic() == Mnemonic::Blsmsk {
                e.set_flag(RflagsBits::ZF, ZF, false);
            }
        }
        Mnemonic::Bzhi => {
            e.read(instruction, 1)?;
            let source = e.save();
            e.read(instruction, 2)?;
            let index = e.temp64();
            e.w().i64_const(0xff).i64_and().local_set(index);
            e.w().local_get(source).i64_const(1).local_get(index).i64_shl().i64_const(1).i64_sub().i64_and();
            e.w().local_get(source).local_get(index).i64_const(bits).i64_lt_u().select().local_set(result);
            e.flags_result(result, size);
            if e.want(RflagsBits::CF) {
                e.w().local_get(index).i64_const(bits - 1).i64_gt_u().local_set(CF);
            }
            e.set_flag(RflagsBits::OF, OF, false);
        }
        Mnemonic::Sarx | Mnemonic::Shlx | Mnemonic::Shrx => {
            e.read(instruction, 1)?;
            if instruction.mnemonic() == Mnemonic::Sarx {
                e.sign_extend(size);
            }
            e.read(instruction, 2)?;
            e.w().i64_const(bits - 1).i64_and();
            match instruction.mnemonic() {
                Mnemonic::Sarx => e.w().i64_shr_s(),
                Mnemonic::Shlx => e.w().i64_shl(),
                _ => e.w().i64_shr_u(),
            };
            e.w().local_set(result);
        }
        Mnemonic::Rorx => {
            e.read(instruction, 1)?;
            let source = e.save();
            let amount = (instruction.immediate8() as i64) & (bits - 1);
            if size == 8 {
                e.w().local_get(source).i64_const(amount).i64_rotr().local_set(result);
            } else {
                e.w().local_get(source).i32_wrap_i64().i32_const(amount as i32).i32_rotr().i64_extend_i32_u().local_set(result);
            }
        }
        Mnemonic::Mulx => {
            e.read(instruction, 2)?;
            let source = e.save();
            let multiplicand = e.temp64();
            e.w().local_get(gpr(2)).i64_const(full).i64_and().local_set(multiplicand);
            let low = e.temp64();
            if size == 8 {
                let umulh = e.context.layout.umulh;
                e.w().local_get(multiplicand).local_get(source).i64_mul().local_set(low);
                e.w().local_get(multiplicand).local_get(source).call(umulh).local_set(result);
            } else {
                e.w().local_get(multiplicand).local_get(source).i64_mul().local_tee(low).i64_const(32).i64_shr_u().local_set(result);
            }
            e.write(instruction, 1, low)?;
        }
        Mnemonic::Pdep | Mnemonic::Pext => {
            e.read(instruction, 1)?;
            let source = e.save();
            e.read(instruction, 2)?;
            let selector = e.save();
            let bit = e.temp64();
            let position = e.temp64();
            e.w().i64_const(0).local_set(result).i64_const(1).local_set(bit);
            e.w().block(BlockType::Empty).loop_(BlockType::Empty);
            e.w().local_get(selector).i64_eqz().br_if(1);
            e.w().local_get(selector).i64_const(0).local_get(selector).i64_sub().i64_and().local_set(position);
            if instruction.mnemonic() == Mnemonic::Pdep {
                e.w().local_get(source).local_get(bit).i64_and().i64_eqz().i32_eqz().if_(BlockType::Empty);
                e.w().local_get(result).local_get(position).i64_or().local_set(result).end();
            } else {
                e.w().local_get(source).local_get(position).i64_and().i64_eqz().i32_eqz().if_(BlockType::Empty);
                e.w().local_get(result).local_get(bit).i64_or().local_set(result).end();
            }
            e.w().local_get(selector).local_get(position).i64_xor().local_set(selector);
            e.w().local_get(bit).i64_const(1).i64_shl().local_set(bit).br(0).end().end();
        }
        _ => return e.unsupported(instruction),
    }
    e.write(instruction, 0, result)
}
