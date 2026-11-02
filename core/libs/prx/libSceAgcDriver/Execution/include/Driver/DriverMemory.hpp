#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRIVERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRIVERMEMORY_HPP

#ifdef __APPLE__

#include <cstdint>

namespace AgcDriver::DriverDetail {

constexpr std::uint64_t Ps5DriverMemoryBase = 0xfe0000000ull;
constexpr std::uint64_t MacOsDriverMemoryBase = 0xee0000000ull;
constexpr std::uint64_t DriverMemorySpan = 2u << 20u;

constexpr std::uint64_t RetargetPs5DriverMemory(std::uint64_t address) {
    return address >= Ps5DriverMemoryBase && address - Ps5DriverMemoryBase < DriverMemorySpan ? address - Ps5DriverMemoryBase + MacOsDriverMemoryBase : address;
}

}

#endif

#endif
