#include "prx/libSceAgcDriver/Execution/include/AtomicMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace AgcDriver::Pm4 {

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename T>
T updated(std::uint32_t operation, T old, T source, T comparison) {
    using Signed = std::make_signed_t<T>;
    switch (operation) {
        case 0x07: return source;
        case 0x08: return old == comparison ? source : old;
        case 0x0f: return static_cast<T>(old + source);
        case 0x10: return static_cast<T>(old - source);
        case 0x11: return std::bit_cast<Signed>(old) < std::bit_cast<Signed>(source) ? old : source;
        case 0x12: return std::min(old, source);
        case 0x13: return std::bit_cast<Signed>(old) > std::bit_cast<Signed>(source) ? old : source;
        case 0x14: return std::max(old, source);
        case 0x15: return old & source;
        case 0x16: return old | source;
        case 0x17: return old ^ source;
        case 0x18: return old >= source ? T{0} : static_cast<T>(old + 1);
        case 0x19: return old == 0 || old > source ? source : static_cast<T>(old - 1);
        default: throw std::runtime_error("ATOMIC_MEM operation is not implemented");
    }
}

template <typename T>
void execute(const AtomicMemoryCommand& command) {
    auto* pointer = reinterpret_cast<T*>(static_cast<std::uintptr_t>(command.address));
    GuestMemory::GuardRead(command.address, sizeof(T));
    GuestMemory::FlushGpuWrites(command.address, sizeof(T));
    GuestMemory::CheckRange(pointer, sizeof(T), std::atomic_ref<T>::required_alignment, true);
    std::atomic_ref<T> word(*pointer);
    const auto source = static_cast<T>(command.source);
    const auto comparison = static_cast<T>(command.comparison);
    GuestMemory::StoreOwnBytes(command.address, sizeof(T), [&] {
        if (command.operation == 0x08) {
            auto expected = comparison;
            word.compare_exchange_strong(expected, source, std::memory_order_acq_rel, std::memory_order_acquire);
        } else {
            auto old = word.load(std::memory_order_acquire);
            while (!word.compare_exchange_weak(old, updated(command.operation, old, source, comparison), std::memory_order_acq_rel, std::memory_order_acquire)) {}
        }
    });
}

}

AtomicMemoryCommand DecodeAtomicMem(std::span<const std::uint32_t> packet) {
    require(packet.size() == 9 && (packet[0] & ~3u) == 0xc0071e00u, "invalid ATOMIC_MEM header or packet size");
    require((packet[1] & ~0xc6000f7fu) == 0 && (packet[8] & ~0x1fffu) == 0, "ATOMIC_MEM reserved control or interval bits are not implemented");
    require((packet[1] >> 30u) <= 1, "ATOMIC_MEM engine is not implemented");
    const auto command = (packet[1] >> 8u) & 0xfu;
    require(command <= 3, "reserved ATOMIC_MEM command");
    require(command != 1, "ATOMIC_MEM loop-until-compare command is not implemented");
    const auto atomic = packet[1] & 0x7fu;
    const auto operation = atomic & 0x1fu;
    require(operation == 0x07 || operation == 0x08 || (operation >= 0x0f && operation <= 0x19), "ATOMIC_MEM floating-point or reserved operation is not implemented");
    const auto bytes = (atomic & 0x20u) != 0 ? 8u : 4u;
    const auto address = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u);
    require(address != 0 && address % bytes == 0, "null or misaligned ATOMIC_MEM destination");
    require(address <= std::numeric_limits<std::uint64_t>::max() - bytes, "ATOMIC_MEM destination range overflow");
    return {address, packet[4] | (static_cast<std::uint64_t>(packet[5]) << 32u), packet[6] | (static_cast<std::uint64_t>(packet[7]) << 32u), operation, bytes};
}

void ExecuteAtomicMem(std::span<const std::uint32_t> packet) {
    const auto command = DecodeAtomicMem(packet);
    if (command.bytes == 8) execute<std::uint64_t>(command);
    else execute<std::uint32_t>(command);
}

}
