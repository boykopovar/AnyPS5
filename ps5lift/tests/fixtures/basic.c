#include "host.h"

typedef unsigned long u64;
typedef long i64;
typedef unsigned int u32;
typedef int i32;
typedef unsigned short u16;
typedef short i16;
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

static void print_unsigned(u64 value) {
    char buffer[32];
    int position = 31;
    buffer[position] = 0;
    do {
        buffer[--position] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    print(buffer + position);
}

static void print_signed(i64 value) {
    if (value < 0) {
        print("-");
        print_unsigned((u64)0 - (u64)value);
    } else {
        print_unsigned((u64)value);
    }
}

static void print_hex(u64 value) {
    static const char digits[] = "0123456789abcdef";
    char buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 16; ++i) buffer[2 + i] = digits[(value >> (60 - 4 * i)) & 0xf];
    buffer[18] = 0;
    print(buffer);
}

static void line(const char* name, i64 value) {
    print(name);
    print(" = ");
    print_signed(value);
    print("\n");
}

static void line_hex(const char* name, u64 value) {
    print(name);
    print(" = ");
    print_hex(value);
    print("\n");
}

[[gnu::noinline]] static u64 fibonacci(u32 n) {
    return n < 2 ? n : fibonacci(n - 1) + fibonacci(n - 2);
}

[[gnu::noinline]] static i64 add(i64 a, i64 b) { return a + b; }
[[gnu::noinline]] static i64 subtract(i64 a, i64 b) { return a - b; }
[[gnu::noinline]] static i64 multiply(i64 a, i64 b) { return a * b; }
[[gnu::noinline]] static i64 divide(i64 a, i64 b) { return a / b; }

static i64 (*volatile operations[])(i64, i64) = {add, subtract, multiply, divide};

[[gnu::noinline]] static i64 many_arguments(i64 a, i64 b, i64 c, i64 d, i64 e, i64 f, i64 g, i64 h, i64 i) {
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h + 9 * i;
}

[[gnu::noinline]] static const char* classify(int value) {
    switch (value) {
    case 0: return "zero";
    case 1: return "one";
    case 2: return "two";
    case 3: return "three";
    case 4: return "four";
    case 5: return "five";
    case 6: return "six";
    case 7: return "seven";
    default: return "many";
    }
}

i64 table_index_copied_after_compare(i64 value);
i64 table_index_copied_before_compare(i64 value);
i64 table_base_before_loop(i64 value);
i64 overlapping_exchange(volatile u64* word, u64 expected, u64 desired, i64 skip_lock);
i64 tail_ping(i64 count);
i64 jump_falls_into_next(i64 value);
i64 next_after_jump(i64 value);
void store_address_taken_by_lea(i64 (**slot)(void));
i64 pass_address_taken_by_lea(void);
void store_address_taken_long_before(i64 (**slot)(void));
i64 call_address_taken_before_nop(void);
i64 (*return_address_taken_by_lea(void))(void);
i64 push_address_taken_by_lea(void);
i64 copy_address_taken_by_lea(void);
i64 (*select_address_taken_by_lea(i64 pick))(void);
i64 tail_pass_address_taken_by_lea(void);
void store_address_past_jump(i64 (**slot)(void));
void store_address_after_call_and_branch(i64 (**slot)(void));
void store_address_through_vector(i64 (**slots)(void));
const char* options_string_through_lea(i64 wide);

[[gnu::noinline]] i64 call_function_pointer(i64 (*function)(void)) {
    return function();
}

[[gnu::noinline]] i64 call_seventh_argument(i64 a, i64 b, i64 c, i64 d, i64 e, i64 f, i64 (*function)(void)) {
    (void)a, (void)b, (void)c, (void)d, (void)e, (void)f;
    return function();
}

__asm__(".text\n"
        ".globl table_index_copied_after_compare\n"
        ".type table_index_copied_after_compare, @function\n"
        "table_index_copied_after_compare:\n"
        "    cmp $2, %edi\n"
        "    ja 3f\n"
        "    mov %edi, %eax\n"
        "    lea 4f(%rip), %rcx\n"
        "    movslq (%rcx,%rax,4), %rax\n"
        "    add %rcx, %rax\n"
        "    jmp *%rax\n"
        "0:  mov $100, %eax\n"
        "    ret\n"
        "1:  mov $101, %eax\n"
        "    ret\n"
        "2:  mov $102, %eax\n"
        "    ret\n"
        "3:  mov $-1, %rax\n"
        "    ret\n"
        "5:  .byte 0xff, 0xff\n"
        ".size table_index_copied_after_compare, .-table_index_copied_after_compare\n"
        ".pushsection .rodata\n"
        ".p2align 2\n"
        "4:  .long 0b-4b, 1b-4b, 2b-4b, 5b-4b\n"
        ".popsection\n"
        ".globl table_index_copied_before_compare\n"
        ".type table_index_copied_before_compare, @function\n"
        "table_index_copied_before_compare:\n"
        "    mov %edi, %edx\n"
        "    cmp $2, %edi\n"
        "    ja 3f\n"
        "    mov %edx, %eax\n"
        "    lea 4f(%rip), %rcx\n"
        "    movslq (%rcx,%rax,4), %rax\n"
        "    add %rcx, %rax\n"
        "    jmp *%rax\n"
        "0:  mov $200, %eax\n"
        "    ret\n"
        "1:  mov $201, %eax\n"
        "    ret\n"
        "2:  mov $202, %eax\n"
        "    ret\n"
        "3:  mov $-1, %rax\n"
        "    ret\n"
        "5:  .byte 0xff, 0xff\n"
        ".size table_index_copied_before_compare, .-table_index_copied_before_compare\n"
        ".pushsection .rodata\n"
        ".p2align 2\n"
        "4:  .long 0b-4b, 1b-4b, 2b-4b, 5b-4b\n"
        ".popsection\n"
        ".globl table_base_before_loop\n"
        ".type table_base_before_loop, @function\n"
        "table_base_before_loop:\n"
        "    lea 9f(%rip), %r8\n"
        "    xor %eax, %eax\n"
        "    jmp 2f\n"
        "1:  mov %rcx, %rdi\n"
        "2:  cmp $3, %edi\n"
        "    ja 8f\n"
        "    mov %edi, %ecx\n"
        "    movslq (%r8,%rcx,4), %rcx\n"
        "    mov %rdi, %rdx\n"
        "    add %r8, %rcx\n"
        "    xor %edx, %edx\n"
        "    jmp *%rcx\n"
        "3:  ret\n"
        "4:  add $10, %rax\n"
        "    mov %r8, %r9\n"
        "    mov $3, %r8d\n"
        "    add %r8, %rax\n"
        "    mov %r9, %r8\n"
        "    mov $0, %ecx\n"
        "    jmp 1b\n"
        "5:  add $100, %rax\n"
        "    mov $7, %r8d\n"
        "    add %r8, %rax\n"
        "    lea 9f(%rip), %r8\n"
        "    mov $1, %ecx\n"
        "    jmp 1b\n"
        "6:  add $1000, %rax\n"
        "    mov $2, %ecx\n"
        "    jmp 1b\n"
        "8:  mov $-1, %rax\n"
        "    ret\n"
        ".size table_base_before_loop, .-table_base_before_loop\n"
        ".pushsection .rodata\n"
        ".p2align 2\n"
        "9:  .long 3b-9b, 4b-9b, 5b-9b, 6b-9b\n"
        ".popsection\n"
        ".text\n");

__asm__(".text\n"
        ".globl tail_ping\n"
        ".type tail_ping, @function\n"
        "tail_ping:\n"
        "    test %rdi, %rdi\n"
        "    jz 1f\n"
        "    dec %rdi\n"
        "    jmp tail_pong\n"
        "1:  mov $42, %eax\n"
        "    ret\n"
        ".size tail_ping, .-tail_ping\n"
        ".globl tail_pong\n"
        ".type tail_pong, @function\n"
        "tail_pong:\n"
        "    lea tail_ping(%rip), %rax\n"
        "    jmp *%rax\n"
        ".size tail_pong, .-tail_pong\n");

__asm__(".text\n"
        ".globl overlapping_exchange\n"
        ".type overlapping_exchange, @function\n"
        "overlapping_exchange:\n"
        "    mov %rsi, %rax\n"
        "    test %rcx, %rcx\n"
        "    jnz 0f\n"
        "    .byte 0xf0\n"
        "0:  cmpxchg %rdx, (%rdi)\n"
        "    setz %al\n"
        "    movzbl %al, %eax\n"
        "    ret\n"
        ".size overlapping_exchange, .-overlapping_exchange\n");

__asm__(".text\n"
        "1:  mov $100, %rax\n"
        "    ret\n"
        ".globl jump_falls_into_next\n"
        ".type jump_falls_into_next, @function\n"
        "jump_falls_into_next:\n"
        "    test %rdi, %rdi\n"
        "    jz 1b\n"
        ".size jump_falls_into_next, .-jump_falls_into_next\n"
        ".globl next_after_jump\n"
        ".type next_after_jump, @function\n"
        "next_after_jump:\n"
        "    lea 7(%rdi), %rax\n"
        "    ret\n"
        ".size next_after_jump, .-next_after_jump\n");

__asm__(".text\n"
        "stored_through_lea:\n"
        "    mov $77, %eax\n"
        "    ret\n"
        "passed_through_lea:\n"
        "    mov $78, %eax\n"
        "    ret\n"
        "stored_late_through_lea:\n"
        "    mov $79, %eax\n"
        "    ret\n"
        ".globl store_address_taken_long_before\n"
        ".type store_address_taken_long_before, @function\n"
        "store_address_taken_long_before:\n"
        "    lea stored_late_through_lea(%rip), %rax\n"
        "    .rept 24\n"
        "    xor %ecx, %ecx\n"
        "    .endr\n"
        "    mov %rax, (%rdi)\n"
        "    ret\n"
        ".size store_address_taken_long_before, .-store_address_taken_long_before\n"
        "called_after_nop_through_lea:\n"
        "    mov $80, %eax\n"
        "    ret\n"
        ".globl call_address_taken_before_nop\n"
        ".type call_address_taken_before_nop, @function\n"
        "call_address_taken_before_nop:\n"
        "    sub $8, %rsp\n"
        "    lea called_after_nop_through_lea(%rip), %rax\n"
        "    nopl 0x0(%rax,%rax,1)\n"
        "    call *%rax\n"
        "    add $8, %rsp\n"
        "    ret\n"
        ".size call_address_taken_before_nop, .-call_address_taken_before_nop\n"
        "returned_through_lea:\n"
        "    mov $81, %eax\n"
        "    ret\n"
        ".globl return_address_taken_by_lea\n"
        ".type return_address_taken_by_lea, @function\n"
        "return_address_taken_by_lea:\n"
        "    push %rbx\n"
        "    lea returned_through_lea(%rip), %rax\n"
        "    pop %rbx\n"
        "    ret\n"
        ".size return_address_taken_by_lea, .-return_address_taken_by_lea\n"
        "pushed_through_lea:\n"
        "    mov $82, %eax\n"
        "    ret\n"
        ".globl push_address_taken_by_lea\n"
        ".type push_address_taken_by_lea, @function\n"
        "push_address_taken_by_lea:\n"
        "    lea pushed_through_lea(%rip), %rax\n"
        "    push %rax\n"
        "    call call_seventh_argument\n"
        "    add $8, %rsp\n"
        "    ret\n"
        ".size push_address_taken_by_lea, .-push_address_taken_by_lea\n"
        "copied_through_lea:\n"
        "    mov $83, %eax\n"
        "    ret\n"
        ".globl copy_address_taken_by_lea\n"
        ".type copy_address_taken_by_lea, @function\n"
        "copy_address_taken_by_lea:\n"
        "    sub $8, %rsp\n"
        "    lea copied_through_lea(%rip), %rax\n"
        "    mov %rax, %rdi\n"
        "    call call_function_pointer\n"
        "    add $8, %rsp\n"
        "    ret\n"
        ".size copy_address_taken_by_lea, .-copy_address_taken_by_lea\n"
        "selected_through_lea:\n"
        "    mov $84, %eax\n"
        "    ret\n"
        ".globl select_address_taken_by_lea\n"
        ".type select_address_taken_by_lea, @function\n"
        "select_address_taken_by_lea:\n"
        "    xor %eax, %eax\n"
        "    lea selected_through_lea(%rip), %rdx\n"
        "    test %rdi, %rdi\n"
        "    cmovne %rdx, %rax\n"
        "    ret\n"
        ".size select_address_taken_by_lea, .-select_address_taken_by_lea\n"
        "tail_passed_through_lea:\n"
        "    mov $85, %eax\n"
        "    ret\n"
        ".globl tail_pass_address_taken_by_lea\n"
        ".type tail_pass_address_taken_by_lea, @function\n"
        "tail_pass_address_taken_by_lea:\n"
        "    lea tail_passed_through_lea(%rip), %rdi\n"
        "    jmp call_function_pointer\n"
        ".size tail_pass_address_taken_by_lea, .-tail_pass_address_taken_by_lea\n"
        "stored_past_jump_through_lea:\n"
        "    mov $86, %eax\n"
        "    ret\n"
        ".globl store_address_past_jump\n"
        ".type store_address_past_jump, @function\n"
        "store_address_past_jump:\n"
        "    lea stored_past_jump_through_lea(%rip), %rax\n"
        "    jmp 1f\n"
        "    ud2\n"
        "1:  mov %rax, (%rdi)\n"
        "    ret\n"
        ".size store_address_past_jump, .-store_address_past_jump\n"
        "stored_after_call_through_lea:\n"
        "    mov $87, %eax\n"
        "    ret\n"
        ".globl store_address_after_call_and_branch\n"
        ".type store_address_after_call_and_branch, @function\n"
        "store_address_after_call_and_branch:\n"
        "    push %rbx\n"
        "    push %r13\n"
        "    sub $8, %rsp\n"
        "    mov %rdi, %rbx\n"
        "    lea stored_after_call_through_lea(%rip), %r13\n"
        "    xor %edi, %edi\n"
        "    call next_after_jump\n"
        "    cmp $7, %rax\n"
        "    jne 1f\n"
        "    mov %r13, (%rbx)\n"
        "1:  add $8, %rsp\n"
        "    pop %r13\n"
        "    pop %rbx\n"
        "    ret\n"
        ".size store_address_after_call_and_branch, .-store_address_after_call_and_branch\n"
        "stored_through_vector_lea:\n"
        "    mov $88, %eax\n"
        "    ret\n"
        ".globl store_address_through_vector\n"
        ".type store_address_through_vector, @function\n"
        "store_address_through_vector:\n"
        "    lea stored_through_vector_lea(%rip), %rcx\n"
        "    vmovq %rcx, %xmm1\n"
        "    lea 8(%rdi), %rcx\n"
        "    vmovq %rcx, %xmm0\n"
        "    vpunpcklqdq %xmm0, %xmm1, %xmm0\n"
        "    vmovdqu %xmm0, (%rdi)\n"
        "    ret\n"
        ".size store_address_through_vector, .-store_address_through_vector\n"
        ".globl store_address_taken_by_lea\n"
        ".type store_address_taken_by_lea, @function\n"
        "store_address_taken_by_lea:\n"
        "    lea stored_through_lea(%rip), %rax\n"
        "    mov %rax, (%rdi)\n"
        "    ret\n"
        ".size store_address_taken_by_lea, .-store_address_taken_by_lea\n"
        ".globl pass_address_taken_by_lea\n"
        ".type pass_address_taken_by_lea, @function\n"
        "pass_address_taken_by_lea:\n"
        "    sub $8, %rsp\n"
        "    lea passed_through_lea(%rip), %rdi\n"
        "    call call_function_pointer\n"
        "    add $8, %rsp\n"
        "    ret\n"
        ".size pass_address_taken_by_lea, .-pass_address_taken_by_lea\n");

__asm__(".text\n"
        ".globl options_string_through_lea\n"
        ".type options_string_through_lea, @function\n"
        "options_string_through_lea:\n"
        "    lea 2f(%rip), %rax\n"
        "    test %rdi, %rdi\n"
        "    jz 1f\n"
        "    add $12, %rax\n"
        "1:  ret\n"
        "2:  .asciz \"aes(8x,int)\"\n"
        "    .asciz \"aes(16x,int)\"\n"
        ".size options_string_through_lea, .-options_string_through_lea\n");

static i64 flags_of(u8 carry, u8 overflow, u8 zero, u8 sign) {
    return carry | overflow << 1 | zero << 2 | sign << 3;
}

[[gnu::noinline]] static void zen2_integer(void) {
    u64 value = 0x0123456789abcdefUL;
    u64 swapped64 = value;
    __asm__("bswap %0" : "+r"(swapped64));
    line_hex("bswap 64", swapped64);
    u32 swapped32 = 0x01234567;
    __asm__("bswap %0" : "+r"(swapped32));
    line_hex("bswap 32", swapped32);

    u64 loaded64;
    __asm__("movbe %1, %0" : "=r"(loaded64) : "m"(value));
    line_hex("movbe load 64", loaded64);
    u16 half = 0x1234;
    u16 loaded16;
    __asm__("movbe %1, %0" : "=r"(loaded16) : "m"(half));
    line_hex("movbe load 16", loaded16);
    u32 stored32;
    __asm__("movbe %1, %0" : "=m"(stored32) : "r"((u32)0x01234567));
    line_hex("movbe store 32", stored32);

    u8 carry, overflow, zero, sign;
    u64 left64 = 0x0123456789abcdefUL;
    __asm__("shld %%cl, %[source], %[target]\n\tsetc %[carry]\n\tsetz %[zero]\n\tsets %[sign]"
            : [target] "+r"(left64), [carry] "=r"(carry), [zero] "=r"(zero), [sign] "=r"(sign) : [source] "r"(0xfedcba9876543210UL), "c"(4) : "cc");
    line_hex("shld 64 by 4", left64);
    line("shld 64 by 4 flags", flags_of(carry, 0, zero, sign));
    u32 left32 = 0x81000001;
    __asm__("shld $8, %[source], %[target]\n\tsetc %[carry]\n\tsetz %[zero]\n\tsets %[sign]"
            : [target] "+r"(left32), [carry] "=r"(carry), [zero] "=r"(zero), [sign] "=r"(sign) : [source] "r"(0xa0000000u) : "cc");
    line_hex("shld 32 by 8", left32);
    line("shld 32 by 8 flags", flags_of(carry, 0, zero, sign));
    u64 right64 = 0x8000000000000001UL;
    __asm__("shrd $1, %[source], %[target]\n\tsetc %[carry]\n\tseto %[overflow]\n\tsetz %[zero]\n\tsets %[sign]"
            : [target] "+r"(right64), [carry] "=r"(carry), [overflow] "=r"(overflow), [zero] "=r"(zero), [sign] "=r"(sign) : [source] "r"(0UL) : "cc");
    line_hex("shrd 64 by 1", right64);
    line("shrd 64 by 1 flags", flags_of(carry, overflow, zero, sign));
    u32 unshifted = 0x12345678;
    __asm__("stc\n\tshrd %%cl, %[source], %[target]\n\tsetc %[carry]" : [target] "+r"(unshifted), [carry] "=r"(carry) : [source] "r"(0xffffffffu), "c"(0) : "cc");
    line_hex("shrd 32 by 0", unshifted);
    line("shrd 32 by 0 carry", carry);
    u32 right32 = 0x80000000;
    __asm__("shrd %%cl, %[source], %[target]\n\tsetc %[carry]\n\tsetz %[zero]\n\tsets %[sign]"
            : [target] "+r"(right32), [carry] "=r"(carry), [zero] "=r"(zero), [sign] "=r"(sign) : [source] "r"(0xffffffffu), "c"(31) : "cc");
    line_hex("shrd 32 by 31", right32);
    line("shrd 32 by 31 flags", flags_of(carry, 0, zero, sign));

    u64 all = ~0UL, high = 0x8000000000000000UL, five = 5, seven = 7;
    __asm__("xor %%eax, %%eax\n\tadcx %[one], %[all]\n\tadox %[high2], %[high]\n\tadcx %[none], %[five]\n\tadox %[none], %[seven]\n\tsetc %[carry]\n\tseto %[overflow]"
            : [all] "+r"(all), [high] "+r"(high), [five] "+r"(five), [seven] "+r"(seven), [carry] "=r"(carry), [overflow] "=r"(overflow)
            : [one] "r"(1UL), [high2] "r"(0x8000000000000000UL), [none] "r"(0UL) : "rax", "cc");
    line("adcx sum", all);
    line("adox sum", high);
    line("adcx carried", five);
    line("adox carried", seven);
    line("adcx adox flags", flags_of(carry, overflow, 0, 0));

    u64 wrapped;
    __asm__("lea 0x10(%k1,%k2), %0" : "=r"(wrapped) : "r"(0xfffffff0UL), "r"(8UL));
    line_hex("lea 32-bit wraps", wrapped);
    static _Alignas(64) u64 line_data[16];
    for (int i = 0; i < 16; ++i) line_data[i] = 0x5a5a5a5a00000000UL + (u64)i;
    __asm__ volatile("clflush (%0)\n\tclflushopt 64(%0)\n\tclwb 8(%0)\n\tmfence" : : "r"(line_data) : "memory");
    u64 flushed = 0;
    for (int i = 0; i < 16; ++i) flushed += line_data[i];
    line_hex("after cache line flushes", flushed);
}

static volatile u64 opaque_misalignment = 13;

[[gnu::noinline]] static void unaligned_accesses(void) {
    static _Alignas(16) u8 bytes[64];
    for (int i = 0; i < 64; ++i) bytes[i] = (u8)i;
    u8* base = bytes + opaque_misalignment;
    u64 quad;
    u32 double_word;
    u16 word;
    __asm__ volatile("movq (%1), %0" : "=r"(quad) : "r"(base) : "memory");
    __asm__ volatile("movl 1(%1), %0" : "=r"(double_word) : "r"(base) : "memory");
    __asm__ volatile("movw 2(%1), %0" : "=r"(word) : "r"(base) : "memory");
    line_hex("unaligned load 64", quad);
    line_hex("unaligned load 32", double_word);
    line_hex("unaligned load 16", word);
    __asm__ volatile("movq %1, 16(%0)" : : "r"(base), "r"(0x1122334455667788UL) : "memory");
    __asm__ volatile("movl %1, 34(%0)" : : "r"(base), "r"(0xaabbccddU) : "memory");
    __asm__ volatile("movw %1, 18(%0)" : : "r"(base), "r"((u16)0xeeff) : "memory");
    for (int i = 3; i < 7; ++i) {
        u64 aligned;
        __asm__ volatile("movq (%1), %0" : "=r"(aligned) : "r"(bytes + 8 * i) : "memory");
        line_hex("after unaligned stores", aligned);
    }
    i64 sign_word;
    i64 sign_double;
    u32 inside;
    __asm__ volatile("movswq 18(%1), %0" : "=r"(sign_word) : "r"(base) : "memory");
    __asm__ volatile("movslq 34(%1), %0" : "=r"(sign_double) : "r"(base) : "memory");
    __asm__ volatile("movl 4(%1), %0" : "=r"(inside) : "r"(base) : "memory");
    line_hex("unaligned sign-extended load 16", (u64)sign_word);
    line_hex("unaligned sign-extended load 32", (u64)sign_double);
    line_hex("unaligned load 32 within 16 bytes", inside);
}

static __thread u64 thread_word = 0x0807060504030201UL;

static u64 word_at(const u8* bytes) {
    u64 word = 0;
    for (int i = 7; i >= 0; --i) word = word << 8 | bytes[i];
    return word;
}

[[gnu::noinline]] static void string_instructions(void) {
    static u8 buffer[64];
    static const u8 letters[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    u8 carry, zero;
    void* to = buffer;
    const void* from = letters;
    u64 count = 8;
    const u64 one = 1, two = 2;
    __asm__ volatile("cmp %[two], %[one]\n\trep movsb\n\tsetc %[carry]\n\tsetz %[zero]"
                     : [carry] "=&q"(carry), [zero] "=&q"(zero), "+D"(to), "+S"(from), "+c"(count) : [one] "r"(one), [two] "r"(two) : "memory", "cc");
    line_hex("rep movsb copy", word_at(buffer));
    line("flags after rep movsb", carry * 2 + zero);
    to = buffer + 8;
    count = 8;
    __asm__ volatile("cmp %[one], %[one]\n\tmov $0x5a, %%al\n\trep stosb\n\tsetc %[carry]\n\tsetz %[zero]"
                     : [carry] "=&q"(carry), [zero] "=&q"(zero), "+D"(to), "+c"(count) : [one] "r"(one) : "rax", "memory", "cc");
    line_hex("rep stosb fill", word_at(buffer + 8));
    line("flags after rep stosb", carry * 2 + zero);
    for (int i = 0; i < 8; ++i) buffer[16 + i] = letters[i];
    to = buffer + 17;
    from = buffer + 16;
    count = 5;
    __asm__ volatile("rep movsb" : "+D"(to), "+S"(from), "+c"(count) : : "memory");
    line_hex("rep movsb one byte ahead", word_at(buffer + 16));
    to = buffer + 31;
    from = letters + 7;
    count = 8;
    __asm__ volatile("std\n\trep movsb\n\tcld" : "+D"(to), "+S"(from), "+c"(count) : : "memory", "cc");
    line_hex("backward rep movsb", word_at(buffer + 24));
    to = buffer + 32;
    count = 2;
    __asm__ volatile("mov $0x11223344, %%eax\n\trep stosl" : "+D"(to), "+c"(count) : : "rax", "memory");
    line_hex("rep stosl fill", word_at(buffer + 32));
    u64 thread_pointer;
    __asm__("mov %%fs:0, %0" : "=r"(thread_pointer));
    to = buffer + 40;
    from = (const void*)((u64)&thread_word - thread_pointer);
    count = 8;
    __asm__ volatile("rep movsb %%fs:(%%rsi), %%es:(%%rdi)" : "+D"(to), "+S"(from), "+c"(count) : : "memory");
    line_hex("rep movsb from fs", word_at(buffer + 40));
}

[[gnu::noinline]] static void string_reads(void) {
    static const u8 left[16] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', '1', '2', '3', '4', '5', '6', '7', '8'};
    static const u8 right[16] = {'a', 'b', 'c', 'd', 'e', 'x', 'g', 'h', '1', '2', '3', '4', '5', '6', '7', '8'};
    u8 carry, zero, sign, overflow;
    const void* first = left;
    const void* second = right;
    u64 count = 8;
    __asm__ volatile("repe cmpsb\n\tsetc %[carry]\n\tsetz %[zero]\n\tsets %[sign]"
                     : [carry] "=&q"(carry), [zero] "=&q"(zero), [sign] "=&q"(sign), "+S"(first), "+D"(second), "+c"(count) : : "memory", "cc");
    line("repe cmpsb count left after the mismatch", (i64)count);
    line("repe cmpsb advance", (const u8*)first - left);
    line("flags after repe cmpsb", carry * 4 + zero * 2 + sign);
    first = left;
    second = left;
    count = 2;
    __asm__ volatile("repe cmpsq\n\tsetc %[carry]\n\tsetz %[zero]\n\tsets %[sign]"
                     : [carry] "=&q"(carry), [zero] "=&q"(zero), [sign] "=&q"(sign), "+S"(first), "+D"(second), "+c"(count) : : "memory", "cc");
    line("repe cmpsq over equal words, count left", (i64)count);
    line("flags after equal repe cmpsq", carry * 4 + zero * 2 + sign);
    first = left;
    second = right;
    count = 0;
    __asm__ volatile("stc\n\trepe cmpsb\n\tsetc %[carry]" : [carry] "=&q"(carry), "+S"(first), "+D"(second), "+c"(count) : : "memory", "cc");
    line("repe cmpsb without a count keeps CF and its pointers", carry * 100 + ((const u8*)first - left) + ((const u8*)second - right));
    first = left;
    second = right;
    count = 4;
    __asm__ volatile("repne cmpsw\n\tsetz %[zero]" : [zero] "=&q"(zero), "+S"(first), "+D"(second), "+c"(count) : : "memory", "cc");
    line("repne cmpsw stops at the first equal word, count left", (i64)count * 10 + zero);
    static const u32 words[2] = {0x80000000u, 1u};
    first = &words[0];
    second = &words[1];
    __asm__ volatile("cmpsl\n\tseto %[overflow]\n\tsets %[sign]\n\tsetc %[carry]"
                     : [overflow] "=&q"(overflow), [sign] "=&q"(sign), [carry] "=&q"(carry), "+S"(first), "+D"(second) : : "memory", "cc");
    line("cmpsl flags", overflow * 4 + sign * 2 + carry);
    line("cmpsl advance", (const u8*)first - (const u8*)words);
    static const char text[] = "lifted";
    const void* scan = text;
    count = ~0ull;
    __asm__ volatile("xor %%eax, %%eax\n\trepne scasb" : "+D"(scan), "+c"(count) : : "rax", "memory", "cc");
    line("repne scasb length", (i64)(~count - 1));
    scan = left + 7;
    count = 8;
    __asm__ volatile("mov $0x63, %%eax\n\tstd\n\trepne scasb\n\tcld" : "+D"(scan), "+c"(count) : : "rax", "memory", "cc");
    line("backward repne scasb stops past the match at", (const u8*)scan - left);
    line("backward repne scasb count left", (i64)count);
    static const u64 above = 0x1122334455667789ull;
    scan = &above;
    __asm__ volatile("mov $0x1122334455667788, %%rax\n\tscasq\n\tsetc %[carry]\n\tsetz %[zero]\n\tsets %[sign]"
                     : [carry] "=&q"(carry), [zero] "=&q"(zero), [sign] "=&q"(sign), "+D"(scan) : : "rax", "memory", "cc");
    line("scasq flags", carry * 4 + zero * 2 + sign);
    u64 loaded = ~0ull;
    first = left + 8;
    count = 3;
    __asm__ volatile("rep lodsb" : "+S"(first), "+c"(count), "+a"(loaded) : : "memory");
    line_hex("rep lodsb keeps the last byte", loaded);
    line("rep lodsb advance", (const u8*)first - (left + 8));
    first = left;
    __asm__ volatile("lodsq" : "+S"(first), "=a"(loaded) : : "memory");
    line_hex("lodsq", loaded);
    loaded = ~0ull;
    __asm__ volatile("lodsl" : "+S"(first), "+a"(loaded) : : "memory");
    line_hex("lodsl zero-extends", loaded);
    u64 thread_pointer;
    __asm__("mov %%fs:0, %0" : "=r"(thread_pointer));
    static const u8 thread_bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    first = (const void*)((u64)&thread_word - thread_pointer);
    second = thread_bytes;
    count = 8;
    __asm__ volatile("repe cmpsb %%es:(%%rdi), %%fs:(%%rsi)\n\tsetz %[zero]" : [zero] "=&q"(zero), "+S"(first), "+D"(second), "+c"(count) : : "memory", "cc");
    line("repe cmpsb from fs, equal and count left", zero * 10 + (i64)count);
}

[[gnu::noinline]] static int byte_guarded_switch(u64 value) {
    int result;
    __asm__ volatile("lea -10(%[value]), %%edx\n\t"
                     "cmp $3, %%dl\n\t"
                     "ja 5f\n\t"
                     "movzbl %%dl, %%edx\n\t"
                     "lea 6f(%%rip), %%rcx\n\t"
                     "lea 6f+12(%%rip), %%rax\n\t"
                     "movslq (%%rcx,%%rdx,4), %%rdx\n\t"
                     "add %%rcx, %%rdx\n\t"
                     "jmp *%%rdx\n"
                     "1:\n\tmov $11, %[result]\n\tjmp 7f\n"
                     "2:\n\tmov $12, %[result]\n\tjmp 7f\n"
                     "3:\n\tmov $13, %[result]\n\tjmp 7f\n"
                     "4:\n\tmov $14, %[result]\n\tjmp 7f\n"
                     "5:\n\tmov $-1, %[result]\n\tjmp 7f\n"
                     ".pushsection .rodata\n\t.p2align 2\n"
                     "6:\n\t.long 1b-6b, 2b-6b, 3b-6b, 4b-6b\n\t"
                     ".popsection\n"
                     "7:"
                     : [result] "=r"(result) : [value] "r"(value) : "rax", "rcx", "rdx", "cc");
    return result;
}

[[gnu::noinline]] static int copied_base_switch(u64 value) {
    int result;
    __asm__ volatile("lea -1(%[value]), %%esi\n\t"
                     "cmp $3, %%esi\n\t"
                     "ja 5f\n\t"
                     "lea 6f(%%rip), %%rdx\n\t"
                     "mov %%rdx, %%rdi\n\t"
                     "movslq (%%rdx,%%rsi,4), %%rdx\n\t"
                     "add %%rdi, %%rdx\n\t"
                     "jmp *%%rdx\n"
                     "1:\n\tmov $21, %[result]\n\tjmp 7f\n"
                     "2:\n\tmov $22, %[result]\n\tjmp 7f\n"
                     "3:\n\tmov $23, %[result]\n\tjmp 7f\n"
                     "4:\n\tmov $24, %[result]\n\tjmp 7f\n"
                     "5:\n\tmov $-1, %[result]\n\tjmp 7f\n"
                     ".pushsection .rodata\n\t.p2align 2\n"
                     "6:\n\t.long 1b-6b, 2b-6b, 3b-6b, 4b-6b\n\t"
                     ".popsection\n"
                     "7:"
                     : [result] "=r"(result) : [value] "r"(value) : "rdx", "rsi", "rdi", "cc");
    return result;
}

#define EXTRQ(length, index)                                                                                          \
    [[gnu::noinline]] static u64 extrq_##length##_##index(u64 value) {                                      \
        __asm__("movq %[v], %%xmm0\n\textrq $" #index ", $" #length ", %%xmm0\n\tmovq %%xmm0, %[v]" : [v] "+r"(value) : : "xmm0"); \
        return value;                                                                                                 \
    }
#define INSERTQ(length, index)                                                                                        \
    [[gnu::noinline]] static u64 insertq_##length##_##index(u64 value, u64 source) {                         \
        __asm__("movq %[v], %%xmm0\n\tmovq %[s], %%xmm1\n\tinsertq $" #index ", $" #length ", %%xmm1, %%xmm0\n\tmovq %%xmm0, %[v]" \
                : [v] "+r"(value) : [s] "r"(source) : "xmm0", "xmm1");                                                \
        return value;                                                                                                 \
    }
EXTRQ(16, 16)
EXTRQ(0, 0)
EXTRQ(4, 60)
INSERTQ(8, 8)
INSERTQ(16, 40)
INSERTQ(0, 0)

[[gnu::noinline]] static void advertised_instructions(void) {
    static const char digits[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    u64 crc = 0xffffffff, eight;
    memcpy(&eight, digits, 8);
    __asm__("crc32q %1, %0" : "+r"(crc) : "r"(eight));
    __asm__("crc32b %1, %k0" : "+r"(crc) : "q"((u8)digits[8]));
    line_hex("crc32c of 123456789 by qword", ~crc & 0xffffffff);
    u32 crc32 = 0xffffffff, four;
    u16 two;
    memcpy(&four, digits, 4);
    __asm__("crc32l %1, %0" : "+r"(crc32) : "r"(four));
    memcpy(&two, digits + 4, 2);
    __asm__("crc32w %1, %0" : "+r"(crc32) : "r"(two));
    __asm__("crc32b %1, %0" : "+r"(crc32) : "q"((u8)digits[6]));
    memcpy(&two, digits + 7, 2);
    __asm__("crc32w %1, %0" : "+r"(crc32) : "r"(two));
    line_hex("crc32c of 123456789 by word and byte", ~crc32);

    static _Alignas(16) volatile u64 pair[2] = {1, 2};
    u64 low = 1, high = 2;
    u8 matched;
    __asm__ volatile("lock cmpxchg16b %0\n\tsetz %1" : "+m"(*pair), "=q"(matched), "+a"(low), "+d"(high) : "b"(3UL), "c"(4UL) : "memory", "cc");
    line("cmpxchg16b matched", matched);
    line_hex("cmpxchg16b stored", pair[0] << 32 | pair[1]);
    low = 1;
    high = 2;
    __asm__ volatile("lock cmpxchg16b %0\n\tsetz %1" : "+m"(*pair), "=q"(matched), "+a"(low), "+d"(high) : "b"(5UL), "c"(6UL) : "memory", "cc");
    line("cmpxchg16b mismatched", matched);
    line_hex("cmpxchg16b loaded", low << 32 | high);
    static volatile u64 single = 0x0000000200000001UL;
    u32 eax = 1, edx = 2;
    __asm__ volatile("lock cmpxchg8b %0\n\tsetz %1" : "+m"(single), "=q"(matched), "+a"(eax), "+d"(edx) : "b"(7u), "c"(8u) : "memory", "cc");
    line("cmpxchg8b matched", matched);
    line_hex("cmpxchg8b stored", single);

    u64 flags_byte;
    __asm__("mov $1, %%al\n\tcmp $2, %%al\n\tlahf\n\tmovzbl %%ah, %k0" : "=r"(flags_byte) : : "rax", "cc");
    line_hex("lahf after 1 - 2", flags_byte);
    u8 zero, carry;
    __asm__("mov $0x41, %%ah\n\tsahf\n\tsetz %0\n\tsetc %1" : "=q"(zero), "=q"(carry) : : "rax", "cc");
    line("sahf zero and carry", zero * 2 + carry);

    u64 random;
    __asm__ volatile("rdrand %0\n\tsetc %1" : "=r"(random), "=q"(carry) : : "cc");
    line("rdrand carry", carry);
    __asm__ volatile("rdseed %0\n\tsetc %1" : "=r"(random), "=q"(carry) : : "cc");
    line("rdseed carry", carry);
    u32 first_low, first_high, second_low, second_high, aux;
    __asm__ volatile("rdtscp" : "=a"(first_low), "=d"(first_high), "=c"(aux));
    __asm__ volatile("rdtscp" : "=a"(second_low), "=d"(second_high), "=c"(aux));
    line("rdtscp counts on", ((u64)second_high << 32 | second_low) >= ((u64)first_high << 32 | first_low));
    u64 processor = ~0UL;
    __asm__ volatile("rdpid %0" : "+r"(processor));
    line("rdpid fits 32 bits", processor >> 32 == 0);
    __asm__ volatile("prefetch %0" : : "m"(pair[0]));
    __asm__ volatile("prefetchw %0" : : "m"(pair[0]));
    u32 used_low, used_high, enabled_low, enabled_high;
    __asm__("xgetbv" : "=a"(used_low), "=d"(used_high) : "c"(1));
    __asm__("xgetbv" : "=a"(enabled_low), "=d"(enabled_high) : "c"(0));
    line("xinuse within xcr0", (used_low & ~enabled_low) == 0 && (used_high & ~enabled_high) == 0);
}

static volatile u64 divide_inputs[4] = {3, 5, 7, 0x8000000000000000UL};

[[gnu::noinline]] static void divisions(void) {
    u64 quotient, remainder;
    __asm__("divq %4" : "=a"(quotient), "=d"(remainder) : "a"(divide_inputs[1]), "d"(divide_inputs[0]), "r"(divide_inputs[2]) : "cc");
    line_hex("div 128 by 64 quotient", quotient);
    line_hex("div 128 by 64 remainder", remainder);
    __asm__("idivq %4" : "=a"(quotient), "=d"(remainder) : "a"(-(i64)divide_inputs[2] * 14 - 2), "d"(-1L), "r"(divide_inputs[2]) : "cc");
    line("idiv narrow quotient", (i64)quotient);
    line("idiv narrow remainder", (i64)remainder);
    __asm__("idivq %4" : "=a"(quotient), "=d"(remainder) : "a"(~divide_inputs[1]), "d"(~divide_inputs[0]), "r"(divide_inputs[2] + 4) : "cc");
    line_hex("idiv wide quotient", quotient);
    line_hex("idiv wide remainder", remainder);
    __asm__("idivq %4" : "=a"(quotient), "=d"(remainder) : "a"(divide_inputs[3]), "d"(0UL), "r"(-1L) : "cc");
    line_hex("idiv 2^63 by -1 quotient", quotient);
    line_hex("idiv 2^63 by -1 remainder", remainder);
    u16 ax = (u16)(divide_inputs[2] * 142 + 6);
    __asm__("divb %1" : "+a"(ax) : "q"((u8)divide_inputs[2]) : "cc");
    line("div 8 quotient", ax & 0xff);
    line("div 8 remainder", ax >> 8);
    u32 eax = (u32)(-(i32)divide_inputs[2]), edx = 0xffffffffu;
    __asm__("idivl %2" : "+a"(eax), "+d"(edx) : "r"((u32)2) : "cc");
    line("idiv 32 quotient", (i32)eax);
    line("idiv 32 remainder", (i32)edx);
}

static volatile u64 locked_word;

#define LOCKED(name, setup, instruction, start)                                                                        \
    do {                                                                                                               \
        locked_word = start;                                                                                           \
        u8 carry;                                                                                                      \
        __asm__ volatile(setup "\n\t" instruction " %0\n\tsetc %1" : "+m"(locked_word), "=q"(carry) : : "memory", "cc"); \
        line_hex(name, locked_word);                                                                                   \
        line(name " carry", carry);                                                                                    \
    } while (0)

[[gnu::noinline]] static void locked_forms(void) {
    LOCKED("lock adc", "stc", "lock adcq $0,", 0xffffffffffffffffUL);
    LOCKED("lock sbb", "stc", "lock sbbq $0,", 0);
    LOCKED("lock neg", "clc", "lock negq", 5);
    LOCKED("lock neg zero", "stc", "lock negq", 0);
    LOCKED("lock not", "stc", "lock notq", 0x0f);
    LOCKED("lock bts", "clc", "lock btsq $3,", 0);
    LOCKED("lock btr", "clc", "lock btrq $3,", 8);
    LOCKED("lock btc", "stc", "lock btcq $3,", 0);
    u64 doubled = 5;
    __asm__("xaddq %0, %0" : "+r"(doubled) : : "cc");
    line("xadd one register", (i64)doubled);
}

static volatile u64 bit_words[4] = {0, 8, 0, 0};

#define BIT_STRING(name, instruction, base, offset)                                                                    \
    do {                                                                                                               \
        u8 carry;                                                                                                      \
        __asm__ volatile(instruction " %2, %0\n\tsetc %1" : "+m"(*(base)), "=q"(carry) : "r"(offset) : "memory", "cc"); \
        line(name " carry", carry);                                                                                    \
    } while (0)

[[gnu::noinline]] static void bit_strings(void) {
    BIT_STRING("bts past the operand", "btsq", &bit_words[1], (i64)130);
    BIT_STRING("btr before the operand", "btrq", &bit_words[2], (i64)-61);
    BIT_STRING("btc 32-bit", "btcl", (volatile u32*)&bit_words[1], (i32)37);
    BIT_STRING("bt 16-bit before the operand", "btw", (volatile u16*)&bit_words[2], (i16)-27);
    BIT_STRING("lock bts past the operand", "lock btsq", &bit_words[0], (i64)200);
    BIT_STRING("bt past the operand", "btq", &bit_words[0], (i64)194);
    line_hex("bit string word 0", bit_words[0]);
    line_hex("bit string word 1", bit_words[1]);
    line_hex("bit string word 2", bit_words[2]);
    line_hex("bit string word 3", bit_words[3]);
}

struct Record {
    i64 key;
    u32 values[6];
    u16 flags;
    u8 tag;
};

[[gnu::noinline]] static struct Record make_record(i64 key) {
    struct Record record;
    memset(&record, 0, sizeof(record));
    record.key = key;
    for (int i = 0; i < 6; ++i) record.values[i] = (u32)(key * (i + 1));
    record.flags = (u16)(key ^ 0xbeef);
    record.tag = (u8)key;
    return record;
}

static void insertion_sort(i32* values, int count) {
    for (int i = 1; i < count; ++i) {
        i32 value = values[i];
        int j = i - 1;
        while (j >= 0 && values[j] > value) {
            values[j + 1] = values[j];
            --j;
        }
        values[j + 1] = value;
    }
}

static volatile u64 opaque_unsigned = 0xfedcba9876543210ul;
static volatile i64 opaque_signed = -1234567890123l;
static volatile u32 opaque_count = 13;
static volatile i32 opaque_small = -77;

long gcc_style_switch(long index);
__asm__(".text\n"
        ".type gcc_style_switch, @function\n"
        "gcc_style_switch:\n"
        "    cmpq $4, %rdi\n"
        "    jae .Lgcc_default\n"
        "    leaq .Lgcc_table(%rip), %rcx\n"
        "    movslq (%rcx,%rdi,4), %rax\n"
        "    addq %rax, %rcx\n"
        "    movq %rdi, %rax\n"
        "    jmpq *%rcx\n"
        ".Lgcc_case0: movl $10, %eax\n ret\n"
        ".Lgcc_case1: movl $20, %eax\n ret\n"
        ".Lgcc_case2: movl $30, %eax\n ret\n"
        ".Lgcc_case3: movl $40, %eax\n ret\n"
        ".Lgcc_default: movq $-1, %rax\n ret\n"
        ".section .rodata\n"
        ".p2align 2\n"
        ".Lgcc_table: .long .Lgcc_case0-.Lgcc_table, .Lgcc_case1-.Lgcc_table, .Lgcc_case2-.Lgcc_table, .Lgcc_case3-.Lgcc_table\n"
        ".text\n");

long shared_tail_entry(long index);
__asm__(".text\n"
        ".type shared_tail_entry, @function\n"
        "shared_tail_entry:\n"
        "    jmp .Lshared_dispatch\n"
        ".type shared_tail_owner, @function\n"
        "shared_tail_owner:\n"
        "    xor %edi, %edi\n"
        ".Lshared_dispatch:\n"
        "    cmpq $3, %rdi\n"
        "    jae .Lshared_default\n"
        "    leaq .Lshared_table(%rip), %rcx\n"
        "    movslq (%rcx,%rdi,4), %rax\n"
        "    addq %rcx, %rax\n"
        "    jmpq *%rax\n"
        ".Lshared_case0: movl $100, %eax\n ret\n"
        ".Lshared_case1: movl $200, %eax\n ret\n"
        ".Lshared_case2: movl $300, %eax\n ret\n"
        ".Lshared_default: movq $-1, %rax\n ret\n"
        ".section .rodata\n"
        ".p2align 2\n"
        ".Lshared_table: .long .Lshared_case0-.Lshared_table, .Lshared_case1-.Lshared_table, .Lshared_case2-.Lshared_table\n"
        ".text\n");

long padded_anchor(void);
extern const volatile long padded_distance;
extern const volatile long padded_immediate_distance;
__asm__(".text\n"
        ".p2align 4, 0xcc\n"
        ".type padded_anchor, @function\n"
        "padded_anchor:\n"
        "    movl $11, %eax\n"
        "    ret\n"
        ".p2align 4, 0xcc\n"
        ".Lpadded_hidden:\n"
        "    movl $29, %eax\n"
        "    ret\n"
        ".p2align 4, 0xcc\n"
        ".asciz \"from: FreeBSD: src/lib/msun/i387/e_remainder.S,v 1.8 2005/02/04 14:08:32 das Exp\"\n"
        "    int3\n"
        ".p2align 4, 0xcc\n"
        ".Lpadded_immediate:\n"
        "    movl $29, %eax\n"
        "    movl $1, %edx\n"
        "    addq %rdx, %rax\n"
        "    movabsq $0x80000000e8cc, %rcx\n"
        "    ret\n"
        ".section .rodata\n"
        ".p2align 3\n"
        "padded_distance: .quad .Lpadded_hidden - padded_anchor\n"
        "padded_immediate_distance: .quad .Lpadded_immediate - padded_anchor\n"
        ".text\n");

static u64 loop_sum(u64 count) {
    u64 sum;
    __asm__("xor %%eax, %%eax\n1:\n\tadd %%rcx, %%rax\n\tloop 1b" : "=&a"(sum), "+c"(count) : : "cc");
    return sum;
}

static u64 loopne_remaining(const char* text, u64 count) {
    __asm__("1:\n\tcmpb $0, (%[p])\n\tleaq 1(%[p]), %[p]\n\tloopne 1b" : [p] "+r"(text), "+c"(count) : : "cc", "memory");
    return count;
}

static u64 loope_remaining(const char* text, u64 count) {
    __asm__("1:\n\tcmpb $'a', (%[p])\n\tleaq 1(%[p]), %[p]\n\tloope 1b" : [p] "+r"(text), "+c"(count) : : "cc", "memory");
    return count;
}

static u64 loop_ecx(void) {
    u64 count = 0x100000003ul, iterations = 0;
    __asm__("1:\n\tinc %[k]\n\taddr32 loop 1b" : [k] "+r"(iterations), "+c"(count) : : "cc");
    return iterations << 40 | count;
}

static u64 push_pop_16(void) {
    u64 result;
    __asm__ volatile("sub $128, %%rsp\n\t"
                     "mov %%rsp, %%rcx\n\t"
                     "pushw $0x1234\n\t"
                     "pushw $0x5678\n\t"
                     "mov %%rcx, %%r8\n\t"
                     "sub %%rsp, %%r8\n\t"
                     "popw %%ax\n\t"
                     "popw %%dx\n\t"
                     "sub %%rsp, %%rcx\n\t"
                     "add $128, %%rsp\n\t"
                     "movzwl %%ax, %%eax\n\t"
                     "movzwl %%dx, %%edx\n\t"
                     "shl $16, %%rdx\n\t"
                     "or %%rdx, %%rax\n\t"
                     "shl $32, %%r8\n\t"
                     "or %%r8, %%rax\n\t"
                     "shl $40, %%rcx\n\t"
                     "or %%rcx, %%rax"
                     : "=a"(result) : : "rcx", "rdx", "r8", "memory");
    return result;
}

int main(void) {
    print("ps5lift basic fixture\n");
    line("fibonacci(20)", (i64)fibonacci(20));

    for (int i = 0; i < 4; ++i) line("operation", operations[i](1000, 7));
    line("many_arguments", many_arguments(1, 2, 3, 4, 5, 6, 7, 8, 9));

    u64 u = opaque_unsigned;
    i64 s = opaque_signed;
    u32 c = opaque_count;
    line_hex("u >> c", u >> c);
    line_hex("u << c", u << c);
    line("s >> c", s >> c);
    line_hex("rotate", (u >> c) | (u << (64 - c)));
    line_hex("u / 1000003", u / 1000003);
    line_hex("u % 1000003", u % 1000003);
    line("s / -97", s / -97);
    line("s % -97", s % -97);
    line("small / 5", opaque_small / 5);
    line("small % 5", opaque_small % 5);
    line_hex("high product", (u64)(((unsigned __int128)u * u) >> 64));
    line("signed high", (i64)(((__int128)s * s) >> 64));
    line("ctz", __builtin_ctzll(u));
    line("clz", __builtin_clzll(u >> 9));
    line("popcount", __builtin_popcountll(u));
    line("byte add", (i64)(u8)((u8)u + (u8)0xf7));
    line("word sub", (i64)(i16)((u16)u - (u16)0x8000));
    line("compare", (s < -5) + 2 * (u > 5) + 4 * ((i32)c == 13) + 8 * (opaque_small >= 0));

    i64 total = 0;
    for (int i = 0; i < 9; ++i) {
        const char* name = classify(i);
        total += (i64)length_of(name) * (i + 1);
    }
    line("classify total", total);
    for (i64 i = 0; i < 4; ++i) line("table after compare", table_index_copied_after_compare(i));
    for (i64 i = 0; i < 4; ++i) line("table before compare", table_index_copied_before_compare(i));
    for (i64 i = 0; i < 5; ++i) line("table base before loop", table_base_before_loop(i));
    static volatile u64 exchanged_word;
    for (i64 skip = 0; skip < 2; ++skip) {
        exchanged_word = 5;
        line(skip ? "exchange past the lock prefix" : "exchange with the lock prefix", overlapping_exchange(&exchanged_word, 5, 9, skip));
        line("exchanged word", (i64)exchanged_word);
    }
    line("a hundred million tail jumps", tail_ping(100000000));
    line("jump taken before the next function", jump_falls_into_next(0));
    line("jump falling into the next function", jump_falls_into_next(5));
    line("the next function itself", next_after_jump(2));
    i64 (*stored)(void);
    store_address_taken_by_lea(&stored);
    line("function stored through lea", call_function_pointer(stored));
    line("function passed through lea", pass_address_taken_by_lea());
    store_address_taken_long_before(&stored);
    line("function stored long after its lea", call_function_pointer(stored));
    line("function called through lea after a nop", call_address_taken_before_nop());
    line("function returned through lea", call_function_pointer(return_address_taken_by_lea()));
    line("function pushed through lea", push_address_taken_by_lea());
    line("function copied through lea", copy_address_taken_by_lea());
    line("function picked by a conditional move", call_function_pointer(select_address_taken_by_lea(1)));
    line("function passed to a tail call", tail_pass_address_taken_by_lea());
    store_address_past_jump(&stored);
    line("function stored past a jump", call_function_pointer(stored));
    store_address_after_call_and_branch(&stored);
    line("function kept across a call and stored past a branch", call_function_pointer(stored));
    i64 (*paired[2])(void);
    store_address_through_vector(paired);
    line("function stored from a vector register", call_function_pointer(paired[0]));
    line("pointer stored beside it", (u64)paired[1] - (u64)paired);
    print("strings returned through lea = ");
    print(options_string_through_lea(0));
    print(" ");
    print(options_string_through_lea(1));
    print("\n");
    zen2_integer();
    unaligned_accesses();
    locked_forms();
    bit_strings();
    string_instructions();
    string_reads();
    line("byte-guarded jump table", byte_guarded_switch(10) + byte_guarded_switch(11) * 100 + byte_guarded_switch(12) * 10000 + byte_guarded_switch(13) * 1000000);
    line("byte-guarded jump table default", byte_guarded_switch(14) + byte_guarded_switch(9));
    line("jump table through a copied base", copied_base_switch(1) + copied_base_switch(2) * 100 + copied_base_switch(3) * 10000 + copied_base_switch(4) * 1000000);
    line("jump table through a copied base default", copied_base_switch(5) + copied_base_switch(0));
    const u64 field_value = 0x1122334455667788ull, field_source = 0xaabbccddeeff0099ull;
    line_hex("extrq 16 bits at 16", extrq_16_16(field_value));
    line_hex("extrq 64 bits at 0", extrq_0_0(field_value));
    line_hex("extrq 4 bits at 60", extrq_4_60(field_value));
    line_hex("insertq 8 bits at 8", insertq_8_8(field_value, field_source));
    line_hex("insertq 16 bits at 40", insertq_16_40(field_value, field_source));
    line_hex("insertq 64 bits at 0", insertq_0_0(field_value, field_source));
    divisions();
    advertised_instructions();

    struct Record record = make_record(41);
    struct Record copy;
    memcpy(&copy, &record, sizeof(copy));
    u64 checksum = (u64)copy.key + copy.flags + copy.tag;
    for (int i = 0; i < 6; ++i) checksum = checksum * 31 + copy.values[i];
    line("record checksum", (i64)checksum);

    i32 values[] = {5, -3, 99, 0, 42, -100, 7, 7, 1};
    insertion_sort(values, 9);
    for (int i = 0; i < 9; ++i) line("sorted", values[i]);

    static u64 counter;
    for (int i = 0; i < 10; ++i) __atomic_fetch_add(&counter, (u64)i, __ATOMIC_SEQ_CST);
    u64 expected = 45;
    int exchanged = __atomic_compare_exchange_n(&counter, &expected, 1000, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    line("atomic counter", (i64)counter);
    line("atomic exchanged", exchanged);
    line_hex("16-bit push and pop", push_pop_16());
    long gcc_sum = 0;
    for (long i = 0; i < 5; ++i) gcc_sum += gcc_style_switch(i);
    line("gcc-style jump table", gcc_sum);
    long shared_sum = 0;
    for (long i = 0; i < 4; ++i) shared_sum += shared_tail_entry(i);
    line("jump table in a shared tail", shared_sum);
    long (*const padded_hidden)(void) = (long (*)(void))((const char*)padded_anchor + padded_distance);
    line("function found by its padding", padded_hidden() + padded_anchor() * 100);
    long (*const padded_immediate)(void) = (long (*)(void))((const char*)padded_anchor + padded_immediate_distance);
    line("function holding an int3 byte in an immediate", padded_immediate());
    line("loop sum", (i64)loop_sum(10));
    line("loopne remaining", (i64)loopne_remaining("abc\0de", 6));
    line("loope remaining", (i64)loope_remaining("aaabaa", 6));
    line_hex("addr32 loop", loop_ecx());

    print("done\n");
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
