#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_SIGNALEVENTS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_SIGNALEVENTS_HPP

#include <cstdint>

namespace GuestSignals {

std::uint64_t Generation(std::uintptr_t signal);

}

#endif
