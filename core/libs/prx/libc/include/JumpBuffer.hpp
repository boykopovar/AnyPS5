#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_JUMPBUFFER_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_JUMPBUFFER_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

namespace JumpBuffer {

using Sigprocmask = int (APS5_VABI*)(int how, const void* set, void* previousSet);

extern "C" {

void GuestSigprocmaskRegister_nid_no_patch(Sigprocmask sigprocmask);
void APS5_VABI JumpBufferSaveMask_nid_no_patch(unsigned char* buffer);
void APS5_VABI JumpBufferRestoreMask_nid_no_patch(const unsigned char* buffer);

}

}

#endif
