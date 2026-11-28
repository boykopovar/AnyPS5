#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/GuestStacks.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/System/include/UserContext.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

using ContextFunction = void (APS5_VABI*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);

constexpr int MaximumArguments = 6;
constexpr std::uint64_t MinimumStackSize = 2048;
constexpr std::size_t SignalInfoSize = 80;
constexpr int SignalArguments = 3;
constexpr int MaximumSignal = 128;
constexpr std::uint64_t FloatingPointFormatNone = 0x10000;
constexpr std::uint64_t FloatingPointOwnedNone = 0x20000;

}

extern "C" {

#if defined(_WIN32) && defined(__SEH__)
[[noreturn]] void APS5_VABI Aps5MakecontextStart_nid_no_patch(UserUcontext* context, ContextFunction function, const std::uint64_t* arguments);

[[noreturn]] void APS5_VABI Aps5MakecontextFinish_nid_no_patch(UserUcontext* context) {
    if (context->link == nullptr) LibcExit_nid_no_patch(0);
    setcontext_nid_postfix(context->link);

    std::abort();
}

asm(R"(
.text
.globl Aps5MakecontextStart_nid_no_patch
.def Aps5MakecontextStart_nid_no_patch; .scl 2; .type 32; .endef
.seh_proc Aps5MakecontextStart_nid_no_patch
Aps5MakecontextStart_nid_no_patch:
    pushq %rbp
    .seh_pushreg %rbp
    subq $48, %rsp
    .seh_stackalloc 48
    leaq 48(%rsp), %rbp
    .seh_setframe %rbp, 48
    .seh_endprologue
    movq %rdi, -8(%rbp)
    movq %rsi, %rax
    movq %rdx, %r10
    movq (%r10), %rdi
    movq 8(%r10), %rsi
    movq 16(%r10), %rdx
    movq 24(%r10), %rcx
    movq 32(%r10), %r8
    movq 40(%r10), %r9
    call *%rax
    movq -8(%rbp), %rdi
    call Aps5MakecontextFinish_nid_no_patch
    ud2
.seh_endproc
)");
#else
[[noreturn]] void APS5_VABI Aps5MakecontextStart_nid_no_patch(UserUcontext* context, ContextFunction function, const std::uint64_t* arguments) {
    function(arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5]);
    if (context->link == nullptr) LibcExit_nid_no_patch(0);
    setcontext_nid_postfix(context->link);

    std::abort();
}
#endif

void APS5_VABI makecontext_nid_postfix(UserUcontext* context, void (APS5_VABI* start)(), int count, ...) {
    if (context == nullptr || context->mcontext.len != sizeof(UserMcontext)) return;
    if (count < 0 || count > MaximumArguments || context->stackPointer == nullptr || context->stackSize < MinimumStackSize) {
        context->mcontext.len = 0;
        return;
    }
    const auto top = (reinterpret_cast<std::uintptr_t>(context->stackPointer) + context->stackSize) & ~static_cast<std::uintptr_t>(15);
    auto* arguments = reinterpret_cast<std::uint64_t*>(top) - MaximumArguments;
    auto* stack = arguments - 1;
#ifdef _WIN32
    __builtin_sysv_va_list list;
    __builtin_sysv_va_start(list, count);
    for (int index = 0; index < count; ++index) arguments[index] = __builtin_va_arg(list, std::uint64_t);
    __builtin_sysv_va_end(list);
#else
    std::va_list list;
    va_start(list, count);
    for (int index = 0; index < count; ++index) arguments[index] = va_arg(list, std::uint64_t);
    va_end(list);
#endif
    for (int index = count; index < MaximumArguments; ++index) arguments[index] = 0;
    *stack = 0;
    auto& machine = context->mcontext;
    machine.rdi = reinterpret_cast<std::uint64_t>(context);
    machine.rsi = reinterpret_cast<std::uint64_t>(start);
    machine.rdx = reinterpret_cast<std::uint64_t>(arguments);
    machine.rbp = 0;
    machine.rbx = reinterpret_cast<std::uint64_t>(stack);
    machine.rsp = reinterpret_cast<std::uint64_t>(stack);
    machine.rip = reinterpret_cast<std::uint64_t>(&Aps5MakecontextStart_nid_no_patch);
    GuestStacks::GuestStackRegister_nid_no_patch(context->stackPointer, context->stackSize);
}

int APS5_VABI signalcontext_nid_postfix(UserUcontext* context, int signal, ContextFunction handler) {
    if (context == nullptr) std::abort();
    auto stack = (context->mcontext.rsp - 128 - sizeof(UserUcontext)) & ~static_cast<std::uint64_t>(15);
    auto* saved = reinterpret_cast<UserUcontext*>(stack);
    std::memcpy(saved, context, sizeof(UserUcontext));
    stack = (stack - SignalInfoSize) & ~static_cast<std::uint64_t>(15);
    auto* info = reinterpret_cast<std::uint8_t*>(stack);
    std::memset(info, 0, SignalInfoSize);
    std::memcpy(info, &signal, sizeof(signal));
    stack -= SignalArguments * sizeof(std::uint64_t);
    auto* arguments = reinterpret_cast<std::uint64_t*>(stack);
    arguments[0] = static_cast<std::uint64_t>(static_cast<std::int64_t>(signal));
    arguments[1] = reinterpret_cast<std::uint64_t>(info);
    arguments[2] = reinterpret_cast<std::uint64_t>(saved);
    stack -= 16;
    *reinterpret_cast<std::uint64_t*>(stack) = 0;
    std::memset(&context->mcontext, 0, sizeof(UserMcontext));
    context->mcontext.fpformat = FloatingPointFormatNone;
    context->mcontext.ownedfp = FloatingPointOwnedNone;
    context->link = saved;
    if (signal >= 1 && signal <= MaximumSignal) context->sigmask[(signal - 1) >> 5] &= ~(1u << ((signal - 1) & 31));
    auto& machine = context->mcontext;
    machine.len = sizeof(UserMcontext);
    machine.rdi = reinterpret_cast<std::uint64_t>(context);
    machine.rsi = reinterpret_cast<std::uint64_t>(handler);
    machine.rdx = reinterpret_cast<std::uint64_t>(arguments);
    machine.rbp = stack;
    machine.rbx = machine.rbp;
    machine.rsp = machine.rbp;
    machine.rip = reinterpret_cast<std::uint64_t>(&Aps5MakecontextStart_nid_no_patch);
    return 0;
}

}
