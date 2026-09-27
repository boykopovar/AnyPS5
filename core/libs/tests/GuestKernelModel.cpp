#include "prx/libkernel/HostRuntime/GuestKernelModel.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    GuestKernelModel guest(4321);
    guest.MapUser(0x1000, 0x100);
    guest.MapKernel(0x80000000, 0x100);
    const std::array<std::byte, 4> payload{std::byte{0x50}, std::byte{0x53}, std::byte{0x35}, std::byte{0}};
    std::array<std::byte, 4> result{};
    Require(guest.WriteUser(0x1010, payload));
    Require(guest.CopyKernelIn(0x80000020, payload));
    Require(guest.CopyKernelOut(0x80000020, result) && result == payload);
    Require(!guest.CopyKernelOut(0x800000ff, result));
    Require(!guest.WriteUser(0x10ff, payload));
    bool overlapRejected = false;
    try { guest.MapKernel(0x800000ff, 2); }
    catch (const std::invalid_argument&) { overlapRejected = true; }
    Require(overlapRejected);

    // The SDK CRT starts with getpid and a firmware/object query (649).
    // The latter must fail explicitly until a real firmware profile exists.
    Require(guest.Dispatch(20, {}) == 4321);
    Require(guest.Dispatch(649, {2, 8, 0x1020}) == -78);
    Require(guest.Dispatch(542, {0x1000, 0}) == 0);
    std::array<std::byte, 8> descriptors{};
    Require(guest.ReadUser(0x1000, descriptors));
    const auto readFd = std::to_integer<int>(descriptors[0]);
    const auto writeFd = std::to_integer<int>(descriptors[4]);
    Require(readFd == 16 && writeFd == 17);
    Require(guest.Dispatch(4, {static_cast<std::uint64_t>(writeFd), 0x1010, payload.size()}) == 4);
    Require(guest.Dispatch(3, {static_cast<std::uint64_t>(readFd), 0x1020, payload.size()}) == 4);
    Require(guest.ReadUser(0x1020, result) && result == payload);
    Require(guest.Dispatch(3, {static_cast<std::uint64_t>(readFd), 0x1020, 1}) == -35);
    Require(guest.Dispatch(4, {static_cast<std::uint64_t>(writeFd), 0x10ff, payload.size()}) == -14);
    Require(guest.Dispatch(6, {static_cast<std::uint64_t>(writeFd)}) == 0);
    Require(guest.Dispatch(3, {static_cast<std::uint64_t>(readFd), 0x1020, 1}) == 0);
    Require(guest.Dispatch(4, {static_cast<std::uint64_t>(writeFd), 0x1010, 1}) == -9);
    Require(guest.Dispatch(542, {0x10ff, 0}) == -14);
}
