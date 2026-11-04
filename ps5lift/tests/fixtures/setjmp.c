#include "host.h"

typedef long jmp_buf[12];
[[gnu::returns_twice]] int setjmp(jmp_buf environment);
[[noreturn]] void longjmp(jmp_buf environment, int value);

typedef unsigned long u64;

static u64 length_of(const char* text) {
    u64 length = 0;
    while (text[length]) ++length;
    return length;
}

static void print(const char* text) {
    host_write(text, length_of(text));
}

static void print_number(long value) {
    char buffer[24];
    int position = 23;
    buffer[position] = 0;
    const int negative = value < 0;
    unsigned long magnitude = negative ? 0ul - (unsigned long)value : (unsigned long)value;
    do {
        buffer[--position] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude);
    if (negative) buffer[--position] = '-';
    print(buffer + position);
}

static void line(const char* name, long value) {
    print(name);
    print(" ");
    print_number(value);
    print("\n");
}

static volatile int unwound;

[[gnu::noinline]] static void dive(int depth, long* target, int value) {
    volatile char pad[48];
    pad[0] = (char)depth;
    if (depth == 0) longjmp(target, value);
    dive(depth - 1, target, value + pad[0] * 0);
    ++unwound;
}

[[gnu::noinline]] static long mix(long seed) {
    return seed * 2654435761l ^ (seed >> 3);
}

static jmp_buf basic_buffer;

static void basic(void) {
    const long a = mix(1), b = mix(2), c = mix(3), d = mix(4), e = mix(5);
    const int result = setjmp(basic_buffer);
    if (result == 0) {
        dive(6, basic_buffer, 42);
        line("basic not reached", 0);
    }
    line("basic", result);
    line("basic saved", a + b * 3 + c * 5 + d * 7 + e * 11 == mix(1) + mix(2) * 3 + mix(3) * 5 + mix(4) * 7 + mix(5) * 11);
    line("basic unwound", unwound);
}

static void zero_value(void) {
    jmp_buf buffer;
    const int result = setjmp(buffer);
    if (result == 0) dive(2, buffer, 0);
    line("zero", result);
}

static void repeated(void) {
    jmp_buf buffer;
    volatile int count = 0;
    const int result = setjmp(buffer);
    ++count;
    if (result < 5) dive(result, buffer, result + 1);
    line("repeated result", result);
    line("repeated count", count);
}

static void two_sites(void) {
    jmp_buf first, second;
    volatile int order = 0;
    const int a = setjmp(first);
    order = order * 10 + 1;
    if (a == 0) {
        const int b = setjmp(second);
        order = order * 10 + 2;
        if (b == 0) dive(3, second, 7);
        line("two inner", b);
        dive(1, first, 9);
    }
    line("two outer", a);
    line("two order", order);
}

static jmp_buf levels[6];
static volatile int visits;

[[gnu::noinline]] static int level(int n) {
    const int result = setjmp(levels[n]);
    if (result != 0) {
        line("level resumed", n * 100 + result);
        return n;
    }
    ++visits;
    if (n < 5) return level(n + 1) + 1000;
    longjmp(levels[2], 3);
}

static void recursive(void) {
    line("level returned", level(0));
    line("level visits", visits);
}

static void (*volatile indirect)(int, long*, int) = dive;

static void through_pointer(void) {
    jmp_buf buffer;
    const int result = setjmp(buffer);
    if (result == 0) indirect(4, buffer, 11);
    line("pointer", result);
}

long switch_stack(long* environment, int value);

__asm__(".text\n"
        ".globl switch_stack\n"
        ".type switch_stack, @function\n"
        "switch_stack:\n"
        "    test %esi, %esi\n"
        "    jnz 1f\n"
        "    push %rsp\n"
        "    pop %rsp\n"
        "    mov $5, %eax\n"
        "    ret\n"
        "1:\n"
        "    mov 16(%rdi), %rcx\n"
        "    mov 0(%rdi), %rdx\n"
        "    mov %rdx, -8(%rcx)\n"
        "    mov %rdi, -16(%rcx)\n"
        "    mov %esi, %eax\n"
        "    mov 8(%rdi), %rbx\n"
        "    mov 24(%rdi), %rbp\n"
        "    mov 32(%rdi), %r12\n"
        "    mov 40(%rdi), %r13\n"
        "    mov 48(%rdi), %r14\n"
        "    mov 56(%rdi), %r15\n"
        "    mov 16(%rdi), %rsp\n"
        "    sub $16, %rsp\n"
        "    pop %rdi\n"
        "    ret\n");

[[gnu::noinline]] static long switch_from(int depth, long* target, int value) {
    volatile char pad[48];
    pad[0] = (char)depth;
    if (depth == 0) return switch_stack(target, value);
    return switch_from(depth - 1, target, value) + pad[0];
}

static void switched_stack(void) {
    jmp_buf buffer;
    const long a = mix(6), b = mix(7);
    const int result = setjmp(buffer);
    if (result == 0) {
        line("stack kept", switch_from(3, buffer, 0));
        switch_from(3, buffer, 13);
        line("stack not switched", 0);
    }
    line("stack switched", result);
    line("stack saved", a + b * 3 == mix(6) + mix(7) * 3);
}

long callee_pops(long first, int how);

__asm__(".text\n"
        "drop_arguments:\n"
        "    mov 8(%rsp), %rax\n"
        "    add 16(%rsp), %rax\n"
        "    add %rdi, %rax\n"
        "    mov (%rsp), %rcx\n"
        "    lea 24(%rsp), %rdx\n"
        "    mov %rdx, %rsp\n"
        "    push %rcx\n"
        "    ret\n"
        "drop_arguments_through_jump:\n"
        "    lea drop_arguments(%rip), %r11\n"
        "    jmp *%r11\n"
        "jump_back:\n"
        "    mov 8(%rsp), %rax\n"
        "    add 16(%rsp), %rax\n"
        "    add %rdi, %rax\n"
        "    mov $16, %edx\n"
        "    pop %rcx\n"
        "    add %rdx, %rsp\n"
        "    jmp *%rcx\n"
        ".globl callee_pops\n"
        ".type callee_pops, @function\n"
        "callee_pops:\n"
        "    push %rbp\n"
        "    mov %rsp, %rbp\n"
        "    push $20\n"
        "    push $300\n"
        "    cmp $1, %esi\n"
        "    je 1f\n"
        "    ja 3f\n"
        "    call drop_arguments\n"
        "    jmp 2f\n"
        "1:  call drop_arguments_through_jump\n"
        "    jmp 2f\n"
        "3:  call jump_back\n"
        "2:  mov %rsp, %rcx\n"
        "    sub %rbp, %rcx\n"
        "    imul $1000, %rcx\n"
        "    add %rcx, %rax\n"
        "    pop %rbp\n"
        "    ret\n");

long nest(long depth, long* frame);
long nest_balanced(long depth, long* frame);

__asm__(".text\n"
        ".globl nest\n"
        ".type nest, @function\n"
        "nest:\n"
        "    push %rbp\n"
        "    mov %rsp, %rbp\n"
        "    push %rdi\n"
        "    push %rsi\n"
        "    cmp $2, %rdi\n"
        "    jne 1f\n"
        "    mov %rsp, 8(%rsi)\n"
        "    mov %rbp, 16(%rsi)\n"
        "1:  test %rdi, %rdi\n"
        "    jz 3f\n"
        "    dec %rdi\n"
        "    call nest\n"
        "2:  mov -16(%rbp), %rsi\n"
        "    incq (%rsi)\n"
        "    mov -8(%rbp), %rax\n"
        "    leave\n"
        "    ret\n"
        "3:  mov 16(%rsi), %rbp\n"
        "    mov 8(%rsi), %rsp\n"
        "    lea 2b(%rip), %rcx\n"
        "    jmp *%rcx\n"
        "discard_frames:\n"
        "    mov 16(%rdi), %rbp\n"
        "    mov 8(%rdi), %rsp\n"
        "    sub $8, %rsp\n"
        "    ret\n"
        ".globl nest_balanced\n"
        ".type nest_balanced, @function\n"
        "nest_balanced:\n"
        "    push %rbp\n"
        "    mov %rsp, %rbp\n"
        "    push %rdi\n"
        "    push %rsi\n"
        "    cmp $2, %rdi\n"
        "    jne 1f\n"
        "    mov %rsp, 8(%rsi)\n"
        "    mov %rbp, 16(%rsi)\n"
        "1:  test %rdi, %rdi\n"
        "    jnz 2f\n"
        "    mov %rsi, %rdi\n"
        "    call discard_frames\n"
        "    ud2\n"
        "2:  dec %rdi\n"
        "    call nest_balanced\n"
        "    mov -16(%rbp), %rsi\n"
        "    incq (%rsi)\n"
        "    mov -8(%rbp), %rax\n"
        "    add $16, %rsp\n"
        "    pop %rbp\n"
        "    ret\n");

long resume_trampoline(long* handler_stack);

__asm__(".text\n"
        ".globl resume_trampoline\n"
        ".type resume_trampoline, @function\n"
        "resume_trampoline:\n"
        "    push %rbp\n"
        "    mov %rsp, %rbp\n"
        "    mov %rsp, (%rdi)\n"
        "    call throwing_handler\n"
        "1:  add $1000, %rax\n"
        "    pop %rbp\n"
        "    ret\n"
        "throwing_handler:\n"
        "    push %rbx\n"
        "    call throw_to_handler\n"
        "    mov $-1, %rax\n"
        "    pop %rbx\n"
        "    ret\n"
        "throw_to_handler:\n"
        "    mov (%rdi), %rsp\n"
        "    lea enter_at_handler(%rip), %rcx\n"
        "    jmp *%rcx\n"
        "enter_at_handler:\n"
        "    lea 1b(%rip), %rbx\n"
        "    push %rbx\n"
        "    lea catch_handler(%rip), %rcx\n"
        "    jmp *%rcx\n"
        "catch_handler:\n"
        "    mov $7, %eax\n"
        "    ret\n");

static void resumed_outer_frame(void) {
    long frame[3];
    frame[0] = 0;
    line("outer frame resumed by a jump", nest(2, frame));
    line("outer frame continued", frame[0]);
    frame[0] = 0;
    line("outer balanced frame resumed by a ret", nest_balanced(2, frame));
    line("outer frame continued", frame[0]);
    line("trampoline resumed through a handler", resume_trampoline(frame));
}

long split_frame(long first);

__asm__(".text\n"
        ".globl split_frame\n"
        ".type split_frame, @function\n"
        "split_frame:\n"
        "    push %rbp\n"
        "    mov %rsp, %rbp\n"
        "    push $40\n"
        "    lea 1f(%rip), %rax\n"
        "    jmp *%rax\n"
        "1:  pop %rsi\n"
        "    call adapt_arguments\n"
        "    pop %rbp\n"
        "    ret\n"
        "adapt_arguments:\n"
        "    sub $8, %rsp\n"
        "    pushq 8(%rsp)\n"
        "    mov %rdi, 16(%rsp)\n"
        "    mov %rsi, 8(%rsp)\n"
        "    jmp drop_arguments\n");

static void popped_arguments(void) {
    line("callee pops", callee_pops(1, 0));
    line("callee pops through a jump", callee_pops(4000, 1));
    line("callee pops and jumps back", callee_pops(50000, 2));
    line("callee pops for a split frame", split_frame(1000));
}

int main(void) {
    basic();
    zero_value();
    repeated();
    two_sites();
    recursive();
    through_pointer();
    switched_stack();
    popped_arguments();
    resumed_outer_frame();
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
