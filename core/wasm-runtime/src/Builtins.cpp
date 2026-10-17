#include "Program.hpp"
#include "Thread.hpp"
#include "wasm-runtime/Guest.hpp"
#include "wasm-runtime/Host.hpp"
#include "wasm-runtime/NidThunk.hpp"
#include <emscripten.h>
#include <array>
#include <cstring>
#include <mutex>

using namespace WasmRuntime;

namespace {

struct CpuidLeaf {
    std::uint32_t leaf;
    std::uint32_t eax;
    std::uint32_t ebx;
    std::uint32_t ecx;
    std::uint32_t edx;
};

constexpr std::uint32_t Bit(unsigned index) {
    return 1u << index;
}

constexpr std::uint32_t Text(const char (&text)[5]) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(text[0])) | static_cast<std::uint32_t>(static_cast<unsigned char>(text[1])) << 8 |
           static_cast<std::uint32_t>(static_cast<unsigned char>(text[2])) << 16 | static_cast<std::uint32_t>(static_cast<unsigned char>(text[3])) << 24;
}

constexpr std::array CpuidLeaves = {
    CpuidLeaf{0x00000000, 0x0000000D, Text("Auth"), Text("cAMD"), Text("enti")},
    CpuidLeaf{0x00000001, 0x00870F10, 0x00080800,
        Bit(0) | Bit(9) | Bit(12) | Bit(13) | Bit(19) | Bit(20) | Bit(22) | Bit(23) | Bit(26) | Bit(27) | Bit(28),
        Bit(0) | Bit(4) | Bit(8) | Bit(15) | Bit(19) | Bit(23) | Bit(24) | Bit(25) | Bit(26) | Bit(28)},
    CpuidLeaf{0x00000007, 0, Bit(3) | Bit(5) | Bit(8) | Bit(9), 0, 0},
    CpuidLeaf{0x0000000D, 0x00000007, 0x00000340, 0x00000340, 0},
    CpuidLeaf{0x80000000, 0x80000008, Text("Auth"), Text("cAMD"), Text("enti")},
    CpuidLeaf{0x80000001, 0x00870F10, 0, Bit(0) | Bit(5) | Bit(8), Bit(11) | Bit(20) | Bit(29)},
    CpuidLeaf{0x80000002, Text("AnyP"), Text("S5 w"), Text("asm "), Text("gues")},
    CpuidLeaf{0x80000003, Text("t CP"), Text("U\0\0\0"), 0, 0},
    CpuidLeaf{0x80000004, 0, 0, 0, 0},
    CpuidLeaf{0x80000008, 0x00003030, 0, 0x00000007, 0},
};

constexpr std::uint64_t ZeroFlag = 0x40;

std::mutex g_cmpxchg16bLock;

std::uint64_t Load64(std::uint64_t address) {
    std::uint64_t value;
    std::memcpy(&value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), sizeof(value));
    return value;
}

void Store64(std::uint64_t address, std::uint64_t value) {
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), &value, sizeof(value));
}

struct TlsIndex {
    std::uint64_t module;
    std::uint64_t offset;
};

std::uint64_t StackCheckGuard = 0x2F8A61C35D0E4B97;

[[noreturn]] void StackCheckFail() {
    GuestFault("the guest detected a stack buffer overflow");
}

void* TlsGetAddr(const TlsIndex* index) {
    const auto& program = CurrentProgram();
    if (index->module == 0 || index->module > program.tls.offsets.size()) GuestFault("__tls_get_addr: module %llu has no TLS block", static_cast<unsigned long long>(index->module));
    const auto address = static_cast<std::int64_t>(ThreadPointer()) + program.tls.offsets[index->module - 1] + static_cast<std::int64_t>(index->offset);
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(address));
}

}  // namespace

std::uint64_t WasmRuntime::ReadTsc() {
    return static_cast<std::uint64_t>(emscripten_get_now() * 1'000'000.0);
}

extern "C" {

[[clang::export_name("aps5.cpuid")]] void Aps5Cpuid(GuestCpu* cpu) {
    const auto leaf = static_cast<std::uint32_t>(cpu->gpr[Rax]);
    const auto subleaf = static_cast<std::uint32_t>(cpu->gpr[Rcx]);
    CpuidLeaf result{leaf, 0, 0, 0, 0};
    for (const auto& known : CpuidLeaves) {
        if (known.leaf == leaf && ((leaf != 0x07 && leaf != 0x0D) || subleaf == 0)) result = known;
    }
    cpu->gpr[Rax] = result.eax;
    cpu->gpr[Rbx] = result.ebx;
    cpu->gpr[Rcx] = result.ecx;
    cpu->gpr[Rdx] = result.edx;
}

[[clang::export_name("aps5.rdtsc")]] void Aps5Rdtsc(GuestCpu* cpu) {
    const auto tsc = ReadTsc();
    cpu->gpr[Rax] = tsc & 0xFFFFFFFFu;
    cpu->gpr[Rdx] = tsc >> 32;
}

[[clang::export_name("aps5.cmpxchg16b")]] void Aps5Cmpxchg16b(GuestCpu* cpu, std::uint64_t address) {
    std::lock_guard lock(g_cmpxchg16bLock);
    const auto low = Load64(address);
    const auto high = Load64(address + 8);
    if (low == cpu->gpr[Rax] && high == cpu->gpr[Rdx]) {
        Store64(address, cpu->gpr[Rbx]);
        Store64(address + 8, cpu->gpr[Rcx]);
        cpu->rflags |= ZeroFlag;
    } else {
        cpu->gpr[Rax] = low;
        cpu->gpr[Rdx] = high;
        cpu->rflags &= ~ZeroFlag;
    }
}

[[clang::export_name("aps5.unresolved_indirect")]] void Aps5UnresolvedIndirect(GuestCpu* cpu, std::uint64_t address) {
    GuestFault("guest branch to 0x%llx has no translated code (rsp 0x%llx, return address 0x%llx)", static_cast<unsigned long long>(address),
        static_cast<unsigned long long>(cpu->gpr[Rsp]), static_cast<unsigned long long>(Load64(cpu->gpr[Rsp])));
}

[[clang::export_name("aps5.unsupported")]] void Aps5Unsupported(GuestCpu*, std::uint64_t address) {
    GuestFault("guest instruction at 0x%llx is not supported", static_cast<unsigned long long>(address));
}

}

APS5_WASM_NID(aps5_nid_tls_get_addr, "__tls_get_addr", TlsGetAddr)
APS5_WASM_NID(aps5_nid_stack_chk_fail, "Ou3iL1abvng", StackCheckFail)
APS5_WASM_DATA(aps5_data_stack_chk_guard, "f7uOxY9mM1U", StackCheckGuard)
