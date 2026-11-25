#if defined(__APPLE__) && defined(__x86_64__)
#include "prx/libc/include/specifics/macos/GuestFaultCopy.hpp"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <pthread.h>
#include <sys/ucontext.h>

extern "C" {
std::size_t GuestFaultCopyBytes(void* destination, const void* source, std::size_t bytes);
extern const char GuestFaultCopyVector[];
extern const char GuestFaultCopyTail[];
extern const char GuestFaultCopyResume[];
}

__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl _GuestFaultCopyBytes\n"
    ".private_extern _GuestFaultCopyBytes\n"
    "_GuestFaultCopyBytes:\n"
    ".cfi_startproc\n"
    "    movq %rdx, %rcx\n"
    "    cmpq $16, %rcx\n"
    "    jae _GuestFaultCopyVector\n"
    "    cmpq $8, %rcx\n"
    "    je LGuestFaultCopyQuadAligned\n"
    "    cmpq $4, %rcx\n"
    "    jne _GuestFaultCopyTail\n"
    "    movq %rsi, %rax\n"
    "    orq %rdi, %rax\n"
    "    testb $3, %al\n"
    "    jnz _GuestFaultCopyTail\n"
    "    jmp LGuestFaultCopyLong\n"
    "LGuestFaultCopyQuadAligned:\n"
    "    movq %rsi, %rax\n"
    "    orq %rdi, %rax\n"
    "    testb $7, %al\n"
    "    jnz _GuestFaultCopyTail\n"
    "    jmp LGuestFaultCopyQuad\n"
    ".globl _GuestFaultCopyVector\n"
    ".private_extern _GuestFaultCopyVector\n"
    ".alt_entry _GuestFaultCopyVector\n"
    "_GuestFaultCopyVector:\n"
    "    cmpq $64, %rcx\n"
    "    jb LGuestFaultCopyVector16\n"
    "LGuestFaultCopyVector64:\n"
    "    movups (%rsi), %xmm0\n"
    "    movups 16(%rsi), %xmm1\n"
    "    movups 32(%rsi), %xmm2\n"
    "    movups 48(%rsi), %xmm3\n"
    "    movups %xmm0, (%rdi)\n"
    "    movups %xmm1, 16(%rdi)\n"
    "    movups %xmm2, 32(%rdi)\n"
    "    movups %xmm3, 48(%rdi)\n"
    "    addq $64, %rsi\n"
    "    addq $64, %rdi\n"
    "    subq $64, %rcx\n"
    "    cmpq $64, %rcx\n"
    "    jae LGuestFaultCopyVector64\n"
    "    cmpq $16, %rcx\n"
    "    jb LGuestFaultCopyVectorLast\n"
    "LGuestFaultCopyVector16:\n"
    "    movups (%rsi), %xmm0\n"
    "    movups %xmm0, (%rdi)\n"
    "    addq $16, %rsi\n"
    "    addq $16, %rdi\n"
    "    subq $16, %rcx\n"
    "    cmpq $16, %rcx\n"
    "    jae LGuestFaultCopyVector16\n"
    "LGuestFaultCopyVectorLast:\n"
    "    testq %rcx, %rcx\n"
    "    jz _GuestFaultCopyResume\n"
    "    movups -16(%rsi,%rcx), %xmm0\n"
    "    movups %xmm0, -16(%rdi,%rcx)\n"
    "    xorl %ecx, %ecx\n"
    "    jmp _GuestFaultCopyResume\n"
    ".globl _GuestFaultCopyTail\n"
    ".private_extern _GuestFaultCopyTail\n"
    ".alt_entry _GuestFaultCopyTail\n"
    "_GuestFaultCopyTail:\n"
    "    testq %rcx, %rcx\n"
    "    jz _GuestFaultCopyResume\n"
    "LGuestFaultCopyByte:\n"
    "    movb (%rsi), %al\n"
    "    movb %al, (%rdi)\n"
    "    incq %rsi\n"
    "    incq %rdi\n"
    "    decq %rcx\n"
    "    jnz LGuestFaultCopyByte\n"
    "    jmp _GuestFaultCopyResume\n"
    "LGuestFaultCopyQuad:\n"
    "    movq (%rsi), %rax\n"
    "    movq %rax, (%rdi)\n"
    "    xorl %ecx, %ecx\n"
    "    jmp _GuestFaultCopyResume\n"
    "LGuestFaultCopyLong:\n"
    "    movl (%rsi), %eax\n"
    "    movl %eax, (%rdi)\n"
    "    xorl %ecx, %ecx\n"
    ".globl _GuestFaultCopyResume\n"
    ".private_extern _GuestFaultCopyResume\n"
    ".alt_entry _GuestFaultCopyResume\n"
    "_GuestFaultCopyResume:\n"
    "    movq %rcx, %rax\n"
    "    ret\n"
    ".cfi_endproc\n");

namespace {

constexpr int CopySignals[] = {SIGSEGV, SIGBUS};
constexpr unsigned SegvSlot = 1u << 0;
constexpr unsigned AllSlots = (1u << std::size(CopySignals)) - 1;

struct sigaction g_previous[2][std::size(CopySignals)]{};
std::atomic<int> g_active{0};
std::atomic<std::uintptr_t> g_installer{0};
std::atomic<unsigned> g_pending{0};
std::atomic<bool> g_ready{false};

void Chain(int signal, siginfo_t* info, void* context) {
    std::size_t slot = 0;
    while (slot < std::size(CopySignals) && CopySignals[slot] != signal) ++slot;
    if (slot == std::size(CopySignals)) return;
    const int active = g_active.load(std::memory_order_acquire);
    const struct sigaction previous = g_previous[active][slot];
    if (previous.sa_handler == SIG_IGN) return;
    if (previous.sa_handler != SIG_DFL) {
        if ((previous.sa_flags & SA_RESETHAND) != 0) {
            g_previous[active][slot] = {};
            g_previous[active][slot].sa_handler = SIG_DFL;
        }
        if ((previous.sa_flags & SA_SIGINFO) != 0) {
            previous.sa_sigaction(signal, info, context);
            return;
        }
        previous.sa_handler(signal);
        return;
    }
    struct sigaction fallback{};
    fallback.sa_handler = SIG_DFL;
    sigemptyset(&fallback.sa_mask);
    sigaction(signal, &fallback, nullptr);
    raise(signal);
}

void OnCopyFault(int signal, siginfo_t* info, void* context) {
    if (GuestFaultCopy::Resume(info, context)) return;
    Chain(signal, info, context);
}

bool InstallOwned(unsigned slots) {
    const int next = 1 - g_active.load(std::memory_order_relaxed);
    struct sigaction current[std::size(CopySignals)]{};
    bool ours = true;
    for (std::size_t slot = 0; slot < std::size(CopySignals); ++slot) {
        if ((slots & (1u << slot)) == 0) continue;
        if (sigaction(CopySignals[slot], nullptr, &current[slot]) != 0) return false;
        ours = ours && (current[slot].sa_flags & SA_SIGINFO) != 0 && current[slot].sa_sigaction == OnCopyFault;
    }
    if (ours) return true;
    for (std::size_t slot = 0; slot < std::size(CopySignals); ++slot) {
        const bool installed = (slots & (1u << slot)) == 0 || ((current[slot].sa_flags & SA_SIGINFO) != 0 && current[slot].sa_sigaction == OnCopyFault);
        g_previous[next][slot] = installed ? g_previous[1 - next][slot] : current[slot];
    }
    g_active.store(next, std::memory_order_release);
    for (std::size_t slot = 0; slot < std::size(CopySignals); ++slot) {
        if ((slots & (1u << slot)) == 0) continue;
        struct sigaction action{};
        action.sa_sigaction = OnCopyFault;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER | (g_previous[next][slot].sa_flags & SA_RESTART);
        action.sa_mask = g_previous[next][slot].sa_mask;
        if (sigaction(CopySignals[slot], &action, nullptr) != 0) return false;
    }
    return true;
}

bool Install(unsigned slots) {
    const auto self = reinterpret_cast<std::uintptr_t>(pthread_self());
    for (std::uintptr_t expected = 0; !g_installer.compare_exchange_weak(expected, self, std::memory_order_acquire); expected = 0) {
        if (expected == self) {
            g_pending.fetch_or(slots, std::memory_order_release);
            return true;
        }
    }
    bool installed = false;
    do {
        installed = InstallOwned(slots);
        slots = g_pending.exchange(0, std::memory_order_acquire);
    } while (installed && slots != 0);
    g_installer.store(0, std::memory_order_release);
    return installed;
}

bool Enabled() {
    return std::getenv("APS5_GUEST_COPY_SYSCALL") == nullptr;
}

}

namespace GuestFaultCopy {

bool Ready() {
    static const bool ready = [] {
        const bool installed = Enabled() && Install(AllSlots);
        g_ready.store(installed, std::memory_order_release);
        return installed;
    }();
    return ready;
}

void Reinstall() {
    if (g_ready.load(std::memory_order_acquire)) static_cast<void>(Install(SegvSlot));
}

bool Copy(void* destination, const void* source, std::size_t bytes) {
    return GuestFaultCopyBytes(destination, source, bytes) == 0;
}

bool Resume(const siginfo_t* info, void* context) {
    if (info == nullptr || context == nullptr) return false;
    if (info->si_signo != SIGSEGV && info->si_signo != SIGBUS) return false;
    const auto state = static_cast<ucontext_t*>(context)->uc_mcontext;
    if (state == nullptr) return false;
    const auto vector = reinterpret_cast<std::uintptr_t>(GuestFaultCopyVector);
    const auto tail = reinterpret_cast<std::uintptr_t>(GuestFaultCopyTail);
    const auto resume = reinterpret_cast<std::uintptr_t>(GuestFaultCopyResume);
    const auto instruction = static_cast<std::uintptr_t>(state->__ss.__rip);
    if (instruction < vector || instruction >= resume) return false;
    state->__ss.__rip = instruction < tail ? tail : resume;
    return true;
}

}
#endif
