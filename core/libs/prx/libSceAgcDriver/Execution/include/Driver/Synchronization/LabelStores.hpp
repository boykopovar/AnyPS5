#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SYNCHRONIZATION_LABELSTORES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SYNCHRONIZATION_LABELSTORES_HPP

#include <cstddef>
#include <cstdint>

namespace AgcDriver::DriverDetail {

inline constexpr std::size_t LabelStoreHistory = 4;
inline constexpr std::size_t LabelStoreBytes = 64;

inline bool KeepsLabelStore(std::uint64_t address, std::size_t bytes) {
    return bytes != 0 && bytes <= LabelStoreBytes && bytes % 4 == 0 && address % 4 == 0;
}

}

#endif
