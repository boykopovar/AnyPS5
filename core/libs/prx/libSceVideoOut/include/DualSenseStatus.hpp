#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_DUALSENSESTATUS_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_DUALSENSESTATUS_HPP

#include <cstddef>
#include <cstdint>

namespace DualSenseStatus {

struct TriggerStatus {
    std::uint8_t leftEffect = 0;
    std::uint8_t leftStatus = 0;
    std::uint8_t rightEffect = 0;
    std::uint8_t rightStatus = 0;
};

inline bool Parse(const std::uint8_t* report, std::size_t size, TriggerStatus& status) {
    if (report == nullptr || size == 0) return false;
    std::size_t state = 0;
    if (report[0] == 0x01 && size >= 64) state = 1;
    else if (report[0] == 0x31 && size >= 78) state = 2;
    else return false;
    status.rightStatus = static_cast<std::uint8_t>(report[state + 41] >> 4u);
    status.leftStatus = static_cast<std::uint8_t>(report[state + 42] >> 4u);
    status.rightEffect = static_cast<std::uint8_t>(report[state + 47] & 0x0Fu);
    status.leftEffect = static_cast<std::uint8_t>(report[state + 47] >> 4u);
    return true;
}

}

#endif
