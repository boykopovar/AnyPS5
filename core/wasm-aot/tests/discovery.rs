mod common;

use common::*;
use iced_x86::BlockEncoderOptions;
use iced_x86::code_asm::*;

#[derive(Clone, Copy, Default)]
struct Shape {
    hoisted: bool,
    alias: bool,
    word_compare: bool,
    reversed: bool,
    unchecked: bool,
}

fn switch(hoisted: bool, alias: bool) -> Vec<u8> {
    switch_with(Shape { hoisted, alias, ..Shape::default() })
}

fn switch_with(shape: Shape) -> Vec<u8> {
    let Shape { hoisted, alias, word_compare, reversed, unchecked } = shape;
    let build = |offsets: &[u32]| -> (Vec<u8>, Vec<u64>, u64) {
        let mut a = CodeAssembler::new(64).unwrap();
        let mut table = a.create_label();
        let mut cases: Vec<CodeLabel> = (0..4).map(|_| a.create_label()).collect();
        let mut default = a.create_label();
        let mut done = a.create_label();
        let mut check = a.create_label();
        a.and(ecx, if unchecked { 3 } else { 7 }).unwrap();
        a.lea(r9, ptr(table)).unwrap();
        if hoisted {
            a.jmp(check).unwrap();
        }
        a.set_label(&mut check).unwrap();
        if !unchecked {
            if word_compare {
                a.cmp(cx, 3).unwrap();
            } else {
                a.cmp(cl, 3).unwrap();
            }
            a.ja(default).unwrap();
        }
        if alias {
            a.movzx(eax, cl).unwrap();
        } else {
            a.mov(eax, ecx).unwrap();
        }
        if reversed {
            a.movsxd(rax, dword_ptr(r9 + rax * 4)).unwrap();
            a.add(r9, rax).unwrap();
            a.jmp(r9).unwrap();
        } else {
            a.movsxd(rax, dword_ptr(r9 + rax * 4)).unwrap();
            a.add(rax, r9).unwrap();
            a.jmp(rax).unwrap();
        }
        for (index, case) in cases.iter_mut().enumerate() {
            a.set_label(case).unwrap();
            a.mov(edx, 10 + index as u32).unwrap();
            a.jmp(done).unwrap();
        }
        a.set_label(&mut default).unwrap();
        a.mov(edx, 99u32).unwrap();
        a.set_label(&mut done).unwrap();
        a.ret().unwrap();
        a.set_label(&mut table).unwrap();
        a.dd(offsets).unwrap();
        let result = a.assemble_options(CODE_BASE, BlockEncoderOptions::RETURN_NEW_INSTRUCTION_OFFSETS).unwrap();
        let case_ips = cases.iter().map(|case| result.label_ip(case).unwrap()).collect();
        (result.inner.code_buffer.clone(), case_ips, result.label_ip(&table).unwrap())
    };
    let (_, case_ips, table_ip) = build(&[0; 4]);
    let offsets: Vec<u32> = case_ips.iter().map(|ip| ip.wrapping_sub(table_ip) as u32).collect();
    build(&offsets).0
}

fn check_code(code: &[u8]) {
    let mask = defined_flags(code);
    for seed in 0..12 {
        assert_matches_oracle(code, &State::random(seed), mask);
    }
}

#[test]
fn jump_table_in_the_same_trace_is_recovered() {
    check_code(&switch(false, false));
}

#[test]
fn jump_table_with_a_hoisted_base_is_recovered() {
    check_code(&switch(true, false));
}

#[test]
fn jump_table_with_a_widened_index_is_recovered() {
    check_code(&switch(true, true));
}

#[test]
fn jump_table_with_a_word_bounds_check_is_recovered() {
    check_code(&switch_with(Shape { hoisted: true, alias: true, word_compare: true, ..Shape::default() }));
}

#[test]
fn jump_table_adding_into_the_base_is_recovered() {
    check_code(&switch_with(Shape { reversed: true, ..Shape::default() }));
}

#[test]
fn jump_table_without_a_bounds_check_is_recovered() {
    check_code(&switch_with(Shape { unchecked: true, hoisted: true, ..Shape::default() }));
}

#[test]
fn indirect_tail_call_reaches_a_code_pointer() {
    check(|a| {
        let mut target = a.create_label();
        let mut over = a.create_label();
        a.jmp(over)?;
        a.set_label(&mut target)?;
        a.mov(edx, 7u32)?;
        a.ret()?;
        a.set_label(&mut over)?;
        a.lea(rax, ptr(target))?;
        a.jmp(rax)
    });
}
