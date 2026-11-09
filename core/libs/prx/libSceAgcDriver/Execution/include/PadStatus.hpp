#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PADSTATUS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PADSTATUS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace AgcDriver {

inline constexpr std::size_t PadStatusSlots = 4;

enum class PadKind : std::uint8_t { Unknown, DualSense, DualShock4, DualShock3, Xbox, Switch, Other };

enum class PadPower : std::uint8_t { Unknown, Empty, Low, Medium, Full, Wired };

struct PadSlotStatus {
    bool connected = false;
    PadKind kind = PadKind::Unknown;
    std::string name;
    PadPower power = PadPower::Unknown;

    bool operator==(const PadSlotStatus&) const = default;
};

struct PadStatusSnapshot {
    std::array<PadSlotStatus, PadStatusSlots> slots;

    bool operator==(const PadStatusSnapshot&) const = default;
};

struct PadStatusProvider {
    std::function<PadStatusSnapshot()> read;
};

}

#endif
