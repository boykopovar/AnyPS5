#include "prx/libSceFiber/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>

namespace {

constexpr std::uint32_t CallerMxcsr = 0x7f80;
constexpr std::uint16_t CallerControl = 0x0b7f;
constexpr std::uint64_t InitializeArgument = 0xfedcba9876543210ull;
constexpr std::uint64_t RunArgument = 0x8123456789abcdefull;
constexpr std::uint64_t ReturnArgument = 0xdedcba9876543210ull;
constexpr std::uint64_t ResumeArgument = 0x923456789abcdef0ull;
constexpr std::uint64_t ResumeReturn = 0xa3456789abcdef01ull;
constexpr std::uint64_t SourceReturn = 0xb456789abcdef012ull;
constexpr std::int32_t FiberErrorInvalid = static_cast<std::int32_t>(0x80590004);

alignas(16) std::uint8_t g_fiberStorage[0x100];
alignas(16) std::uint8_t g_sourceStorage[0x100];
alignas(16) std::uint8_t g_context[65536];
alignas(16) std::uint8_t g_sourceContext[65536];
auto* const g_fiber = reinterpret_cast<FiberObject*>(g_fiberStorage);
auto* const g_source = reinterpret_cast<FiberObject*>(g_sourceStorage);
std::uint32_t g_entryMxcsr;
std::uint16_t g_entryControl;
std::uint64_t g_initialized;
std::uint64_t g_run;
unsigned g_entries;
unsigned g_failures;
const char* g_case;
bool g_contextless;

void Check(bool condition, const char* message) {
    if (!condition) {
        ++g_failures;
        std::fprintf(stderr, "%s %s: %s\n", g_case, g_contextless ? "contextless" : "attached", message);
    }
}

std::uint32_t ReadMxcsr() {
    std::uint32_t result;
    asm volatile("stmxcsr %0" : "=m"(result));
    return result;
}

std::uint16_t ReadControl() {
    std::uint16_t result;
    asm volatile("fnstcw %0" : "=m"(result));
    return result;
}

void WriteControls(std::uint32_t mxcsr, std::uint16_t control) {
    asm volatile("ldmxcsr %0\n\tfldcw %1" : : "m"(mxcsr), "m"(control));
}

void APS5_VABI Entry(std::uint64_t initialized, std::uint64_t run) {
    asm volatile("stmxcsr %0\n\tfnstcw %1" : "=m"(g_entryMxcsr), "=m"(g_entryControl));
    g_initialized = initialized;
    g_run = run;
    ++g_entries;
    WriteControls(0x1f80, 0x037f);
    std::uint64_t received = 0;
    const auto result = sceFiberReturnToThread(ReturnArgument, &received);
    std::uint32_t resumedMxcsr;
    std::uint16_t resumedControl;
    asm volatile("stmxcsr %0\n\tfnstcw %1" : "=m"(resumedMxcsr), "=m"(resumedControl));
    Check(result == 0, "return code after attached resume");
    Check(received == ResumeArgument, "64-bit attached resume argument");
    Check(resumedMxcsr == 0x1f80, "attached resume preserves changed MXCSR");
    Check(resumedControl == 0x037f, "attached resume preserves changed x87 control");
    for (;;) sceFiberReturnToThread(ResumeReturn, nullptr);
}

void APS5_VABI SwitchSource(std::uint64_t, std::uint64_t) {
    WriteControls(CallerMxcsr, CallerControl);
    std::uint64_t received = 0;
    const auto result = sceFiberSwitch(g_fiber, RunArgument, &received);
    Check(result == 0, "switch code after source resume");
    Check(received == ResumeArgument, "64-bit switch resume argument");
    for (;;) sceFiberReturnToThread(SourceReturn, nullptr);
}

struct TestCase {
    const char* name;
    bool internal;
    std::uint32_t version;
    std::uint32_t flags;
    std::uint32_t expectedMxcsr;
    std::uint16_t expectedControl;
};

void CheckCaller() {
    const auto mxcsr = ReadMxcsr();
    const auto control = ReadControl();
    Check(mxcsr == CallerMxcsr, "caller MXCSR restoration");
    Check(control == CallerControl, "caller x87 control restoration");
}

void CheckEntry(const TestCase& test, unsigned entries) {
    if (g_entryMxcsr != test.expectedMxcsr || g_entryControl != test.expectedControl) {
        ++g_failures;
        std::fprintf(stderr, "%s %s: entry MXCSR=0x%08x x87=0x%04x, expected MXCSR=0x%08x x87=0x%04x\n",
            g_case, g_contextless ? "contextless" : "attached", g_entryMxcsr,
            static_cast<unsigned>(g_entryControl), test.expectedMxcsr, static_cast<unsigned>(test.expectedControl));
    }
    Check(g_initialized == InitializeArgument, "64-bit initialize argument");
    Check(g_run == RunArgument, "64-bit run argument");
    Check(g_entries == entries, "entry count");
}

void RunCase(const TestCase& test, bool contextless) {
    g_case = test.name;
    g_contextless = contextless;
    g_entries = 0;
    g_entryMxcsr = 0;
    g_entryControl = 0;
    g_initialized = 0;
    g_run = 0;
    WriteControls(CallerMxcsr, CallerControl);
    void* context = contextless ? nullptr : g_context;
    const auto size = contextless ? 0 : sizeof(g_context);
    const auto initialized = test.internal
        ? _sceFiberInitializeWithInternalOptionImpl(g_fiber, test.name, Entry, InitializeArgument, context, size, nullptr, test.flags, test.version)
        : _sceFiberInitializeImpl_nid_postfix(g_fiber, test.name, Entry, InitializeArgument, context, size, nullptr, test.version);
    CheckCaller();
    Check(initialized == 0, "initialize code");
    if (initialized != 0) return;
    if ((test.flags & 0x10) != 0 && !contextless) {
        FiberInfo info{};
        info.size = sizeof(info);
        Check(sceFiberGetInfo(g_fiber, &info) == 0, "flag 0x10 info code");
        Check(info.size_context_margin == sizeof(g_context), "flag 0x10 context margin");
    }
    constexpr unsigned runs = 2;
    for (unsigned run = 0; run < runs; ++run) {
        WriteControls(CallerMxcsr, CallerControl);
        std::uint64_t returned = 0;
        const auto result = sceFiberRun_nid_postfix(g_fiber, contextless || run == 0 ? RunArgument : ResumeArgument, &returned);
        CheckCaller();
        Check(result == 0, "run code");
        Check(returned == (contextless || run == 0 ? ReturnArgument : ResumeReturn), "64-bit return argument");
        CheckEntry(test, contextless ? run + 1 : 1);
    }
    if (contextless) {
        const auto sourceInitialized = _sceFiberInitializeImpl_nid_postfix(g_source, "switch source", SwitchSource, 0, g_sourceContext, sizeof(g_sourceContext), nullptr, 0x034fffff);
        Check(sourceInitialized == 0, "switch source initialize code");
        if (sourceInitialized == 0) {
            WriteControls(CallerMxcsr, CallerControl);
            std::uint64_t returned = 0;
            const auto result = sceFiberRun_nid_postfix(g_source, 0, &returned);
            CheckCaller();
            Check(result == 0, "switch source run code");
            Check(returned == ReturnArgument, "switch 64-bit return argument");
            CheckEntry(test, runs + 1);
            returned = 0;
            WriteControls(CallerMxcsr, CallerControl);
            const auto resumed = sceFiberRun_nid_postfix(g_source, ResumeArgument, &returned);
            CheckCaller();
            Check(resumed == 0, "switch source resume code");
            Check(returned == SourceReturn, "64-bit switch source return argument");
            Check(sceFiberFinalize(g_source) == 0, "switch source finalize code");
            Check(sceFiberFinalize(g_source) == FiberErrorInvalid, "switch source finalized state");
        }
    }
    Check(sceFiberFinalize(g_fiber) == 0, "finalize code");
    Check(sceFiberFinalize(g_fiber) == FiberErrorInvalid, "finalized state");
}

}

int main() {
    const auto originalMxcsr = ReadMxcsr();
    const auto originalControl = ReadControl();
    constexpr TestCase cases[] = {
        {"public below", false, 0x034fffff, 0, 0x7f80, 0x0b7f},
        {"public at", false, 0x03500000, 0, 0x9fc0, 0x037f},
        {"public above", false, 0x03500001, 0, 0x9fc0, 0x037f},
        {"internal below", true, 0x034fffff, 0, 0x7f80, 0x0b7f},
        {"internal at", true, 0x03500000, 0, 0x9fc0, 0x037f},
        {"internal above", true, 0x03500001, 0, 0x9fc0, 0x037f},
        {"explicit below", true, 0x034fffff, 0x100, 0x9fc0, 0x037f},
        {"margin below", true, 0x034fffff, 0x10, 0x7f80, 0x0b7f},
        {"margin at", true, 0x03500000, 0x10, 0x9fc0, 0x037f},
    };
    for (const auto& test : cases) {
        RunCase(test, false);
        RunCase(test, true);
    }
    WriteControls(originalMxcsr, originalControl);
    return g_failures == 0 ? 0 : 1;
}
