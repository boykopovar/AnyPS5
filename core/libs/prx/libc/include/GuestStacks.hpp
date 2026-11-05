#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTSTACKS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTSTACKS_HPP

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace GuestStacks {

extern "C" {

void GuestStackRegister_nid_no_patch(const void* pointer, std::size_t bytes);
void APS5_VABI GuestStackSwitch_nid_no_patch(std::uintptr_t stackPointer);

}

}

#endif
