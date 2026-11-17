#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/System/include/UserContext.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using SignalHandler = void (APS5_VABI*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

namespace {

constexpr int GuestSigBlock = 1;
constexpr int GuestSigSetmask = 3;
constexpr int Signal = 5;
constexpr std::size_t StackBytes = 256 * 1024;
constexpr std::size_t SignalInfoBytes = 80;
constexpr std::uint32_t SignalBit = 0x10;
constexpr std::uint32_t OtherBit = 0x20;
constexpr std::uint32_t SecondWordBit = 0x8000;
constexpr std::uint32_t LastBit = 0x80000000u;

struct alignas(16) Stack {
    unsigned char bytes[StackBytes];
};

Stack g_stack;
UserUcontext g_main = {};
UserUcontext g_context = {};
UserUcontext g_snapshot = {};
std::uint32_t g_original[4] = {};
int g_handlerRuns = 0;
std::uint64_t g_handlerArguments[3] = {};
std::uint32_t g_handlerMask = 0;
bool g_handlerAligned = false;
bool g_handlerInfoClean = false;
bool g_handlerCopySane = false;

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "Signal context check failed: %s\n", what);
        std::abort();
    }
}

std::uint64_t StackTop() {
    return (reinterpret_cast<std::uint64_t>(g_stack.bytes) + StackBytes) & ~static_cast<std::uint64_t>(15);
}

}

extern "C" void APS5_VABI Handler(std::uint64_t first, std::uint64_t second, std::uint64_t third) {
    ++g_handlerRuns;
    g_handlerArguments[0] = first;
    g_handlerArguments[1] = second;
    g_handlerArguments[2] = third;
    std::uint32_t current[4] = {};
    sigprocmask_nid_postfix(GuestSigBlock, nullptr, current);
    g_handlerMask = current[0];
    g_handlerAligned = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)) % 16 == 0;
    const auto* info = reinterpret_cast<const std::uint8_t*>(second);
    std::int32_t number = 0;
    std::memcpy(&number, info, sizeof(number));
    g_handlerInfoClean = number == Signal;
    for (std::size_t index = sizeof(number); index < SignalInfoBytes; ++index) g_handlerInfoClean = g_handlerInfoClean && info[index] == 0;
    g_handlerCopySane = std::memcmp(reinterpret_cast<const void*>(third), &g_snapshot, sizeof(UserUcontext)) == 0;
}

extern "C" void APS5_VABI Resumed() {
    Require(g_handlerRuns == 1, "the handler ran once before uc_link was resumed");
    std::uint32_t current[4] = {};
    Require(sigprocmask_nid_postfix(GuestSigBlock, nullptr, current) == 0 && current[0] == (SignalBit | OtherBit), "resuming uc_link restores the original mask");
    Require(sigprocmask_nid_postfix(GuestSigSetmask, g_original, nullptr) == 0, "restore the signal mask");
    setcontext_nid_postfix(&g_main);

    Require(false, "setcontext into the main context returned");
}

namespace {

SignalHandler HandlerPointer() {
    return reinterpret_cast<SignalHandler>(&Handler);
}

void PrepareContext(UserUcontext* context) {
    Require(getcontext_nid_postfix(context) == 0, "capture the context template");
    context->mcontext.rsp = StackTop() - 8;
    context->mcontext.rip = reinterpret_cast<std::uint64_t>(&Resumed);
    context->mcontext.rdi = 0x1111;
    context->mcontext.r12 = 0x2222;
    context->mcontext.rax = 0x3333;
    context->link = reinterpret_cast<UserUcontext*>(0x4444);
    context->sigmask[0] = SignalBit | OtherBit;
    context->sigmask[1] = SecondWordBit;
    context->sigmask[3] = LastBit;
}

std::uint64_t CopyAddress(std::uint64_t rsp) {
    return (rsp - 128 - sizeof(UserUcontext)) & ~static_cast<std::uint64_t>(15);
}

std::uint64_t InfoAddress(std::uint64_t rsp) {
    return (CopyAddress(rsp) - SignalInfoBytes) & ~static_cast<std::uint64_t>(15);
}

void CheckLayout() {
    PrepareContext(&g_context);
    g_snapshot = g_context;
    const auto rsp = g_context.mcontext.rsp;
    Require(signalcontext_nid_postfix(&g_context, Signal, HandlerPointer()) == 0, "signalcontext returns 0");
    const auto copy = CopyAddress(rsp);
    const auto info = InfoAddress(rsp);
    const auto arguments = info - 3 * sizeof(std::uint64_t);
    const auto start = arguments - 16;
    Require(reinterpret_cast<std::uint64_t>(g_context.link) == copy, "uc_link points at the copy placed 128 bytes below the old stack pointer");
    Require(std::memcmp(g_context.link, &g_snapshot, sizeof(UserUcontext)) == 0, "the copy is the context as it was");
    const auto& machine = g_context.mcontext;
    Require(machine.len == sizeof(UserMcontext) && machine.fpformat == 0x10000 && machine.ownedfp == 0x20000, "the new machine context sets mc_len, mc_fpformat and mc_ownedfp");
    Require(machine.rdi == reinterpret_cast<std::uint64_t>(&g_context) && machine.rsi == reinterpret_cast<std::uint64_t>(&Handler) && machine.rdx == arguments, "the entry gets the context, the handler and the argument block");
    Require(machine.rsp == start && machine.rbp == start && machine.rbx == start && (machine.rsp + 8) % 16 == 0, "the stack pointer sits 40 bytes below the info block with an odd alignment");
    Require(machine.rax == 0 && machine.r12 == 0 && machine.rcx == 0 && machine.rflags == 0 && machine.trapno == 0 && machine.fsbase == 0, "the other machine fields are zeroed");
    for (const auto word : machine.fpstate) Require(word == 0, "the floating point image is zeroed");
    const auto* block = reinterpret_cast<const std::uint64_t*>(arguments);
    Require(block[0] == Signal && block[1] == info && block[2] == copy, "the argument block holds the signal, the info and the copy");
    Require(g_context.sigmask[0] == OtherBit && g_context.sigmask[1] == SecondWordBit && g_context.sigmask[3] == LastBit, "the signal leaves the mask");
    Require(g_context.stackPointer == g_snapshot.stackPointer && g_context.stackSize == g_snapshot.stackSize && g_context.flags == g_snapshot.flags, "uc_stack and uc_flags stay");
}

void CheckSignalRange() {
    for (const int signal : {0, -1, 129}) {
        PrepareContext(&g_context);
        const auto rsp = g_context.mcontext.rsp;
        Require(signalcontext_nid_postfix(&g_context, signal, HandlerPointer()) == 0, "signalcontext accepts a number outside 1 to 128");
        Require(g_context.sigmask[0] == (SignalBit | OtherBit) && g_context.sigmask[1] == SecondWordBit && g_context.sigmask[3] == LastBit, "a number outside 1 to 128 leaves the mask alone");
        std::int32_t number = 0;
        std::memcpy(&number, reinterpret_cast<const void*>(InfoAddress(rsp)), sizeof(number));
        Require(number == signal, "the info carries the number as given");
    }
    PrepareContext(&g_context);
    Require(signalcontext_nid_postfix(&g_context, 128, HandlerPointer()) == 0 && g_context.sigmask[3] == 0, "signal 128 is the last bit of the mask");
    PrepareContext(&g_context);
    g_context.sigmask[1] = LastBit;
    Require(signalcontext_nid_postfix(&g_context, 64, HandlerPointer()) == 0 && g_context.sigmask[1] == 0 && g_context.sigmask[3] == LastBit, "signal 64 is the last bit of the second word");
}

void CheckRun() {
    Require(sigprocmask_nid_postfix(GuestSigBlock, nullptr, g_original) == 0, "read the signal mask");
    volatile int phase = 0;
    Require(getcontext_nid_postfix(&g_main) == 0, "capture the main context");
    if (phase == 0) {
        phase = 1;
        PrepareContext(&g_context);
        g_snapshot = g_context;
        Require(signalcontext_nid_postfix(&g_context, Signal, HandlerPointer()) == 0, "signalcontext prepares the handler");
        const std::uint32_t mask[4] = {SignalBit | OtherBit, 0, 0, 0};
        Require(sigprocmask_nid_postfix(GuestSigSetmask, mask, nullptr) == 0, "block the signals before the switch");
        setcontext_nid_postfix(&g_context);

        Require(false, "setcontext into the handler returned");
    }
    Require(g_handlerRuns == 1, "the handler ran once");
    Require(g_handlerArguments[0] == Signal && g_handlerArguments[1] != 0, "the handler gets the signal and the info");
    Require(g_handlerArguments[2] == reinterpret_cast<std::uint64_t>(g_context.link), "the third argument is the copy");
    Require(g_handlerInfoClean, "the info block holds only the signal number");
    Require(g_handlerCopySane, "the copy the handler sees is the context as it was");
    Require(g_handlerMask == OtherBit, "the handler runs without the signal in the mask");
    Require(g_handlerAligned, "the handler starts with an aligned stack");
}

}

int main() {
    CheckLayout();
    CheckSignalRange();
    CheckRun();
    return 0;
}
