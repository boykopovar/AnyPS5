#include "host.h"

typedef unsigned long u64;
typedef unsigned int u32;

static u64 length_of(const char* text) {
    u64 length = 0;
    while (text[length]) ++length;
    return length;
}

static void print(const char* text) {
    host_write(text, length_of(text));
}

static void print_hex(u32 value) {
    static const char digits[] = "0123456789abcdef";
    char buffer[11];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 8; ++i) buffer[2 + i] = digits[(value >> (28 - 4 * i)) & 0xf];
    buffer[10] = 0;
    print(buffer);
}

static void identify(u32 leaf, u32 subleaf) {
    u32 eax, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(leaf), "c"(subleaf));
    print("cpuid ");
    print_hex(leaf);
    print(".");
    print_hex(subleaf);
    print(" =");
    const u32 values[] = {eax, ebx, ecx, edx};
    for (int i = 0; i < 4; ++i) {
        print(" ");
        print_hex(values[i]);
    }
    print("\n");
}

int main(void) {
    static const u32 queries[][2] = {{0x0, 0}, {0x1, 0}, {0x7, 0}, {0x7, 1}, {0xd, 0}, {0xd, 1}, {0xd, 2}, {0x80000000, 0}, {0x80000001, 0}, {0x80000008, 0}, {0x20, 0}};
    for (u64 i = 0; i < sizeof(queries) / sizeof(queries[0]); ++i) identify(queries[i][0], queries[i][1]);
    u32 low, high;
    __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
    print("xgetbv 0 = ");
    print_hex(high);
    print(" ");
    print_hex(low);
    print("\n");
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
