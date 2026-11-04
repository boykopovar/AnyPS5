#include "host.h"

#include <immintrin.h>

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

void* memcpy(void* destination, const void* source, u64 length) {
    u8* to = destination;
    const u8* from = source;
    while (length--) *to++ = *from++;
    return destination;
}

void* memset(void* destination, int value, u64 length) {
    u8* to = destination;
    while (length--) *to++ = (u8)value;
    return destination;
}

static u64 length_of(const char* text) {
    u64 length = 0;
    while (text[length]) ++length;
    return length;
}

static void print(const char* text) {
    host_write(text, length_of(text));
}

static void print_hex(u64 value) {
    static const char digits[] = "0123456789abcdef";
    char buffer[17];
    for (int i = 0; i < 16; ++i) buffer[i] = digits[(value >> (60 - 4 * i)) & 0xf];
    buffer[16] = 0;
    print(buffer);
}

static const u32 float_specials[] = {
    0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000,
    0x7fa12345, 0xff812345, 0x7fd00001, 0x00000001, 0x80400000, 0x007fffff, 0x7f7fffff, 0x4f000000,
    0xcf000000, 0x4f000001, 0xcf000001, 0x5f000000, 0xdf000000, 0x3f000000, 0x3fc00000, 0xc0200000,
    0x40490fdb, 0x00800000, 0x0c000000, 0x33d6bf95, 0x4b800000, 0xcb800001, 0x3eaaaaab, 0x42f6e979,
};

static const u64 double_specials[] = {
    0x0000000000000000ul, 0x8000000000000000ul, 0x3ff0000000000000ul, 0xbff0000000000000ul,
    0x7ff0000000000000ul, 0xfff0000000000000ul, 0x7ff8000000000000ul, 0xfff8000000000000ul,
    0x7ff4000000012345ul, 0xfff0000000000001ul, 0x7ffc00000000beeful, 0x0000000000000001ul,
    0x800fffffffffffful, 0x7fefffffffffffful, 0x41e0000000000000ul, 0xc1e0000000200000ul,
    0x41dfffffffc00000ul, 0xc1e0000000000000ul, 0x43e0000000000000ul, 0xc3e0000000000000ul,
    0x3fe0000000000000ul, 0x3ff8000000000000ul, 0xc004000000000000ul, 0x400921fb54442d18ul,
    0x0010000000000000ul, 0x36a0000000000000ul, 0x3810000000000000ul, 0x47efffffe0000000ul,
    0x380fffffe0000000ul, 0xc1dfffffffffffffull, 0x3fd5555555555555ul, 0x405edd2f1a9fbe77ul,
};

enum { Specials = 32 };

static u64 rng_state = 0x452821e638d01377ul;

static u64 next_random(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

[[gnu::noinline]] static __m256 floats(int round, int shift) {
    u32 lanes[8];
    for (int i = 0; i < 8; ++i) {
        int index = (round * 8 + i * (shift + 1) + shift * 5) % (Specials + 8);
        lanes[i] = index < Specials ? float_specials[index] : (u32)next_random();
    }
    __m256 value;
    memcpy(&value, lanes, sizeof(value));
    return value;
}

[[gnu::noinline]] static __m256d doubles(int round, int shift) {
    u64 lanes[4];
    for (int i = 0; i < 4; ++i) {
        int index = (round * 4 + i * (shift + 1) + shift * 3) % (Specials + 4);
        lanes[i] = index < Specials ? double_specials[index] : next_random();
    }
    __m256d value;
    memcpy(&value, lanes, sizeof(value));
    return value;
}

static u64 hash_bytes(const void* data, int size) {
    const u8* bytes = data;
    u64 hash = 0xcbf29ce484222325ul;
    for (int i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 0x100000001b3ul;
    return hash;
}

#define FOLD(value) hash_bytes(&(value), sizeof(value))

#define TEST_PS(name, expression)                                                          \
    [[gnu::noinline]] static u64 test_##name(void) {                               \
        u64 hash = 0;                                                                      \
        for (int round = 0; round < 24; ++round) {                                         \
            __m256 a = floats(round, 0), b = floats(round, 1), c = floats(round, 2);       \
            __m128 a4 = _mm256_castps256_ps128(a), b4 = _mm256_castps256_ps128(b);         \
            (void)a4; (void)b4; (void)c;                                                   \
            __typeof__(expression) result = expression;                                    \
            hash = hash * 31 + FOLD(result);                                               \
        }                                                                                  \
        return hash;                                                                       \
    }

#define TEST_PD(name, expression)                                                          \
    [[gnu::noinline]] static u64 test_##name(void) {                               \
        u64 hash = 0;                                                                      \
        for (int round = 0; round < 24; ++round) {                                         \
            __m256d a = doubles(round, 0), b = doubles(round, 1), c = doubles(round, 2);   \
            __m128d a2 = _mm256_castpd256_pd128(a), b2 = _mm256_castpd256_pd128(b);        \
            (void)a2; (void)b2; (void)c;                                                   \
            __typeof__(expression) result = expression;                                    \
            hash = hash * 31 + FOLD(result);                                               \
        }                                                                                  \
        return hash;                                                                       \
    }

TEST_PS(addps, _mm256_add_ps(a, b))
TEST_PS(subps, _mm256_sub_ps(a, b))
TEST_PS(mulps, _mm256_mul_ps(a, b))
TEST_PS(divps, _mm256_div_ps(a, b))
TEST_PS(minps, _mm256_min_ps(a, b))
TEST_PS(maxps, _mm256_max_ps(a, b))
TEST_PS(sqrtps, _mm256_sqrt_ps(a))
TEST_PS(addss, _mm_add_ss(a4, b4))
TEST_PS(mulss, _mm_mul_ss(a4, b4))
TEST_PS(divss, _mm_div_ss(a4, b4))
TEST_PS(minss, _mm_min_ss(a4, b4))
TEST_PS(sqrtss, _mm_sqrt_ss(a4))
TEST_PS(addsub, _mm256_addsub_ps(a, b))
TEST_PS(hadd, _mm256_hadd_ps(a, b))
TEST_PS(hsub, _mm_hsub_ps(a4, b4))
TEST_PS(fmadd, _mm256_fmadd_ps(a, b, c))
TEST_PS(fmsub, _mm256_fmsub_ps(a, b, c))
TEST_PS(fnmadd, _mm256_fnmadd_ps(a, b, c))
TEST_PS(fnmsub, _mm_fnmsub_ss(a4, b4, _mm256_castps256_ps128(c)))
TEST_PS(fmaddsub, _mm256_fmaddsub_ps(a, b, c))
TEST_PS(fmsubadd, _mm256_fmsubadd_ps(a, b, c))
TEST_PS(cmpps, _mm256_or_ps(_mm256_or_ps(_mm256_and_ps(_mm256_cmp_ps(a, b, _CMP_LT_OQ), _mm256_set1_ps(1.0f)), _mm256_and_ps(_mm256_cmp_ps(a, b, _CMP_UNORD_Q), _mm256_set1_ps(2.0f))), _mm256_and_ps(_mm256_cmp_ps(a, b, _CMP_NGE_UQ), _mm256_set1_ps(4.0f))))
TEST_PS(cmpss, _mm_cmp_ss(a4, b4, _CMP_NEQ_UQ))
TEST_PS(comiss, _mm_set1_epi32(_mm_comilt_ss(a4, b4) + 2 * _mm_ucomieq_ss(a4, b4) + 4 * _mm_comige_ss(a4, b4) + 8 * _mm_ucomineq_ss(a4, b4)))
TEST_PS(round_nearest, _mm256_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC))
TEST_PS(round_floor, _mm256_round_ps(a, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC))
TEST_PS(round_ceil, _mm_round_ss(a4, b4, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC))
TEST_PS(round_trunc, _mm256_round_ps(b, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC))
TEST_PS(cvttps2dq, _mm256_cvttps_epi32(a))
TEST_PS(cvtps2dq, _mm256_cvtps_epi32(b))
TEST_PS(cvtdq2ps, _mm256_cvtepi32_ps(_mm256_castps_si256(a)))
TEST_PS(cvttss2si, _mm_set1_epi64x((long long)_mm_cvttss_si32(a4) ^ (_mm_cvttss_si64(b4) << 1)))
TEST_PS(cvtss2si, _mm_set1_epi64x((long long)_mm_cvtss_si32(a4) ^ (_mm_cvtss_si64(b4) << 1)))
TEST_PS(cvtsi2ss, _mm_cvtsi64_ss(_mm_cvtsi32_ss(a4, (int)_mm_cvtsi128_si32(_mm_castps_si128(b4))), (long long)next_random()))
TEST_PS(cvtps2pd, _mm256_cvtps_pd(a4))
TEST_PS(cvtss2sd, _mm_cvtss_sd(_mm_castps_pd(b4), a4))
TEST_PS(cvtph, _mm256_cvtph_ps(_mm_cvtps_ph(a4, _MM_FROUND_TO_NEAREST_INT)))
TEST_PS(cvtph_raw, _mm256_cvtph_ps(_mm_castps_si128(b4)))
TEST_PS(shuffles, _mm256_add_ps(_mm256_shuffle_ps(a, b, 0x93), _mm256_permute_ps(a, 0x1b)))
TEST_PS(moves, _mm_add_ps(_mm_add_ps(_mm_movehl_ps(a4, b4), _mm_movelh_ps(a4, b4)), _mm_add_ps(_mm_movehdup_ps(a4), _mm_moveldup_ps(b4))))
TEST_PS(loadhl, _mm_loadh_pi(_mm_loadl_pi(a4, (const __m64*)&b4), (const __m64*)&b4 + 1))
TEST_PS(insertps, _mm_insert_ps(a4, b4, 0x9a))
TEST_PS(blendv, _mm256_blendv_ps(a, b, c))
TEST_PS(broadcast, _mm256_broadcastss_ps(b4))
TEST_PS(logic, _mm256_xor_ps(_mm256_andnot_ps(a, b), _mm256_or_ps(a, c)))

TEST_PD(addpd, _mm256_add_pd(a, b))
TEST_PD(subpd, _mm256_sub_pd(a, b))
TEST_PD(mulpd, _mm256_mul_pd(a, b))
TEST_PD(divpd, _mm256_div_pd(a, b))
TEST_PD(minpd, _mm256_min_pd(a, b))
TEST_PD(maxpd, _mm256_max_pd(a, b))
TEST_PD(sqrtpd, _mm256_sqrt_pd(b))
TEST_PD(addsd, _mm_add_sd(a2, b2))
TEST_PD(divsd, _mm_div_sd(a2, b2))
TEST_PD(maxsd, _mm_max_sd(a2, b2))
TEST_PD(sqrtsd, _mm_sqrt_sd(a2, b2))
TEST_PD(fmaddpd, _mm256_fmadd_pd(a, b, c))
TEST_PD(fmsubsd, _mm_fmsub_sd(a2, b2, _mm256_castpd256_pd128(c)))
TEST_PD(haddpd, _mm256_hadd_pd(a, b))
TEST_PD(cmppd, _mm256_cmp_pd(a, b, _CMP_LE_OS))
TEST_PD(comisd, _mm_set1_epi32(_mm_comile_sd(a2, b2) + 2 * _mm_ucomigt_sd(a2, b2)))
TEST_PD(roundpd, _mm256_round_pd(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC))
TEST_PD(cvttpd2dq, _mm256_cvttpd_epi32(a))
TEST_PD(cvtpd2dq, _mm256_cvtpd_epi32(b))
TEST_PD(cvtpd2ps, _mm256_cvtpd_ps(a))
TEST_PD(cvtsd2ss, _mm_cvtsd_ss(_mm_castpd_ps(b2), a2))
TEST_PD(cvttsd2si, _mm_set1_epi64x((long long)_mm_cvttsd_si32(a2) ^ (_mm_cvttsd_si64(b2) << 1)))
TEST_PD(cvtsd2si, _mm_set1_epi64x((long long)_mm_cvtsd_si32(a2) ^ (_mm_cvtsd_si64(b2) << 1)))
TEST_PD(cvtdq2pd, _mm256_cvtepi32_pd(_mm_castpd_si128(a2)))
TEST_PD(cvtsi2sd, _mm_cvtsi64_sd(a2, (long long)next_random()))
TEST_PD(shufpd, _mm256_shuffle_pd(a, b, 0x6))
TEST_PD(permutepd, _mm256_permute_pd(a, 0x9))
TEST_PD(movedup, _mm256_movedup_pd(b))
TEST_PD(blendpd, _mm256_blend_pd(a, b, 0x5))

[[gnu::noinline]] static u64 test_scalar_c(void) {
    u64 hash = 0;
    for (int round = 0; round < 24; ++round) {
        volatile float x = ((const float*)float_specials)[round % Specials];
        volatile double y = ((const double*)double_specials)[(round * 7) % Specials];
        float fx = x * 1.5f + (float)y;
        double dy = y / (double)x - (double)(int)(x > 1e9f || x < -1e9f || x != x ? 7 : x);
        long long conversion = (long long)(y > 1e18 || y < -1e18 || y != y ? 3 : y);
        hash = hash * 31 + FOLD(fx);
        hash = hash * 31 + FOLD(dy);
        hash = hash * 31 + (u64)conversion;
    }
    return hash;
}

struct Test {
    const char* name;
    u64 (*run)(void);
};

#define ENTRY(name) {#name, test_##name}

static const struct Test tests[] = {
    ENTRY(addps), ENTRY(subps), ENTRY(mulps), ENTRY(divps), ENTRY(minps), ENTRY(maxps), ENTRY(sqrtps), ENTRY(addss),
    ENTRY(mulss), ENTRY(divss), ENTRY(minss), ENTRY(sqrtss), ENTRY(addsub), ENTRY(hadd), ENTRY(hsub), ENTRY(fmadd),
    ENTRY(fmsub), ENTRY(fnmadd), ENTRY(fnmsub), ENTRY(fmaddsub), ENTRY(fmsubadd), ENTRY(cmpps), ENTRY(cmpss), ENTRY(comiss),
    ENTRY(round_nearest), ENTRY(round_floor), ENTRY(round_ceil), ENTRY(round_trunc), ENTRY(cvttps2dq), ENTRY(cvtps2dq),
    ENTRY(cvtdq2ps), ENTRY(cvttss2si), ENTRY(cvtss2si), ENTRY(cvtsi2ss), ENTRY(cvtps2pd), ENTRY(cvtss2sd), ENTRY(cvtph),
    ENTRY(cvtph_raw), ENTRY(shuffles), ENTRY(moves), ENTRY(loadhl), ENTRY(insertps), ENTRY(blendv), ENTRY(broadcast), ENTRY(logic),
    ENTRY(addpd), ENTRY(subpd), ENTRY(mulpd), ENTRY(divpd), ENTRY(minpd), ENTRY(maxpd), ENTRY(sqrtpd), ENTRY(addsd), ENTRY(divsd),
    ENTRY(maxsd), ENTRY(sqrtsd), ENTRY(fmaddpd), ENTRY(fmsubsd), ENTRY(haddpd), ENTRY(cmppd), ENTRY(comisd), ENTRY(roundpd),
    ENTRY(cvttpd2dq), ENTRY(cvtpd2dq), ENTRY(cvtpd2ps), ENTRY(cvtsd2ss), ENTRY(cvttsd2si), ENTRY(cvtsd2si), ENTRY(cvtdq2pd),
    ENTRY(cvtsi2sd), ENTRY(shufpd), ENTRY(permutepd), ENTRY(movedup), ENTRY(blendpd), ENTRY(scalar_c),
};

static const struct {
    const char* name;
    u32 mxcsr;
} modes[] = {
    {"default", 0x1f80},
    {"ftz-daz", 0x9fc0},
    {"round-down", 0x3f80},
};

int main(void) {
    for (u64 m = 0; m < sizeof(modes) / sizeof(modes[0]); ++m) {
        _mm_setcsr(modes[m].mxcsr);
        for (u64 i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
            rng_state = 0x452821e638d01377ul + i;
            print(modes[m].name);
            print(" ");
            print(tests[i].name);
            print(" ");
            print_hex(tests[i].run());
            print("\n");
        }
    }
    return 0;
}

__asm__(".globl _start\n"
        "_start:\n"
        "    xor %ebp, %ebp\n"
        "    and $-16, %rsp\n"
        "    call main\n"
        "    mov %eax, %edi\n"
        "    call host_exit\n"
        "    ud2\n");
