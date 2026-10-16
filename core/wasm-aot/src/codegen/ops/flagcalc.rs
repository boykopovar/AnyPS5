use iced_x86::{ConditionCode, RflagsBits};

use crate::codegen::function::{AF, CF, Emitter, OF, PF, SF, ZF};
use crate::codegen::ops::operand::mask;

impl Emitter<'_> {
    pub fn bit_of(&mut self, local: u32, bit: u32) {
        self.w().local_get(local);
        if bit != 0 {
            self.w().i64_const(bit as i64).i64_shr_u();
        }
        self.w().i32_wrap_i64().i32_const(1).i32_and();
    }

    pub fn set_flag(&mut self, flag: u32, local_flag: u32, value: bool) {
        if self.want(flag) {
            self.w().i32_const(value as i32).local_set(local_flag);
        }
    }

    pub fn flags_result(&mut self, result: u32, size: u32) {
        if self.want(RflagsBits::ZF) {
            self.w().local_get(result);
            if size < 8 {
                self.w().i64_const(mask(size) as i64).i64_and();
            }
            self.w().i64_eqz().local_set(ZF);
        }
        if self.want(RflagsBits::SF) {
            self.bit_of(result, size * 8 - 1);
            self.w().local_set(SF);
        }
        if self.want(RflagsBits::PF) {
            self.w().local_get(result).i64_const(0xff).i64_and().i64_popcnt().i32_wrap_i64().i32_const(1).i32_and().i32_eqz().local_set(PF);
        }
    }

    pub fn flags_add(&mut self, a: u32, b: u32, result: u32, size: u32) {
        let top = size * 8 - 1;
        if self.want(RflagsBits::CF) {
            let carries = self.temp64();
            self.w().local_get(a).local_get(b).i64_and();
            self.w().local_get(a).local_get(b).i64_or().local_get(result).i64_const(-1).i64_xor().i64_and().i64_or().local_set(carries);
            self.bit_of(carries, top);
            self.w().local_set(CF);
        }
        if self.want(RflagsBits::OF) {
            let overflow = self.temp64();
            self.w().local_get(a).local_get(result).i64_xor().local_get(b).local_get(result).i64_xor().i64_and().local_set(overflow);
            self.bit_of(overflow, top);
            self.w().local_set(OF);
        }
        self.flag_af(a, b, result);
        self.flags_result(result, size);
    }

    pub fn flags_sub(&mut self, a: u32, b: u32, result: u32, size: u32) {
        let top = size * 8 - 1;
        if self.want(RflagsBits::CF) {
            let borrows = self.temp64();
            self.w().local_get(a).i64_const(-1).i64_xor().local_get(b).i64_and();
            self.w().local_get(a).i64_const(-1).i64_xor().local_get(b).i64_or().local_get(result).i64_and().i64_or().local_set(borrows);
            self.bit_of(borrows, top);
            self.w().local_set(CF);
        }
        if self.want(RflagsBits::OF) {
            let overflow = self.temp64();
            self.w().local_get(a).local_get(b).i64_xor().local_get(a).local_get(result).i64_xor().i64_and().local_set(overflow);
            self.bit_of(overflow, top);
            self.w().local_set(OF);
        }
        self.flag_af(a, b, result);
        self.flags_result(result, size);
    }

    fn flag_af(&mut self, a: u32, b: u32, result: u32) {
        if self.want(RflagsBits::AF) {
            let mixed = self.temp64();
            self.w().local_get(a).local_get(b).i64_xor().local_get(result).i64_xor().local_set(mixed);
            self.bit_of(mixed, 4);
            self.w().local_set(AF);
        }
    }

    pub fn flags_logic(&mut self, result: u32, size: u32) {
        self.set_flag(RflagsBits::CF, CF, false);
        self.set_flag(RflagsBits::OF, OF, false);
        self.flags_result(result, size);
    }

    pub fn condition(&mut self, condition: ConditionCode) {
        let mut w = self.w();
        match condition {
            ConditionCode::o => w.local_get(OF),
            ConditionCode::no => w.local_get(OF).i32_eqz(),
            ConditionCode::b => w.local_get(CF),
            ConditionCode::ae => w.local_get(CF).i32_eqz(),
            ConditionCode::e => w.local_get(ZF),
            ConditionCode::ne => w.local_get(ZF).i32_eqz(),
            ConditionCode::be => w.local_get(CF).local_get(ZF).i32_or(),
            ConditionCode::a => w.local_get(CF).local_get(ZF).i32_or().i32_eqz(),
            ConditionCode::s => w.local_get(SF),
            ConditionCode::ns => w.local_get(SF).i32_eqz(),
            ConditionCode::p => w.local_get(PF),
            ConditionCode::np => w.local_get(PF).i32_eqz(),
            ConditionCode::l => w.local_get(SF).local_get(OF).i32_ne(),
            ConditionCode::ge => w.local_get(SF).local_get(OF).i32_eq(),
            ConditionCode::le => w.local_get(ZF).local_get(SF).local_get(OF).i32_ne().i32_or(),
            ConditionCode::g => w.local_get(ZF).i32_eqz().local_get(SF).local_get(OF).i32_eq().i32_and(),
            ConditionCode::None => unreachable!("instruction without a condition"),
        };
    }
}
