use iced_x86::{ConditionCode, Instruction, MemorySize, Mnemonic, OpKind, Register, RflagsBits};
use wasm_encoder::ValType;

use crate::abi::cpu;
use crate::codegen::body::mem;
use crate::codegen::function::{AF, CF, CTX, Emitter, OF, PF, SF, ZF, xlo};
use crate::{Error, Result};

const C0: i32 = 1 << 8;
const C1: i32 = 1 << 9;
const C2: i32 = 1 << 10;
const C3: i32 = 1 << 14;

fn st_index(register: Register) -> u32 {
    register as u32 - Register::ST0 as u32
}

impl Emitter<'_> {
    fn f64_temp(&mut self) -> u32 {
        self.body.temp(ValType::F64)
    }

    fn st_address(&mut self, index: u32) {
        self.w().local_get(CTX).i32_load(mem(cpu::X87_TOP)).i32_const(index as i32).i32_add().i32_const(7).i32_and();
        self.w().i64_extend_i32_u().i64_const(3).i64_shl().i64_add();
    }

    fn load_st(&mut self, index: u32) -> u32 {
        let value = self.f64_temp();
        self.w().local_get(CTX);
        self.st_address(index);
        self.w().f64_load(mem(cpu::X87_ST)).local_set(value);
        value
    }

    fn store_st(&mut self, index: u32, value: u32) {
        self.w().local_get(CTX);
        self.st_address(index);
        self.w().local_get(value).f64_store(mem(cpu::X87_ST));
    }

    fn x87_push(&mut self, value: u32) {
        let top = self.temp32();
        self.w().local_get(CTX).i32_load(mem(cpu::X87_TOP)).i32_const(1).i32_sub().i32_const(7).i32_and().local_set(top);
        self.w().local_get(CTX).local_get(top).i32_store(mem(cpu::X87_TOP));
        self.w().local_get(CTX).local_get(CTX).i32_load16_u(mem(cpu::X87_TAGS)).i32_const(1).local_get(top).i32_shl().i32_or().i32_store16(mem(cpu::X87_TAGS));
        self.store_st(0, value);
    }

    fn x87_pop(&mut self) {
        let top = self.temp32();
        self.w().local_get(CTX).i32_load(mem(cpu::X87_TOP)).local_set(top);
        self.w()
            .local_get(CTX)
            .local_get(CTX)
            .i32_load16_u(mem(cpu::X87_TAGS))
            .i32_const(1)
            .local_get(top)
            .i32_shl()
            .i32_const(-1)
            .i32_xor()
            .i32_and()
            .i32_store16(mem(cpu::X87_TAGS));
        self.w().local_get(CTX).local_get(top).i32_const(1).i32_add().i32_const(7).i32_and().i32_store(mem(cpu::X87_TOP));
    }

    fn x87_operand(&mut self, instruction: &Instruction, operand: u32) -> Result<u32> {
        match instruction.op_kind(operand) {
            OpKind::Register => Ok(self.load_st(st_index(instruction.op_register(operand)))),
            OpKind::Memory => {
                let address = self.address(instruction)?;
                let value = self.f64_temp();
                let x80_load = self.context.layout.x80_load;
                self.w().local_get(address);
                match instruction.memory_size() {
                    MemorySize::Float32 => self.w().f32_load(mem(0)).f64_promote_f32(),
                    MemorySize::Float64 => self.w().f64_load(mem(0)),
                    MemorySize::Float80 => self.w().call(x80_load),
                    MemorySize::Int16 => self.w().i32_load16_s(mem(0)).f64_convert_i32_s(),
                    MemorySize::Int32 => self.w().i32_load(mem(0)).f64_convert_i32_s(),
                    MemorySize::Int64 => self.w().i64_load(mem(0)).f64_convert_i64_s(),
                    size => {
                        return Err(Error::new(format!("{:#x}: x87 load of {size:?}", instruction.ip())));
                    }
                };
                self.w().local_set(value);
                Ok(value)
            }
            kind => Err(Error::new(format!("{:#x}: x87 operand {kind:?}", instruction.ip()))),
        }
    }

    fn round_by_control(&mut self, value: u32) {
        let control = self.temp32();
        self.w().local_get(CTX).i32_load16_u(mem(cpu::X87_FCW)).i32_const(10).i32_shr_u().i32_const(3).i32_and().local_set(control);
        let rounded = self.f64_temp();
        self.w().local_get(value).f64_trunc().local_set(rounded);
        self.w().local_get(value).f64_ceil().local_get(rounded).local_get(control).i32_const(2).i32_eq().select().local_set(rounded);
        self.w().local_get(value).f64_floor().local_get(rounded).local_get(control).i32_const(1).i32_eq().select().local_set(rounded);
        self.w().local_get(value).f64_nearest().local_get(rounded).local_get(control).i32_eqz().select();
    }

    fn x87_store(&mut self, instruction: &Instruction, value: u32, truncate: bool) -> Result<()> {
        let address = self.address(instruction)?;
        let size = instruction.memory_size();
        if matches!(size, MemorySize::Int16 | MemorySize::Int32 | MemorySize::Int64) {
            let rounded = self.f64_temp();
            if truncate {
                self.w().local_get(value).f64_trunc();
            } else {
                self.round_by_control(value);
            }
            self.w().local_set(rounded);
            let (low, high, indefinite) = match size {
                MemorySize::Int16 => (-32768.0, 32767.0, 0x8000i64),
                MemorySize::Int32 => (-2147483648.0, 2147483647.0, 0x8000_0000),
                _ => (-9223372036854775808.0, 9223372036854774784.0, i64::MIN),
            };
            self.w().local_get(address).local_get(rounded).i64_trunc_sat_f64_s().i64_const(indefinite);
            self.w().local_get(rounded).f64_const(low.into()).f64_ge().local_get(rounded).f64_const(high.into()).f64_le().i32_and().select();
            match size {
                MemorySize::Int16 => self.w().i64_store16(mem(0)),
                MemorySize::Int32 => self.w().i64_store32(mem(0)),
                _ => self.w().i64_store(mem(0)),
            };
            return Ok(());
        }
        let x80_store = self.context.layout.x80_store;
        self.w().local_get(address).local_get(value);
        match size {
            MemorySize::Float32 => self.w().f32_demote_f64().f32_store(mem(0)),
            MemorySize::Float64 => self.w().f64_store(mem(0)),
            MemorySize::Float80 => self.w().call(x80_store),
            other => {
                return Err(Error::new(format!("{:#x}: x87 store of {other:?}", instruction.ip())));
            }
        };
        Ok(())
    }

    fn status_word(&mut self) {
        self.w().local_get(CTX).i32_load16_u(mem(cpu::X87_FSW)).i32_const(!0x3800).i32_and();
        self.w().local_get(CTX).i32_load(mem(cpu::X87_TOP)).i32_const(11).i32_shl().i32_or();
    }

    fn condition_codes(&mut self, a: u32, b: u32) {
        let codes = self.temp32();
        self.w().i32_const(C0 | C2 | C3);
        self.w().i32_const(0).i32_const(C3).local_get(a).local_get(b).f64_eq().select();
        self.w().i32_const(C0).local_get(a).local_get(b).f64_lt().select();
        self.w().local_get(a).local_get(a).f64_ne().local_get(b).local_get(b).f64_ne().i32_or().select().local_set(codes);
        self.w().local_get(CTX).local_get(CTX).i32_load16_u(mem(cpu::X87_FSW)).i32_const(!(C0 | C1 | C2 | C3)).i32_and();
        self.w().local_get(codes).i32_or().i32_store16(mem(cpu::X87_FSW));
    }
}

pub fn translate(e: &mut Emitter<'_>, instruction: &Instruction) -> Option<Result<()>> {
    use Mnemonic::*;
    let mnemonic = instruction.mnemonic();
    let result = match mnemonic {
        Fld | Fild => (|| {
            let value = e.x87_operand(instruction, instruction.op_count() - 1)?;
            e.x87_push(value);
            Ok(())
        })(),
        Fldz | Fld1 | Fldpi | Fldl2e | Fldl2t | Fldlg2 | Fldln2 => {
            let constant = match mnemonic {
                Fldz => 0.0,
                Fld1 => 1.0,
                Fldpi => std::f64::consts::PI,
                Fldl2e => std::f64::consts::LOG2_E,
                Fldl2t => std::f64::consts::LOG2_10,
                Fldlg2 => std::f64::consts::LOG10_2,
                _ => std::f64::consts::LN_2,
            };
            let value = e.f64_temp();
            e.w().f64_const(constant.into()).local_set(value);
            e.x87_push(value);
            Ok(())
        }
        Fst | Fstp | Fist | Fistp | Fisttp => (|| {
            let value = e.load_st(0);
            if instruction.op0_kind() == OpKind::Register {
                e.store_st(st_index(instruction.op0_register()), value);
            } else {
                e.x87_store(instruction, value, mnemonic == Fisttp)?;
            }
            if matches!(mnemonic, Fstp | Fistp | Fisttp) {
                e.x87_pop();
            }
            Ok(())
        })(),
        Fadd | Faddp | Fiadd | Fsub | Fsubp | Fisub | Fsubr | Fsubrp | Fisubr | Fmul | Fmulp | Fimul | Fdiv | Fdivp | Fidiv | Fdivr | Fdivrp | Fidivr => {
            arithmetic(e, instruction, mnemonic)
        }
        Fxch => {
            let other = st_index(instruction.op1_register());
            let first = e.load_st(0);
            let second = e.load_st(other);
            e.store_st(0, second);
            e.store_st(other, first);
            Ok(())
        }
        Fchs | Fabs | Fsqrt | Frndint => {
            let value = e.load_st(0);
            let result = e.f64_temp();
            match mnemonic {
                Fchs => {
                    e.w().local_get(value).f64_neg();
                }
                Fabs => {
                    e.w().local_get(value).f64_abs();
                }
                Fsqrt => {
                    e.w().local_get(value).f64_sqrt();
                }
                _ => e.round_by_control(value),
            }
            e.w().local_set(result);
            e.store_st(0, result);
            Ok(())
        }
        Fscale => {
            let value = e.load_st(0);
            let exponent = e.load_st(1);
            let result = e.f64_temp();
            let scalbn = e.context.layout.scalbn;
            e.w()
                .local_get(value)
                .local_get(exponent)
                .f64_trunc()
                .f64_const((-1.0e5f64).into())
                .f64_max()
                .f64_const(1.0e5f64.into())
                .f64_min()
                .i32_trunc_f64_s();
            e.w().call(scalbn).local_set(result);
            e.store_st(0, result);
            Ok(())
        }
        Fprem | Fprem1 => {
            let x = e.load_st(0);
            let y = e.load_st(1);
            let quotient = e.f64_temp();
            let result = e.f64_temp();
            e.w().local_get(x).local_get(y).f64_div();
            if mnemonic == Fprem {
                e.w().f64_trunc();
            } else {
                e.w().f64_nearest();
            }
            e.w().local_set(quotient).local_get(x).local_get(quotient).local_get(y).f64_mul().f64_sub().local_set(result);
            e.store_st(0, result);
            let bits = e.temp32();
            e.w().local_get(quotient).f64_abs().i64_trunc_sat_f64_u().i32_wrap_i64().local_set(bits);
            e.w().local_get(CTX).local_get(CTX).i32_load16_u(mem(cpu::X87_FSW)).i32_const(!(C0 | C1 | C2 | C3)).i32_and();
            e.w().local_get(bits).i32_const(4).i32_and().i32_const(6).i32_shl().i32_or();
            e.w().local_get(bits).i32_const(2).i32_and().i32_const(13).i32_shl().i32_or();
            e.w().local_get(bits).i32_const(1).i32_and().i32_const(9).i32_shl().i32_or().i32_store16(mem(cpu::X87_FSW));
            Ok(())
        }
        Fxtract => {
            let value = e.load_st(0);
            let exponent = e.f64_temp();
            let significand = e.f64_temp();
            e.w()
                .local_get(value)
                .i64_reinterpret_f64()
                .i64_const(52)
                .i64_shr_u()
                .i64_const(0x7ff)
                .i64_and()
                .i64_const(1023)
                .i64_sub()
                .f64_convert_i64_s()
                .local_set(exponent);
            e.w()
                .local_get(value)
                .i64_reinterpret_f64()
                .i64_const(!(0x7ffi64 << 52))
                .i64_and()
                .i64_const(1023i64 << 52)
                .i64_or()
                .f64_reinterpret_i64()
                .local_set(significand);
            e.store_st(0, exponent);
            e.x87_push(significand);
            Ok(())
        }
        Fucomi | Fucomip | Fcomi | Fcomip => {
            let a = e.load_st(0);
            let b = e.load_st(st_index(instruction.op1_register()));
            let unordered = e.temp32();
            e.w().local_get(a).local_get(a).f64_ne().local_get(b).local_get(b).f64_ne().i32_or().local_set(unordered);
            if e.want(RflagsBits::ZF) {
                e.w().local_get(a).local_get(b).f64_eq().local_get(unordered).i32_or().local_set(ZF);
            }
            if e.want(RflagsBits::CF) {
                e.w().local_get(a).local_get(b).f64_lt().local_get(unordered).i32_or().local_set(CF);
            }
            if e.want(RflagsBits::PF) {
                e.w().local_get(unordered).local_set(PF);
            }
            for (flag, local) in [(RflagsBits::OF, OF), (RflagsBits::SF, SF), (RflagsBits::AF, AF)] {
                e.set_flag(flag, local, false);
            }
            if matches!(mnemonic, Fucomip | Fcomip) {
                e.x87_pop();
            }
            Ok(())
        }
        Fucom | Fucomp | Fucompp | Fcom | Fcomp | Fcompp | Ftst => (|| {
            let a = e.load_st(0);
            let b = if mnemonic == Ftst {
                let zero = e.f64_temp();
                e.w().f64_const(0.0f64.into()).local_set(zero);
                zero
            } else if instruction.op_count() == 0 {
                e.load_st(1)
            } else {
                e.x87_operand(instruction, instruction.op_count() - 1)?
            };
            e.condition_codes(a, b);
            if matches!(mnemonic, Fucomp | Fcomp | Fucompp | Fcompp) {
                e.x87_pop();
            }
            if matches!(mnemonic, Fucompp | Fcompp) {
                e.x87_pop();
            }
            Ok(())
        })(),
        Fcmovb | Fcmove | Fcmovbe | Fcmovu | Fcmovnb | Fcmovne | Fcmovnbe | Fcmovnu => {
            let condition = match mnemonic {
                Fcmovb => ConditionCode::b,
                Fcmove => ConditionCode::e,
                Fcmovbe => ConditionCode::be,
                Fcmovu => ConditionCode::p,
                Fcmovnb => ConditionCode::ae,
                Fcmovne => ConditionCode::ne,
                Fcmovnbe => ConditionCode::a,
                _ => ConditionCode::np,
            };
            let current = e.load_st(0);
            let source = e.load_st(st_index(instruction.op1_register()));
            let result = e.f64_temp();
            e.w().local_get(source).local_get(current);
            e.condition(condition);
            e.w().select().local_set(result);
            e.store_st(0, result);
            Ok(())
        }
        Fnstsw | Fstsw => (|| {
            e.status_word();
            let word = e.temp32();
            e.w().local_set(word);
            if instruction.op0_kind() == OpKind::Register {
                let value = e.temp64();
                e.w().local_get(word).i64_extend_i32_u().local_set(value);
                e.write_gpr(0, 2, false, value);
            } else {
                let address = e.address(instruction)?;
                e.w().local_get(address).local_get(word).i32_store16(mem(0));
            }
            Ok(())
        })(),
        Fnstcw | Fstcw => (|| {
            let address = e.address(instruction)?;
            e.w().local_get(address).local_get(CTX).i32_load16_u(mem(cpu::X87_FCW)).i32_store16(mem(0));
            Ok(())
        })(),
        Fldcw => (|| {
            let address = e.address(instruction)?;
            e.w().local_get(CTX).local_get(address).i32_load16_u(mem(0)).i32_store16(mem(cpu::X87_FCW));
            Ok(())
        })(),
        Fnstenv | Fstenv => environment(e, instruction, true),
        Fldenv => environment(e, instruction, false),
        Fnclex | Fclex => {
            e.w().local_get(CTX).local_get(CTX).i32_load16_u(mem(cpu::X87_FSW)).i32_const(!0x80ff).i32_and().i32_store16(mem(cpu::X87_FSW));
            Ok(())
        }
        Fninit | Finit => {
            e.w().local_get(CTX).i32_const(0x37f).i32_store16(mem(cpu::X87_FCW));
            e.w().local_get(CTX).i32_const(0).i32_store16(mem(cpu::X87_FSW));
            e.w().local_get(CTX).i32_const(0).i32_store16(mem(cpu::X87_TAGS));
            e.w().local_get(CTX).i32_const(0).i32_store(mem(cpu::X87_TOP));
            Ok(())
        }
        Wait | Fnop => Ok(()),
        Ffree => {
            let index = st_index(instruction.op0_register());
            e.w().local_get(CTX).local_get(CTX).i32_load16_u(mem(cpu::X87_TAGS));
            e.w().i32_const(1).local_get(CTX).i32_load(mem(cpu::X87_TOP)).i32_const(index as i32).i32_add().i32_const(7).i32_and().i32_shl();
            e.w().i32_const(-1).i32_xor().i32_and().i32_store16(mem(cpu::X87_TAGS));
            Ok(())
        }
        Fxsave | Fxsave64 => fxsave(e, instruction, true),
        Fxrstor | Fxrstor64 => fxsave(e, instruction, false),
        _ => return None,
    };
    Some(result)
}

fn arithmetic(e: &mut Emitter<'_>, instruction: &Instruction, mnemonic: Mnemonic) -> Result<()> {
    use Mnemonic::*;
    let (destination_index, source) = if instruction.op_count() == 2 { (st_index(instruction.op0_register()), 1) } else { (0, 0) };
    let a = e.load_st(destination_index);
    let b = e.x87_operand(instruction, source)?;
    let reversed = matches!(mnemonic, Fsubr | Fsubrp | Fisubr | Fdivr | Fdivrp | Fidivr);
    let result = e.f64_temp();
    if reversed {
        e.w().local_get(b).local_get(a);
    } else {
        e.w().local_get(a).local_get(b);
    }
    match mnemonic {
        Fadd | Faddp | Fiadd => e.w().f64_add(),
        Fsub | Fsubp | Fisub | Fsubr | Fsubrp | Fisubr => e.w().f64_sub(),
        Fmul | Fmulp | Fimul => e.w().f64_mul(),
        _ => e.w().f64_div(),
    };
    e.w().local_set(result);
    e.store_st(destination_index, result);
    if matches!(mnemonic, Faddp | Fsubp | Fsubrp | Fmulp | Fdivp | Fdivrp) {
        e.x87_pop();
    }
    Ok(())
}

fn environment(e: &mut Emitter<'_>, instruction: &Instruction, store: bool) -> Result<()> {
    let address = e.address(instruction)?;
    if store {
        e.w().local_get(address).local_get(CTX).i32_load16_u(mem(cpu::X87_FCW)).i32_store(mem(0));
        e.status_word();
        let status = e.temp32();
        e.w().local_set(status).local_get(address).local_get(status).i32_store(mem(4));
        let tags = e.temp32();
        let value = e.f64_temp();
        e.w().i32_const(0).local_set(tags);
        for register in 0..8u64 {
            e.w().local_get(CTX).f64_load(mem(cpu::X87_ST + 8 * register)).local_set(value);
            e.w().i32_const(3);
            e.w().i32_const(1).i32_const(0).i32_const(2);
            e.w().local_get(value).f64_abs().f64_const(f64::INFINITY.into()).f64_lt().select();
            e.w().local_get(value).f64_const(0.0.into()).f64_eq().select();
            e.w().local_get(CTX).i32_load16_u(mem(cpu::X87_TAGS)).i32_const(1 << register).i32_and().i32_eqz().select();
            e.w().i32_const(2 * register as i32).i32_shl().local_get(tags).i32_or().local_set(tags);
        }
        e.w().local_get(address).local_get(tags).i32_store(mem(8));
        for offset in [12u64, 16, 20, 24] {
            e.w().local_get(address).i32_const(0).i32_store(mem(offset));
        }
        e.w().local_get(CTX).local_get(CTX).i32_load16_u(mem(cpu::X87_FCW)).i32_const(0x3f).i32_or().i32_store16(mem(cpu::X87_FCW));
        return Ok(());
    }
    e.w().local_get(CTX).local_get(address).i32_load16_u(mem(0)).i32_store16(mem(cpu::X87_FCW));
    let status = e.temp32();
    e.w().local_get(address).i32_load16_u(mem(4)).local_set(status);
    e.w().local_get(CTX).local_get(status).i32_const(!0x3800).i32_and().i32_store16(mem(cpu::X87_FSW));
    e.w().local_get(CTX).local_get(status).i32_const(11).i32_shr_u().i32_const(7).i32_and().i32_store(mem(cpu::X87_TOP));
    let tags = e.temp32();
    e.w().i32_const(0).local_set(tags);
    for register in 0..8 {
        e.w().local_get(address).i32_load16_u(mem(8)).i32_const(2 * register).i32_shr_u().i32_const(3).i32_and().i32_const(3).i32_ne();
        e.w().i32_const(register).i32_shl().local_get(tags).i32_or().local_set(tags);
    }
    e.w().local_get(CTX).local_get(tags).i32_store16(mem(cpu::X87_TAGS));
    Ok(())
}

fn fxsave(e: &mut Emitter<'_>, instruction: &Instruction, save: bool) -> Result<()> {
    let address = e.address(instruction)?;
    let (x80_load, x80_store) = (e.context.layout.x80_load, e.context.layout.x80_store);
    if save {
        e.w().local_get(address).local_get(CTX).i32_load16_u(mem(cpu::X87_FCW)).i32_store16(mem(0));
        e.status_word();
        let status = e.temp32();
        e.w().local_set(status).local_get(address).local_get(status).i32_store16(mem(2));
        e.w().local_get(address).local_get(CTX).i32_load16_u(mem(cpu::X87_TAGS)).i32_store8(mem(4));
        e.w().local_get(address).local_get(CTX).i32_load(mem(cpu::MXCSR)).i32_store(mem(24));
        e.w().local_get(address).i32_const(0xffff).i32_store(mem(28));
        for index in 0..8 {
            let value = e.load_st(index);
            e.w().local_get(address).i64_const(32 + 16 * index as i64).i64_add().local_get(value).call(x80_store);
        }
        for index in 0..16 {
            e.w().local_get(address).local_get(xlo(index)).v128_store(mem(160 + 16 * index as u64));
        }
        return Ok(());
    }
    e.w().local_get(CTX).local_get(address).i32_load16_u(mem(0)).i32_store16(mem(cpu::X87_FCW));
    let status = e.temp32();
    e.w().local_get(address).i32_load16_u(mem(2)).local_set(status);
    e.w().local_get(CTX).local_get(status).i32_const(!0x3800).i32_and().i32_store16(mem(cpu::X87_FSW));
    e.w().local_get(CTX).local_get(status).i32_const(11).i32_shr_u().i32_const(7).i32_and().i32_store(mem(cpu::X87_TOP));
    e.w().local_get(CTX).local_get(address).i32_load8_u(mem(4)).i32_store16(mem(cpu::X87_TAGS));
    e.w().local_get(CTX).local_get(address).i32_load(mem(24)).i32_store(mem(cpu::MXCSR));
    for index in 0..8 {
        let value = e.f64_temp();
        e.w().local_get(address).i64_const(32 + 16 * index as i64).i64_add().call(x80_load).local_set(value);
        e.store_st(index, value);
    }
    for index in 0..16 {
        e.w().local_get(address).v128_load(mem(160 + 16 * index as u64)).local_set(xlo(index));
    }
    Ok(())
}
