#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>

namespace AgcDriver::GuestMemory {
namespace {

thread_local ReadSite t_readSite = ReadSite::Unknown;
thread_local PacketTag t_packet{NoPacket, 0};

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("AGC driver: ") + reason);
}

}  // namespace

void CheckRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    static_cast<void>(writable);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    Require(pointer != nullptr || bytes == 0, "null guest memory range");
    Require(alignment == 0 || address % alignment == 0, "misaligned guest memory range");
    Require(bytes <= ~std::uintptr_t{0} - address, "guest memory range wraps around");
}

bool Accessible(const void* pointer, std::size_t bytes, bool writable) {
    static_cast<void>(writable);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return pointer != nullptr && bytes <= ~std::uintptr_t{0} - address;
}

void Read(std::uint64_t address, std::span<std::byte> destination, std::size_t alignment) {
    CheckRange(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), destination.size(), alignment);
    std::memcpy(destination.data(), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), destination.size());
}

void Write(std::uint64_t address, std::span<const std::byte> source, std::size_t alignment) {
    CheckRange(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), source.size(), alignment, true);
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), source.data(), source.size());
}

void FlushGpuWrites(std::uint64_t, std::size_t) {}

void SetCurrentPacket(std::uint32_t opcode, std::uint32_t queue) {
    t_packet = {opcode, queue};
}

PacketTag CurrentPacket() {
    return t_packet;
}

ReadSite SetReadSite(ReadSite site) {
    const auto previous = t_readSite;
    t_readSite = site;
    return previous;
}

ReadSite CurrentReadSite() {
    return t_readSite;
}

}  // namespace AgcDriver::GuestMemory

extern "C" void AgcDriverCheckGuestMemory_nid_postfix(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable) {
    AgcDriver::GuestMemory::CheckRange(pointer, bytes, alignment, writable);
}

extern "C" void AgcDriverStoreGuestLabel_nid_postfix(std::uint64_t* pointer, std::uint64_t value) {
    std::atomic_ref<std::uint64_t>(*pointer).store(value, std::memory_order_release);
}
