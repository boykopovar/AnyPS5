mod common;

use common::*;
use iced_x86::code_asm::*;

fn integers(a: &mut CodeAssembler) -> Result<(), iced_x86::IcedError> {
    a.mov(rbx, DATA_BASE)?;
    for (index, value) in [7i64, -3, 12, 5, 1024, -9].iter().enumerate() {
        a.mov(rax, *value)?;
        a.mov(qword_ptr(rbx + 0x100 + 8 * index as i32), rax)?;
    }
    Ok(())
}

#[test]
fn loads_stores_and_conversions_match() {
    check_native(|a| {
        integers(a)?;
        a.fild(qword_ptr(rbx + 0x100))?;
        a.fild(qword_ptr(rbx + 0x108))?;
        a.fstp(qword_ptr(rbx + 0x200))?;
        a.fstp(tword_ptr(rbx + 0x210))?;
        a.fld(tword_ptr(rbx + 0x210))?;
        a.fstp(dword_ptr(rbx + 0x220))?;
        a.fld(dword_ptr(rbx + 0x220))?;
        a.fistp(qword_ptr(rbx + 0x228))
    });
    check_native(|a| {
        integers(a)?;
        a.mov(dword_ptr(rbx + 0x300), 0x3fc0_0000u32)?;
        a.fld(dword_ptr(rbx + 0x300))?;
        a.fld(qword_ptr(rbx + 0x10))?;
        a.fstp(tword_ptr(rbx + 0x310))?;
        a.fst(qword_ptr(rbx + 0x320))?;
        a.fstp(dword_ptr(rbx + 0x328))
    });
    check_native(|a| {
        integers(a)?;
        a.fild(dword_ptr(rbx + 0x108))?;
        a.fild(word_ptr(rbx + 0x110))?;
        a.fisttp(dword_ptr(rbx + 0x200))?;
        a.fisttp(qword_ptr(rbx + 0x208))
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.fldz()?;
        a.fld1()?;
        a.fstp(qword_ptr(rbx + 0x200))?;
        a.fstp(qword_ptr(rbx + 0x208))
    });
}

#[test]
fn arithmetic_matches() {
    check_native(|a| {
        integers(a)?;
        a.fild(qword_ptr(rbx + 0x100))?;
        a.fild(qword_ptr(rbx + 0x108))?;
        a.fadd_2(st0, st1)?;
        a.fild(qword_ptr(rbx + 0x110))?;
        a.fmulp(st1, st0)?;
        a.fild(qword_ptr(rbx + 0x118))?;
        a.fsubp(st1, st0)?;
        a.fild(qword_ptr(rbx + 0x120))?;
        a.fdivrp(st1, st0)?;
        a.fstp(qword_ptr(rbx + 0x200))?;
        a.fstp(qword_ptr(rbx + 0x208))
    });
    check_native(|a| {
        integers(a)?;
        a.fild(qword_ptr(rbx + 0x110))?;
        a.fild(qword_ptr(rbx + 0x118))?;
        a.fsub_2(st1, st0)?;
        a.fsubr_2(st0, st1)?;
        a.fdiv_2(st1, st0)?;
        a.fdivr_2(st0, st1)?;
        a.fmul_2(st1, st0)?;
        a.fsubrp(st1, st0)?;
        a.fchs()?;
        a.fstp(qword_ptr(rbx + 0x200))
    });
    check_native(|a| {
        integers(a)?;
        a.mov(rax, 0x4010_0000_0000_0000u64)?;
        a.mov(qword_ptr(rbx + 0x180), rax)?;
        a.mov(dword_ptr(rbx + 0x188), 0x4080_0000u32)?;
        a.fild(qword_ptr(rbx + 0x100))?;
        a.fadd(qword_ptr(rbx + 0x180))?;
        a.fmul(dword_ptr(rbx + 0x188))?;
        a.fsub(qword_ptr(rbx + 0x180))?;
        a.fdivr(qword_ptr(rbx + 0x180))?;
        a.fdiv(dword_ptr(rbx + 0x188))?;
        a.fsubr(qword_ptr(rbx + 0x180))?;
        a.fiadd(word_ptr(rbx + 0x110))?;
        a.fabs()?;
        a.fsqrt()?;
        a.fstp(qword_ptr(rbx + 0x200))
    });
    check_native(|a| {
        integers(a)?;
        a.fild(qword_ptr(rbx + 0x108))?;
        a.fild(qword_ptr(rbx + 0x120))?;
        a.fscale()?;
        a.fstp(qword_ptr(rbx + 0x200))?;
        a.fstp(qword_ptr(rbx + 0x208))
    });
}

#[test]
fn stack_and_moves_match() {
    check_native(|a| {
        integers(a)?;
        a.fild(qword_ptr(rbx + 0x100))?;
        a.fild(qword_ptr(rbx + 0x108))?;
        a.fild(qword_ptr(rbx + 0x110))?;
        a.fxch(st0, st2)?;
        a.fld(st1)?;
        a.fstp(st3)?;
        a.fstp(qword_ptr(rbx + 0x200))?;
        a.fstp(qword_ptr(rbx + 0x208))?;
        a.fstp(qword_ptr(rbx + 0x210))
    });
    for condition in 0..6 {
        check_native(|a| {
            integers(a)?;
            a.fild(qword_ptr(rbx + 0x100))?;
            a.fild(qword_ptr(rbx + 0x108))?;
            a.cmp(rcx, rdx)?;
            match condition {
                0 => a.fcmovne(st0, st1)?,
                1 => a.fcmovnbe(st0, st1)?,
                2 => a.fcmovu(st0, st1)?,
                3 => a.fcmovnb(st0, st1)?,
                4 => a.fcmove(st0, st1)?,
                _ => a.fcmovb(st0, st1)?,
            }
            a.fstp(qword_ptr(rbx + 0x200))?;
            a.fstp(qword_ptr(rbx + 0x208))
        });
    }
}

#[test]
fn comparisons_match() {
    for (first, second) in [(0x100, 0x108), (0x108, 0x100), (0x110, 0x110)] {
        check_native(|a| {
            integers(a)?;
            a.fild(qword_ptr(rbx + first))?;
            a.fild(qword_ptr(rbx + second))?;
            a.fucomi(st0, st1)?;
            a.seta(al)?;
            a.setb(cl)?;
            a.sete(dl)?;
            a.fucomip(st0, st1)?;
            a.setp(sil)?;
            a.fstp(qword_ptr(rbx + 0x200))
        });
    }
    check_native(|a| {
        integers(a)?;
        a.fild(qword_ptr(rbx + 0x100))?;
        a.fnstsw(ax)?;
        a.fild(qword_ptr(rbx + 0x108))?;
        a.fnstsw(word_ptr(rbx + 0x200))?;
        a.fstp(st0)?;
        a.fstp(st0)
    });
}

#[test]
fn control_and_environment_match() {
    check_native(|a| {
        integers(a)?;
        a.fnstcw(word_ptr(rbx + 0x200))?;
        a.mov(ax, word_ptr(rbx + 0x200))?;
        a.or(ax, 0x0c00u32)?;
        a.mov(word_ptr(rbx + 0x202), ax)?;
        a.fldcw(word_ptr(rbx + 0x202))?;
        a.mov(rax, 0x400c_0000_0000_0000u64)?;
        a.mov(qword_ptr(rbx + 0x180), rax)?;
        a.fld(qword_ptr(rbx + 0x180))?;
        a.fistp(qword_ptr(rbx + 0x210))?;
        a.fld(qword_ptr(rbx + 0x180))?;
        a.frndint()?;
        a.fstp(qword_ptr(rbx + 0x218))?;
        a.fldcw(word_ptr(rbx + 0x200))?;
        a.fld(qword_ptr(rbx + 0x180))?;
        a.fistp(qword_ptr(rbx + 0x220))?;
        a.wait()?;
        a.fnclex()
    });
    check_native(|a| {
        integers(a)?;
        a.fnstenv(ptr(rbx + 0x200))?;
        a.fldenv(ptr(rbx + 0x200))?;
        a.mov(eax, dword_ptr(rbx + 0x200))?;
        a.mov(ecx, dword_ptr(rbx + 0x204))?;
        a.mov(edx, dword_ptr(rbx + 0x208))?;
        a.mov(qword_ptr(rbx + 0x20c), 0i32)?;
        a.mov(qword_ptr(rbx + 0x214), 0i32)?;
        a.mov(dword_ptr(rbx + 0x218), 0u32)
    });
}

#[test]
fn environment_tags_classify_zero_and_special_values() {
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.mov(rax, 0x7ff0_0000_0000_0000u64)?;
        a.mov(qword_ptr(rbx + 0x180), rax)?;
        a.fld1()?;
        a.fldz()?;
        a.fld(qword_ptr(rbx + 0x180))?;
        a.fnstenv(ptr(rbx + 0x200))?;
        a.mov(qword_ptr(rbx + 0x20c), 0i32)?;
        a.mov(qword_ptr(rbx + 0x214), 0i32)?;
        a.mov(dword_ptr(rbx + 0x218), 0u32)?;
        a.fninit()
    });
}

#[test]
fn byte_swapping_loads_match() {
    check(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.movbe(rcx, qword_ptr(rbx + 0x20))?;
        a.movbe(edx, dword_ptr(rbx + 0x30))?;
        a.movbe(word_ptr(rbx + 0x40), dx)?;
        a.movbe(qword_ptr(rbx + 0x48), rax)
    });
}
