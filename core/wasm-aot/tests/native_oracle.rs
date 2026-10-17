mod common;

use common::*;
use iced_x86::code_asm::*;

#[test]
fn native_oracle_agrees_on_integer_code() {
    check_native(|a| a.add(rax, rbx));
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.sub(qword_ptr(rbx + 8), rcx)?;
        a.push(rdx)?;
        a.pop(rsi)
    });
    check_native(|a| {
        a.cmp(eax, ebx)?;
        a.setl(cl)?;
        a.cmovg(rdx, r8)
    });
}

#[test]
fn blsi_sets_carry_for_a_nonzero_source() {
    check_native(|a| a.blsi(rax, rbx));
}
