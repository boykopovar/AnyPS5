mod common;

use common::*;
use iced_x86::code_asm::*;

macro_rules! binary_ops {
    ($($name:ident => $op:ident),* $(,)?) => {$(
        #[test]
        fn $name() {
            check(|a| a.$op(rax, rbx));
            check(|a| a.$op(ecx, edx));
            check(|a| a.$op(si, di));
            check(|a| a.$op(r8b, r9b));
            check(|a| a.$op(ah, bl));
            check(|a| a.$op(bh, ch));
            check(|a| a.$op(rax, 0x7fi32));
            check(|a| a.$op(rax, -0x1234_5678i32));
            check(|a| a.$op(r10d, 0x8000_0001u32));
            check(|a| a.$op(dx, 0x8001u32));
            check(|a| a.$op(cl, 0x80u32));
            check(|a| { a.mov(rbx, DATA_BASE)?; a.$op(qword_ptr(rbx + 8), rdx) });
            check(|a| { a.mov(rbx, DATA_BASE)?; a.$op(edx, dword_ptr(rbx + 0x20)) });
            check(|a| { a.mov(rbx, DATA_BASE)?; a.mov(ecx, 3u32)?; a.$op(word_ptr(rbx + rcx * 4 + 2), 0x1234u32) });
            check(|a| { a.mov(rbx, DATA_BASE)?; a.$op(byte_ptr(rbx + 0x31), 0x7fu32) });
        }
    )*};
}

binary_ops! {
    add_matches => add,
    sub_matches => sub,
    adc_matches => adc,
    sbb_matches => sbb,
    and_matches => and,
    or_matches => or,
    xor_matches => xor,
    cmp_matches => cmp,
}

#[test]
fn test_matches() {
    check(|a| a.test(rax, rbx));
    check(|a| a.test(ecx, edx));
    check(|a| a.test(al, 0x81u32));
    check(|a| a.test(r11, -2i32));
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.test(dword_ptr(rbx + 4), ecx)
    });
}

#[test]
fn unary_ops_match() {
    for size in 0..4 {
        check(|a| match size {
            0 => a.inc(rax),
            1 => a.inc(ecx),
            2 => a.inc(dx),
            _ => a.inc(bh),
        });
        check(|a| match size {
            0 => a.dec(rax),
            1 => a.dec(ecx),
            2 => a.dec(dx),
            _ => a.dec(bl),
        });
        check(|a| match size {
            0 => a.neg(rax),
            1 => a.neg(ecx),
            2 => a.neg(dx),
            _ => a.neg(ah),
        });
        check(|a| match size {
            0 => a.not(rax),
            1 => a.not(ecx),
            2 => a.not(dx),
            _ => a.not(r9b),
        });
    }
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.inc(qword_ptr(rbx + 16))
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.neg(word_ptr(rbx + 16))
    });
    check(|a| {
        a.xor(eax, eax)?;
        a.neg(rax)
    });
    check(|a| {
        a.mov(rax, 0x8000_0000_0000_0000u64)?;
        a.neg(rax)
    });
    check(|a| {
        a.mov(eax, 0x7fff_ffffu32)?;
        a.inc(eax)
    });
    check(|a| {
        a.mov(eax, 0x8000_0000u32)?;
        a.dec(eax)
    });
}

#[test]
fn moves_and_extensions_match() {
    check(|a| a.mov(r12, r13));
    check(|a| a.mov(eax, ebx));
    check(|a| a.mov(cx, dx));
    check(|a| a.mov(ah, dl));
    check(|a| a.mov(sil, 0x55u32));
    check(|a| a.mov(rdx, 0x1122_3344_5566_7788u64));
    check(|a| a.mov(rdx, -5i64));
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.mov(rcx, qword_ptr(rbx + 0x40))?;
        a.mov(dword_ptr(rbx + 0x48), ecx)?;
        a.mov(byte_ptr(rbx + 0x50), ah)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.mov(qword_ptr(rbx + 0x58), -1i32)
    });
    check(|a| a.movzx(eax, bl));
    check(|a| a.movzx(eax, bh));
    check(|a| a.movzx(ecx, dx));
    check(|a| a.movsx(rax, bl));
    check(|a| a.movsx(eax, dx));
    check(|a| a.movsx(cx, ah));
    check(|a| a.movsxd(rax, ebx));
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.movzx(eax, word_ptr(rbx + 6))?;
        a.movsx(rcx, byte_ptr(rbx + 7))?;
        a.movsxd(rdx, dword_ptr(rbx + 8))
    });
    check(|a| a.lea(rax, qword_ptr(rbx + rcx * 8 + 0x40)));
    check(|a| a.lea(eax, dword_ptr(rbx + rcx * 2 - 3)));
    check(|a| a.lea(rsi, qword_ptr(rdi + 1)));
    check(|a| a.xchg(rax, rbx));
    check(|a| a.xchg(ecx, edx));
    check(|a| a.xchg(al, ah));
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.xchg(qword_ptr(rbx + 8), rcx)
    });
}

#[test]
fn sign_conversions_match() {
    check(|a| a.cbw());
    check(|a| a.cwde());
    check(|a| a.cdqe());
    check(|a| a.cwd());
    check(|a| a.cdq());
    check(|a| a.cqo());
}

#[test]
fn conditions_match() {
    for condition in 0..16 {
        check(|a| {
            a.cmp(rax, rbx)?;
            match condition {
                0 => a.seto(cl),
                1 => a.setno(cl),
                2 => a.setb(cl),
                3 => a.setae(cl),
                4 => a.sete(cl),
                5 => a.setne(cl),
                6 => a.setbe(cl),
                7 => a.seta(cl),
                8 => a.sets(cl),
                9 => a.setns(cl),
                10 => a.setp(cl),
                11 => a.setnp(cl),
                12 => a.setl(cl),
                13 => a.setge(cl),
                14 => a.setle(cl),
                _ => a.setg(cl),
            }
        });
        check(|a| {
            a.test(eax, ebx)?;
            match condition {
                0 => a.cmovo(rcx, rdx),
                1 => a.cmovno(ecx, edx),
                2 => a.cmovb(rcx, rdx),
                3 => a.cmovae(ecx, edx),
                4 => a.cmove(rcx, rdx),
                5 => a.cmovne(ecx, edx),
                6 => a.cmovbe(rcx, rdx),
                7 => a.cmova(ecx, edx),
                8 => a.cmovs(rcx, rdx),
                9 => a.cmovns(ecx, edx),
                10 => a.cmovp(rcx, rdx),
                11 => a.cmovnp(ecx, edx),
                12 => a.cmovl(rcx, rdx),
                13 => a.cmovge(ecx, edx),
                14 => a.cmovle(rcx, rdx),
                _ => a.cmovg(ecx, edx),
            }
        });
    }
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.cmp(eax, ecx)?;
        a.cmovl(rdx, qword_ptr(rbx + 8))
    });
}

#[test]
fn branches_match() {
    check(|a| {
        let mut skip = a.create_label();
        a.cmp(rax, rbx)?;
        a.jl(skip)?;
        a.mov(ecx, 1u32)?;
        a.set_label(&mut skip)?;
        a.add(rcx, 2i32)
    });
    check(|a| {
        let mut top = a.create_label();
        a.mov(ecx, 10u32)?;
        a.xor(eax, eax)?;
        a.set_label(&mut top)?;
        a.add(rax, rcx)?;
        a.dec(ecx)?;
        a.jnz(top)
    });
    check(|a| {
        let mut done = a.create_label();
        let mut other = a.create_label();
        a.test(al, 1u32)?;
        a.je(other)?;
        a.mov(edx, 7u32)?;
        a.jmp(done)?;
        a.set_label(&mut other)?;
        a.mov(edx, 9u32)?;
        a.set_label(&mut done)?;
        a.nop()
    });
}

#[test]
fn stack_operations_match() {
    check(|a| {
        a.push(rax)?;
        a.push(rbx)?;
        a.pop(rcx)?;
        a.pop(rdx)
    });
    check(|a| {
        a.push(-5i32)?;
        a.pop(r8)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.push(qword_ptr(rbx + 8))?;
        a.pop(qword_ptr(rbx + 16))
    });
    check(|a| {
        a.push(rbp)?;
        a.mov(rbp, rsp)?;
        a.sub(rsp, 0x20i32)?;
        a.mov(qword_ptr(rbp - 8), rax)?;
        a.leave()
    });
}

#[test]
fn multiplication_matches() {
    check(|a| a.imul_2(rax, rbx));
    check(|a| a.imul_2(ecx, edx));
    check(|a| a.imul_2(si, di));
    check(|a| a.imul_3(rax, rbx, 0x1234i32));
    check(|a| a.imul_3(ecx, edx, -7i32));
    check(|a| a.imul(rbx));
    check(|a| a.imul(ecx));
    check(|a| a.imul(dx));
    check(|a| a.imul(bl));
    check(|a| a.mul(rbx));
    check(|a| a.mul(ecx));
    check(|a| a.mul(dx));
    check(|a| a.mul(bl));
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.imul_2(rax, qword_ptr(rbx + 8))?;
        a.mul(dword_ptr(rbx + 16))
    });
}

#[test]
fn division_matches() {
    check(|a| {
        a.xor(edx, edx)?;
        a.or(rbx, 1i32)?;
        a.div(rbx)
    });
    check(|a| {
        a.xor(edx, edx)?;
        a.or(ecx, 1u32)?;
        a.div(ecx)
    });
    check(|a| {
        a.cqo()?;
        a.mov(rcx, 0x1234_5678_9abci64)?;
        a.idiv(rcx)
    });
    check(|a| {
        a.cdq()?;
        a.mov(ecx, -0x3456_789ai32)?;
        a.idiv(ecx)
    });
    check(|a| {
        a.cwd()?;
        a.mov(cx, 0x7001u32)?;
        a.idiv(cx)
    });
    check(|a| {
        a.movzx(eax, al)?;
        a.mov(cl, 0xfdu32)?;
        a.div(cl)
    });
    check(|a| {
        a.movsx(ax, al)?;
        a.mov(cl, 0x7fu32)?;
        a.idiv(cl)
    });
    check(|a| {
        a.mov(edx, 0x1234u32)?;
        a.mov(rcx, 0x8000_0000_0000_0001u64)?;
        a.div(rcx)
    });
}

#[test]
fn shifts_match() {
    for count in [0u32, 1, 3, 31, 32, 63] {
        check(|a| a.shl(rax, count));
        check(|a| a.shr(ecx, count));
        check(|a| a.sar(dx, count));
        check(|a| a.rol(rbx, count));
        check(|a| a.ror(esi, count));
        check(|a| a.sar(bh, count));
    }
    check(|a| a.shl(rax, cl));
    check(|a| a.shr(edx, cl));
    check(|a| a.sar(r9, cl));
    check(|a| a.rol(ax, cl));
    check(|a| a.ror(bl, cl));
    check(|a| {
        a.and(ecx, 0x3fu32)?;
        a.shl(rdx, cl)
    });
    check(|a| {
        a.xor(ecx, ecx)?;
        a.shr(rdx, cl)
    });
    check(|a| a.rcl(rax, 1u32));
    check(|a| a.rcr(ebx, 1u32));
    check(|a| a.shld(rax, rbx, 7u32));
    check(|a| a.shrd(eax, ebx, 9u32));
    check(|a| a.shld(rax, rbx, cl));
    check(|a| a.shrd(dx, si, 3u32));
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.shl(qword_ptr(rbx + 8), 5u32)?;
        a.sar(dword_ptr(rbx + 16), cl)
    });
}

#[test]
fn bit_operations_match() {
    check(|a| a.bt(rax, rbx));
    check(|a| a.bt(ecx, 17u32));
    check(|a| a.bts(rax, 63u32));
    check(|a| a.btr(edx, ecx));
    check(|a| a.btc(si, 3u32));
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.and(ecx, 0x3ffu32)?;
        a.bts(qword_ptr(rbx + 0x80), rcx)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.btr(dword_ptr(rbx + 8), 5u32)
    });
    check(|a| {
        a.or(rbx, 0x100i32)?;
        a.bsf(rax, rbx)
    });
    check(|a| {
        a.or(ebx, 0x100u32)?;
        a.bsr(eax, ebx)
    });
    check(|a| a.tzcnt(rax, rbx));
    check(|a| a.lzcnt(ecx, edx));
    check(|a| a.popcnt(rax, rbx));
    check(|a| {
        a.xor(ebx, ebx)?;
        a.tzcnt(rax, rbx)?;
        a.lzcnt(rcx, rbx)
    });
    check(|a| a.bswap(rax));
    check(|a| a.bswap(ecx));
}

#[test]
fn bmi_matches() {
    check(|a| a.andn(rax, rbx, rcx));
    check(|a| a.andn(eax, ebx, ecx));
    check(|a| {
        a.mov(ecx, 0x0c04u32)?;
        a.bextr(rax, rbx, rcx)
    });
    check_except(1, |a| a.blsi(rax, rbx));
    check(|a| a.blsr(eax, ebx));
    check(|a| a.blsmsk(rax, rbx));
    check(|a| {
        a.mov(ecx, 13u32)?;
        a.bzhi(rax, rbx, rcx)
    });
    check(|a| a.sarx(rax, rbx, rcx));
    check(|a| a.shlx(eax, ebx, ecx));
    check(|a| a.shrx(rax, rbx, rcx));
    check(|a| a.rorx(rax, rbx, 13u32));
    check(|a| a.mulx(r8, r9, rbx));
    check(|a| a.pdep(rax, rbx, rcx));
    check(|a| a.pext(eax, ebx, ecx));
}

#[test]
fn string_operations_match() {
    check(|a| {
        a.mov(rsi, DATA_BASE)?;
        a.lea(rdi, qword_ptr(rsi + 0x200))?;
        a.mov(ecx, 37u32)?;
        a.rep().movsb()
    });
    check(|a| {
        a.mov(rsi, DATA_BASE)?;
        a.lea(rdi, qword_ptr(rsi + 0x300))?;
        a.mov(ecx, 9u32)?;
        a.rep().movsq()
    });
    check(|a| {
        a.mov(rdi, DATA_BASE)?;
        a.mov(ecx, 41u32)?;
        a.rep().stosb()
    });
    check(|a| {
        a.mov(rdi, DATA_BASE)?;
        a.mov(ecx, 5u32)?;
        a.rep().stosd()
    });
    check(|a| {
        a.mov(rsi, DATA_BASE)?;
        a.lodsq()?;
        a.lodsb()
    });
    check(|a| {
        a.mov(rdi, DATA_BASE)?;
        a.stosq()?;
        a.stosw()
    });
    check(|a| {
        a.mov(rsi, DATA_BASE)?;
        a.lea(rdi, qword_ptr(rsi + 0x10))?;
        a.movsd()
    });
    check(|a| {
        a.mov(rdi, DATA_BASE)?;
        a.mov(ecx, 64u32)?;
        a.mov(byte_ptr(rdi + 20), al)?;
        a.repne().scasb()
    });
    check(|a| {
        a.mov(rsi, DATA_BASE)?;
        a.lea(rdi, qword_ptr(rsi + 0x100))?;
        a.mov(ecx, 8u32)?;
        a.repe().cmpsb()
    });
}

#[test]
fn atomics_match() {
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.lock().add(qword_ptr(rbx + 8), rcx)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.lock().xadd(dword_ptr(rbx + 16), ecx)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.lock().inc(qword_ptr(rbx + 24))
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.mov(rax, qword_ptr(rbx + 32))?;
        a.lock().cmpxchg(qword_ptr(rbx + 32), rcx)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.lock().cmpxchg(dword_ptr(rbx + 40), ecx)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.lock().or(dword_ptr(rbx + 44), edx)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.lock().bts(qword_ptr(rbx + 48), 9u32)
    });
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.cmpxchg(word_ptr(rbx + 52), dx)
    });
    check(|a| a.cmpxchg(rcx, rdx));
    check(|a| a.xadd(rcx, rdx));
    check(|a| {
        a.mfence()?;
        a.lfence()?;
        a.sfence()?;
        a.pause()
    });
}

#[test]
fn repeated_compares_with_a_zero_count_keep_the_flags() {
    const ADJUST: u64 = 0x10;
    check_except(ADJUST, |a| {
        a.mov(rsi, DATA_BASE)?;
        a.lea(rdi, qword_ptr(rsi + 0x100))?;
        a.xor(ecx, ecx)?;
        a.repe().cmpsb()?;
        a.setne(al)
    });
    check_except(ADJUST, |a| {
        a.mov(rdi, DATA_BASE)?;
        a.mov(ecx, 7u32)?;
        a.shr(rcx, 3)?;
        a.repne().scasq()?;
        a.sete(al)
    });
}
