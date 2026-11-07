#include "host.h"

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef long long v2 [[gnu::vector_size(16)]];

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

static u64 seed = 0x9e3779b97f4a7c15ul;

static u64 next(void) {
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

static void mix(u64* hash, u64 value) {
    for (int i = 0; i < 8; ++i) *hash = (*hash ^ ((value >> (8 * i)) & 0xff)) * 0x100000001b3ul;
}

static const u8 alphabet[16] = {0, 'a', 'b', 'c', 'z', 0x80, 0xff, 0x7f, 'a', 'b', 0x01, 0xfe, 'a', 'c', 0x81, 'b'};

static v2 random_vector(void) {
    v2 value;
    u8 bytes[16];
    const u64 bits = next();
    for (int i = 0; i < 16; ++i) bytes[i] = (next() & 7) == 0 ? 0 : alphabet[1 + ((bits >> (4 * i)) & 15) % 15];
    memcpy(&value, bytes, 16);
    return value;
}

#define FLAGS "lahf\n\tseto %%al\n\tmovzwl %%ax, %k[flags]"
#define CONTROLS(X) X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31) X(32) X(33) X(34) X(35) X(36) X(37) X(38) X(39) X(40) X(41) X(42) X(43) X(44) X(45) X(46) X(47) X(48) X(49) X(50) X(51) X(52) X(53) X(54) X(55) X(56) X(57) X(58) X(59) X(60) X(61) X(62) X(63) X(64) X(65) X(66) X(67) X(68) X(69) X(70) X(71) X(72) X(73) X(74) X(75) X(76) X(77) X(78) X(79) X(80) X(81) X(82) X(83) X(84) X(85) X(86) X(87) X(88) X(89) X(90) X(91) X(92) X(93) X(94) X(95) X(96) X(97) X(98) X(99) X(100) X(101) X(102) X(103) X(104) X(105) X(106) X(107) X(108) X(109) X(110) X(111) X(112) X(113) X(114) X(115) X(116) X(117) X(118) X(119) X(120) X(121) X(122) X(123) X(124) X(125) X(126) X(127)

#define DEFINE_COMPARES(prefix, name)                                                                                       \
    static u64 name##_index_implicit(int control, v2 a, v2 b) {                                                         \
        u64 index = 0, flags = 0;                                                                                       \
        switch (control) {                                                                                              \
            CONTROLS(name##_INDEX_IMPLICIT)                                                                             \
        }                                                                                                               \
        return index << 32 | flags;                                                                           \
    }                                                                                                                   \
    static u64 name##_index_explicit(int control, v2 a, v2 b, int la, int lb) {                                         \
        u64 index = 0, flags = 0;                                                                                       \
        switch (control) {                                                                                              \
            CONTROLS(name##_INDEX_EXPLICIT)                                                                             \
        }                                                                                                               \
        return index << 32 | flags;                                                                           \
    }                                                                                                                   \
    static u64 name##_mask_implicit(int control, v2 a, v2 b, v2* mask) {                                                \
        u64 flags = 0;                                                                                                  \
        switch (control) {                                                                                              \
            CONTROLS(name##_MASK_IMPLICIT)                                                                              \
        }                                                                                                               \
        return flags;                                                                                           \
    }                                                                                                                   \
    static u64 name##_mask_explicit(int control, v2 a, v2 b, int la, int lb, v2* mask) {                                \
        u64 flags = 0;                                                                                                  \
        switch (control) {                                                                                              \
            CONTROLS(name##_MASK_EXPLICIT)                                                                              \
        }                                                                                                               \
        return flags;                                                                                           \
    }

#define legacy_INDEX_IMPLICIT(c) case c: __asm__ volatile("xor %%ecx, %%ecx\n\tpcmpistri $" #c ", %[b], %[a]\n\t" FLAGS : "=&c"(index), [flags] "=r"(flags) : [a] "x"(a), [b] "x"(b) : "rax", "cc", "memory"); break;
#define legacy_INDEX_EXPLICIT(c) case c: __asm__ volatile("xor %%ecx, %%ecx\n\tpcmpestri $" #c ", %[b], %[a]\n\t" FLAGS : "=&c"(index), [flags] "=r"(flags), "+a"(la) : [a] "x"(a), [b] "x"(b), "d"(lb) : "cc", "memory"); break;
#define legacy_MASK_IMPLICIT(c) case c: __asm__ volatile("pcmpistrm $" #c ", %[b], %[a]\n\t" FLAGS : "=Yz"(*mask), [flags] "=r"(flags) : [a] "x"(a), [b] "x"(b) : "rax", "cc", "memory"); break;
#define legacy_MASK_EXPLICIT(c) case c: __asm__ volatile("pcmpestrm $" #c ", %[b], %[a]\n\t" FLAGS : "=Yz"(*mask), [flags] "=r"(flags), "+a"(la) : [a] "x"(a), [b] "x"(b), "d"(lb) : "cc", "memory"); break;

DEFINE_COMPARES(, legacy)

#ifdef __AVX__
#define vex_INDEX_IMPLICIT(c) case c: __asm__ volatile("xor %%ecx, %%ecx\n\tvpcmpistri $" #c ", %[b], %[a]\n\t" FLAGS : "=&c"(index), [flags] "=r"(flags) : [a] "x"(a), [b] "x"(b) : "rax", "cc", "memory"); break;
#define vex_INDEX_EXPLICIT(c) case c: __asm__ volatile("xor %%ecx, %%ecx\n\tvpcmpestri $" #c ", %[b], %[a]\n\t" FLAGS : "=&c"(index), [flags] "=r"(flags), "+a"(la) : [a] "x"(a), [b] "x"(b), "d"(lb) : "cc", "memory"); break;
#define vex_MASK_IMPLICIT(c) case c: __asm__ volatile("vpcmpistrm $" #c ", %[b], %[a]\n\t" FLAGS : "=Yz"(*mask), [flags] "=r"(flags) : [a] "x"(a), [b] "x"(b) : "rax", "cc", "memory"); break;
#define vex_MASK_EXPLICIT(c) case c: __asm__ volatile("vpcmpestrm $" #c ", %[b], %[a]\n\t" FLAGS : "=Yz"(*mask), [flags] "=r"(flags), "+a"(la) : [a] "x"(a), [b] "x"(b), "d"(lb) : "cc", "memory"); break;

DEFINE_COMPARES(v, vex)
#endif

enum { Cases = 64 };

int main(void) {
    u64 index_implicit = 0xcbf29ce484222325ul, index_explicit = index_implicit, mask_implicit = index_implicit, mask_explicit = index_implicit;
#ifdef __AVX__
    u64 vex_agrees = 1;
#endif
    for (int control = 0; control < 128; ++control) {
        for (int i = 0; i < Cases; ++i) {
            const v2 a = random_vector(), b = random_vector();
            const int la = (int)(next() % 41) - 20, lb = (int)(next() % 41) - 20;
            v2 mask, mask_explicit_value;
            const u64 ii = legacy_index_implicit(control, a, b);
            const u64 ie = legacy_index_explicit(control, a, b, la, lb);
            const u64 mi = legacy_mask_implicit(control, a, b, &mask);
            const u64 me = legacy_mask_explicit(control, a, b, la, lb, &mask_explicit_value);
            mix(&index_implicit, ii);
            mix(&index_explicit, ie);
            mix(&mask_implicit, mi);
            mix(&mask_implicit, (u64)mask[0]);
            mix(&mask_implicit, (u64)mask[1]);
            mix(&mask_explicit, me);
            mix(&mask_explicit, (u64)mask_explicit_value[0]);
            mix(&mask_explicit, (u64)mask_explicit_value[1]);
#ifdef __AVX__
            v2 vex_implicit, vex_explicit;
            vex_agrees &= vex_index_implicit(control, a, b) == ii;
            vex_agrees &= vex_index_explicit(control, a, b, la, lb) == ie;
            vex_agrees &= vex_mask_implicit(control, a, b, &vex_implicit) == mi && vex_implicit[0] == mask[0] && vex_implicit[1] == mask[1];
            vex_agrees &= vex_mask_explicit(control, a, b, la, lb, &vex_explicit) == me && vex_explicit[0] == mask_explicit_value[0] &&
                          vex_explicit[1] == mask_explicit_value[1];
#endif
        }
    }
    line("pcmpistri", index_implicit);
    line("pcmpestri", index_explicit);
    line("pcmpistrm", mask_implicit);
    line("pcmpestrm", mask_explicit);
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
