#include "prx/libc/include/exceptions/Unwind.hpp"
#include "prx/libc/src/specifics/x86_64/RegisterContext.cpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <exception>
#include <string>
#include <thread>

#if !defined(__x86_64__) || (!defined(__linux__) && !defined(__APPLE__))
#error Register context probes require an x86-64 System V host
#endif

static_assert(sizeof(_Unwind_Context::registers) == 17 * sizeof(std::uintptr_t));
static_assert(offsetof(_Unwind_Context, cfa) == 17 * sizeof(std::uintptr_t));

extern "C" void CaptureProbe(std::uintptr_t*);
extern "C" void RestoreProbe(std::uintptr_t*, std::uintptr_t*);

#ifdef __APPLE__
#define CONTEXT_SYMBOL(name) "_" #name
#else
#define CONTEXT_SYMBOL(name) #name
#endif

asm(
".text\n"
".globl " CONTEXT_SYMBOL(CaptureProbe) "\n"
CONTEXT_SYMBOL(CaptureProbe) ":\n"
"pushq %rbp\npushq %rbx\npushq %r12\npushq %r13\npushq %r14\npushq %r15\n"
"subq $24,%rsp\n"
"movq %rsp,136(%rdi)\n"
"leaq 1f(%rip),%rax\nmovq %rax,144(%rdi)\n"
"movabsq $0x1234567800000100,%rax\n"
"movabsq $0x1234567800000101,%rdx\n"
"movabsq $0x1234567800000102,%rcx\n"
"movabsq $0x1234567800000103,%rbx\n"
"movabsq $0x1234567800000104,%rsi\n"
"movabsq $0x1234567800000106,%rbp\n"
"movabsq $0x1234567800000108,%r8\n"
"movabsq $0x1234567800000109,%r9\n"
"movabsq $0x123456780000010a,%r10\n"
"movabsq $0x123456780000010b,%r11\n"
"movabsq $0x123456780000010c,%r12\n"
"movabsq $0x123456780000010d,%r13\n"
"movabsq $0x123456780000010e,%r14\n"
"movabsq $0x123456780000010f,%r15\n"
"call " CONTEXT_SYMBOL(LibcCaptureRegisters) "\n"
"1:\n"
"addq $24,%rsp\n"
"popq %r15\npopq %r14\npopq %r13\npopq %r12\npopq %rbx\npopq %rbp\nret\n"
".globl " CONTEXT_SYMBOL(RestoreProbe) "\n"
CONTEXT_SYMBOL(RestoreProbe) ":\n"
"pushq %rbp\npushq %rbx\npushq %r12\npushq %r13\npushq %r14\npushq %r15\n"
"subq $24,%rsp\n"
"movq %rsi,0(%rsp)\nmovq %rsp,56(%rdi)\n"
"leaq 2f(%rip),%rax\nmovq %rax,128(%rdi)\n"
"call LRegisterContextRestoreNested\nud2\n"
"2:\n"
"movq 0(%rsp),%r10\n"
"movq %rax,0(%r10)\nmovq %rdx,8(%r10)\nmovq %rcx,16(%r10)\nmovq %rbx,24(%r10)\n"
"movq %rsi,32(%r10)\nmovq %rdi,40(%r10)\nmovq %rbp,48(%r10)\nmovq %rsp,56(%r10)\n"
"movq %r8,64(%r10)\nmovq %r9,72(%r10)\n"
"movq %r12,96(%r10)\nmovq %r13,104(%r10)\nmovq %r14,112(%r10)\nmovq %r15,120(%r10)\n"
"leaq 2b(%rip),%r11\nmovq %r11,128(%r10)\n"
"addq $24,%rsp\n"
"popq %r15\npopq %r14\npopq %r13\npopq %r12\npopq %rbx\npopq %rbp\nret\n"
"LRegisterContextRestoreNested:\n"
"subq $200,%rsp\n"
"xorq %rbx,%rbx\nxorq %rbp,%rbp\nxorq %r12,%r12\nxorq %r13,%r13\nxorq %r14,%r14\nxorq %r15,%r15\n"
"call " CONTEXT_SYMBOL(LibcRestoreRegisters) "\nud2\n"
);

#undef CONTEXT_SYMBOL

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::uintptr_t Guard = 0xa1b2c3d4e5f60718;
constexpr std::uintptr_t SeedsPerRun = 256;

struct Capture {
    std::uintptr_t before = Guard;
    std::array<std::uintptr_t, 17> registers{};
    std::uintptr_t expectedStack = 0;
    std::uintptr_t expectedInstruction = 0;
    std::uintptr_t after = Guard;
};

static_assert(offsetof(Capture, expectedStack) - offsetof(Capture, registers) == 136);
static_assert(offsetof(Capture, expectedInstruction) - offsetof(Capture, registers) == 144);

struct Registers {
    std::uintptr_t before = Guard;
    std::array<std::uintptr_t, 17> values{};
    std::uintptr_t after = Guard;
};

void CheckCapture() {
    Capture captured;
    CaptureProbe(captured.registers.data());
    Require(captured.before == Guard && captured.after == Guard, "capture wrote outside its register context");
    for (std::size_t i = 0; i < 16; ++i) {
        const auto expected = i == 5 ? reinterpret_cast<std::uintptr_t>(captured.registers.data())
            : i == 7 ? captured.expectedStack : 0x1234567800000100 + i;
        RequireEqual(captured.registers[i], expected, "capture saved general register " + std::to_string(i));
    }
    RequireEqual(captured.registers[16], captured.expectedInstruction, "capture saved the return instruction");
    RequireEqual(captured.registers[7] % 16, std::uintptr_t{0}, "capture saved the caller stack, not the callee stack");
}

void CheckRestore(std::uintptr_t seed) {
    Registers restored;
    Registers observed;
    for (std::size_t i = 0; i < restored.values.size(); ++i) {
        restored.values[i] = (0xfedcba9800000000 | (seed << 8) | i);
    }
    RestoreProbe(restored.values.data(), observed.values.data());
    Require(restored.before == Guard && restored.after == Guard, "restore overwrote its source context");
    Require(observed.before == Guard && observed.after == Guard, "restore probe overwrote its destination context");
    for (std::size_t i = 0; i < restored.values.size(); ++i) {
        if (i == 10 || i == 11) continue;
        RequireEqual(observed.values[i], restored.values[i],
            "restore installed landing pad register " + std::to_string(i) + " for seed " + std::to_string(seed));
    }
}

const Case capture{"CaptureRegisters_KnownRegisterValues_SavesCallerContext", [] {
    CheckCapture();
}};

const Case restore{"RestoreRegisters_SeededContexts_InstallsLandingPadContext", [] {
    for (std::uintptr_t seed = 0; seed < SeedsPerRun; ++seed) CheckRestore(seed);
}};

const Case threads{"CaptureAndRestore_ConcurrentThreads_KeepContextsIsolated", [] {
    std::array<std::string, 4> failures;
    std::array<std::thread, 4> workers;
    for (std::size_t i = 0; i < workers.size(); ++i) {
        workers[i] = std::thread([&failures, i] {
            try {
                for (std::uintptr_t step = 0; step < SeedsPerRun; ++step) {
                    CheckCapture();
                    CheckRestore(i * SeedsPerRun + step);
                }
            } catch (const std::exception& error) {
                failures[i] = error.what();
            }
        });
    }
    for (auto& worker : workers) worker.join();
    for (std::size_t i = 0; i < failures.size(); ++i) {
        Require(failures[i].empty(), "thread " + std::to_string(i) + ": " + failures[i]);
    }
}};

} // namespace
