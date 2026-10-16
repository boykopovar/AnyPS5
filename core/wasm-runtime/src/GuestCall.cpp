#include "Thread.hpp"
#include "wasm-runtime/Guest.hpp"
#include "wasm-runtime/NidThunk.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <pthread.h>
#include <mutex>
#include <string>

namespace WasmRuntime {
namespace {

constexpr std::uint64_t ReturnSentinel = 0xF100;
constexpr unsigned MaxStackArguments = 16;

thread_local bool t_transfer = false;
thread_local VaList* t_vaList = nullptr;

std::map<std::string, const void*>& DataExports() {
    static std::map<std::string, const void*> exports;
    return exports;
}

std::mutex& DataExportLock() {
    static std::mutex lock;
    return lock;
}

void Store64(std::uint64_t address, std::uint64_t value) {
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), &value, sizeof(value));
}

std::uint64_t Load64(std::uint64_t address) {
    std::uint64_t value;
    std::memcpy(&value, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), sizeof(value));
    return value;
}

}  // namespace

void RegisterDataExport(const char* nid, const void* address) {
    std::lock_guard lock(DataExportLock());
    DataExports()[nid] = address;
}

const void* FindDataExport(const char* nid) {
    std::lock_guard lock(DataExportLock());
    const auto found = DataExports().find(nid);
    return found == DataExports().end() ? nullptr : found->second;
}

GuestResult CallGuest(std::uint64_t target, std::span<const GuestValue> arguments) {
    GuestCpu& cpu = CurrentCpu();
    const GuestCpu saved = cpu;
    unsigned integers = 0;
    unsigned floats = 0;
    std::uint64_t stackValues[MaxStackArguments];
    unsigned stackCount = 0;
    for (const auto& argument : arguments) {
        const bool integer = argument.kind == ValueKind::Integer;
        if (integer && integers < 6) {
            cpu.gpr[IntegerArgumentRegisters[integers++]] = argument.bits;
        } else if (!integer && floats < FloatArgumentRegisters) {
            std::memset(cpu.ymm[floats], 0, 16);
            std::memcpy(cpu.ymm[floats++], &argument.bits, argument.kind == ValueKind::Float32 ? 4 : 8);
        } else {
            if (stackCount == MaxStackArguments) GuestFault("guest call to 0x%llx passes too many stack arguments", static_cast<unsigned long long>(target));
            stackValues[stackCount++] = argument.bits;
        }
    }
    std::uint64_t rsp = (cpu.gpr[Rsp] - 128) & ~std::uint64_t{15};
    if (stackCount % 2 != 0) rsp -= 8;
    for (unsigned index = stackCount; index-- > 0;) {
        rsp -= 8;
        Store64(rsp, stackValues[index]);
    }
    rsp -= 8;
    Store64(rsp, ReturnSentinel);
    cpu.gpr[Rsp] = rsp;
    cpu.gpr[Rax] = floats;
    cpu.rip = target;
    cpu.stopRip = ReturnSentinel;
    Dispatch(cpu);
    GuestResult result;
    result.rax = cpu.gpr[Rax];
    result.rdx = cpu.gpr[Rdx];
    std::memcpy(&result.xmm0, cpu.ymm[0], sizeof(result.xmm0));
    cpu = saved;
    return result;
}

void RunGuestEntry(std::uint64_t entry, std::uint64_t stack) {
    GuestCpu& cpu = CurrentCpu();
    cpu.gpr[Rsp] = stack;
    cpu.rip = entry;
    cpu.stopRip = ReturnSentinel;
    Dispatch(cpu);
    std::exit(static_cast<int>(cpu.gpr[Rax]));
}

void TransferGuest(std::uint64_t rip, std::uint64_t rsp) {
    GuestCpu& cpu = CurrentCpu();
    cpu.rip = rip;
    cpu.gpr[Rsp] = rsp;
    t_transfer = true;
}

bool GuestTransferPending() {
    return t_transfer;
}

bool TakeGuestTransfer() {
    const bool transfer = t_transfer;
    t_transfer = false;
    return transfer;
}

VaList* GuestVaList() {
    if (!t_vaList) GuestFault("a variadic export read its guest arguments outside a guest call");
    return t_vaList;
}

void SetGuestVaList(VaList* list) {
    t_vaList = list;
}

void GuestFault(const char* format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    std::fputs("anyps5 runtime: ", stderr);
    std::vfprintf(stderr, format, arguments);
    std::fputc('\n', stderr);
    va_end(arguments);
    std::fflush(stderr);
    std::abort();
}

namespace Thunk {

void UnsupportedSignature(const char* name) {
    GuestFault("%s has a signature the guest call bridge cannot pass", name);
}

bool TraceCalls() {
    static const bool enabled = std::getenv("APS5_TRACE_NIDS") != nullptr;
    return enabled;
}

void TraceCall(const char* name, const GuestCpu& cpu) {
    std::fprintf(stderr, "[nid %lx] %s(%llx, %llx, %llx, %llx) from %llx\n", static_cast<unsigned long>(pthread_self()), name, static_cast<unsigned long long>(cpu.gpr[Rdi]), static_cast<unsigned long long>(cpu.gpr[Rsi]),
        static_cast<unsigned long long>(cpu.gpr[Rdx]), static_cast<unsigned long long>(cpu.gpr[Rcx]), static_cast<unsigned long long>(Load64(cpu.gpr[Rsp])));
}

void TraceReturn(const char* name, const GuestCpu& cpu) {
    std::fprintf(stderr, "[nid %lx] %s -> %llx\n", static_cast<unsigned long>(pthread_self()), name, static_cast<unsigned long long>(cpu.gpr[Rax]));
}

void ReportEscape(const char* name, const std::exception& error) {
    std::fprintf(stderr, "anyps5 runtime: %s threw into guest code: %s\n", name, error.what());
    std::fflush(stderr);
}

void ReturnToGuest(GuestCpu& cpu) {
    if (TakeGuestTransfer()) return;
    const auto rsp = cpu.gpr[Rsp];
    cpu.rip = Load64(rsp);
    cpu.gpr[Rsp] = rsp + 8;
}

}  // namespace Thunk

extern "C" [[clang::export_name("aps5_guest_indirect")]] std::uint64_t Aps5GuestIndirect(std::uint64_t target, std::uint32_t descriptor, std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5, std::uint64_t a6, std::uint64_t a7) {
    const std::uint64_t values[8] = {a0, a1, a2, a3, a4, a5, a6, a7};
    const unsigned count = descriptor & 0xF;
    if (count > 8) GuestFault("host code called guest address 0x%llx through a function type the guest call bridge cannot pass", static_cast<unsigned long long>(target));
    const unsigned floatMask = (descriptor >> 4) & 0xFF;
    const unsigned singleMask = (descriptor >> 12) & 0xFF;
    const unsigned resultKind = (descriptor >> 20) & 0x3;
    GuestValue arguments[8];
    for (unsigned index = 0; index < count; ++index) {
        arguments[index].bits = values[index];
        if (floatMask & (1u << index)) arguments[index].kind = (singleMask & (1u << index)) ? ValueKind::Float32 : ValueKind::Float64;
    }
    const auto result = CallGuest(target, std::span<const GuestValue>(arguments, count));
    return resultKind >= 2 ? result.xmm0 : result.rax;
}

}  // namespace WasmRuntime
