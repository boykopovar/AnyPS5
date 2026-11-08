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
#define LOWERING_EXECUTION_AVAILABLE 1
#elif defined(_WIN32) && defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <windows.h>
#undef far
#undef near
#define LOWERING_EXECUTION_AVAILABLE 1
#else
#define LOWERING_EXECUTION_AVAILABLE 0
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

inline std::size_t AppendHostEntry(Bytes& bytes) {
#if defined(_WIN32)
    const auto entry = bytes.size();
    bytes.insert(bytes.end(), {0x56, 0x57, 0x48, 0x83, 0xEC, 0x08, 0x48, 0x89, 0xCF, 0xE8});
    const auto displacement = static_cast<std::int32_t>(0 - static_cast<std::int64_t>(bytes.size() + 4));
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(displacement) >> shift));
    bytes.insert(bytes.end(), {0x48, 0x83, 0xC4, 0x08, 0x5F, 0x5E, 0xC3});
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

#endif

}

#endif
