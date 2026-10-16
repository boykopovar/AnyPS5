#include "wasm-runtime/Guest.hpp"
#include "wasm-runtime/NidThunk.hpp"
#include <cstdio>
#include <cstring>

namespace {

using WasmRuntime::CurrentCpu;

constexpr unsigned SavedRegisters[] = {WasmRuntime::Rbx, WasmRuntime::Rbp, WasmRuntime::R12, WasmRuntime::R13, WasmRuntime::R14, WasmRuntime::R15};

void HostPrintLong(long value) {
    std::printf("print %ld\n", value);
    std::fflush(stdout);
}

long HostAdd(long first, long second) {
    return first + second;
}

double HostScale(double value, long factor) {
    return value * static_cast<double>(factor);
}

long HostApply(long (*function)(long), long value) {
    return function(value);
}

long HostSave(unsigned long* buffer) {
    auto& cpu = CurrentCpu();
    const auto rsp = cpu.gpr[WasmRuntime::Rsp];
    std::memcpy(&buffer[0], reinterpret_cast<const void*>(static_cast<std::uintptr_t>(rsp)), sizeof(buffer[0]));
    buffer[1] = rsp + 8;
    for (unsigned slot = 0; slot < 6; ++slot) buffer[2 + slot] = cpu.gpr[SavedRegisters[slot]];
    return 0;
}

long HostRestore(unsigned long* buffer, long value) {
    auto& cpu = CurrentCpu();
    for (unsigned slot = 0; slot < 6; ++slot) cpu.gpr[SavedRegisters[slot]] = buffer[2 + slot];
    cpu.gpr[WasmRuntime::Rax] = static_cast<std::uint64_t>(value);
    WasmRuntime::TransferGuest(buffer[0], buffer[1]);
    return value;
}

}  // namespace

APS5_WASM_NID(aps5_test_print_long, "host_print_long", HostPrintLong)
APS5_WASM_NID(aps5_test_add, "host_add", HostAdd)
APS5_WASM_NID(aps5_test_scale, "host_scale", HostScale)
APS5_WASM_NID(aps5_test_apply, "host_apply", HostApply)
APS5_WASM_NID(aps5_test_save, "host_save", HostSave)
APS5_WASM_NID(aps5_test_restore, "host_restore", HostRestore)
