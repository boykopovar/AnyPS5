#include "host.h"

long module_scale;
long module_counter = 40;
static __thread long module_local = 5;

static unsigned long length_of(const char* text) {
    unsigned long length = 0;
    while (text[length]) ++length;
    return length;
}

[[gnu::constructor]] static void initialize(void) {
    static const char message[] = "module initialized\n";
    module_scale = 7;
    module_counter += 2;
    host_write(message, length_of(message));
}

long module_value(long x) {
    return x * module_scale + module_counter;
}

long module_tls(void) {
    return ++module_local;
}

long (*module_function(void))(long) {
    return module_value;
}

long* module_local_address(void) {
    return &module_local;
}

static long twice(long x) {
    return 2 * x;
}

static long squared(long x) {
    return x * x;
}

static long (*const module_table[])(long) = {twice, squared, module_value};

long module_table_call(int index, long x) {
    return module_table[index](x);
}

__asm__(".globl module_absolute\n.set module_absolute, 0x12345");
__asm__(".pushsection .text\n"
        ".globl module_text_data\n"
        ".type module_text_data, @object\n"
        ".size module_text_data, 4\n"
        "module_text_data: .byte 0x61, 0x62, 0x63, 0x64\n"
        ".popsection");
extern char module_absolute[];

long module_absolute_address(void) {
    return (long)module_absolute;
}

__asm__(".text\n"
        ".globl \"gNQ1V2vfXDE#guest\"\n"
        ".type \"gNQ1V2vfXDE#guest\", @function\n"
        "\"gNQ1V2vfXDE#guest\":\n"
        "    mov (%rsp), %rdx\n"
        "    mov %rdx, 0(%rdi)\n"
        "    mov %rbx, 8(%rdi)\n"
        "    lea 8(%rsp), %rdx\n"
        "    mov %rdx, 16(%rdi)\n"
        "    mov %rbp, 24(%rdi)\n"
        "    mov %r12, 32(%rdi)\n"
        "    mov %r13, 40(%rdi)\n"
        "    mov %r14, 48(%rdi)\n"
        "    mov %r15, 56(%rdi)\n"
        "    xor %eax, %eax\n"
        "    ret\n"
        ".globl \"lKEN2IebgJ0#guest\"\n"
        ".type \"lKEN2IebgJ0#guest\", @function\n"
        "\"lKEN2IebgJ0#guest\":\n"
        "    mov %esi, %eax\n"
        "    test %eax, %eax\n"
        "    jnz 1f\n"
        "    inc %eax\n"
        "1:  mov 8(%rdi), %rbx\n"
        "    mov 16(%rdi), %rsp\n"
        "    mov 24(%rdi), %rbp\n"
        "    mov 32(%rdi), %r12\n"
        "    mov 40(%rdi), %r13\n"
        "    mov 48(%rdi), %r14\n"
        "    mov 56(%rdi), %r15\n"
        "    jmp *0(%rdi)\n");

