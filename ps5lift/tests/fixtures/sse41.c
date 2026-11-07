#include "host.h"

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned char u8;
typedef float v4 [[gnu::vector_size(16)]];
typedef double v2 [[gnu::vector_size(16)]];
typedef float v8 [[gnu::vector_size(32)]];

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

static void line(const char* name, u64 value) {
    print(name);
    print(" ");
    print_hex(value);
    print("\n");
}

static u64 seed = 0x2545f4914f6cdd1dul;

static u64 next(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

static void mix(u64* hash, const void* data, int size) {
    const u8* bytes = data;
    for (int i = 0; i < size; ++i) *hash = (*hash ^ bytes[i]) * 0x100000001b3ul;
}

static const u32 special_floats[16] = {0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x7f800000, 0xff800000, 0x7fc00000, 0x7fa00001,
                                       0xffc12345, 0x00000001, 0x80400000, 0x7f7fffff, 0x0b800000, 0x5f000000, 0x3eaaaaab, 0x40490fdb};
static const u64 special_doubles[16] = {0x0ul, 0x8000000000000000ul, 0x3ff0000000000000ul, 0xbff0000000000000ul, 0x7ff0000000000000ul, 0xfff0000000000000ul,
                                        0x7ff8000000000000ul, 0x7ff4000000000001ul, 0xfff8000000012345ul, 0x1ul, 0x8008000000000000ul, 0x7fefffffffffffffull,
                                        0x0170000000000000ul, 0x5ff0000000000000ul, 0x3fd5555555555555ul, 0x400921fb54442d18ul};

static u32 random_float(void) {
    const u64 r = next();
    if (r & 1) return special_floats[(r >> 1) & 15];
    return (u32)((r >> 8) & 0x807fffff) | (u32)(107 + ((r >> 40) % 40)) << 23;
}

static u64 random_double(void) {
    const u64 r = next();
    if (r & 1) return special_doubles[(r >> 1) & 15];
    return ((next() & 0x800fffffffffffful)) | (u64)(1003 + ((r >> 40) % 40)) << 52;
}

static v4 random_floats(void) {
    u32 bits[4];
    for (int i = 0; i < 4; ++i) bits[i] = random_float();
    v4 value;
    memcpy(&value, bits, 16);
    return value;
}

static v2 random_doubles(void) {
    u64 bits[2];
    for (int i = 0; i < 2; ++i) bits[i] = random_double();
    v2 value;
    memcpy(&value, bits, 16);
    return value;
}

static void set_mxcsr(u32 value) {
    __asm__ volatile("ldmxcsr %0" : : "m"(value) : "memory");
}

static int same(const void* a, const void* b, int size) {
    const u8* x = a;
    const u8* y = b;
    for (int i = 0; i < size; ++i) {
        if (x[i] != y[i]) return 0;
    }
    return 1;
}

#define CONTROLS(X) X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31) X(32) X(33) X(34) X(35) X(36) X(37) X(38) X(39) X(40) X(41) X(42) X(43) X(44) X(45) X(46) X(47) X(48) X(49) X(50) X(51) X(52) X(53) X(54) X(55) X(56) X(57) X(58) X(59) X(60) X(61) X(62) X(63) X(64) X(65) X(66) X(67) X(68) X(69) X(70) X(71) X(72) X(73) X(74) X(75) X(76) X(77) X(78) X(79) X(80) X(81) X(82) X(83) X(84) X(85) X(86) X(87) X(88) X(89) X(90) X(91) X(92) X(93) X(94) X(95) X(96) X(97) X(98) X(99) X(100) X(101) X(102) X(103) X(104) X(105) X(106) X(107) X(108) X(109) X(110) X(111) X(112) X(113) X(114) X(115) X(116) X(117) X(118) X(119) X(120) X(121) X(122) X(123) X(124) X(125) X(126) X(127) X(128) X(129) X(130) X(131) X(132) X(133) X(134) X(135) X(136) X(137) X(138) X(139) X(140) X(141) X(142) X(143) X(144) X(145) X(146) X(147) X(148) X(149) X(150) X(151) X(152) X(153) X(154) X(155) X(156) X(157) X(158) X(159) X(160) X(161) X(162) X(163) X(164) X(165) X(166) X(167) X(168) X(169) X(170) X(171) X(172) X(173) X(174) X(175) X(176) X(177) X(178) X(179) X(180) X(181) X(182) X(183) X(184) X(185) X(186) X(187) X(188) X(189) X(190) X(191) X(192) X(193) X(194) X(195) X(196) X(197) X(198) X(199) X(200) X(201) X(202) X(203) X(204) X(205) X(206) X(207) X(208) X(209) X(210) X(211) X(212) X(213) X(214) X(215) X(216) X(217) X(218) X(219) X(220) X(221) X(222) X(223) X(224) X(225) X(226) X(227) X(228) X(229) X(230) X(231) X(232) X(233) X(234) X(235) X(236) X(237) X(238) X(239) X(240) X(241) X(242) X(243) X(244) X(245) X(246) X(247) X(248) X(249) X(250) X(251) X(252) X(253) X(254) X(255)

#define DPPS(c) case c: __asm__ volatile("dpps $" #c ", %[b], %[a]" : [a] "+x"(a) : [b] "x"(b)); break;
#define DPPD(c) case c: __asm__ volatile("dppd $" #c ", %[b], %[a]" : [a] "+x"(a) : [b] "x"(b)); break;

[[gnu::noinline]] static v4 dpps(int control, v4 a, v4 b) {
    switch (control) { CONTROLS(DPPS) }
    return a;
}

[[gnu::noinline]] static v2 dppd(int control, v2 a, v2 b) {
    switch (control) { CONTROLS(DPPD) }
    return a;
}

#ifdef __AVX__
#define VDPPS(c) case c: __asm__ volatile("vdpps $" #c ", %[b], %[a], %[r]" : [r] "=x"(r) : [a] "x"(a), [b] "x"(b)); break;
#define VDPPS_YMM(c) case c: __asm__ volatile("vdpps $" #c ", %[b], %[a], %[r]" : [r] "=x"(r) : [a] "x"(a), [b] "x"(b)); break;
#define VDPPD(c) case c: __asm__ volatile("vdppd $" #c ", %[b], %[a], %[r]" : [r] "=x"(r) : [a] "x"(a), [b] "x"(b)); break;

[[gnu::noinline]] static v4 vdpps(int control, v4 a, v4 b) {
    v4 r = a;
    switch (control) { CONTROLS(VDPPS) }
    return r;
}

[[gnu::noinline]] static v8 vdpps_ymm(int control, v8 a, v8 b) {
    v8 r = a;
    switch (control) { CONTROLS(VDPPS_YMM) }
    return r;
}

[[gnu::noinline]] static v2 vdppd(int control, v2 a, v2 b) {
    v2 r = a;
    switch (control) { CONTROLS(VDPPD) }
    return r;
}
#endif

enum { Cases = 24 };

static int several_special(const float* a, const float* b, int count, int control) {
    int special = 0;
    for (int i = 0; i < count; ++i) {
        if (!((control >> (4 + i)) & 1)) continue;
        const float product = a[i] * b[i];
        special += product != product || product - product != 0;
    }
    return special >= 2;
}

static int several_special_doubles(const double* a, const double* b, int control) {
    int special = 0;
    for (int i = 0; i < 2; ++i) {
        if (!((control >> (4 + i)) & 1)) continue;
        const double product = a[i] * b[i];
        special += product != product || product - product != 0;
    }
    return special >= 2;
}

static void canonical_floats(v4* value) {
    for (int i = 0; i < 4; ++i) {
        if ((*value)[i] != (*value)[i]) {
            const u32 nan = 0x7fc00000;
            memcpy((char*)value + 4 * i, &nan, 4);
        }
    }
}

static void canonical_doubles(v2* value) {
    for (int i = 0; i < 2; ++i) {
        if ((*value)[i] != (*value)[i]) {
            const u64 nan = 0x7ff8000000000000ul;
            memcpy((char*)value + 8 * i, &nan, 8);
        }
    }
}

int main(void) {
    static const u32 modes[2] = {0x1f80, 0x9fc0};
    u64 single = 0xcbf29ce484222325ul, pair = single;
#ifdef __AVX__
    u64 vex_agrees = 1;
#endif
    for (int mode = 0; mode < 2; ++mode) {
        set_mxcsr(modes[mode]);
        for (int control = 0; control < 256; ++control) {
            for (int i = 0; i < Cases; ++i) {
                const v4 a = random_floats(), b = random_floats();
                const v4 high_a = random_floats(), high_b = random_floats();
                const v2 c = random_doubles(), d = random_doubles();
                const v4 product = dpps(control, a, b);
                const v2 product2 = dppd(control, c, d);
                float fa[4], fb[4];
                double da[2], db[2];
                memcpy(fa, &a, 16);
                memcpy(fb, &b, 16);
                memcpy(da, &c, 16);
                memcpy(db, &d, 16);
                v4 hashed = product;
                v2 hashed2 = product2;
                if (several_special(fa, fb, 4, control)) canonical_floats(&hashed);
                if (several_special_doubles(da, db, control)) canonical_doubles(&hashed2);
                mix(&single, &hashed, 16);
                mix(&pair, &hashed2, 16);
#ifdef __AVX__
                const v4 high = dpps(control, high_a, high_b);
                v8 wide_a, wide_b;
                memcpy(&wide_a, &a, 16);
                memcpy((char*)&wide_a + 16, &high_a, 16);
                memcpy(&wide_b, &b, 16);
                memcpy((char*)&wide_b + 16, &high_b, 16);
                const v4 narrow = vdpps(control, a, b);
                const v8 wide = vdpps_ymm(control, wide_a, wide_b);
                const v2 narrow2 = vdppd(control, c, d);
                vex_agrees &= same(&narrow, &product, 16) && same(&wide, &product, 16) && same((const char*)&wide + 16, &high, 16) && same(&narrow2, &product2, 16);
#else
                (void)high_a;
                (void)high_b;
#endif
            }
        }
    }
    set_mxcsr(0x1f80);
    line("dpps", single);
    line("dppd", pair);
#ifdef __AVX__
    line("vex agrees", vex_agrees);
#endif
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
