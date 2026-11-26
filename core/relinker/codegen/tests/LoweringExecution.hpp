#ifndef CODEGEN_TESTS_LOWERINGEXECUTION_HPP
#define CODEGEN_TESTS_LOWERINGEXECUTION_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <vector>
#if (defined(__linux__) || defined(__APPLE__)) && defined(__x86_64__)
#include <csetjmp>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#if !defined(__APPLE__)
#include <ucontext.h>
#endif
#define LOWERING_EXECUTION_AVAILABLE 1
#elif defined(_WIN32) && defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <windows.h>
#undef far
#undef near
#define LOWERING_EXECUTION_AVAILABLE 1
#else
#define LOWERING_EXECUTION_AVAILABLE 0
#endif

#if LOWERING_EXECUTION_AVAILABLE && !defined(_WIN32)
#define LOWERING_FAULT_AVAILABLE 1
#else
#define LOWERING_FAULT_AVAILABLE 0
#endif

namespace LoweringExecution {

using Bytes = std::vector<std::uint8_t>;

struct State {
    std::array<std::uint64_t, 16> Input{};
    std::array<std::uint64_t, 16> Output{};
    std::uint64_t Flags = 0;
    std::array<std::uint64_t, 16> RedZone{};
    std::uint64_t InputFlags = 0;
};

inline constexpr std::uint64_t kSentinel = 0xBADC0FFEE0DDF00Dull;
inline constexpr std::uint64_t kFlagMask = 0x8C1u;
inline constexpr std::uint64_t kCarry = 0x1u;
inline constexpr std::uint64_t kZero = 0x40u;
inline constexpr std::uint64_t kSign = 0x80u;

inline constexpr std::uint64_t kBaseFlags = 0x202u;

inline State InitialState() {
    State state;
    for (std::size_t reg = 0; reg < 16; ++reg) state.Input[reg] = 0xABCD123456789000ull + reg * 0x1234567ull;
    state.InputFlags = kBaseFlags | kFlagMask;
    return state;
}

#if LOWERING_EXECUTION_AVAILABLE

#if defined(_WIN32)
inline std::uint8_t* AllocatePages(std::size_t size) {
    return static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
}

inline bool ProtectPages(std::uint8_t* pages, std::size_t size, bool executable, bool writable) {
    DWORD previous = 0;
    const DWORD protection = executable ? PAGE_EXECUTE_READ : writable ? PAGE_READWRITE : PAGE_NOACCESS;
    return VirtualProtect(pages, size, protection, &previous) != 0 && (!executable || FlushInstructionCache(GetCurrentProcess(), pages, size) != 0);
}

inline void FreePages(std::uint8_t* pages, std::size_t) {
    if (pages) VirtualFree(pages, 0, MEM_RELEASE);
}
#else
inline std::uint8_t* AllocatePages(std::size_t size) {
    void* pages = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return pages == MAP_FAILED ? nullptr : static_cast<std::uint8_t*>(pages);
}

inline bool ProtectPages(std::uint8_t* pages, std::size_t size, bool executable, bool writable) {
    const int protection = executable ? PROT_READ | PROT_EXEC : writable ? PROT_READ | PROT_WRITE : PROT_NONE;
    return mprotect(pages, size, protection) == 0;
}

inline void FreePages(std::uint8_t* pages, std::size_t size) {
    if (pages) munmap(pages, size);
}
#endif

inline std::size_t PageSize() {
#if defined(_WIN32)
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
}

inline std::size_t AppendHostEntry(Bytes& bytes) {
#if defined(_WIN32)
    constexpr std::uint32_t kFrame = 8 + 10 * 16;
    const auto append32 = [&bytes](std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    };
    const auto moveXmm = [&](std::uint8_t opcode, unsigned reg, std::uint32_t displacement) {
        if (reg >= 8) bytes.push_back(0x44);
        bytes.insert(bytes.end(), {0x0F, opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24});
        append32(displacement);
    };
    const auto entry = bytes.size();
    bytes.insert(bytes.end(), {0x56, 0x57, 0x48, 0x81, 0xEC});
    append32(kFrame);
    for (unsigned reg = 6; reg < 16; ++reg) moveXmm(0x11, reg, (reg - 6) * 16);
    bytes.insert(bytes.end(), {0x48, 0x89, 0xCF, 0xE8});
    append32(static_cast<std::uint32_t>(static_cast<std::int32_t>(0 - static_cast<std::int64_t>(bytes.size() + 4))));
    for (unsigned reg = 6; reg < 16; ++reg) moveXmm(0x10, reg, (reg - 6) * 16);
    bytes.insert(bytes.end(), {0x48, 0x81, 0xC4});
    append32(kFrame);
    bytes.insert(bytes.end(), {0x5F, 0x5E, 0xC3});
    return entry;
#else
    (void)bytes;
    return 0;
#endif
}

inline void Call(const std::uint8_t* code, std::size_t entry, void* argument) { reinterpret_cast<void (*)(void*)>(const_cast<std::uint8_t*>(code + entry))(argument); }

#if defined(_WIN32)
inline std::uint8_t*& ProbeCode() {
    static std::uint8_t* code = nullptr;
    return code;
}

inline std::size_t& ProbeLength() {
    static std::size_t length = 0;
    return length;
}

inline bool& ProbeFaulted() {
    static bool faulted = false;
    return faulted;
}

inline LONG CALLBACK ProbeHandler(EXCEPTION_POINTERS* info) {
    const EXCEPTION_RECORD* const record = info->ExceptionRecord;
    const auto* const address = static_cast<const std::uint8_t*>(record->ExceptionAddress);
    if (record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION || !ProbeCode() || address < ProbeCode() || address >= ProbeCode() + ProbeLength()) return EXCEPTION_CONTINUE_SEARCH;
    ProbeFaulted() = true;
    info->ContextRecord->Rip = reinterpret_cast<DWORD64>(ProbeCode() + ProbeLength());
    return EXCEPTION_CONTINUE_EXECUTION;
}

inline bool HostExecutes(const Bytes& instruction) {
    std::uint8_t* const code = AllocatePages(4096);
    if (!code) throw std::runtime_error("Cannot allocate the instruction probe page");
    std::memcpy(code, instruction.data(), instruction.size());
    code[instruction.size()] = 0xC3;
    if (!ProtectPages(code, 4096, true, false)) {
        FreePages(code, 4096);
        throw std::runtime_error("Cannot make the instruction probe executable");
    }
    ProbeCode() = code;
    ProbeLength() = instruction.size();
    ProbeFaulted() = false;
    void* const handler = AddVectoredExceptionHandler(1, ProbeHandler);
    alignas(64) std::uint8_t scratch[64] = {};
    asm volatile(
        "sub $128, %%rsp\n\t"
        "call *%[code]\n\t"
        "add $128, %%rsp\n\t"
        :
        : [code] "r"(code), "D"(scratch)
        : "rax", "rcx", "rdx", "xmm0", "memory", "cc");
    if (handler) RemoveVectoredExceptionHandler(handler);
    ProbeCode() = nullptr;
    FreePages(code, 4096);
    return !ProbeFaulted();
}
#else
inline sigjmp_buf& ProbeJump() {
    static sigjmp_buf jump;
    return jump;
}

inline bool HostExecutes(const Bytes& instruction) {
    auto* const code = static_cast<std::uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (code == MAP_FAILED) throw std::runtime_error("Cannot allocate the instruction probe page");
    std::memcpy(code, instruction.data(), instruction.size());
    code[instruction.size()] = 0xC3;
    if (mprotect(code, 4096, PROT_READ | PROT_EXEC) != 0) {
        munmap(code, 4096);
        throw std::runtime_error("Cannot make the instruction probe executable");
    }
    alignas(64) std::uint8_t scratch[64] = {};
    struct sigaction action {};
    action.sa_handler = [](int) { siglongjmp(ProbeJump(), 1); };
    sigemptyset(&action.sa_mask);
    struct sigaction previous {};
    sigaction(SIGILL, &action, &previous);
    volatile bool executed = false;
    if (sigsetjmp(ProbeJump(), 1) == 0) {
        asm volatile(
            "sub $128, %%rsp\n\t"
            "call *%[code]\n\t"
            "add $128, %%rsp\n\t"
            :
            : [code] "r"(code), "D"(scratch)
            : "rax", "rcx", "rdx", "xmm0", "memory", "cc");
        executed = true;
    }
    sigaction(SIGILL, &previous, nullptr);
    munmap(code, 4096);
    return executed;
}
#endif

class Execution {
public:
    using Emit = std::function<void(Codegen::StubBodyBuilder&, const Bytes& site)>;

    Execution(const Bytes& site, std::uint64_t memoryValue, bool ripRelative, const Emit& emit, std::size_t trailing = 0) {
        for (auto reg : {3, 5, 12, 13, 14, 15}) push(static_cast<std::uint8_t>(reg));
        memory(0x8D, 4, 4, -128);
        rip({0x48, 0x89, 0x3D}, 0);
        rip({0x48, 0x89, 0x25}, 8);
        memory(0x89, 4, 7, 32);
        memory(0xFF, 6, 7, 49 * 8);
        bytes.push_back(0x9D);
        bytes.insert(bytes.end(), {0x48, 0xB8});
        append64(kSentinel);
        for (int displacement = -128; displacement < 0; displacement += 8)
            memory(0x89, 0, 4, displacement);
        memory(0x8B, 0, 7, 16 * 8);
        memory(0x89, 0, 4, 64);
        for (std::uint8_t reg = 0; reg < 16; ++reg)
            if (reg != 4 && reg != 7) memory(0x8B, reg, 7, reg * 8);
        memory(0x8B, 7, 7, 7 * 8);
        const auto siteOffset = bytes.size();
        auto relocated = site;
        if (ripRelative) {
            const auto displacement = static_cast<std::int32_t>(4096 + 24 - (siteOffset + site.size()));
            std::memcpy(relocated.data() + relocated.size() - 4 - trailing, &displacement, sizeof(displacement));
        }
        Codegen::StubBodyBuilder builder;
        emit(builder, relocated);
        auto body = builder.Finish();
        const auto stubOffset = bytes.size();
        Codegen::ApplyStubRelocations(body.Bytes, body.Relocations, siteOffset, stubOffset, 0);
        bytes.insert(bytes.end(), body.Bytes.begin(), body.Bytes.begin() + static_cast<std::ptrdiff_t>(body.ReturnBranchOffset));
        rip({0x48, 0x89, 0x25}, 16);
        rip({0x48, 0x8B, 0x25}, 8);
        memory(0x8D, 4, 4, -256);
        for (std::uint8_t reg = 0; reg < 16; ++reg) if (reg != 4) push(reg);
        bytes.push_back(0x9C);
        rip({0x48, 0x8B, 0x3D}, 0);
        memory(0x8B, 0, 4, 0);
        memory(0x89, 0, 7, 32 * 8);
        int offset = 8;
        for (int reg = 15; reg >= 0; --reg) {
            if (reg == 4) continue;
            memory(0x8B, 0, 4, offset);
            memory(0x89, 0, 7, (16 + reg) * 8);
            offset += 8;
        }
        rip({0x48, 0x8B, 0x05}, 16);
        memory(0x89, 0, 7, 20 * 8);
        for (int slot = 0; slot < 16; ++slot) {
            memory(0x8B, 0, 4, 256 + 128 - 128 + slot * 8);
            memory(0x89, 0, 7, (33 + slot) * 8);
        }
        rip({0x48, 0x8B, 0x25}, 8);
        memory(0x8D, 4, 4, 128);
        for (auto reg : {15, 14, 13, 12, 5, 3}) pop(static_cast<std::uint8_t>(reg));
        bytes.push_back(0xC3);
        entry = AppendHostEntry(bytes);
        if (bytes.size() >= 4096) throw std::runtime_error("Execution wrapper exceeded its code page");
        code = AllocatePages(8192);
        if (!code) throw std::runtime_error("Cannot allocate execution test pages");
        std::memcpy(code, bytes.data(), bytes.size());
        std::memcpy(code + 4096 + 24, &memoryValue, sizeof(memoryValue));
        if (!ProtectPages(code, 4096, true, false)) throw std::runtime_error("Cannot make lowering test code executable");
    }

    ~Execution() { FreePages(code, 8192); }
    Execution(const Execution&) = delete;
    Execution& operator=(const Execution&) = delete;

    void Run(State& state) const { Call(code, entry, &state); }
    const void* Memory() const { return code + 4096 + 24; }
    const std::uint8_t* Code() const { return code; }
    std::uint64_t StackPointer() const {
        std::uint64_t value = 0;
        std::memcpy(&value, code + 4096 + 8, sizeof(value));
        return value;
    }

private:
    Bytes bytes;
    std::uint8_t* code = nullptr;
    std::size_t entry = 0;

    void append32(std::int32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
    }
    void append64(std::uint64_t value) {
        for (unsigned shift = 0; shift < 64; shift += 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
    void rip(std::initializer_list<std::uint8_t> opcode, int slot) {
        bytes.insert(bytes.end(), opcode);
        append32(static_cast<std::int32_t>(4096 + slot - (bytes.size() + 4)));
    }
    void push(std::uint8_t reg) {
        if (reg >= 8) bytes.push_back(0x41);
        bytes.push_back(static_cast<std::uint8_t>(0x50 | (reg & 7)));
    }
    void pop(std::uint8_t reg) {
        if (reg >= 8) bytes.push_back(0x41);
        bytes.push_back(static_cast<std::uint8_t>(0x58 | (reg & 7)));
    }
    void memory(std::uint8_t opcode, std::uint8_t reg, std::uint8_t base, int displacement) {
        bytes.insert(bytes.end(), {static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1) | ((base & 8) >> 3)), opcode,
            static_cast<std::uint8_t>(0x80 | ((reg & 7) << 3) | (base & 7))});
        if ((base & 7) == 4) bytes.push_back(0x24);
        append32(displacement);
    }
};

#if LOWERING_FAULT_AVAILABLE

enum class FaultAccess { Load, Store };

struct FaultObservation {
    std::array<std::uint64_t, 16> Registers{};
    std::uint64_t Address = 0;
    std::uint64_t Instruction = 0;
    std::uint64_t Stack = 0;
    std::uint64_t Page = 0;
    std::uint64_t PageLength = 0;
    std::size_t Faults = 0;
    int Signal = 0;
    bool Returned = false;
};

struct FaultProbe {
    sigjmp_buf Jump{};
    FaultObservation Observation{};
    std::uint64_t Code = 0;
    std::uint64_t Length = 0;
};

inline FaultProbe& FaultProbeState() {
    static FaultProbe probe;
    return probe;
}

inline FaultObservation FaultContext(const int number, const siginfo_t& info, const ucontext_t& context) {
    FaultObservation observation;
    observation.Signal = number;
    observation.Address = reinterpret_cast<std::uint64_t>(info.si_addr);
#if defined(__APPLE__)
    const auto& state = context.uc_mcontext->__ss;
    observation.Registers = {state.__rax, state.__rcx, state.__rdx, state.__rbx, state.__rsp, state.__rbp,
        state.__rsi, state.__rdi, state.__r8, state.__r9, state.__r10, state.__r11, state.__r12, state.__r13,
        state.__r14, state.__r15};
    observation.Instruction = static_cast<std::uint64_t>(state.__rip);
    observation.Stack = static_cast<std::uint64_t>(state.__rsp);
#else
    const auto* const state = context.uc_mcontext.gregs;
    observation.Registers = {static_cast<std::uint64_t>(state[REG_RAX]), static_cast<std::uint64_t>(state[REG_RCX]),
        static_cast<std::uint64_t>(state[REG_RDX]), static_cast<std::uint64_t>(state[REG_RBX]),
        static_cast<std::uint64_t>(state[REG_RSP]), static_cast<std::uint64_t>(state[REG_RBP]),
        static_cast<std::uint64_t>(state[REG_RSI]), static_cast<std::uint64_t>(state[REG_RDI]),
        static_cast<std::uint64_t>(state[REG_R8]), static_cast<std::uint64_t>(state[REG_R9]),
        static_cast<std::uint64_t>(state[REG_R10]), static_cast<std::uint64_t>(state[REG_R11]),
        static_cast<std::uint64_t>(state[REG_R12]), static_cast<std::uint64_t>(state[REG_R13]),
        static_cast<std::uint64_t>(state[REG_R14]), static_cast<std::uint64_t>(state[REG_R15])};
    observation.Instruction = static_cast<std::uint64_t>(state[REG_RIP]);
    observation.Stack = static_cast<std::uint64_t>(state[REG_RSP]);
#endif
    return observation;
}

inline void FaultHandler(const int number, siginfo_t* info, void* context) {
    auto& probe = FaultProbeState();
    auto observation = FaultContext(number, *info, *static_cast<const ucontext_t*>(context));
    if (observation.Instruction < probe.Code || observation.Instruction >= probe.Code + probe.Length) {
        signal(number, SIG_DFL);
        return;
    }
    observation.Faults = 1;
    probe.Observation = observation;
    siglongjmp(probe.Jump, 1);
}

inline FaultObservation RunFault(const Execution& execution, State& state, const std::uint8_t base, const FaultAccess access) {
    if (base >= state.Input.size() || base == 4 || base == 7) throw std::runtime_error("The fault probe base register is not loaded by the execution wrapper");
    std::vector<std::uint8_t> signalStack(64 * 1024);
    const auto page = PageSize();
    auto* const guarded = AllocatePages(page);
    if (!guarded) throw std::runtime_error("Cannot allocate the fault probe page");
    if (mprotect(guarded, page, access == FaultAccess::Store ? PROT_READ : PROT_NONE) != 0) {
        FreePages(guarded, page);
        throw std::runtime_error("Cannot protect the fault probe page");
    }
    stack_t alternate{};
    alternate.ss_sp = signalStack.data();
    alternate.ss_size = signalStack.size();
    alternate.ss_flags = 0;
    stack_t previousStack{};
    if (sigaltstack(&alternate, &previousStack) != 0) {
        FreePages(guarded, page);
        throw std::runtime_error("Cannot install the fault probe signal stack");
    }
    struct sigaction action {};
    action.sa_sigaction = FaultHandler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    struct sigaction previousSegv {};
    struct sigaction previousBus {};
    if (sigaction(SIGSEGV, &action, &previousSegv) != 0) {
        sigaltstack(&previousStack, nullptr);
        FreePages(guarded, page);
        throw std::runtime_error("Cannot install the fault probe handler");
    }
    if (sigaction(SIGBUS, &action, &previousBus) != 0) {
        sigaction(SIGSEGV, &previousSegv, nullptr);
        sigaltstack(&previousStack, nullptr);
        FreePages(guarded, page);
        throw std::runtime_error("Cannot install the fault probe handler");
    }
    auto& probe = FaultProbeState();
    probe.Observation = FaultObservation{};
    probe.Code = reinterpret_cast<std::uint64_t>(execution.Code());
    probe.Length = 4096;
    state.Input[base] = reinterpret_cast<std::uint64_t>(guarded);
    volatile bool returned = false;
    if (sigsetjmp(probe.Jump, 1) == 0) {
        execution.Run(state);
        returned = true;
    }
    sigaction(SIGSEGV, &previousSegv, nullptr);
    sigaction(SIGBUS, &previousBus, nullptr);
    sigaltstack(&previousStack, nullptr);
    FreePages(guarded, page);
    auto observation = probe.Observation;
    observation.Page = reinterpret_cast<std::uint64_t>(guarded);
    observation.PageLength = page;
    observation.Returned = returned;
    return observation;
}

#endif

#endif

}

#endif
