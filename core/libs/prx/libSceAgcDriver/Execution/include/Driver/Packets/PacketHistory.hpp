#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETHISTORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETHISTORY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>

namespace AgcDriver::DriverDetail {

struct PacketHistory {
    std::span<const std::uint32_t> commands;
    std::array<std::size_t, 64> offsets{};
    std::size_t count = 0;

    void Record(std::size_t offset);

    std::string Format(std::size_t offset) const;

    void Print(std::FILE* file, const char* format) const;
};

}

#endif
