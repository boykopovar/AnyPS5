mod common;

use common::*;
use iced_x86::code_asm::*;

#[test]
fn vector_moves_match() {
    check_native(|a| a.vmovups(ymm0, ymm5));
    check_native(|a| a.vmovaps(xmm1, xmm9));
    check_native(|a| a.movaps(xmm1, xmm9));
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovups(ymm1, ymmword_ptr(rbx + 0x24))?;
        a.vmovups(ymmword_ptr(rbx + 0x101), ymm2)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovdqu(xmm3, xmmword_ptr(rbx + 3))?;
        a.vmovdqa(xmmword_ptr(rbx + 0x40), xmm4)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.movups(xmm3, xmmword_ptr(rbx + 3))?;
        a.movdqu(xmmword_ptr(rbx + 0x50), xmm7)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovupd(xmm3, xmmword_ptr(rbx))?;
        a.vmovapd(ymm4, ymm6)?;
        a.vmovntdq(ymmword_ptr(rbx + 0x60), ymm4)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovntdqa(ymm1, ymmword_ptr(rbx + 0x80))
    });
    check_native(|a| {
        a.vmovd(xmm0, ecx)?;
        a.vmovq(xmm1, rdx)?;
        a.vmovd(esi, xmm2)?;
        a.vmovq(rdi, xmm3)
    });
    check_native(|a| {
        a.movd(xmm0, ecx)?;
        a.movq(xmm1, rdx)?;
        a.movd(esi, xmm2)?;
        a.movq(rdi, xmm3)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovd(xmm0, dword_ptr(rbx + 4))?;
        a.vmovq(xmm1, qword_ptr(rbx + 8))?;
        a.vmovq(qword_ptr(rbx + 0x10), xmm2)?;
        a.vmovq(xmm4, xmm5)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovss(xmm0, dword_ptr(rbx + 4))?;
        a.vmovsd(xmm1, qword_ptr(rbx + 8))?;
        a.vmovss(dword_ptr(rbx + 0x20), xmm2)?;
        a.vmovsd(qword_ptr(rbx + 0x28), xmm3)
    });
    check_native(|a| {
        a.vmovss_3(xmm0, xmm1, xmm2)?;
        a.vmovsd_3(xmm3, xmm4, xmm5)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.movss(xmm0, dword_ptr(rbx + 4))?;
        a.movsd_2(xmm1, qword_ptr(rbx + 8))?;
        a.movss(xmm2, xmm3)?;
        a.movsd_2(xmm4, xmm5)?;
        a.movsd_2(qword_ptr(rbx + 0x30), xmm6)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovhpd_3(xmm0, xmm1, qword_ptr(rbx + 8))?;
        a.vmovlpd_3(xmm2, xmm3, qword_ptr(rbx + 16))?;
        a.vmovhpd(qword_ptr(rbx + 0x40), xmm4)?;
        a.vmovlps(qword_ptr(rbx + 0x48), xmm5)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovhps_3(xmm0, xmm1, qword_ptr(rbx + 8))?;
        a.vmovlps_3(xmm2, xmm3, qword_ptr(rbx + 16))?;
        a.vmovlhps(xmm4, xmm5, xmm6)?;
        a.vmovhlps(xmm7, xmm8, xmm9)
    });
    check_native(|a| {
        a.vmovddup(ymm0, ymm1)?;
        a.vmovshdup(xmm2, xmm3)?;
        a.vmovsldup(ymm4, ymm5)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmovddup(xmm0, qword_ptr(rbx + 8))
    });
    check_native(|a| a.vzeroupper());
}

#[test]
fn broadcasts_and_lane_moves_match() {
    check_native(|a| {
        a.vpbroadcastq(ymm0, xmm1)?;
        a.vpbroadcastd(xmm2, xmm3)?;
        a.vpbroadcastb(ymm4, xmm5)?;
        a.vpbroadcastw(xmm6, xmm7)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vpbroadcastq(ymm0, qword_ptr(rbx + 8))?;
        a.vbroadcastss(ymm1, dword_ptr(rbx + 4))?;
        a.vbroadcastsd(ymm2, qword_ptr(rbx + 16))?;
        a.vbroadcastf128(ymm3, xmmword_ptr(rbx + 32))
    });
    check_native(|a| {
        a.vbroadcastss(xmm1, xmm4)?;
        a.vbroadcastsd(ymm2, xmm5)
    });
    check_native(|a| {
        a.vextracti128(xmm0, ymm1, 1u32)?;
        a.vextractf128(xmm2, ymm3, 0u32)?;
        a.vinserti128(ymm4, ymm5, xmm6, 1u32)?;
        a.vinsertf128(ymm7, ymm8, xmm9, 0u32)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vextractf128(xmmword_ptr(rbx + 0x30), ymm3, 1u32)?;
        a.vinsertf128(ymm1, ymm2, xmmword_ptr(rbx + 8), 1u32)
    });
    check_native(|a| {
        a.vpextrd(eax, xmm1, 3u32)?;
        a.vpextrq(rcx, xmm2, 1u32)?;
        a.vpextrb(edx, xmm3, 13u32)?;
        a.vpextrw(esi, xmm4, 5u32)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vpextrd(dword_ptr(rbx + 4), xmm1, 2u32)?;
        a.vpextrb(byte_ptr(rbx + 9), xmm2, 7u32)
    });
    check_native(|a| {
        a.vpinsrd(xmm0, xmm1, eax, 2u32)?;
        a.vpinsrq(xmm2, xmm3, rcx, 1u32)?;
        a.vpinsrb(xmm4, xmm5, edx, 11u32)?;
        a.vpinsrw(xmm6, xmm7, esi, 6u32)
    });
    check_native(|a| {
        a.pextrd(eax, xmm1, 3u32)?;
        a.pinsrd(xmm2, ecx, 1u32)?;
        a.pinsrq(xmm3, rdx, 0u32)
    });
    check_native(|a| {
        a.vextractps(eax, xmm1, 2u32)?;
        a.vinsertps(xmm2, xmm3, xmm4, 0x9au32)?;
        a.vinsertps(xmm5, xmm6, xmm7, 0x30u32)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vinsertps(xmm2, xmm3, dword_ptr(rbx + 12), 0x21u32)
    });
    check_native(|a| {
        a.vmovmskps(eax, ymm1)?;
        a.vmovmskpd(ecx, xmm2)?;
        a.vpmovmskb(edx, ymm3)?;
        a.movmskps(esi, xmm4)
    });
}

#[test]
fn shuffles_match() {
    check_native(|a| {
        a.vpshufd(ymm0, ymm1, 0x1bu32)?;
        a.vpshufhw(xmm2, xmm3, 0x4eu32)?;
        a.vpshuflw(ymm4, ymm5, 0xb1u32)
    });
    check_native(|a| {
        a.vpshufb(ymm0, ymm1, ymm2)?;
        a.pshufb(xmm3, xmm4)
    });
    check_native(|a| {
        a.vshufps(ymm0, ymm1, ymm2, 0x93u32)?;
        a.vshufpd(ymm3, ymm4, ymm5, 0x6u32)?;
        a.shufps(xmm6, xmm7, 0x27u32)
    });
    check_native(|a| {
        a.vunpcklpd(ymm0, ymm1, ymm2)?;
        a.vunpckhpd(xmm3, xmm4, xmm5)?;
        a.vunpcklps(ymm6, ymm7, ymm8)?;
        a.vunpckhps(xmm9, xmm10, xmm11)
    });
    check_native(|a| {
        a.vpunpcklbw(ymm0, ymm1, ymm2)?;
        a.vpunpckhbw(xmm3, xmm4, xmm5)?;
        a.vpunpcklwd(ymm6, ymm7, ymm8)?;
        a.vpunpckhwd(xmm9, xmm10, xmm11)
    });
    check_native(|a| {
        a.vpunpckldq(ymm0, ymm1, ymm2)?;
        a.vpunpckhdq(xmm3, xmm4, xmm5)?;
        a.vpunpcklqdq(ymm6, ymm7, ymm8)?;
        a.vpunpckhqdq(xmm9, xmm10, xmm11)
    });
    check_native(|a| {
        a.vpermilps(ymm0, ymm1, 0x39u32)?;
        a.vpermilpd(ymm2, ymm3, 0x5u32)?;
        a.vpermilps(xmm4, xmm5, xmm6)?;
        a.vpermilpd(ymm7, ymm8, ymm9)
    });
    check_native(|a| {
        a.vpermq(ymm0, ymm1, 0x4eu32)?;
        a.vpermpd(ymm2, ymm3, 0x93u32)?;
        a.vpermd(ymm4, ymm5, ymm6)?;
        a.vpermps(ymm7, ymm8, ymm9)
    });
    check_native(|a| {
        a.vperm2f128(ymm0, ymm1, ymm2, 0x21u32)?;
        a.vperm2i128(ymm3, ymm4, ymm5, 0x83u32)?;
        a.vperm2f128(ymm6, ymm7, ymm8, 0x30u32)
    });
    check_native(|a| {
        a.vblendps(ymm0, ymm1, ymm2, 0xa5u32)?;
        a.vblendpd(ymm3, ymm4, ymm5, 0x6u32)?;
        a.vpblendd(ymm6, ymm7, ymm8, 0x3cu32)?;
        a.vpblendw(ymm9, ymm10, ymm11, 0x96u32)
    });
    check_native(|a| {
        a.vblendvps(ymm0, ymm1, ymm2, ymm3)?;
        a.vblendvpd(xmm4, xmm5, xmm6, xmm7)?;
        a.vpblendvb(ymm8, ymm9, ymm10, ymm11)
    });
    check_native(|a| {
        a.vpalignr(ymm0, ymm1, ymm2, 5u32)?;
        a.vpalignr(xmm3, xmm4, xmm5, 20u32)?;
        a.palignr(xmm6, xmm7, 9u32)
    });
    check_native(|a| {
        a.vpslldq(ymm0, ymm1, 3u32)?;
        a.vpsrldq(xmm2, xmm3, 7u32)?;
        a.psrldq(xmm4, 12u32)
    });
    check_native(|a| {
        a.vpmovzxbd(ymm0, xmm1)?;
        a.vpmovzxdq(ymm2, xmm3)?;
        a.vpmovzxwd(xmm4, xmm5)?;
        a.vpmovsxdq(ymm6, xmm7)?;
        a.vpmovsxbd(xmm8, xmm9)?;
        a.vpmovsxwd(ymm10, xmm11)?;
        a.vpmovsxbw(ymm12, xmm13)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vpmovzxbd(ymm0, qword_ptr(rbx + 8))?;
        a.vpmovsxdq(xmm2, qword_ptr(rbx + 16))
    });
}

#[test]
fn integer_vectors_match() {
    check_native(|a| {
        a.vpand(ymm0, ymm1, ymm2)?;
        a.vpor(xmm3, xmm4, xmm5)?;
        a.vpxor(ymm6, ymm7, ymm8)?;
        a.vpandn(ymm9, ymm10, ymm11)?;
        a.pxor(xmm12, xmm13)
    });
    check_native(|a| {
        a.vpaddd(ymm0, ymm1, ymm2)?;
        a.vpaddq(xmm3, xmm4, xmm5)?;
        a.vpaddb(ymm6, ymm7, ymm8)?;
        a.vpaddw(xmm9, xmm10, xmm11)
    });
    check_native(|a| {
        a.vpsubd(ymm0, ymm1, ymm2)?;
        a.vpsubq(xmm3, xmm4, xmm5)?;
        a.paddd(xmm6, xmm7)?;
        a.psubb(xmm8, xmm9)
    });
    check_native(|a| {
        a.vpcmpeqd(ymm0, ymm1, ymm2)?;
        a.vpcmpeqq(xmm3, xmm4, xmm4)?;
        a.vpcmpeqb(ymm5, ymm6, ymm7)?;
        a.vpcmpgtd(ymm8, ymm9, ymm10)?;
        a.vpcmpgtb(xmm11, xmm12, xmm13)?;
        a.vpcmpgtq(ymm14, ymm15, ymm0)
    });
    check_native(|a| {
        a.vpmaxud(ymm0, ymm1, ymm2)?;
        a.vpmaxub(xmm3, xmm4, xmm5)?;
        a.vpmaxsd(ymm6, ymm7, ymm8)?;
        a.vpminsd(xmm9, xmm10, xmm11)
    });
    check_native(|a| {
        a.vpmulld(ymm0, ymm1, ymm2)?;
        a.vpmuludq(ymm3, ymm4, ymm5)?;
        a.vpmullw(xmm6, xmm7, xmm8)?;
        a.pmuludq(xmm9, xmm10)
    });
    check_native(|a| {
        a.vpackssdw(ymm0, ymm1, ymm2)?;
        a.vpackusdw(xmm3, xmm4, xmm5)?;
        a.vpackuswb(ymm6, ymm7, ymm8)?;
        a.vpacksswb(xmm9, xmm10, xmm11)
    });
    for count in [0u32, 3, 15, 16, 31, 32, 63, 64] {
        check_native(|a| {
            a.vpslld(ymm0, ymm1, count)?;
            a.vpsrld(xmm2, xmm3, count)?;
            a.vpsrad(ymm4, ymm5, count)
        });
        check_native(|a| {
            a.vpsllq(ymm0, ymm1, count)?;
            a.vpsrlq(xmm2, xmm3, count)?;
            a.vpsllw(ymm4, ymm5, count)?;
            a.vpsrlw(xmm6, xmm7, count)?;
            a.vpsraw(xmm8, xmm9, count)
        });
    }
    check_native(|a| {
        a.mov(eax, 7u32)?;
        a.vmovd(xmm6, eax)?;
        a.vpslld(ymm0, ymm1, xmm6)?;
        a.vpsrld(ymm2, ymm3, xmm6)?;
        a.vpsrad(xmm4, xmm5, xmm6)
    });
    check_native(|a| {
        a.mov(eax, 40u32)?;
        a.vmovd(xmm6, eax)?;
        a.vpslld(ymm0, ymm1, xmm6)?;
        a.vpsrlq(ymm2, ymm3, xmm6)?;
        a.vpsrad(xmm4, xmm5, xmm6)
    });
    check_native(|a| {
        a.vpsllvd(ymm0, ymm1, ymm2)?;
        a.vpsrlvd(xmm3, xmm4, xmm5)?;
        a.vpsllvq(ymm6, ymm7, ymm8)?;
        a.vpsrlvq(xmm9, xmm10, xmm11)?;
        a.vpsravd(ymm12, ymm13, ymm14)
    });
    check_native(|a| {
        a.mov(eax, 0x1f)?;
        a.vmovd(xmm6, eax)?;
        a.vpbroadcastd(ymm6, xmm6)?;
        a.vpand(ymm2, ymm2, ymm6)?;
        a.vpsllvd(ymm0, ymm1, ymm2)?;
        a.vpsravd(ymm3, ymm1, ymm2)
    });
    let kept_by_rosetta = 0x800 | 0x80 | 0x10 | 0x4;
    check_native_except(kept_by_rosetta, |a| {
        a.vptest(ymm0, ymm1)?;
        a.setz(al)?;
        a.setc(cl)?;
        a.vptest(xmm2, xmm2)?;
        a.setz(dl)
    });
    check_native_except(kept_by_rosetta, |a| {
        a.vpxor(xmm0, xmm0, xmm0)?;
        a.vptest(ymm0, ymm1)?;
        a.setz(al)?;
        a.setc(cl)
    });
}

#[test]
fn float_arithmetic_matches() {
    check_f32(|a| {
        a.vaddps(ymm0, ymm1, ymm2)?;
        a.vsubps(xmm3, xmm4, xmm5)?;
        a.vmulps(ymm6, ymm7, ymm8)?;
        a.vdivps(xmm9, xmm10, xmm11)
    });
    check_f64(|a| {
        a.vaddpd(ymm0, ymm1, ymm2)?;
        a.vsubpd(xmm3, xmm4, xmm5)?;
        a.vmulpd(ymm6, ymm7, ymm8)?;
        a.vdivpd(xmm9, xmm10, xmm11)
    });
    check_f32(|a| {
        a.vaddss(xmm0, xmm1, xmm2)?;
        a.vsubss(xmm3, xmm4, xmm5)?;
        a.vmulss(xmm6, xmm7, xmm8)?;
        a.vdivss(xmm9, xmm10, xmm11)
    });
    check_f64(|a| {
        a.vaddsd(xmm0, xmm1, xmm2)?;
        a.vsubsd(xmm3, xmm4, xmm5)?;
        a.vmulsd(xmm6, xmm7, xmm8)?;
        a.vdivsd(xmm9, xmm10, xmm11)
    });
    check_f64(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vmulsd(xmm0, xmm1, qword_ptr(rbx + 8))?;
        a.vaddpd(ymm2, ymm3, ymmword_ptr(rbx + 32))?;
        a.addsd(xmm4, xmm5)?;
        a.mulsd(xmm6, qword_ptr(rbx + 16))
    });
    check_f32(|a| {
        a.addss(xmm0, xmm1)?;
        a.mulps(xmm2, xmm3)?;
        a.subss(xmm4, xmm5)?;
        a.divps(xmm6, xmm7)
    });
    check_f32(|a| {
        a.vminps(ymm0, ymm1, ymm2)?;
        a.vmaxps(xmm3, xmm4, xmm5)?;
        a.vminss(xmm6, xmm7, xmm8)?;
        a.vmaxss(xmm9, xmm10, xmm11)
    });
    check_f64(|a| {
        a.vminpd(ymm0, ymm1, ymm2)?;
        a.vmaxpd(xmm3, xmm4, xmm5)?;
        a.vminsd(xmm6, xmm7, xmm8)?;
        a.vmaxsd(xmm9, xmm10, xmm11)
    });
    check_f64(|a| {
        a.vxorpd(xmm0, xmm0, xmm0)?;
        a.vminsd(xmm1, xmm0, xmm2)?;
        a.vmaxpd(ymm3, ymm0, ymm4)
    });
    check_f32(|a| {
        a.vmulps(ymm1, ymm1, ymm1)?;
        a.vmulps(ymm4, ymm4, ymm4)?;
        a.vsqrtps(ymm0, ymm1)?;
        a.vsqrtss(xmm2, xmm3, xmm4)
    });
    check_f64(|a| {
        a.vandpd(ymm0, ymm1, ymm2)?;
        a.vmulpd(ymm4, ymm4, ymm4)?;
        a.vmulpd(ymm7, ymm7, ymm7)?;
        a.vsqrtpd(xmm3, xmm4)?;
        a.vsqrtsd(xmm5, xmm6, xmm7)
    });
    check_f32(|a| {
        a.vandps(ymm0, ymm1, ymm2)?;
        a.vandnps(xmm3, xmm4, xmm5)?;
        a.vorps(ymm6, ymm7, ymm8)?;
        a.vxorps(xmm9, xmm10, xmm11)
    });
    check_f64(|a| {
        a.vandnpd(ymm0, ymm1, ymm2)?;
        a.vorpd(xmm3, xmm4, xmm5)?;
        a.vaddsubpd(ymm6, ymm7, ymm8)?;
        a.vhaddpd(ymm9, ymm10, ymm11)
    });
    for predicate in 0..32u32 {
        check_f32(|a| {
            a.vcmpps(ymm0, ymm1, ymm2, predicate)?;
            a.vcmpss(xmm3, xmm1, xmm1, predicate)
        });
        check_f64(|a| {
            a.vcmppd(xmm0, xmm1, xmm2, predicate)?;
            a.vcmpsd(xmm3, xmm4, xmm5, predicate)
        });
    }
    check_f64(|a| {
        a.vucomisd(xmm0, xmm1)?;
        a.seta(al)?;
        a.setb(cl)?;
        a.setp(dl)?;
        a.vucomisd(xmm2, xmm2)?;
        a.sete(bl)
    });
    check_f32(|a| {
        a.vucomiss(xmm0, xmm1)?;
        a.setae(al)?;
        a.setbe(cl)?;
        a.vcomiss(xmm3, xmm3)?;
        a.sete(dl)
    });
    for mode in [0u32, 1, 2, 3, 4, 8, 9, 10, 11] {
        check_f64(|a| {
            a.vroundsd(xmm0, xmm1, xmm2, mode)?;
            a.vroundpd(ymm3, ymm4, mode)
        });
        check_f32(|a| {
            a.vroundss(xmm0, xmm1, xmm2, mode)?;
            a.vroundps(xmm3, xmm4, mode)
        });
    }
    check_f64(|a| {
        a.vfmadd213sd(xmm0, xmm1, xmm2)?;
        a.vfmadd231pd(ymm3, ymm4, ymm5)?;
        a.vfmadd132pd(xmm6, xmm7, xmm8)
    });
    check_f32(|a| {
        a.vfmadd213ss(xmm0, xmm1, xmm2)?;
        a.vfmadd231ps(ymm3, ymm4, ymm5)?;
        a.vfnmadd213ps(xmm6, xmm7, xmm8)?;
        a.vfmsub231ps(ymm9, ymm10, ymm11)
    });
}

#[test]
fn conversions_match() {
    check_native(|a| {
        a.vcvtsi2sd(xmm0, xmm1, rax)?;
        a.vcvtsi2sd(xmm2, xmm3, ecx)?;
        a.vcvtsi2ss(xmm4, xmm5, rdx)?;
        a.vcvtsi2ss(xmm6, xmm7, esi)
    });
    check_native(|a| {
        a.cvtsi2sd(xmm0, rax)?;
        a.cvtsi2ss(xmm1, ecx)
    });
    check_f64(|a| {
        a.vcvttsd2si(eax, xmm1)?;
        a.vcvttsd2si(rcx, xmm2)?;
        a.vcvtsd2si(edx, xmm3)?;
        a.vcvtsd2si(rsi, xmm4)
    });
    check_f32(|a| {
        a.vcvttss2si(eax, xmm1)?;
        a.vcvttss2si(rcx, xmm2)?;
        a.vcvtss2si(edx, xmm3)?;
        a.cvtss2si(rsi, xmm4)
    });
    check_native(|a| {
        a.mov(rax, 0x7ff8_0000_0000_0000u64)?;
        a.vmovq(xmm1, rax)?;
        a.vcvttsd2si(ecx, xmm1)?;
        a.vcvttsd2si(rdx, xmm1)
    });
    check_native(|a| {
        a.mov(rax, 0x41f0_0000_0000_0000u64)?;
        a.vmovq(xmm1, rax)?;
        a.vcvttsd2si(ecx, xmm1)?;
        a.vcvtsd2si(rdx, xmm1)
    });
    check_f32(|a| {
        a.vcvtss2sd(xmm0, xmm1, xmm2)?;
        a.vcvtps2pd(ymm3, xmm4)?;
        a.vcvtps2pd(xmm5, xmm6)
    });
    check_f64(|a| {
        a.vcvtsd2ss(xmm0, xmm1, xmm2)?;
        a.vcvtpd2ps(xmm3, ymm4)?;
        a.vcvtpd2ps(xmm5, xmm6)
    });
    check_native(|a| {
        a.vcvtdq2ps(ymm0, ymm1)?;
        a.vcvtdq2pd(ymm2, xmm3)?;
        a.vcvtdq2pd(xmm4, xmm5)
    });
    check_f32(|a| {
        a.vcvtps2dq(ymm0, ymm1)?;
        a.vcvttps2dq(xmm2, xmm3)
    });
    check_f64(|a| {
        a.vcvttpd2dq(xmm0, ymm1)?;
        a.vcvttpd2dq(xmm2, xmm3)
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vstmxcsr(dword_ptr(rbx + 8))?;
        a.vldmxcsr(dword_ptr(rbx + 8))?;
        a.stmxcsr(dword_ptr(rbx + 12))
    });
}

fn string_operands(a: &mut CodeAssembler) -> Result<(), iced_x86::IcedError> {
    a.mov(rbx, DATA_BASE)?;
    a.mov(rcx, 0x0707_0707_0707_0707u64)?;
    a.vmovq(xmm7, rcx)?;
    a.vpbroadcastq(xmm7, xmm7)?;
    a.vpand(xmm1, xmm7, xmmword_ptr(rbx + 0x10))?;
    a.vpand(xmm2, xmm7, xmmword_ptr(rbx + 0x20))?;
    a.and(eax, 0x1f)?;
    a.sub(eax, 10)?;
    a.and(edx, 0x1f)?;
    a.sub(edx, 10)
}

#[test]
fn string_comparisons_match() {
    for format in 0..4u32 {
        for aggregation in 0..4u32 {
            let control = format | (aggregation << 2);
            check_native(|a| {
                string_operands(a)?;
                a.vpcmpistri(xmm1, xmm2, control)?;
                a.mov(r8, rcx)?;
                a.setc(r9b)?;
                a.setz(r10b)?;
                a.sets(r11b)?;
                a.seto(r12b)?;
                a.pcmpestri(xmm1, xmm2, control | 0x40)?;
                a.vpcmpistrm(xmm1, xmmword_ptr(rbx + 0x20), control | 0x10)
            });
        }
    }
    for control in [0x30u32, 0x20, 0x70, 0x48, 0x0c, 0x4c, 0x1a, 0x7d] {
        check_native(|a| {
            string_operands(a)?;
            a.vpcmpestri(xmm1, xmm2, control)?;
            a.mov(r8, rcx)?;
            a.pcmpistrm(xmm1, xmm2, control)?;
            a.vmovdqa(xmm3, xmm0)?;
            a.vpcmpestrm(xmm2, xmm1, control ^ 0x40)
        });
    }
}

#[test]
fn masked_moves_match() {
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vpmaskmovd(xmm0, xmm1, xmmword_ptr(rbx + 0x40))?;
        a.vpmaskmovd(ymm2, ymm3, ymmword_ptr(rbx + 0x80))?;
        a.vpmaskmovq(xmm4, xmm5, xmmword_ptr(rbx + 0x60))
    });
    check_native(|a| {
        a.mov(rbx, DATA_BASE)?;
        a.vpmaskmovd(xmmword_ptr(rbx + 0x40), xmm1, xmm6)?;
        a.vpmaskmovq(ymmword_ptr(rbx + 0x80), ymm3, ymm7)
    });
}
