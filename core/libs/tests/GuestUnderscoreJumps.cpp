#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/JumpBuffer.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <xmmintrin.h>

extern "C" {

int APS5_VABI UnderscoreJumpKeepingCalleeSaved(std::uint64_t* buffer);
}

#define TEST_ASM_FUNCTION(name) APS5_ASM_FUNCTION(name)
#if defined(_WIN32) || defined(__APPLE__)
#define TEST_ASM_CALL(name) "    call " APS5_ASM_SYMBOL(name) "\n"
#else
#define TEST_ASM_CALL(name) "    call " name "@PLT\n"
#endif

asm(".text\n"
    TEST_ASM_FUNCTION("UnderscoreJumpKeepingCalleeSaved")
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
    TEST_ASM_CALL("_setjmp_nid_postfix")
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
    TEST_ASM_CALL("_longjmp_nid_postfix")
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

static constexpr std::uint32_t Filler = 0xa5a5a5a5u;

static std::uint32_t BlockedMask() {
    GuestSignalSet current{};
    Require(_sigprocmask_nid_postfix(1, nullptr, &current) == 0);
    return current.bits[0];
}

static void SetBlockedMask(std::uint32_t mask) {
    const GuestSignalSet set{{mask, 0, 0, 0}};
    Require(_sigprocmask_nid_postfix(3, &set, nullptr) == 0);
}

static std::uint32_t BufferWord(const std::uint64_t* buffer, std::size_t offset) {
    std::uint32_t word = 0;
    std::memcpy(&word, reinterpret_cast<const unsigned char*>(buffer) + offset, sizeof(word));
    return word;
}

static int JumpBack(int value, std::uint32_t maskAtSet, std::uint32_t maskAtJump) {
    std::uint64_t buffer[12];
    for (auto& word : buffer) word = 0xa5a5a5a5a5a5a5a5ull;
    const auto context = JumpBuffer::GuestContextCurrent_nid_no_patch();
    volatile int jumped = 0;
    SetBlockedMask(maskAtSet);
    const int result = _setjmp_nid_postfix(buffer);
    if (jumped == 0) {
        Require(result == 0);
        for (std::size_t offset = 72; offset < 88; offset += 4) Require(BufferWord(buffer, offset) == Filler);
        Require(BufferWord(buffer, 88) == 0);
        Require(BufferWord(buffer, 92) == context);
        jumped = 1;
        SetBlockedMask(maskAtJump);
        alignas(16) std::uint64_t copied[12];
        std::memcpy(copied, buffer, sizeof(copied));
        const auto other = JumpBuffer::GuestContextCreate_nid_no_patch(0);
        JumpBuffer::GuestContextSet_nid_no_patch(other.identity);
        _longjmp_nid_postfix(copied, value);
    }
    Require(JumpBuffer::GuestContextCurrent_nid_no_patch() == context);
    return result;
}

static int LongjmpToUnderscoreBuffer(std::uint32_t maskAtSet, std::uint32_t maskAtJump) {
    std::uint64_t buffer[12];
    for (auto& word : buffer) word = 0xa5a5a5a5a5a5a5a5ull;
    volatile int jumped = 0;
    SetBlockedMask(maskAtSet);
    const int result = _setjmp_nid_postfix(buffer);
    if (jumped == 0) {
        jumped = 1;
        SetBlockedMask(maskAtJump);
        longjmp_nid_postfix(buffer, 4);
    }
    return result;
}

static int UnderscoreJumpToSetjmpBuffer(std::uint32_t maskAtSet, std::uint32_t maskAtJump) {
    std::uint64_t buffer[12]{};
    volatile int jumped = 0;
    SetBlockedMask(maskAtSet);
    const int result = setjmp_nid_postfix(buffer);
    if (jumped == 0) {
        Require(BufferWord(buffer, 88) == 1);
        jumped = 1;
        SetBlockedMask(maskAtJump);
        _longjmp_nid_postfix(buffer, 6);
    }
    return result;
}

static void CheckSignalMask() {
    Require(JumpBack(0, 0x5u, 0x30u) == 1);
    Require(BlockedMask() == 0x30u);
    Require(JumpBack(9, 0x80000009u, 0u) == 9);
    Require(BlockedMask() == 0u);
    Require(JumpBack(-2, 0u, 0x3u) == -2);
    Require(BlockedMask() == 0x3u);
    Require(LongjmpToUnderscoreBuffer(0x5u, 0x40u) == 4);
    Require(BlockedMask() == 0x40u);
    Require(UnderscoreJumpToSetjmpBuffer(0x5u, 0x30u) == 6);
    Require(BlockedMask() == 0x30u);
    SetBlockedMask(0);
}

static unsigned short ControlWord() {
    unsigned short word = 0;
    asm volatile("fnstcw %0" : "=m"(word));
    return word;
}

static void SetControlWord(unsigned short word) {
    asm volatile("fldcw %0" : : "m"(word));
}

static void CheckFloatingPointState() {
    const unsigned int savedCsr = _mm_getcsr();
    const unsigned short savedWord = ControlWord();
    std::uint64_t buffer[12]{};
    volatile int jumped = 0;
    _mm_setcsr((savedCsr & ~0x603fu) | 0x6000u);
    SetControlWord(static_cast<unsigned short>((savedWord & ~0x0300u) | 0x0200u));
    const int result = _setjmp_nid_postfix(buffer);
    if (jumped == 0) {
        jumped = 1;
        _mm_setcsr((savedCsr & ~0x603fu) | 0x0025u);
        SetControlWord(static_cast<unsigned short>(savedWord | 0x0300u));
        _longjmp_nid_postfix(buffer, 3);
    }
    const unsigned int csr = _mm_getcsr();
    const unsigned short word = ControlWord();
    _mm_setcsr(savedCsr);
    SetControlWord(savedWord);
    Require(result == 3);
    Require((csr & 0x6000u) == 0x6000u);
    Require((csr & 0x3fu) == 0x25u);
    Require((csr & 0x1f80u) == (savedCsr & 0x1f80u));
    Require((word & 0x0300u) == 0x0200u);
}

int main() {
    std::uint64_t buffer[12]{};
    Require(UnderscoreJumpKeepingCalleeSaved(buffer) == 42);
    CheckSignalMask();
    CheckFloatingPointState();
}
