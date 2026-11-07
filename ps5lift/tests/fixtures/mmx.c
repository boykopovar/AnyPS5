#include "host.h"

typedef unsigned long u64;
typedef long i64;
typedef unsigned short u16;

static u64 length_of(const char* text) {
    u64 length = 0;
    while (text[length]) ++length;
    return length;
}

static void print(const char* text) {
    host_write(text, length_of(text));
}

static void line_hex(const char* name, u64 value) {
    static const char digits[] = "0123456789abcdef";
    char buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 16; ++i) buffer[2 + i] = digits[(value >> (60 - 4 * i)) & 0xf];
    buffer[18] = 0;
    print(name);
    print(" = ");
    print(buffer);
    print("\n");
}

#define BINARY(op, a, b)                                                                                             \
    ({                                                                                                               \
        u64 result;                                                                                                  \
        __asm__("movq %1, %%mm0\n\tmovq %2, %%mm1\n\t" op " %%mm1, %%mm0\n\tmovq %%mm0, %0\n\temms"                  \
                : "=r"(result) : "r"(a), "r"(b) : "mm0", "mm1");                                                     \
        result;                                                                                                      \
    })

#define IMMEDIATE(op, a, count)                                                                                      \
    ({                                                                                                               \
        u64 result;                                                                                                  \
        __asm__("movq %1, %%mm0\n\t" op " $" #count ", %%mm0\n\tmovq %%mm0, %0\n\temms" : "=r"(result) : "r"(a) : "mm0"); \
        result;                                                                                                      \
    })

#define ALIGN(a, b, count)                                                                                           \
    ({                                                                                                               \
        u64 result;                                                                                                  \
        __asm__("movq %1, %%mm0\n\tmovq %2, %%mm1\n\tpalignr $" #count ", %%mm1, %%mm0\n\tmovq %%mm0, %0\n\temms"      \
                : "=r"(result) : "r"(a), "r"(b) : "mm0", "mm1");                                                     \
        result;                                                                                                      \
    })

static const u64 a = 0x80017fff0100ff80UL;
static const u64 b = 0x7fff8000fffe0081UL;
static const u64 selector = 0x8007060504030201UL;

static void arithmetic(void) {
    line_hex("paddb", BINARY("paddb", a, b));
    line_hex("paddw", BINARY("paddw", a, b));
    line_hex("paddd", BINARY("paddd", a, b));
    line_hex("paddsw", BINARY("paddsw", a, b));
    line_hex("paddusb", BINARY("paddusb", a, b));
    line_hex("paddusw", BINARY("paddusw", a, b));
    line_hex("psubb", BINARY("psubb", a, b));
    line_hex("psubw", BINARY("psubw", a, b));
    line_hex("psubusb", BINARY("psubusb", a, b));
    line_hex("pmullw", BINARY("pmullw", a, b));
    line_hex("pmulhw", BINARY("pmulhw", a, b));
    line_hex("pmaddwd", BINARY("pmaddwd", a, b));
    line_hex("pavgb", BINARY("pavgb", a, b));
    line_hex("pavgw", BINARY("pavgw", a, b));
    line_hex("pminsw", BINARY("pminsw", a, b));
    line_hex("pmaxsw", BINARY("pmaxsw", a, b));
    line_hex("pmulhrsw", BINARY("pmulhrsw", a, b));
    line_hex("pmaddubsw", BINARY("pmaddubsw", a, b));
    line_hex("phaddw", BINARY("phaddw", a, b));
    line_hex("phsubsw", BINARY("phsubsw", a, b));
    line_hex("psadbw", BINARY("psadbw", a, b));
}

static void logic_and_compares(void) {
    line_hex("pand", BINARY("pand", a, b));
    line_hex("pandn", BINARY("pandn", a, b));
    line_hex("por", BINARY("por", a, b));
    line_hex("pxor", BINARY("pxor", a, b));
    line_hex("pcmpeqw", BINARY("pcmpeqw", a, a ^ 0xffff0000UL));
    line_hex("pcmpeqd", BINARY("pcmpeqd", a, a ^ 0x1UL));
    line_hex("pcmpgtw", BINARY("pcmpgtw", a, b));
}

static void shuffles(void) {
    line_hex("punpcklbw", BINARY("punpcklbw", a, b));
    line_hex("punpckhbw", BINARY("punpckhbw", a, b));
    line_hex("punpcklwd", BINARY("punpcklwd", a, b));
    line_hex("punpckhwd", BINARY("punpckhwd", a, b));
    line_hex("punpckldq", BINARY("punpckldq", a, b));
    line_hex("punpckhdq", BINARY("punpckhdq", a, b));
    line_hex("packuswb", BINARY("packuswb", a, b));
    line_hex("packssdw", BINARY("packssdw", a, b));
    line_hex("pshufb", BINARY("pshufb", a, selector));
    u64 shuffled;
    __asm__("movq %1, %%mm1\n\tpshufw $0x1b, %%mm1, %%mm0\n\tmovq %%mm0, %0\n\temms" : "=r"(shuffled) : "r"(a) : "mm0", "mm1");
    line_hex("pshufw 0x1b", shuffled);
    line_hex("palignr 3", ALIGN(a, b, 3));
    line_hex("palignr 8", ALIGN(a, b, 8));
    line_hex("palignr 13", ALIGN(a, b, 13));
    line_hex("palignr 20", ALIGN(a, b, 20));
}

static void shifts(void) {
    line_hex("psllw 4", BINARY("psllw", a, 4UL));
    line_hex("psrlw 4", BINARY("psrlw", a, 4UL));
    line_hex("psraw 4", BINARY("psraw", a, 4UL));
    line_hex("pslld 4", BINARY("pslld", a, 4UL));
    line_hex("psrad 4", BINARY("psrad", a, 4UL));
    line_hex("psllq 4", BINARY("psllq", a, 4UL));
    line_hex("psrlq 4", BINARY("psrlq", a, 4UL));
    line_hex("psrlw 16", BINARY("psrlw", a, 16UL));
    line_hex("psraw 99", BINARY("psraw", a, 99UL));
    line_hex("psraw $3", IMMEDIATE("psraw", a, 3));
    line_hex("psllw $1", IMMEDIATE("psllw", a, 1));
    line_hex("psrlq $12", IMMEDIATE("psrlq", a, 12));
    line_hex("psllq $40", IMMEDIATE("psllq", a, 40));
}

static void moves(void) {
    u64 through32;
    __asm__("movq %1, %%mm2\n\tmovd %k2, %%mm2\n\tmovq %%mm2, %0\n\temms" : "=r"(through32) : "r"(a), "r"(0xdeadbeefcafef00dUL) : "mm2");
    line_hex("movd to mm", through32);
    unsigned int low;
    __asm__("movq %1, %%mm3\n\tmovd %%mm3, %0\n\temms" : "=r"(low) : "r"(a) : "mm3");
    line_hex("movd from mm", low);
    static u64 source = 0x0123456789abcdefUL;
    u64 target = 0;
    __asm__("movq %1, %%mm4\n\tmovntq %%mm4, %0\n\temms" : "=m"(target) : "m"(source) : "mm4");
    line_hex("movq load and movntq", target);
}

static void aliasing(void) {
    struct {
        u64 significand;
        u16 exponent;
    } stored;
    __asm__("movq %1, %%mm0\n\tfstpt %0\n\temms" : "=m"(stored) : "r"(0x8123456789abcdefUL) : "mm0");
    line_hex("x87 sees mm0 significand", stored.significand);
    line_hex("x87 sees mm0 exponent", stored.exponent);
    volatile long double half = 1.5L;
    volatile long double quarter = 2.25L;
    line_hex("x87 after emms", (u64)((half + quarter) * 4));
}

int main(void) {
    arithmetic();
    logic_and_compares();
    shuffles();
    shifts();
    moves();
    aliasing();
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
