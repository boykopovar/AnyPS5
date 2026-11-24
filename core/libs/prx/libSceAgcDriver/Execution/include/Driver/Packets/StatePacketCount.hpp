#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETS_STATEPACKETCOUNT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_PACKETS_STATEPACKETCOUNT_HPP

#include <cstdint>

namespace AgcDriver::DriverDetail {

void AddStatePackets(std::uint64_t count);
std::uint64_t StatePackets();

}

#endif
