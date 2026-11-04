#include "host.h"

#include <immintrin.h>

typedef unsigned long u64;
typedef unsigned int u32;
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

static u64 rng_state = 0x243f6a8885a308d3ul;

static u64 next_random(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static volatile u64 sink;

[[gnu::noinline]] static __m256i random256(int special) {
    u64 lanes[4];
    for (int i = 0; i < 4; ++i) lanes[i] = next_random();
    if (special == 1) lanes[1] = 0x8000000080000000ul;
    if (special == 2) lanes[2] = 0x7fff80007fff8000ul;
    if (special == 3) lanes[3] = 0x00ff7f8001fe0000ul;
    __m256i value;
    memcpy(&value, lanes, sizeof(value));
    return value;
}

static u64 fold256(__m256i value) {
    u64 lanes[4];
    memcpy(lanes, &value, sizeof(lanes));
    u64 hash = 0xcbf29ce484222325ul;
    for (int i = 0; i < 4; ++i) hash = (hash ^ lanes[i]) * 0x100000001b3ul;
    return hash;
}

static u64 fold128(__m128i value) {
    return fold256(_mm256_set_m128i(value, _mm_setzero_si128())) ^ fold256(_mm256_set_m128i(_mm_setzero_si128(), value));
}

static void report(const char* name, u64 value) {
    print(name);
    print(" ");
    print_hex(value);
    print("\n");
}

#define BINARY256(name, expression)                                     \
    [[gnu::noinline]] static u64 test_##name(void) {            \
        u64 hash = 0;                                                   \
        for (int round = 0; round < 8; ++round) {                       \
            __m256i a = random256(round & 3), b = random256((round + 1) & 3); \
            hash = hash * 31 + fold256(expression);                     \
        }                                                               \
        return hash;                                                    \
    }

#define BINARY128(name, expression)                                     \
    [[gnu::noinline]] static u64 test_##name(void) {            \
        u64 hash = 0;                                                   \
        for (int round = 0; round < 8; ++round) {                       \
            __m128i a = _mm256_castsi256_si128(random256(round & 3));   \
            __m128i b = _mm256_extracti128_si256(random256((round + 1) & 3), 1); \
            hash = hash * 31 + fold128(expression);                     \
        }                                                               \
        return hash;                                                    \
    }

BINARY256(add8, _mm256_add_epi8(a, b))
BINARY256(add16, _mm256_add_epi16(a, b))
BINARY256(add32, _mm256_add_epi32(a, b))
BINARY256(add64, _mm256_add_epi64(a, b))
BINARY256(sub8, _mm256_sub_epi8(a, b))
BINARY256(sub64, _mm256_sub_epi64(a, b))
BINARY256(adds8, _mm256_adds_epi8(a, b))
BINARY256(adds16, _mm256_adds_epi16(a, b))
BINARY256(addsu8, _mm256_adds_epu8(a, b))
BINARY256(subs16, _mm256_subs_epi16(a, b))
BINARY256(subsu16, _mm256_subs_epu16(a, b))
BINARY256(mullo16, _mm256_mullo_epi16(a, b))
BINARY256(mullo32, _mm256_mullo_epi32(a, b))
BINARY256(mulhi16, _mm256_mulhi_epi16(a, b))
BINARY256(mulhiu16, _mm256_mulhi_epu16(a, b))
BINARY256(mulu32, _mm256_mul_epu32(a, b))
BINARY256(mul32, _mm256_mul_epi32(a, b))
BINARY256(madd16, _mm256_madd_epi16(a, b))
BINARY256(min8, _mm256_min_epi8(a, b))
BINARY256(minu16, _mm256_min_epu16(a, b))
BINARY256(max32, _mm256_max_epi32(a, b))
BINARY256(maxu32, _mm256_max_epu32(a, b))
BINARY256(cmpeq8, _mm256_cmpeq_epi8(a, _mm256_or_si256(a, _mm256_and_si256(b, _mm256_set1_epi8(1)))))
BINARY256(cmpeq64, _mm256_cmpeq_epi64(a, _mm256_blend_epi32(a, b, 0x30)))
BINARY256(cmpgt16, _mm256_cmpgt_epi16(a, b))
BINARY256(cmpgt64, _mm256_cmpgt_epi64(a, b))
BINARY256(avg8, _mm256_avg_epu8(a, b))
BINARY256(avg16, _mm256_avg_epu16(a, b))
BINARY256(andnot, _mm256_andnot_si256(a, b))
BINARY256(abs8, _mm256_abs_epi8(a))
BINARY256(abs32, _mm256_abs_epi32(b))
BINARY256(slli16, _mm256_slli_epi16(a, 5))
BINARY256(slli64, _mm256_slli_epi64(a, 63))
BINARY256(srli32, _mm256_srli_epi32(a, 31))
BINARY256(srai16, _mm256_srai_epi16(a, 3))
BINARY256(srai32big, _mm256_srai_epi32(a, 40))
BINARY256(sll32, _mm256_sll_epi32(a, _mm_cvtsi32_si128((int)(next_random() & 31))))
BINARY256(srl64big, _mm256_srl_epi64(a, _mm_cvtsi32_si128(70)))
BINARY256(sra16, _mm256_sra_epi16(a, _mm_cvtsi32_si128((int)(next_random() & 15))))
BINARY256(sllv32, _mm256_sllv_epi32(a, _mm256_and_si256(b, _mm256_set1_epi32(63))))
BINARY256(srlv64, _mm256_srlv_epi64(a, _mm256_and_si256(b, _mm256_set1_epi64x(127))))
BINARY256(srav32, _mm256_srav_epi32(a, _mm256_and_si256(b, _mm256_set1_epi32(63))))
BINARY256(bslli, _mm256_bslli_epi128(a, 5))
BINARY256(bsrli, _mm256_bsrli_epi128(a, 11))
BINARY256(shuffle32, _mm256_shuffle_epi32(a, 0x9c))
BINARY256(shufflelo, _mm256_shufflelo_epi16(a, 0x1b))
BINARY256(shufflehi, _mm256_shufflehi_epi16(a, 0xe4 ^ 0x5a))
BINARY256(shufps, _mm256_castps_si256(_mm256_shuffle_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b), 0x4e)))
BINARY256(unpacklo8, _mm256_unpacklo_epi8(a, b))
BINARY256(unpackhi16, _mm256_unpackhi_epi16(a, b))
BINARY256(unpacklo32, _mm256_unpacklo_epi32(a, b))
BINARY256(unpackhi64, _mm256_unpackhi_epi64(a, b))
BINARY256(packs16, _mm256_packs_epi16(a, b))
BINARY256(packs32, _mm256_packs_epi32(a, b))
BINARY256(packus16, _mm256_packus_epi16(a, b))
BINARY256(packus32, _mm256_packus_epi32(a, b))
BINARY256(alignr, _mm256_alignr_epi8(a, b, 7))
BINARY256(alignr20, _mm256_alignr_epi8(a, b, 20))
BINARY256(shuffle8, _mm256_shuffle_epi8(a, b))
BINARY256(permute4x64, _mm256_permute4x64_epi64(a, 0x72))
BINARY256(permutevar8x32, _mm256_permutevar8x32_epi32(a, b))
BINARY256(permute2x128, _mm256_permute2x128_si256(a, b, 0x21))
BINARY256(permute2x128zero, _mm256_permute2x128_si256(a, b, 0x83))
BINARY256(inserti128, _mm256_inserti128_si256(a, _mm256_castsi256_si128(b), 1))
BINARY256(blend32, _mm256_blend_epi32(a, b, 0xa5))
BINARY256(blend16, _mm256_blend_epi16(a, b, 0x3c))
BINARY256(blendv8, _mm256_blendv_epi8(a, b, _mm256_xor_si256(a, b)))
BINARY256(cvt8to16, _mm256_cvtepu8_epi16(_mm256_castsi256_si128(a)))
BINARY256(cvt8to64s, _mm256_cvtepi8_epi64(_mm256_castsi256_si128(a)))
BINARY256(cvt16to32s, _mm256_cvtepi16_epi32(_mm256_castsi256_si128(b)))
BINARY256(cvt32to64, _mm256_cvtepu32_epi64(_mm256_castsi256_si128(b)))
BINARY256(movemask, _mm256_set1_epi32(_mm256_movemask_epi8(a)))
BINARY256(movemaskps, _mm256_set1_epi32(_mm256_movemask_ps(_mm256_castsi256_ps(a))))
BINARY256(testz, _mm256_set1_epi32(_mm256_testz_si256(a, b) + 2 * _mm256_testc_si256(a, a) + 4 * _mm256_testz_si256(a, _mm256_setzero_si256())))
BINARY256(broadcast, _mm256_add_epi32(_mm256_broadcastd_epi32(_mm256_castsi256_si128(a)), _mm256_broadcastb_epi8(_mm256_castsi256_si128(b))))

BINARY128(add16x, _mm_add_epi16(a, b))
BINARY128(sub32x, _mm_sub_epi32(a, b))
BINARY128(shuffle8x, _mm_shuffle_epi8(a, b))
BINARY128(unpacklo8x, _mm_unpacklo_epi8(a, b))
BINARY128(packus16x, _mm_packus_epi16(a, b))
BINARY128(alignrx, _mm_alignr_epi8(a, b, 3))
BINARY128(slli32x, _mm_slli_epi32(a, 9))
BINARY128(srli64x, _mm_srli_epi64(a, 33))
BINARY128(bsrlix, _mm_srli_si128(a, 9))
BINARY128(shuffle32x, _mm_shuffle_epi32(a, 0x39))
BINARY128(blendvx, _mm_blendv_epi8(a, b, _mm_xor_si128(a, b)))
BINARY128(extract, _mm_set_epi32(_mm_extract_epi8(a, 13), _mm_extract_epi16(b, 6), _mm_extract_epi32(a, 2), (int)_mm_extract_epi64(b, 1)))
BINARY128(insert, _mm_insert_epi64(_mm_insert_epi32(_mm_insert_epi16(_mm_insert_epi8(a, 0x5a, 9), 0x1234, 3), (int)0xdeadbeef, 2), (long long)0x0123456789abcdef, 0))
BINARY128(extracti128, _mm256_extracti128_si256(_mm256_set_m128i(a, b), 1))
BINARY128(cvt16to64x, _mm_cvtepi16_epi64(a))
BINARY128(min32ux, _mm_min_epu32(a, b))
BINARY128(cmpeq32x, _mm_cmpeq_epi32(a, _mm_blend_epi16(a, b, 0x0f)))
BINARY128(movemaskx, _mm_set1_epi32(_mm_movemask_epi8(a)))
BINARY256(maddubs16, _mm256_maddubs_epi16(a, b))
BINARY256(mulhrs16, _mm256_mulhrs_epi16(a, b))
BINARY256(hadd32, _mm256_hadd_epi32(a, b))
BINARY256(hadds16, _mm256_hadds_epi16(a, b))
BINARY256(hsub16, _mm256_hsub_epi16(a, b))
BINARY256(hsubs16, _mm256_hsubs_epi16(a, b))
BINARY256(sad8, _mm256_sad_epu8(a, b))
BINARY256(testps, _mm256_set1_epi32(_mm256_testz_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b)) * 2 + _mm256_testc_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b))))
BINARY256(testpd, _mm256_set1_epi32(_mm256_testz_pd(_mm256_castsi256_pd(a), _mm256_castsi256_pd(a)) * 2 + _mm256_testc_pd(_mm256_castsi256_pd(b), _mm256_castsi256_pd(a))))
BINARY128(maddubs16x, _mm_maddubs_epi16(a, b))
BINARY128(mulhrs16x, _mm_mulhrs_epi16(a, b))
BINARY128(hadd16x, _mm_hadd_epi16(a, b))
BINARY128(hsub32x, _mm_hsub_epi32(a, b))
BINARY128(sad8x, _mm_sad_epu8(a, b))

static void report_bytes(const char* name, __m128i value) {
    static const char digits[] = "0123456789abcdef";
    u8 bytes[16];
    memcpy(bytes, &value, sizeof(bytes));
    char buffer[33];
    for (int i = 0; i < 16; ++i) {
        buffer[2 * i] = digits[bytes[i] >> 4];
        buffer[2 * i + 1] = digits[bytes[i] & 15];
    }
    buffer[32] = 0;
    print(name);
    print(" ");
    print(buffer);
    print("\n");
}

static volatile u8 aes_key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
static volatile u8 aes_plain[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
static volatile u64 clmul_inputs[4] = {0x8000000000000001ul, 0x0123456789abcdeful, 0xfedcba9876543210ul, 0xfffffffffffffffful};

static __m128i load_volatile(const volatile u8* source) {
    u8 bytes[16];
    for (int i = 0; i < 16; ++i) bytes[i] = source[i];
    __m128i value;
    memcpy(&value, bytes, sizeof(value));
    return value;
}

#define VEX_AESENC(a, b) _mm_aesenc_si128(a, b)
#define VEX_AESENCLAST(a, b) _mm_aesenclast_si128(a, b)
#define VEX_AESDEC(a, b) _mm_aesdec_si128(a, b)
#define VEX_AESDECLAST(a, b) _mm_aesdeclast_si128(a, b)
#define VEX_AESIMC(a) _mm_aesimc_si128(a)
#define VEX_AESKEYGENASSIST(a, constant) _mm_aeskeygenassist_si128(a, constant)
#define VEX_PCLMULQDQ(a, b, selector) _mm_clmulepi64_si128(a, b, selector)
#define LEGACY_BINARY(instruction, a, b)                                  \
    ({                                                                    \
        __m128i result_ = (a);                                            \
        __asm__(instruction " %1, %0" : "+x"(result_) : "x"(b));          \
        result_;                                                          \
    })
#define SSE_AESENC(a, b) LEGACY_BINARY("aesenc", a, b)
#define SSE_AESENCLAST(a, b) LEGACY_BINARY("aesenclast", a, b)
#define SSE_AESDEC(a, b) LEGACY_BINARY("aesdec", a, b)
#define SSE_AESDECLAST(a, b) LEGACY_BINARY("aesdeclast", a, b)
#define SSE_AESIMC(a)                                         \
    ({                                                        \
        __m128i result_;                                      \
        __asm__("aesimc %1, %0" : "=x"(result_) : "x"(a));    \
        result_;                                              \
    })
#define SSE_AESKEYGENASSIST(a, constant)                                         \
    ({                                                                           \
        __m128i result_;                                                         \
        __asm__("aeskeygenassist %2, %1, %0" : "=x"(result_) : "x"(a), "i"(constant)); \
        result_;                                                                 \
    })
#define SSE_PCLMULQDQ(a, b, selector)                                                  \
    ({                                                                                 \
        __m128i result_ = (a);                                                         \
        __asm__("pclmulqdq %2, %1, %0" : "+x"(result_) : "x"(b), "i"(selector));       \
        result_;                                                                       \
    })

#define KEY_ROUND(form, keys, i, constant)                                                                    \
    do {                                                                                                       \
        __m128i generated = _mm_shuffle_epi32(form##_AESKEYGENASSIST(keys[i - 1], constant), 0xff);          \
        __m128i key = keys[i - 1];                                                                             \
        key = _mm_xor_si128(key, _mm_slli_si128(key, 4));                                                      \
        key = _mm_xor_si128(key, _mm_slli_si128(key, 4));                                                      \
        key = _mm_xor_si128(key, _mm_slli_si128(key, 4));                                                      \
        keys[i] = _mm_xor_si128(key, generated);                                                               \
    } while (0)

#define AES128_TEST(form, label)                                                                    \
    [[gnu::noinline]] static void aes128_##form(void) {                                     \
        __m128i keys[11];                                                                           \
        keys[0] = load_volatile(aes_key);                                                           \
        KEY_ROUND(form, keys, 1, 0x01);                                                             \
        KEY_ROUND(form, keys, 2, 0x02);                                                             \
        KEY_ROUND(form, keys, 3, 0x04);                                                             \
        KEY_ROUND(form, keys, 4, 0x08);                                                             \
        KEY_ROUND(form, keys, 5, 0x10);                                                             \
        KEY_ROUND(form, keys, 6, 0x20);                                                             \
        KEY_ROUND(form, keys, 7, 0x40);                                                             \
        KEY_ROUND(form, keys, 8, 0x80);                                                             \
        KEY_ROUND(form, keys, 9, 0x1b);                                                             \
        KEY_ROUND(form, keys, 10, 0x36);                                                            \
        __m128i block = _mm_xor_si128(load_volatile(aes_plain), keys[0]);                           \
        for (int i = 1; i < 10; ++i) block = form##_AESENC(block, keys[i]);                         \
        block = form##_AESENCLAST(block, keys[10]);                                                 \
        report_bytes("aes128 encrypt " label, block);                                               \
        block = _mm_xor_si128(block, keys[10]);                                                     \
        for (int i = 9; i > 0; --i) block = form##_AESDEC(block, form##_AESIMC(keys[i]));           \
        block = form##_AESDECLAST(block, keys[0]);                                                  \
        report_bytes("aes128 decrypt " label, block);                                               \
    }

#define CLMUL_TEST(form, label)                                                                     \
    [[gnu::noinline]] static void clmul_##form(void) {                                      \
        __m128i a = _mm_set_epi64x((long long)clmul_inputs[1], (long long)clmul_inputs[0]);        \
        __m128i b = _mm_set_epi64x((long long)clmul_inputs[3], (long long)clmul_inputs[2]);        \
        report_bytes("clmul " label " 00", form##_PCLMULQDQ(a, b, 0x00));                           \
        report_bytes("clmul " label " 01", form##_PCLMULQDQ(a, b, 0x01));                           \
        report_bytes("clmul " label " 10", form##_PCLMULQDQ(a, b, 0x10));                           \
        report_bytes("clmul " label " 11", form##_PCLMULQDQ(a, b, 0x11));                           \
    }

static volatile u8 sha_message[3] = {'a', 'b', 'c'};

static void sha_block(u8 block[64]) {
    for (int i = 0; i < 64; ++i) block[i] = 0;
    for (int i = 0; i < 3; ++i) block[i] = sha_message[i];
    block[3] = 0x80;
    block[63] = 24;
}

static void report_words(const char* name, const u32* words, int count) {
    static const char digits[] = "0123456789abcdef";
    char buffer[65];
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < 8; ++j) buffer[8 * i + j] = digits[(words[i] >> (28 - 4 * j)) & 15];
    }
    buffer[8 * count] = 0;
    print(name);
    print(" ");
    print(buffer);
    print("\n");
}

static const u32 sha256_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74,
    0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d,
    0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e,
    0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

[[gnu::noinline]] static void sha256_abc(void) {
    u32 state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    u8 block[64];
    sha_block(block);
    const __m128i swap = _mm_set_epi64x(0x0c0d0e0f08090a0bLL, 0x0405060700010203LL);
    __m128i dcba = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&state[0]), 0xb1);
    __m128i efgh = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)&state[4]), 0x1b);
    __m128i abef = _mm_alignr_epi8(dcba, efgh, 8);
    __m128i cdgh = _mm_blend_epi16(efgh, dcba, 0xf0);
    const __m128i abef_saved = abef, cdgh_saved = cdgh;
    __m128i words[16];
    for (int i = 0; i < 4; ++i) words[i] = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)&block[16 * i]), swap);
    for (int i = 4; i < 16; ++i) {
        __m128i next = _mm_add_epi32(_mm_sha256msg1_epu32(words[i - 4], words[i - 3]), _mm_alignr_epi8(words[i - 1], words[i - 2], 4));
        words[i] = _mm_sha256msg2_epu32(next, words[i - 1]);
    }
    for (int i = 0; i < 16; ++i) {
        __m128i keyed = _mm_add_epi32(words[i], _mm_loadu_si128((const __m128i*)&sha256_constants[4 * i]));
        cdgh = _mm_sha256rnds2_epu32(cdgh, abef, keyed);
        abef = _mm_sha256rnds2_epu32(abef, cdgh, _mm_shuffle_epi32(keyed, 0x0e));
    }
    abef = _mm_add_epi32(abef, abef_saved);
    cdgh = _mm_add_epi32(cdgh, cdgh_saved);
    __m128i feba = _mm_shuffle_epi32(abef, 0x1b);
    __m128i dchg = _mm_shuffle_epi32(cdgh, 0xb1);
    _mm_storeu_si128((__m128i*)&state[0], _mm_blend_epi16(feba, dchg, 0xf0));
    _mm_storeu_si128((__m128i*)&state[4], _mm_alignr_epi8(dchg, feba, 8));
    report_words("sha256 abc", state, 8);
}

[[gnu::noinline]] static void sha1_abc(void) {
    u32 state[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    u8 block[64];
    sha_block(block);
    const __m128i reverse = _mm_set_epi64x(0x0001020304050607LL, 0x08090a0b0c0d0e0fLL);
    __m128i abcd = _mm_shuffle_epi32(_mm_loadu_si128((const __m128i*)state), 0x1b);
    __m128i e = _mm_set_epi32((int)state[4], 0, 0, 0);
    const __m128i abcd_saved = abcd, e_saved = e;
    __m128i words[20];
    for (int i = 0; i < 4; ++i) words[i] = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)&block[16 * i]), reverse);
    for (int i = 4; i < 20; ++i) words[i] = _mm_sha1msg2_epu32(_mm_xor_si128(_mm_sha1msg1_epu32(words[i - 4], words[i - 3]), words[i - 2]), words[i - 1]);
    __m128i previous = abcd;
    e = _mm_add_epi32(e, words[0]);
    abcd = _mm_sha1rnds4_epu32(abcd, e, 0);
#define SHA1_GROUP(i, function)                       \
    e = _mm_sha1nexte_epu32(previous, words[i]);      \
    previous = abcd;                                  \
    abcd = _mm_sha1rnds4_epu32(abcd, e, function)
    SHA1_GROUP(1, 0); SHA1_GROUP(2, 0); SHA1_GROUP(3, 0); SHA1_GROUP(4, 0);
    SHA1_GROUP(5, 1); SHA1_GROUP(6, 1); SHA1_GROUP(7, 1); SHA1_GROUP(8, 1); SHA1_GROUP(9, 1);
    SHA1_GROUP(10, 2); SHA1_GROUP(11, 2); SHA1_GROUP(12, 2); SHA1_GROUP(13, 2); SHA1_GROUP(14, 2);
    SHA1_GROUP(15, 3); SHA1_GROUP(16, 3); SHA1_GROUP(17, 3); SHA1_GROUP(18, 3); SHA1_GROUP(19, 3);
#undef SHA1_GROUP
    e = _mm_sha1nexte_epu32(previous, e_saved);
    abcd = _mm_add_epi32(abcd, abcd_saved);
    _mm_storeu_si128((__m128i*)state, _mm_shuffle_epi32(abcd, 0x1b));
    state[4] = (u32)_mm_extract_epi32(e, 3);
    report_words("sha1 abc", state, 5);
}

static __m128i from_floats(float a, float b, float c, float d) {
    const float values[4] = {a, b, c, d};
    __m128i result;
    memcpy(&result, values, sizeof(result));
    return result;
}

static __m128i from_doubles(double a, double b) {
    const double values[2] = {a, b};
    __m128i result;
    memcpy(&result, values, sizeof(result));
    return result;
}

#define LEGACY_SCALAR(name, instruction, destination, source)            \
    [[gnu::noinline]] static void legacy_##name(void) {         \
        __m128i result = destination;                                    \
        __asm__(instruction " %1, %0" : "+x"(result) : "x"(source));     \
        report_bytes("legacy " #name, result);                           \
    }

LEGACY_SCALAR(addss, "addss", from_floats(1, 2, 3, 4), from_floats(10, 20, 30, 40))
LEGACY_SCALAR(maxss, "maxss", from_floats(1, 2, 3, 4), from_floats(10, 20, 30, 40))
LEGACY_SCALAR(sqrtsd, "sqrtsd", from_doubles(16, 5), from_doubles(9, 7))
LEGACY_SCALAR(cmpltss, "cmpltss", from_floats(1, 2, 3, 4), from_floats(10, 20, 30, 40))
LEGACY_SCALAR(cvtss2sd, "cvtss2sd", from_doubles(16, 5), from_floats(10, 20, 30, 40))
LEGACY_SCALAR(cvtsd2ss, "cvtsd2ss", from_floats(1, 2, 3, 4), from_doubles(6, 8))
LEGACY_SCALAR(roundss, "roundss $1,", from_floats(1, 2, 3, 4), from_floats(2.5f, 20, 30, 40))
LEGACY_SCALAR(cmpps9, "cmpps $9,", from_floats(__builtin_nanf(""), 1, 5, 2), from_floats(1, 2, 3, 4))

static volatile float half_inputs[4] = {0.0f, -0.0f, 1e-40f, 1.0f};

#define CONVERT_HALF(result, values, control, mxcsr)                                                          \
    do {                                                                                                     \
        unsigned mode = mxcsr;                                                                               \
        __m128i halves;                                                                                      \
        __asm__ volatile("ldmxcsr %2\n\tvcvtps2ph $" #control ", %1, %0" : "=x"(halves) : "x"(values), "m"(mode) : "memory"); \
        _mm_storel_epi64((__m128i*)&result, halves);                                                         \
    } while (0)

[[gnu::noinline]] static void half_conversions(void) {
    const __m128 values = _mm_setr_ps(half_inputs[0], half_inputs[1], half_inputs[2], half_inputs[3]);
    u64 up, down, flushed;
    unsigned saved;
    __asm__ volatile("stmxcsr %0" : "=m"(saved) : : "memory");
    CONVERT_HALF(up, values, 2, saved & ~0x40u);
    CONVERT_HALF(down, values, 1, saved & ~0x40u);
    CONVERT_HALF(flushed, values, 2, saved | 0x40u);
    __asm__ volatile("ldmxcsr %0" : : "m"(saved) : "memory");
    report("cvtps2ph up", up);
    report("cvtps2ph down", down);
    report("cvtps2ph up with daz", flushed);
}

[[gnu::noinline]] static void legacy_cvtsi2sd(void) {
    __m128i result = from_doubles(16, 5);
    const long seven = 7;
    __asm__("cvtsi2sdq %1, %0" : "+x"(result) : "r"(seven));
    report_bytes("legacy cvtsi2sd", result);
}

AES128_TEST(VEX, "vex")
AES128_TEST(SSE, "sse")
CLMUL_TEST(VEX, "vex")
CLMUL_TEST(SSE, "sse")

struct Test {
    const char* name;
    u64 (*run)(void);
};

#define ENTRY(name) {#name, test_##name}

static const struct Test tests[] = {
    ENTRY(add8), ENTRY(add16), ENTRY(add32), ENTRY(add64), ENTRY(sub8), ENTRY(sub64), ENTRY(adds8), ENTRY(adds16), ENTRY(addsu8),
    ENTRY(subs16), ENTRY(subsu16), ENTRY(mullo16), ENTRY(mullo32), ENTRY(mulhi16), ENTRY(mulhiu16), ENTRY(mulu32), ENTRY(mul32),
    ENTRY(madd16), ENTRY(min8), ENTRY(minu16), ENTRY(max32), ENTRY(maxu32), ENTRY(cmpeq8), ENTRY(cmpeq64), ENTRY(cmpgt16),
    ENTRY(cmpgt64), ENTRY(avg8), ENTRY(avg16), ENTRY(andnot), ENTRY(abs8), ENTRY(abs32), ENTRY(slli16), ENTRY(slli64), ENTRY(srli32),
    ENTRY(srai16), ENTRY(srai32big), ENTRY(sll32), ENTRY(srl64big), ENTRY(sra16), ENTRY(sllv32), ENTRY(srlv64), ENTRY(srav32),
    ENTRY(bslli), ENTRY(bsrli), ENTRY(shuffle32), ENTRY(shufflelo), ENTRY(shufflehi), ENTRY(shufps), ENTRY(unpacklo8),
    ENTRY(unpackhi16), ENTRY(unpacklo32), ENTRY(unpackhi64), ENTRY(packs16), ENTRY(packs32), ENTRY(packus16), ENTRY(packus32),
    ENTRY(alignr), ENTRY(alignr20), ENTRY(shuffle8), ENTRY(permute4x64), ENTRY(permutevar8x32), ENTRY(permute2x128),
    ENTRY(permute2x128zero), ENTRY(inserti128), ENTRY(blend32), ENTRY(blend16), ENTRY(blendv8), ENTRY(cvt8to16), ENTRY(cvt8to64s),
    ENTRY(cvt16to32s), ENTRY(cvt32to64), ENTRY(movemask), ENTRY(movemaskps), ENTRY(testz), ENTRY(broadcast),
    ENTRY(add16x), ENTRY(sub32x), ENTRY(shuffle8x), ENTRY(unpacklo8x), ENTRY(packus16x), ENTRY(alignrx), ENTRY(slli32x),
    ENTRY(srli64x), ENTRY(bsrlix), ENTRY(shuffle32x), ENTRY(blendvx), ENTRY(extract), ENTRY(insert), ENTRY(extracti128),
    ENTRY(cvt16to64x), ENTRY(min32ux), ENTRY(cmpeq32x), ENTRY(movemaskx),
    ENTRY(maddubs16), ENTRY(mulhrs16), ENTRY(hadd32), ENTRY(hadds16), ENTRY(hsub16), ENTRY(hsubs16), ENTRY(sad8), ENTRY(testps),
    ENTRY(testpd), ENTRY(maddubs16x), ENTRY(mulhrs16x), ENTRY(hadd16x), ENTRY(hsub32x), ENTRY(sad8x),
};

int main(void) {
    for (u64 i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) report(tests[i].name, tests[i].run());
    aes128_VEX();
    aes128_SSE();
    clmul_VEX();
    clmul_SSE();
    sha256_abc();
    sha1_abc();
    legacy_addss();
    legacy_maxss();
    legacy_sqrtsd();
    legacy_cmpltss();
    legacy_cvtss2sd();
    legacy_cvtsd2ss();
    legacy_roundss();
    legacy_cmpps9();
    legacy_cvtsi2sd();
    half_conversions();
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
