#if defined(__linux__) && defined(__x86_64__)
#include "prx/libc/include/specifics/linux/GuestFaultCopy.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <sys/syscall.h>
#include <unistd.h>
#include <iterator>
#include <ucontext.h>

extern "C" {
std::size_t GuestFaultCopyBytes(void* destination, const void* source, std::size_t bytes);
extern const char GuestFaultCopyAccess[];
extern const char GuestFaultCopyResume[];
}

__asm__(
    ".pushsection .text\n"
    ".p2align 4\n"
    ".globl GuestFaultCopyBytes\n"
    ".hidden GuestFaultCopyBytes\n"
    ".type GuestFaultCopyBytes, @function\n"
    "GuestFaultCopyBytes:\n"
    ".cfi_startproc\n"
    "    movq %rdx, %rcx\n"
    ".globl GuestFaultCopyAccess\n"
    ".hidden GuestFaultCopyAccess\n"
    "GuestFaultCopyAccess:\n"
    "    rep movsb\n"
    ".globl GuestFaultCopyResume\n"
    ".hidden GuestFaultCopyResume\n"
    "GuestFaultCopyResume:\n"
    "    movq %rcx, %rax\n"
    "    ret\n"
    ".cfi_endproc\n"
    ".size GuestFaultCopyBytes, .-GuestFaultCopyBytes\n"
    ".popsection\n");

namespace {

constexpr int CopySignals[] = {SIGSEGV, SIGBUS};
constexpr unsigned SegvSlot = 1u << 0;
constexpr unsigned AllSlots = (1u << std::size(CopySignals)) - 1;

struct sigaction g_previous[2][std::size(CopySignals)]{};
std::atomic<int> g_active{0};
std::atomic<pid_t> g_installer{0};
std::atomic<unsigned> g_pending{0};
std::atomic<bool> g_ready{false};

void Chain(int signal, siginfo_t* info, void* context) {
    std::size_t slot = 0;
    while (slot < std::size(CopySignals) && CopySignals[slot] != signal) ++slot;
    if (slot == std::size(CopySignals)) return;
    const struct sigaction& previous = g_previous[g_active.load(std::memory_order_acquire)][slot];
    if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
        previous.sa_sigaction(signal, info, context);
        return;
    }
    if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
        previous.sa_handler(signal);
        return;
    }
    if (info->si_code <= 0 && previous.sa_handler == SIG_IGN) return;
    struct sigaction fallback{};
    fallback.sa_handler = SIG_DFL;
    sigemptyset(&fallback.sa_mask);
    sigaction(signal, &fallback, nullptr);
    if (info->si_code <= 0) raise(signal);
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
    const auto self = static_cast<pid_t>(syscall(SYS_gettid));
    for (pid_t expected = 0; !g_installer.compare_exchange_weak(expected, self, std::memory_order_acquire); expected = 0) {
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
    if (info == nullptr || context == nullptr || info->si_code <= 0) return false;
    if (info->si_signo != SIGSEGV && info->si_signo != SIGBUS) return false;
    auto& registers = static_cast<ucontext_t*>(context)->uc_mcontext.gregs;
    if (registers[REG_RIP] != reinterpret_cast<greg_t>(GuestFaultCopyAccess)) return false;
    registers[REG_RIP] = reinterpret_cast<greg_t>(GuestFaultCopyResume);
    return true;
}

}
#endif
