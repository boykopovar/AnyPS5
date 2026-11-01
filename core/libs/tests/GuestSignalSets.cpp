#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern "C" {

int APS5_VABI JumpKeepingCalleeSaved(std::uint64_t* buffer);
}

#define TEST_ASM_FUNCTION(name) APS5_ASM_FUNCTION(name)
#if defined(_WIN32) || defined(__APPLE__)
#define TEST_ASM_CALL(name) "    call " APS5_ASM_SYMBOL(name) "\n"
#else
#define TEST_ASM_CALL(name) "    call " name "@PLT\n"
#endif

asm(".text\n"
    TEST_ASM_FUNCTION("JumpKeepingCalleeSaved")
    "    pushq %rbx\n"
    "    pushq %rbp\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    pushq %rdi\n"
    "    movabsq $0x1111111111111111, %rbx\n"
    "    movabsq $0x2222222222222222, %rbp\n"
    "    movabsq $0x3333333333333333, %r12\n"
    "    movabsq $0x4444444444444444, %r13\n"
    "    movabsq $0x5555555555555555, %r14\n"
    "    movabsq $0x6666666666666666, %r15\n"
    "    movl $1, %esi\n"
    TEST_ASM_CALL("sigsetjmp_nid_postfix")
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    xorl %ebx, %ebx\n"
    "    xorl %ebp, %ebp\n"
    "    xorl %r12d, %r12d\n"
    "    xorl %r13d, %r13d\n"
    "    xorl %r14d, %r14d\n"
    "    xorl %r15d, %r15d\n"
    "    movq 0(%rsp), %rdi\n"
    "    movl $42, %esi\n"
    TEST_ASM_CALL("siglongjmp_nid_postfix")
    "1:\n"
    "    movabsq $0x1111111111111111, %rcx\n"
    "    cmpq %rcx, %rbx\n"
    "    jne 2f\n"
    "    movabsq $0x2222222222222222, %rcx\n"
    "    cmpq %rcx, %rbp\n"
    "    jne 2f\n"
    "    movabsq $0x3333333333333333, %rcx\n"
    "    cmpq %rcx, %r12\n"
    "    jne 2f\n"
    "    movabsq $0x4444444444444444, %rcx\n"
    "    cmpq %rcx, %r13\n"
    "    jne 2f\n"
    "    movabsq $0x5555555555555555, %rcx\n"
    "    cmpq %rcx, %r14\n"
    "    jne 2f\n"
    "    movabsq $0x6666666666666666, %rcx\n"
    "    cmpq %rcx, %r15\n"
    "    je 3f\n"
    "2:\n"
    "    movl $-1, %eax\n"
    "3:\n"
    "    popq %rdi\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbp\n"
    "    popq %rbx\n"
    "    ret\n");

static void Require(bool value) { if (!value) std::abort(); }

static bool Equals(const GuestSignalSet& set, std::uint32_t w0, std::uint32_t w1, std::uint32_t w2, std::uint32_t w3) {
    return set.bits[0] == w0 && set.bits[1] == w1 && set.bits[2] == w2 && set.bits[3] == w3;
}

static std::uint32_t BlockedMask() {
    GuestSignalSet current{};
    Require(_sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    return current.bits[0];
}

static void SetBlockedMask(std::uint32_t mask) {
    const GuestSignalSet set{{mask, 0, 0, 0}};
    Require(_sigprocmask_nid_postfix(3, &set, nullptr) == 0);
}

static int JumpBack(int saveMask, int value, std::uint32_t maskAtSet, std::uint32_t maskAtJump) {
    std::uint64_t buffer[12]{};
    volatile int jumped = 0;
    SetBlockedMask(maskAtSet);
    const int result = sigsetjmp_nid_postfix(buffer, saveMask);
    if (jumped == 0) {
        Require(result == 0);
        jumped = 1;
        SetBlockedMask(maskAtJump);
        siglongjmp_nid_postfix(buffer, value);
    }
    return result;
}

static void CheckSiglongjmp() {
    Require(JumpBack(1, 0, 0x5u, 0x30u) == 1);
    Require(BlockedMask() == 0x5u);
    Require(JumpBack(1, 7, 0x9u, 0u) == 7);
    Require(BlockedMask() == 0x9u);
    Require(JumpBack(0, -3, 0x5u, 0x30u) == -3);
    Require(BlockedMask() == 0x30u);
    Require(JumpBack(2, 5, 0x3u, 0x40u) == 5);
    Require(BlockedMask() == 0x3u);
    SetBlockedMask(0);
}

static std::uint32_t BufferWord(const std::uint64_t* buffer, std::size_t offset) {
    std::uint32_t word = 0;
    std::memcpy(&word, reinterpret_cast<const unsigned char*>(buffer) + offset, sizeof(word));
    return word;
}

static void CheckSigsetjmp() {
    static constexpr std::uint32_t Filler = 0xa5a5a5a5u;
    std::uint64_t buffer[12];
    for (auto& word : buffer) word = 0xa5a5a5a5a5a5a5a5ull;
    SetBlockedMask(0x30u);
    Require(sigsetjmp_nid_postfix(buffer, 0) == 0);
    for (std::size_t offset = 72; offset < 88; offset += 4) Require(BufferWord(buffer, offset) == Filler);
    Require(BufferWord(buffer, 88) == 0);
    Require(BufferWord(buffer, 92) == Filler);
    Require(BlockedMask() == 0x30u);

    SetBlockedMask(0x80000005u);
    Require(sigsetjmp_nid_postfix(buffer, 1) == 0);
    Require(BufferWord(buffer, 72) == 0x80000005u);
    for (std::size_t offset = 76; offset < 88; offset += 4) Require(BufferWord(buffer, offset) == 0);
    Require(BufferWord(buffer, 88) == 1);
    Require(BufferWord(buffer, 92) == Filler);
    Require(BlockedMask() == 0x80000005u);

    SetBlockedMask(0x6u);
    Require(JumpKeepingCalleeSaved(buffer) == 42);
    Require(BlockedMask() == 0x6u);
    SetBlockedMask(0);
}

static int LongjumpBack(int value, std::uint32_t maskAtSet, std::uint32_t maskAtJump) {
    std::uint64_t buffer[12]{};
    volatile int jumped = 0;
    SetBlockedMask(maskAtSet);
    const int result = setjmp_nid_postfix(buffer);
    if (jumped == 0) {
        Require(result == 0);
        Require(BufferWord(buffer, 72) == maskAtSet);
        Require(BufferWord(buffer, 88) == 1);
        jumped = 1;
        SetBlockedMask(maskAtJump);
        longjmp_nid_postfix(buffer, value);
    }
    return result;
}

static void CheckLongjmp() {
    Require(LongjumpBack(0, 0x5u, 0x30u) == 1);
    Require(BlockedMask() == 0x5u);
    Require(LongjumpBack(7, 0x80000009u, 0u) == 7);
    Require(BlockedMask() == 0x80000009u);
    SetBlockedMask(0);
}

static void CheckInvalidHow() {
    SetBlockedMask(0x5u);
    const GuestSignalSet blocked{{0x30u, 0, 0, 0}};
    for (const int how : {0, 4, -1, 1000}) {
        GuestSignalSet previous{{0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u}};
        *__error_nid_postfix() = 0;
        Require(_sigprocmask_nid_postfix(how, &blocked, &previous) == -1 && *__error_nid_postfix() == 22);
        Require(Equals(previous, 0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u));
        *__error_nid_postfix() = 0;
        Require(sigprocmask_nid_postfix(how, &blocked, &previous) == -1 && *__error_nid_postfix() == 22);
        Require(Equals(previous, 0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u, 0xa5a5a5a5u));
        Require(BlockedMask() == 0x5u);
        Require(_sigprocmask_nid_postfix(how, nullptr, &previous) == 0);
        Require(Equals(previous, 0x5u, 0, 0, 0));
    }
    SetBlockedMask(0);
}

int main() {
    CheckSigsetjmp();
    CheckSiglongjmp();
    CheckLongjmp();
    CheckInvalidHow();
    static constexpr int Untouched = 5;
    GuestSignalSet set{{0x12345678u, 0x9abcdef0u, 0xffffffffu, 1u}};
    *__error_nid_postfix() = Untouched;
    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(Equals(set, 0, 0, 0, 0));
    Require(*__error_nid_postfix() == Untouched);

    Require(sigfillset_nid_postfix(&set) == 0);
    Require(Equals(set, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));
    for (int signal = 1; signal <= 128; ++signal) Require(sigismember_nid_postfix(&set, signal) == 1);
    Require(*__error_nid_postfix() == Untouched);

    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(sigaddset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 1u, 0, 0, 0));
    Require(sigaddset_nid_postfix(&set, 32) == 0);
    Require(Equals(set, 0x80000001u, 0, 0, 0));
    Require(sigaddset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0, 0));
    Require(sigaddset_nid_postfix(&set, 65) == 0);
    Require(Equals(set, 0x80000001u, 1u, 1u, 0));
    Require(sigaddset_nid_postfix(&set, 96) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0));
    Require(sigaddset_nid_postfix(&set, 128) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0x80000000u));
    Require(sigaddset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0x80000001u, 1u, 0x80000001u, 0x80000000u));
    Require(*__error_nid_postfix() == Untouched);

    Require(sigismember_nid_postfix(&set, 1) == 1);
    Require(sigismember_nid_postfix(&set, 2) == 0);
    Require(sigismember_nid_postfix(&set, 32) == 1);
    Require(sigismember_nid_postfix(&set, 33) == 1);
    Require(sigismember_nid_postfix(&set, 64) == 0);
    Require(sigismember_nid_postfix(&set, 65) == 1);
    Require(sigismember_nid_postfix(&set, 96) == 1);
    Require(sigismember_nid_postfix(&set, 97) == 0);
    Require(sigismember_nid_postfix(&set, 128) == 1);
    Require(*__error_nid_postfix() == Untouched);

    Require(sigdelset_nid_postfix(&set, 1) == 0);
    Require(Equals(set, 0x80000000u, 1u, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 32) == 0);
    Require(Equals(set, 0, 1u, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 33) == 0);
    Require(Equals(set, 0, 0, 0x80000001u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 96) == 0);
    Require(Equals(set, 0, 0, 1u, 0x80000000u));
    Require(sigdelset_nid_postfix(&set, 128) == 0);
    Require(Equals(set, 0, 0, 1u, 0));
    Require(sigdelset_nid_postfix(&set, 2) == 0);
    Require(Equals(set, 0, 0, 1u, 0));
    Require(sigismember_nid_postfix(&set, 128) == 0);
    Require(*__error_nid_postfix() == Untouched);

    for (const int invalid : {0, -1, 129, 1000, INT_MIN, INT_MAX}) {
        *__error_nid_postfix() = 0;
        Require(sigaddset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigdelset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigismember_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(Equals(set, 0, 0, 1u, 0));
    }

    Require(sigfillset_nid_postfix(&set) == 0);
    for (const int invalid : {0, -1, 129, 1000, INT_MIN, INT_MAX}) {
        Require(sigaddset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(sigdelset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(sigismember_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        Require(Equals(set, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu));
    }

    const GuestSignalSet empty{{0, 0, 0, 0}};
    Require(sigismember_nid_postfix(&empty, 1) == 0);
    Require(sigismember_nid_postfix(&empty, 128) == 0);
    return 0;
}
